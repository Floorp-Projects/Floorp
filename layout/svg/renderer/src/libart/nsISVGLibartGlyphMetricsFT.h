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

#ifndef __NS_ISVGLIBART_GLYPHMETRICS_FT_H__
#define __NS_ISVGLIBART_GLYPHMETRICS_FT_H__

#include "nsISVGRendererGlyphMetrics.h"
#include "nsIFreeType2.h"

// {b74698f5-b016-4423-a742-d2a14e5d8c45}
#define NS_ISVGLIBARTGLYPHMETRICSFT_IID \
{ 0xb74698f5, 0xb016, 0x4423, { 0xa7, 0x42, 0xd2, 0xa1, 0x4e, 0x5d, 0x8c, 0x45 } }

/**
 * \addtogroup libart_renderer Libart Rendering Engine
 * @{
 */
//////////////////////////////////////////////////////////////////////
/**
 *  'Private' rendering engine interface
 */
class nsISVGLibartGlyphMetricsFT : public nsISVGRendererGlyphMetrics
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISVGLIBARTGLYPHMETRICSFT_IID)
  NS_IMETHOD_(FT_Face) GetFTFace() = 0;
  NS_IMETHOD_(float) GetPixelScale() = 0;
  NS_IMETHOD_(float) GetTwipsToPixels() = 0;
  NS_IMETHOD_(const FT_BBox*) GetBoundingBox() = 0;
  NS_IMETHOD_(PRUint32) GetGlyphCount() = 0;
  NS_IMETHOD_(FT_Glyph) GetGlyphAt(PRUint32 pos) = 0;
};

/** @} */

#endif // __NS_ISVGLIBART_GLYPHMETRICS_FT_H__
