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
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
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

#ifndef __NS_ISVGGDIPLUS_GLYPHMETRICS_H__
#define __NS_ISVGGDIPLUS_GLYPHMETRICS_H__

#include "nsISVGRendererGlyphMetrics.h"

// {A0B0E2A4-4237-44C8-9FD8-8B1B48F43C93}
#define NS_ISVGGDIPLUSGLYPHMETRICS_IID \
{ 0xa0b0e2a4, 0x4237, 0x44c8, { 0x9f, 0xd8, 0x8b, 0x1b, 0x48, 0xf4, 0x3c, 0x93 } }

/**
 * \addtogroup gdiplus_renderer GDI+ Rendering Engine
 * @{
 */

/**
 * 'Private' rendering engine interface
 */
class nsISVGGDIPlusGlyphMetrics : public nsISVGRendererGlyphMetrics
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISVGGDIPLUSGLYPHMETRICS_IID)

  /**
   * Obtain the bounding rectangle of the (composite) glyph as a Gdiplus::RectF.
   */
  NS_IMETHOD_(const RectF*) GetBoundingRect()=0;

  /**
   * Obtain the bounding rectangle of the glyphs of part of the
   * character string as a Gdiplus::RectF.
   *
   * @param charoffset Offset into nsISVGGlyphMetricsSource::characterData.
   * @param count      Number of characters for which to get bounds.
   */   
  NS_IMETHOD_(void) GetSubBoundingRect(PRUint32 charoffset, PRUint32 count, RectF* retval)=0;

  /**
   * Obtain the Gdiplus::Font object selected for this glyph metrics object.
   */
  NS_IMETHOD_(const Font*) GetFont()=0;

  /**
   * Obtain the Gdiplus::TextRenderingHint for this glyph metrics object.
   */
  NS_IMETHOD_(TextRenderingHint) GetTextRenderingHint()=0;
};

/** @} */

#endif // __NS_ISVGGDIPLUS_GLYPHMETRICS_H__
