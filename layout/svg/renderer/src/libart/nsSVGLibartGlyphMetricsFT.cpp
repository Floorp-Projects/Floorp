/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Leon Sha <leon.sha@sun.com>
 *   Alex Fritze <alex@croczilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsCOMPtr.h"
#include "nsISVGGlyphMetricsSource.h"
#include "nsISVGRendererGlyphMetrics.h"
#include "nsPromiseFlatString.h"
#include "nsFont.h"
#include "nsIFontMetrics.h"
#include "nsIPresContext.h"
#include "float.h"
#include "nsIDOMSVGMatrix.h"
#include "nsIDOMSVGRect.h"
#include "nsSVGTypeCIDs.h"
#include "nsIComponentManager.h"
#include "nsISVGLibartGlyphMetricsFT.h"
#include "nsIDeviceContext.h"
#include "nsSVGLibartFreetype.h"
#include "libart-incs.h"
#include "nsArray.h"
#include "nsDataHashtable.h"

/**
 * \addtogroup libart_renderer Libart Rendering Engine
 * @{
 */
////////////////////////////////////////////////////////////////////////
/**
 * Libart freetype-based glyph metrics implementation.
 * Conditionally compiled in for freetype-enabled builds only.
 */
class nsSVGLibartGlyphMetricsFT : public nsISVGLibartGlyphMetricsFT
{
protected:
  friend nsresult NS_NewSVGLibartGlyphMetricsFT(nsISVGRendererGlyphMetrics **result,
                                                nsISVGGlyphMetricsSource *src);
  friend void NS_InitSVGLibartGlyphMetricsFTGlobals();
  friend void NS_FreeSVGLibartGlyphMetricsFTGlobals();

  nsSVGLibartGlyphMetricsFT(nsISVGGlyphMetricsSource *src);
  ~nsSVGLibartGlyphMetricsFT();

  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsISVGRendererGlyphMetrics interface:
  NS_DECL_NSISVGRENDERERGLYPHMETRICS

  // nsISVGLibartGlyphMetrics interface:
  NS_IMETHOD_(FT_Face) GetFTFace();
  NS_IMETHOD_(float) GetPixelScale();
  NS_IMETHOD_(float) GetTwipsToPixels();
  NS_IMETHOD_(const FT_BBox*) GetBoundingBox();
  NS_IMETHOD_(PRUint32) GetGlyphCount();
  NS_IMETHOD_(FT_Glyph) GetGlyphAt(PRUint32 pos);

  // helpers :
  void InitializeFace();
  void ClearFace() { mFace = nsnull; }
  void InitializeGlyphArray();
  void ClearGlyphArray();
private:
  FT_Face mFace;

  struct GlyphDescriptor {
    GlyphDescriptor() : index(0), image(nsnull) {};
    ~GlyphDescriptor() { if (image) nsSVGLibartFreetype::ft2->DoneGlyph(image); }
    FT_UInt   index; // glyph index
    FT_Glyph  image; // glyph image translated to correct position within string
  };
  
  GlyphDescriptor *mGlyphArray;
  PRUint32 mGlyphArrayLength;
  FT_BBox mBBox;
  
  nsCOMPtr<nsISVGGlyphMetricsSource> mSource;
    
public:
  static nsDataHashtable<nsStringHashKey,const nsDependentString*> sFontAliases;  
};

/** @} */

//----------------------------------------------------------------------
// nsSVGLibartGlyphMetricsFT implementation:

nsDataHashtable<nsStringHashKey,const nsDependentString*>
nsSVGLibartGlyphMetricsFT::sFontAliases;


nsSVGLibartGlyphMetricsFT::nsSVGLibartGlyphMetricsFT(nsISVGGlyphMetricsSource *src)
    : mSource(src), mFace(nsnull), mGlyphArray(nsnull), mGlyphArrayLength(0)
{
}

nsSVGLibartGlyphMetricsFT::~nsSVGLibartGlyphMetricsFT()
{
  ClearGlyphArray();
  ClearFace();
}

nsresult
NS_NewSVGLibartGlyphMetricsFT(nsISVGRendererGlyphMetrics **result,
                              nsISVGGlyphMetricsSource *src)
{
  *result = new nsSVGLibartGlyphMetricsFT(src);
  if (!*result) return NS_ERROR_OUT_OF_MEMORY;
  
  NS_ADDREF(*result);
  return NS_OK;
}

