/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is 
 * Crocodile Clips Ltd..
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
 *    Bradley Baetz <bbaetz@cs.mcgill.ca>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#include "nsSVGRenderingContext.h"
#include "nsSVGRenderItem.h"
#include "nsIDeviceContext.h"
#include "nsGfxCIID.h"
#include "nsIRegion.h"

#include "libart-incs.h"

////////////////////////////////////////////////////////////////////////
// nsSVGRenderingContext

nsSVGRenderingContext::nsSVGRenderingContext(nsIPresContext* presContext,
                                             nsIRenderingContext *renderingContext,
                                             const nsRect& dirtyRectTwips)
    : mRenderingContext(renderingContext),
      mPresContext(presContext),
      mDirtyRect(dirtyRectTwips),
      mDirtyRectTwips(dirtyRectTwips),
      mTempBuffer(nsnull)
{
  float twipsToPixels;
  mPresContext->GetTwipsToPixels(&twipsToPixels);
  mDirtyRect *= twipsToPixels;
  if (mDirtyRect.width < 1)
    mDirtyRect.width = 1;

  if (mDirtyRect.height < 1)
    mDirtyRect.height = 1;

  // Set up our buffer. Do it here, because this is *SLOW* on some
  // platforms
  InitializeBuffer();
}

nsSVGRenderingContext::~nsSVGRenderingContext()
{

}

//----------------------------------------------------------------------

void
nsSVGRenderingContext::PaintSVGRenderItem(nsSVGRenderItem* item)
{
  if (!item->GetSvp()) return;
  
  ArtRender* render = NewRender();
  if (!render) return;
  
  art_render_mask_solid(render, (int)(0x10000 * item->GetOpacity()));

  art_render_svp(render, item->GetSvp());

  nscolor rgb = item->GetColor();
  ArtPixMaxDepth col[3];

  col[0] = ART_PIX_MAX_FROM_8(NS_GET_R(rgb));
  col[1] = ART_PIX_MAX_FROM_8(NS_GET_G(rgb));
  col[2] = ART_PIX_MAX_FROM_8(NS_GET_B(rgb));
  
  art_render_image_solid(render, col);
  
  InvokeRender(render);
}

void nsSVGRenderingContext::ClearBuffer(nscolor color)
{
  // XXX - should use art_render_clear, but that crashes on non-24 bit
  // displays, still
  nsIRenderingContext* ctx = LockMozRenderingContext();
  ctx->PushState();
  ctx->SetColor(color);
  ctx->FillRect(mDirtyRectTwips);
  PRBool aClipEmpty;
  ctx->PopState(aClipEmpty);
  UnlockMozRenderingContext();
}

nsIRenderingContext* nsSVGRenderingContext::LockMozRenderingContext()
{
  NS_ASSERTION(mTempBuffer==0, "nested LockMozRenderingContext() calls?");
  mRenderingContext->GetDrawingSurface(&mTempBuffer);
  mBuffer->Unlock(); // We don't own this any more
  mRenderingContext->SelectOffScreenDrawingSurface(mBuffer);
  
  // prepare the context for the new surface:
  mRenderingContext->PushState();
  nsTransform2D* xform;
  mRenderingContext->GetCurrentTransform(xform);

  xform->SetTranslation((float)-mDirtyRect.x, (float)-mDirtyRect.y);

  // setting the clip rectangle is essential here. If we don't, and
  // none of the saved parent states has a valid clip rect, then bad
  // things can happen if our caller (usually a <foreignObject>) sets
  // up a clip rect. In that case, the rendering context (at least on
  // Windows) doesn't know which clip area to restore when the state
  // is popped and we're stuck with the caller's clip rect for all
  // eternity. The result is erratic redraw-behaviour...
  PRBool aClipEmpty;
  mRenderingContext->SetClipRect(mDirtyRectTwips, nsClipCombine_kReplace, aClipEmpty);
  
  return mRenderingContext;
}

void nsSVGRenderingContext::UnlockMozRenderingContext()
{
  NS_ASSERTION(mTempBuffer, "no drawing surface to restore");
  PRBool aClipEmpty;
  mRenderingContext->PopState(aClipEmpty);
  mRenderingContext->SelectOffScreenDrawingSurface(mTempBuffer);
  mTempBuffer = nsnull;
  
  PRInt32 bytesPerWidth;
  // Reclaim ownership
  mBuffer->Lock(0, 0, mDirtyRect.width, mDirtyRect.height,(void **)&mBitBuf,
                &mStride, &bytesPerWidth,0);
}

// Define this to get the image dumped to file. This is slow.
// Its _really_ useful for debugging, though
//#define DUMP_IMAGE

