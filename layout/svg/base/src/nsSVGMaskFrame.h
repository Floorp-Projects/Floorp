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
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef __NS_SVGMASKFRAME_H__
#define __NS_SVGMASKFRAME_H__

#include "nsISupports.h"

class nsISVGRendererCanvas;
class nsISVGRendererSurface;
class nsISVGChildFrame;
class nsIDOMSVGMatrix;
class nsIURI;
class nsIContent;

#define NS_ISVGMASKFRAME_IID \
{0x122fe62b, 0xa4cb, 0x49d4, {0xbf, 0xd7, 0x26, 0x67, 0x58, 0x28, 0xff, 0x02}}

class nsISVGMaskFrame : public nsISupports {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISVGMASKFRAME_IID)

  NS_IMETHOD MaskPaint(nsISVGRendererCanvas* aCanvas,
                       nsISVGRendererSurface* aSurface,
                       nsISVGChildFrame* aParent,
                       nsCOMPtr<nsIDOMSVGMatrix> aMatrix,
                       float aOpacity = 1.0f) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsISVGMaskFrame, NS_ISVGMASKFRAME_IID)

nsresult
NS_GetSVGMaskFrame(nsISVGMaskFrame **aResult,
                   nsIURI *aURI, nsIContent *aContent);

#endif