void NS_InitSVGLibartGlyphMetricsFTGlobals()
{
  
  NS_ASSERTION(!nsSVGLibartGlyphMetricsFT::sFontAliases.IsInitialized(),
               "font aliases already initialized");
  nsSVGLibartGlyphMetricsFT::sFontAliases.Init(3);

  static NS_NAMED_LITERAL_STRING(arial, "arial");
  nsSVGLibartGlyphMetricsFT::sFontAliases.Put(NS_LITERAL_STRING("helvetica"),
                                              &arial);

  static NS_NAMED_LITERAL_STRING(courier, "courier new");
  nsSVGLibartGlyphMetricsFT::sFontAliases.Put(NS_LITERAL_STRING("courier"),
                                              &courier);

  static NS_NAMED_LITERAL_STRING(times, "times new roman");
  nsSVGLibartGlyphMetricsFT::sFontAliases.Put(NS_LITERAL_STRING("times"),
                                              &times);
}

void NS_FreeSVGLibartGlyphMetricsFTGlobals()
{
    NS_ASSERTION(nsSVGLibartGlyphMetricsFT::sFontAliases.IsInitialized(),
               "font aliases never initialized");
  nsSVGLibartGlyphMetricsFT::sFontAliases.Clear();
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(nsSVGLibartGlyphMetricsFT)
NS_IMPL_RELEASE(nsSVGLibartGlyphMetricsFT)

NS_INTERFACE_MAP_BEGIN(nsSVGLibartGlyphMetricsFT)
  NS_INTERFACE_MAP_ENTRY(nsISVGRendererGlyphMetrics)
  NS_INTERFACE_MAP_ENTRY(nsISVGLibartGlyphMetricsFT)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
// nsISVGRendererGlyphMetrics methods:

/** Implements float getBaselineOffset(in unsigned short baselineIdentifier); */
NS_IMETHODIMP
nsSVGLibartGlyphMetricsFT::GetBaselineOffset(PRUint16 baselineIdentifier, float *_retval)
{
  *_retval = 0.0f;
  
  switch (baselineIdentifier) {
    case BASELINE_TEXT_BEFORE_EDGE:
      break;
    case BASELINE_TEXT_AFTER_EDGE:
      break;
    case BASELINE_CENTRAL:
    case BASELINE_MIDDLE:
      break;
    case BASELINE_ALPHABETIC:
    default:
    break;
  }
  
  return NS_OK;
}


/** Implements readonly attribute float #advance; */
NS_IMETHODIMP
nsSVGLibartGlyphMetricsFT::GetAdvance(float *aAdvance)
{
  InitializeGlyphArray();
  *aAdvance = mBBox.xMax-mBBox.yMin;
  return NS_OK;
}

/** Implements readonly attribute nsIDOMSVGRect #boundingBox; */
NS_IMETHODIMP
nsSVGLibartGlyphMetricsFT::GetBoundingBox(nsIDOMSVGRect * *aBoundingBox)
{
  InitializeGlyphArray();

  *aBoundingBox = nsnull;

  nsCOMPtr<nsIDOMSVGRect> rect = do_CreateInstance(NS_SVGRECT_CONTRACTID);

  NS_ASSERTION(rect, "could not create rect");
  if (!rect) return NS_ERROR_FAILURE;
  
  rect->SetX(mBBox.xMin);
  rect->SetY(mBBox.yMin);
  rect->SetWidth(mBBox.xMax-mBBox.xMin);
  rect->SetHeight(mBBox.yMax-mBBox.yMin);

  *aBoundingBox = rect;
  NS_ADDREF(*aBoundingBox);
  
  return NS_OK;
}

/** Implements [noscript] nsIDOMSVGRect getExtentOfChar(in unsigned long charnum); */
NS_IMETHODIMP
nsSVGLibartGlyphMetricsFT::GetExtentOfChar(PRUint32 charnum, nsIDOMSVGRect **_retval)
{
  *_retval = nsnull;

  nsCOMPtr<nsIDOMSVGRect> rect = do_CreateInstance(NS_SVGRECT_CONTRACTID);

  NS_ASSERTION(rect, "could not create rect");
  if (!rect) return NS_ERROR_FAILURE;
  
  rect->SetX(0);
  rect->SetY(0);
  rect->SetWidth(0);
  rect->SetHeight(0);

  *_retval = rect;
  NS_ADDREF(*_retval);
  
  return NS_OK;
}

/** Implements boolean update(in unsigned long updatemask); */
NS_IMETHODIMP
nsSVGLibartGlyphMetricsFT::Update(PRUint32 updatemask, PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGLibartGlyphMetrics methods:

NS_IMETHODIMP_(FT_Face)
nsSVGLibartGlyphMetricsFT::GetFTFace()
{
  InitializeFace();
  return mFace;
}

NS_IMETHODIMP_(float)
nsSVGLibartGlyphMetricsFT::GetPixelScale()
{
  nsCOMPtr<nsIPresContext> presContext;
  mSource->GetPresContext(getter_AddRefs(presContext));
  if (!presContext) {
    NS_ERROR("null prescontext");
    return 1.0f;
  }

  nsIDeviceContext* devicecontext = presContext->DeviceContext();

  float scale;
  devicecontext->GetCanonicalPixelScale(scale);
  return scale;
}  

NS_IMETHODIMP_(float)
nsSVGLibartGlyphMetricsFT::GetTwipsToPixels()
{
  nsCOMPtr<nsIPresContext> presContext;
  mSource->GetPresContext(getter_AddRefs(presContext));
  if (!presContext) {
    NS_ERROR("null prescontext");
    return 1.0f;
  }
  float twipstopixel;
  twipstopixel = presContext->TwipsToPixels();
  return twipstopixel;
}  

NS_IMETHODIMP_(const FT_BBox*)
nsSVGLibartGlyphMetricsFT::GetBoundingBox()
{
  InitializeGlyphArray();
  return &mBBox;
}

NS_IMETHODIMP_(PRUint32)
nsSVGLibartGlyphMetricsFT::GetGlyphCount()
{
  InitializeGlyphArray();
  return mGlyphArrayLength;
}

NS_IMETHODIMP_(FT_Glyph)
nsSVGLibartGlyphMetricsFT::GetGlyphAt(PRUint32 pos)
{
  NS_ASSERTION(pos<mGlyphArrayLength, "out of range");
  return mGlyphArray[pos].image;
}

//----------------------------------------------------------------------
// Implementation helpers:


struct FindFontStruct {
  nsCOMPtr<nsITrueTypeFontCatalogEntry> font_entry;
  nsFont font;
};

static PRBool
FindFont(const nsString& aFamily, PRBool aGeneric, void *aData)
{
  PRBool retval = PR_TRUE;
  
  FindFontStruct* font_data = (FindFontStruct*) aData;
  
#ifdef DEBUG
  printf("trying to instantiate font %s, generic=%d\n", NS_ConvertUCS2toUTF8(aFamily).get(),
         aGeneric);
#endif

  nsCAutoString family_name;
  
  if (aGeneric) {
    PRUint8 id;
    nsFont::GetGenericID(aFamily, &id);
    switch (id) {
      case kGenericFont_serif:
        family_name.AssignLiteral("times new roman");
        break;
      case kGenericFont_monospace:
        family_name.AssignLiteral("courier new");
        break;
      case kGenericFont_sans_serif:
      default:
        family_name.AssignLiteral("arial");
        break;
    }
  }
  else {
    family_name = NS_ConvertUCS2toUTF8(aFamily);
    ToLowerCase(family_name);
  }
  
  nsCAutoString language;
  PRUint16 slant = nsIFontCatalogService::kFCSlantRoman;
  if (font_data->font.style && NS_FONT_STYLE_ITALIC)
    slant = nsIFontCatalogService::kFCSlantItalic;
    
  nsCOMPtr<nsIArray> arr;
  nsSVGLibartFreetype::fontCatalog->GetFontCatalogEntries(family_name,
                                                          language,
                                                          font_data->font.weight,
                                                          nsIFontCatalogService::kFCWeightAny,
                                                          slant,
                                                          nsIFontCatalogService::kFCSpacingAny,
                                                          getter_AddRefs(arr));
  PRUint32 count;
  arr->GetLength(&count);
#ifdef DEBUG
  printf("FindFont(%s): number of fonts found: %d\n", family_name.get(), count);
#endif
  if (count>0) {
    retval =  PR_FALSE; // break
    // just take the first one for now:
    arr->QueryElementAt(0, NS_GET_IID(nsITrueTypeFontCatalogEntry),
                        getter_AddRefs(font_data->font_entry));
  }
  else {
    // try alias if there is one:
    const nsDependentString *alias = nsnull;
    nsSVGLibartGlyphMetricsFT::sFontAliases.Get(NS_ConvertUTF8toUCS2(family_name),
                                                &alias);
    if (alias) {
      // XXX this might cause a stack-overflow if there are cyclic
      // aliases in sFontAliases
      retval = FindFont(nsString(*alias), PR_FALSE, aData);      
    }
  }
  
  return retval;      
}

void
nsSVGLibartGlyphMetricsFT::InitializeFace()
{
  if (mFace) return; // already initialized
  
  FindFontStruct font_data;
  mSource->GetFont(&font_data.font);

  font_data.font.EnumerateFamilies(FindFont, (void*)&font_data);

  if (!font_data.font_entry) {
    // try to find *any* font
    nsAutoString empty;
    FindFont(empty, PR_FALSE, (void*)&font_data);
  }
  
  if (!font_data.font_entry) {
    NS_ERROR("svg libart renderer can't find a font (let alone a suitable one)");
    return;
  }

  FTC_Image_Desc imageDesc;
  imageDesc.font.face_id=(void*)font_data.font_entry.get(); // XXX do we need to addref?
  float twipstopixel = GetTwipsToPixels();
  float scale = GetPixelScale();
  imageDesc.font.pix_width = (int)((float)(font_data.font.size)*twipstopixel/scale);
  imageDesc.font.pix_height = (int)((float)(font_data.font.size)*twipstopixel/scale);
  imageDesc.image_type |= ftc_image_grays;

  // get the face
  nsresult rv;
  FTC_Manager mgr;
  nsSVGLibartFreetype::ft2->GetFTCacheManager(&mgr);
  rv = nsSVGLibartFreetype::ft2->ManagerLookupSize(mgr, &imageDesc.font, &mFace, nsnull);
  NS_ASSERTION(mFace, "failed to get face/size");
}

void nsSVGLibartGlyphMetricsFT::InitializeGlyphArray()
{
  if (mGlyphArray) return; // already initialized

  InitializeFace();
  if (!mFace) {
    NS_ERROR("no face");
    return;
  }
  FT_GlyphSlot glyphslot=mFace->glyph;

  mBBox.xMin = mBBox.yMin = 3200;
  mBBox.xMax = mBBox.yMax = -3200;
  
  nsAutoString text;
  mSource->GetCharacterData(text);
  mGlyphArrayLength = text.Length();
  if (mGlyphArrayLength == 0) return;

  mGlyphArray = new GlyphDescriptor[mGlyphArrayLength];
  NS_ASSERTION(mGlyphArray, "could not allocate glyph descriptor array");

  GlyphDescriptor* glyph = mGlyphArray;
  bool use_kerning = FT_HAS_KERNING(mFace);
  FT_UInt previous_glyph = 0;
  FT_Vector pen; // pen position in 26.6 format
  pen.x = 0;
  pen.y = 0;
  
  nsAString::const_iterator start, end;
  text.BeginReading(start);
  text.EndReading(end);
  PRUint32 size;
  
  for ( ; start!=end; start.advance(size)) {
    const PRUnichar* buf = start.get();
    size = start.size_forward();
    // fragment at 'buf' is 'size' characters long
    for (PRUint32 i=0; i<size; ++i) {
      nsSVGLibartFreetype::ft2->GetCharIndex(mFace, buf[i], &glyph->index);
      
      if (use_kerning && previous_glyph && glyph->index) {
        FT_Vector delta;
        nsSVGLibartFreetype::ft2->GetKerning(mFace, previous_glyph,
                                             glyph->index,
                                             FT_KERNING_DEFAULT,
                                             &delta);
        pen.x += delta.x;
      }

      // load glyph into the face's shared glyph slot:
      if (NS_FAILED(nsSVGLibartFreetype::ft2->LoadGlyph(mFace,
                                                        glyph->index,
                                                        FT_LOAD_DEFAULT))) {
        NS_ERROR("error loading glyph");
        continue;
      }

      // copy glyph image into array:
      if (NS_FAILED(nsSVGLibartFreetype::ft2->GetGlyph(glyphslot, &glyph->image))) {
        NS_ERROR("error copying glyph");
        continue;
      }

      // translate glyph image to correct location within string:
      nsSVGLibartFreetype::ft2->GlyphTransform(glyph->image, 0, &pen);

      // update the string's bounding box:
      FT_BBox glyph_bbox;
      nsSVGLibartFreetype::ft2->GlyphGetCBox(glyph->image, ft_glyph_bbox_pixels, &glyph_bbox);
      mBBox.xMin = PR_MIN(mBBox.xMin, glyph_bbox.xMin);
      mBBox.xMax = PR_MAX(mBBox.xMax, glyph_bbox.xMax);
      mBBox.yMin = PR_MIN(mBBox.yMin, glyph_bbox.yMin);
      mBBox.yMax = PR_MAX(mBBox.yMax, glyph_bbox.yMax);
      
      pen.x += glyphslot->advance.x;
      previous_glyph = glyph->index;
      ++glyph;
    }
  }
}

void nsSVGLibartGlyphMetricsFT::ClearGlyphArray()
{
  if (mGlyphArray)
    delete[] mGlyphArray;
  mGlyphArray = nsnull;
}