void nsSVGRenderingContext::DumpImage() {
#ifdef DUMP_IMAGE
  static int numOut=0;
  char buf[30];
  sprintf(buf,"svg%d.ppm",numOut);
  ++numOut;

  printf("dumping %s\n",buf);

  FILE* f = fopen(buf, "wb");
  
  PRUint32 width = mDirtyRect.width;
  PRUint32 height = mDirtyRect.height;

  fprintf(f,"P3\n%d %d\n255\n",width,height);

  nsPixelFormat format;
  mBuffer->GetPixelFormat(&format);

  for(int row=0; row < mDirtyRect.height;++row) {
    for (int col=0;col<mDirtyRect.width;++col) {
      PRUint16 val = *((PRUint16*)(mBitBuf+row*mStride+col*2));
      PRUint8 rgb[3];
      rgb[0] = (val&format.mRedMask) >> format.mRedShift;
      rgb[1] = (val&format.mGreenMask) >> format.mGreenShift;
      rgb[2] = (val&format.mBlueMask) >> format.mBlueShift;
      fprintf(f,"%d %d %d  ",rgb[0],rgb[1],rgb[2]);
      //fprintf(f,"%d ",val);
    }
    fprintf(f,"\n");
  }

  fclose(f);
#endif
}

void nsSVGRenderingContext::Render()
{
  DumpImage();
  mBuffer->Unlock(); // OK, its not ours any more

  /* This is usefull to debug clip region problems
  PRInt32 bytesPerWidth;
  mBuffer->Lock(0, 0, mDirtyRect.width, mDirtyRect.height,(void **)&mBitBuf,
                &mStride, &bytesPerWidth,0);
  DumpImage();
  mBuffer->Unlock();
  */

  PRBool clip;
  mRenderingContext->PopState(clip);

  mRenderingContext->CopyOffScreenBits(mBuffer, 0, 0, mDirtyRectTwips,
                                       NS_COPYBITS_TO_BACK_BUFFER |
                                       NS_COPYBITS_XFORM_DEST_VALUES);
}

//----------------------------------------------------------------------
// implementation helpers:


ArtRender* nsSVGRenderingContext::NewRender()
{
  ArtRender* render=nsnull;
  
  nsPixelFormat format;
  mBuffer->GetPixelFormat(&format);

  render = art_render_new(mDirtyRect.x, mDirtyRect.y,
                          mDirtyRect.x+ mDirtyRect.width,
                          mDirtyRect.y+ mDirtyRect.height,
                          mBitBuf,
                          mStride,
                          NS_STATIC_CAST(ArtDestinationPixelFormat,
                                         mArtPixelFormat),
                          ART_ALPHA_NONE, // alpha
                          NULL);

  static int foo=0;
  //art_render_clear_rgb(render,NS_RGB(0,255,foo?255:0));
  foo = 1-foo;
  return render;
}

void nsSVGRenderingContext::InvokeRender(ArtRender* render)
{
  art_render_invoke(render); // also frees the render
}


void nsSVGRenderingContext::InitializeBuffer()
{
  // OK. We need to reset the clip rect. I think this sucks:
  // a) I shouldn't have to do this manually
  // b) I should be able to clear a clip rect directly...
  // That would probably speed things up, too, because (at least with X)
  // I'd be able to reuse the GC
  // Apparently (a) isn't a bug, though
  mRenderingContext->PushState();

  nsRect r;
  r.x = r.y = 0;
  r.width = mDirtyRectTwips.width;
  r.height = mDirtyRectTwips.height;

  nsTransform2D* xform;
  mRenderingContext->GetCurrentTransform(xform);
  xform->TransformCoord(&r.x,&r.y);
  
  // And -> twips!
  float twipsPerPx;
  mPresContext->GetPixelsToTwips(&twipsPerPx);
  r.x *= (int)-twipsPerPx;
  r.y *= (int)-twipsPerPx;

  PRBool clipEmpty;
  mRenderingContext->SetClipRect(r,nsClipCombine_kReplace,clipEmpty);

  mRenderingContext->CreateDrawingSurface(&mDirtyRect,
                                          NS_CREATEDRAWINGSURFACE_FOR_PIXEL_ACCESS,
                                          (void*&)*getter_AddRefs(mBuffer));
  nsCOMPtr<nsIDeviceContext> dc;
  mRenderingContext->GetDeviceContext(*getter_AddRefs(dc));
  	
  	unsigned int depth;
  	dc->GetDepth(depth);
  	
  	switch (depth) {
  	case 16: {
  		nsPixelFormat format;
  		mBuffer->GetPixelFormat(&format);
  		
  		if (format.mGreenCount == 5)
  			mArtPixelFormat = ART_PF_555;
  		else
  			mArtPixelFormat = ART_PF_565;
  			
  		} break;
  	
  	case 24:
  		mArtPixelFormat = ART_PF_RGB8;
  		break;
  			
  	case 32:	
  		// need to be sure it isn't RGBA8 ?
  		mArtPixelFormat = ART_PF_ARGB8;
  		break;
  		
  	default:
  		 NS_ASSERTION(false, "unsupported pixel depth");
  	}
    
    PRInt32 bytesPerWidth;

    // And set up the locked bits
    //RenderingContext->PushState();
    mBuffer->Lock(0, 0,
                  mDirtyRect.width, mDirtyRect.height,(void **)&mBitBuf,
                  &mStride, &bytesPerWidth,0);

    ClearBuffer(NS_RGB(255,255,255));
}
