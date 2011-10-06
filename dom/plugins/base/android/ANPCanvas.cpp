/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Android NPAPI support code
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Doug Turner <dougt@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "assert.h"
#include "ANPBase.h"
#include <android/log.h>

#include "cairo.h"
#include "gfxPlatform.h"
#include "gfxASurface.h"
#include "gfxImageSurface.h"
#include "gfxUtils.h"
#include "gfxContext.h"

#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "GeckoPlugins" , ## args)
#define ASSIGN(obj, name)   (obj)->name = anp_canvas_##name


ANPCanvas*
anp_canvas_newCanvas(const ANPBitmap* bitmap)
{
  PRUint32 stride;
  gfxASurface::gfxImageFormat  format;

  if (bitmap->format == ANPBitmapFormats::kRGBA_8888_ANPBitmapFormat) {
    stride = bitmap->width * 4;
    format = gfxImageSurface::ImageFormatARGB32;
  }
  else if (bitmap->format == ANPBitmapFormats::kRGB_565_ANPBitmapFormat) {
    stride = bitmap->width * 2;
    format = gfxImageSurface::ImageFormatRGB16_565;
  }
  else {
    LOG("%s -- Unknown format", __PRETTY_FUNCTION__);
    return nsnull;
  }

  gfxImageSurface* pluginSurface = new gfxImageSurface(static_cast<unsigned char*>(bitmap->baseAddr),
                                                       gfxIntSize(bitmap->width,  bitmap->height), 
                                                       stride,
                                                       format);
  if (pluginSurface->CairoStatus()) {
    LOG("%s -- %d x %d FAILED to create gfxImageSurface", __PRETTY_FUNCTION__, bitmap->width, bitmap->height);
    return nsnull;
  }

  gfxContext *pluginContext = new gfxContext(pluginSurface);
  NS_ADDREF(pluginContext);
  return (ANPCanvas*) pluginContext;
}

void
anp_canvas_deleteCanvas(ANPCanvas* canvas)
{
  if (!canvas)
    return;
  gfxContext *ctx = (gfxContext*)canvas;
  NS_RELEASE( ctx );
}

void
anp_canvas_save(ANPCanvas* canvas)
{
  if (!canvas)
    return;

  gfxContext* ctx = (gfxContext*)canvas;
  ctx->Save();
}

void
anp_canvas_restore(ANPCanvas* canvas)
{
  if (!canvas)
    return;

  gfxContext* ctx = (gfxContext*)canvas;
  ctx->Restore();
}

void
anp_canvas_translate(ANPCanvas* canvas, float tx, float ty)
{
  if (!canvas)
    return;

  gfxContext* ctx = (gfxContext*)canvas;
  ctx->Translate(gfxPoint(tx,ty));
}

void
anp_canvas_scale(ANPCanvas* canvas, float sx, float sy)
{
  if (!canvas)
    return;

  gfxContext* ctx = (gfxContext*)canvas;
  ctx->Scale(sx, sy);
}

void
anp_canvas_rotate(ANPCanvas* canvas, float degrees)
{
  if (!canvas)
    return;

  gfxContext* ctx = (gfxContext*)canvas;
  ctx->Rotate(degrees);
}

void
anp_canvas_skew(ANPCanvas* canvas, float kx, float ky)
{
  if (!canvas)
    return;

  gfxContext* ctx = (gfxContext*)canvas;
  LOG("%s is not impl.", __PRETTY_FUNCTION__);
}

void
anp_canvas_concat(ANPCanvas* canvas, const ANPMatrix*)
{
  if (!canvas)
    return;

  gfxContext* ctx = (gfxContext*)canvas;
  LOG("%s is not impl.", __PRETTY_FUNCTION__);
}

void
anp_canvas_clipRect(ANPCanvas* canvas, const ANPRectF* r)
{
  if (!canvas)
    return;

  gfxContext* ctx = (gfxContext*)canvas;
  ctx->Clip(gfxRect(r->left,
                    r->top,
                    r->right - r->left,
                    r->bottom - r->top));
}

void
anp_canvas_clipPath(ANPCanvas* canvas, const ANPPath*)
{
  if (!canvas)
    return;

  gfxContext* ctx = (gfxContext*)canvas;
  LOG("%s is not impl.", __PRETTY_FUNCTION__);
}

void
anp_canvas_getTotalMatrix(ANPCanvas* canvas, ANPMatrix*)
{
  if (!canvas)
    return;

  gfxContext* ctx = (gfxContext*)canvas;
  LOG("%s is not impl.", __PRETTY_FUNCTION__);
}

bool
anp_canvas_getLocalClipBounds(ANPCanvas* canvas, ANPRectF* bounds, bool aa)
{
  if (!canvas)
    return false;

  gfxContext* ctx = (gfxContext*)canvas;
  LOG("%s is not impl.", __PRETTY_FUNCTION__);
  return false;
}

bool
anp_canvas_getDeviceClipBounds(ANPCanvas* canvas, ANPRectI* bounds)
{
  if (!canvas)
    return false;

  gfxContext* ctx = (gfxContext*)canvas;
  LOG("%s is not impl.", __PRETTY_FUNCTION__);
  return false;
}

