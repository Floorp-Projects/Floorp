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
 * Alex Fritze.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex@croczilla.com> (original author)
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

#ifndef __NS_ISVGLIBART_CANVAS_H__
#define __NS_ISVGLIBART_CANVAS_H__

#include "nsISVGRendererCanvas.h"
#include "libart-incs.h"
typedef PRUint32 nscolor;
typedef ArtPixMaxDepth ArtColor[3];

// {6F963B6F-8D8E-4C8D-B4A1-FA87FB825973}
#define NS_ISVGLIBARTCANVAS_IID \
{ 0x6f963b6f, 0x8d8e, 0x4c8d, { 0xb4, 0xa1, 0xfa, 0x87, 0xfb, 0x82, 0x59, 0x73 } }

/**
 * \addtogroup libart_renderer Libart Rendering Engine
 * @{
 */
//////////////////////////////////////////////////////////////////////
/**
 * 'Private' rendering engine interface
 */
class nsISVGLibartCanvas : public nsISVGRendererCanvas
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_ISVGLIBARTCANVAS_IID; return iid; }

  /**
   * Deprecated. Use NewRender(int x0, int y0, int x1, int y1).
   */
  NS_IMETHOD_(ArtRender*) NewRender()=0;

  /**
   * Construct a new render object for the given rect.
   *
   * @return New render object or 0 if the requested rect doesn't
   *         overlap the dirty rect or if there is an error.
   */   
  NS_IMETHOD_(ArtRender*) NewRender(int x0, int y0, int x1, int y1)=0;

  /**
   * Invoke the render object previously constructed with a call to
   * NewRender().
   */
  NS_IMETHOD_(void) InvokeRender(ArtRender* render)=0;

  /**
   * Convert an rgb value into a libart color value.
   */
  NS_IMETHOD_(void) GetArtColor(nscolor rgb, ArtColor& artColor)=0;
};

/** @} */

#endif //__NS_ISVGLIBART_CANVAS_H__