void
anp_canvas_drawColor(ANPCanvas* canvas, ANPColor c)
{
  if (!canvas)
    return;

  gfxContext* ctx = (gfxContext*)canvas;
  ctx->SetDeviceColor(gfxRGBA(c, gfxRGBA::PACKED_ARGB));
  LOG("returning from %s", __PRETTY_FUNCTION__);
}

void
anp_canvas_drawPaint(ANPCanvas* canvas, const ANPPaint* paint)
{
  if (!canvas)
    return;

  gfxContext* ctx = (gfxContext*)canvas;
  LOG("%s", "                    **************   NOT IMPLEMENTED!!!");
}

void
anp_canvas_drawLine(ANPCanvas* canvas, float x0, float y0, float x1, float y1,
                    const ANPPaint* paint)
{
  if (!canvas)
    return;

  gfxContext* ctx = (gfxContext*)canvas;
  ctx->NewPath();
  ctx->SetColor(((ANPPaintPrivate*)paint)->color);
  ctx->Line(gfxPoint(x0, y0), gfxPoint(x1, y1));
  ctx->Fill();
}

void
anp_canvas_drawRect(ANPCanvas* canvas, const ANPRectF* r, const ANPPaint* paint)
{
  if (!canvas)
    return;

  gfxContext* ctx = (gfxContext*)canvas;

  ctx->NewPath();
  ctx->SetColor(((ANPPaintPrivate*)paint)->color);
  ctx->Rectangle(gfxRect(r->left,
                         r->top,
                         r->right - r->left,
                         r->bottom - r->top));
  ctx->Fill();
}

void
anp_canvas_drawOval(ANPCanvas* canvas, const ANPRectF* r, const ANPPaint* paint)
{
  if (!canvas)
    return;

  gfxContext* ctx = (gfxContext*)canvas;
   
  ctx->NewPath();
  ctx->SetColor(((ANPPaintPrivate*)paint)->color);

  float sizeX = (r->right   - r->left);
  float sizeY = (r->bottom  - r->top);

  ctx->Ellipse(gfxPoint(r->left + ( sizeX / 2), r->top  + ( sizeY  / 2)),
               gfxSize(sizeX, sizeY));
  ctx->Fill();
}

void
anp_canvas_drawPath(ANPCanvas* canvas, const ANPPath*, const ANPPaint* paint)
{
  if (!canvas)
    return;

  gfxContext* ctx = (gfxContext*)canvas;
  LOG("%s is not impl.", __PRETTY_FUNCTION__);
}

void
anp_canvas_drawText(ANPCanvas* canvas, const void* text, uint32_t byteLength,
                                float x, float y, const ANPPaint* paint)
{
  if (!canvas)
    return;

  gfxContext* ctx = (gfxContext*)canvas;
  LOG("%s is not impl.", __PRETTY_FUNCTION__);
}

void
anp_canvas_drawPosText(ANPCanvas* canvas, const void* text, uint32_t byteLength,
                       const float xy[], const ANPPaint* paint)
{
  if (!canvas)
    return;

  gfxContext* ctx = (gfxContext*)canvas;
  LOG("%s is not impl.", __PRETTY_FUNCTION__);
}

void
anp_canvas_drawBitmap(ANPCanvas* canvas, const ANPBitmap*, float x, float y,
                      const ANPPaint* paint)
{
  if (!canvas)
    return;

  gfxContext* ctx = (gfxContext*)canvas;
  LOG("%s is not impl.", __PRETTY_FUNCTION__);
}

void
anp_canvas_drawBitmapRect(ANPCanvas* canvas, const ANPBitmap*,
                          const ANPRectI* src, const ANPRectF* dst,
                          const ANPPaint* paint)
{
  if (!canvas)
    return;

  gfxContext* ctx = (gfxContext*)canvas;
  LOG("%s is not impl.", __PRETTY_FUNCTION__);
}

void InitCanvasInterface(ANPCanvasInterfaceV0 *i) {
  _assert(i->inSize == sizeof(*i));
  ASSIGN(i, newCanvas);
  ASSIGN(i, deleteCanvas);
  ASSIGN(i, save);
  ASSIGN(i, restore);
  ASSIGN(i, translate);
  ASSIGN(i, scale);
  ASSIGN(i, rotate);
  ASSIGN(i, skew);
  ASSIGN(i, concat);
  ASSIGN(i, clipRect);
  ASSIGN(i, clipPath);
  ASSIGN(i, getTotalMatrix);
  ASSIGN(i, getLocalClipBounds);
  ASSIGN(i, getDeviceClipBounds);
  ASSIGN(i, drawColor);
  ASSIGN(i, drawPaint);
  ASSIGN(i, drawLine);
  ASSIGN(i, drawRect);
  ASSIGN(i, drawOval);
  ASSIGN(i, drawPath);
  ASSIGN(i, drawText);
  ASSIGN(i, drawPosText);
  ASSIGN(i, drawBitmap);
  ASSIGN(i, drawBitmapRect);
}
