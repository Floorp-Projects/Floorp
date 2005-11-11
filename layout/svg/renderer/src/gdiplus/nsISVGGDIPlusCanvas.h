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
 * Crocodile Clips Ltd..
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
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

#ifndef __NS_ISVGGDIPLUS_CANVAS_H__
#define __NS_ISVGGDIPLUS_CANVAS_H__

#include <windows.h>

// unknwn.h is needed to build with WIN32_LEAN_AND_MEAN
#include <unknwn.h>

#include <Gdiplus.h>
using namespace Gdiplus;

#include "nsISVGRendererCanvas.h"

// {c4cd849e-a0aa-405b-bc75-ba8982319fe4}
#define NS_ISVGGDIPLUSCANVAS_IID \
{ 0xc4cd849e, 0xa0aa, 0x405b, { 0xbc, 0x75, 0xba, 0x89, 0x82, 0x31, 0x9f, 0xe4} }

/**
 * \addtogroup gdiplus_renderer GDI+ Rendering Engine
 * @{
 */

/**
 * 'Private' rendering engine interface
 */
class nsISVGGDIPlusCanvas : public nsISVGRendererCanvas
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISVGGDIPLUSCANVAS_IID)

  /**
   * Obtain the Gdiplus::Graphics object for this canvas.
   */
  NS_IMETHOD_(Graphics*) GetGraphics()=0;

  /**
   * Obtain the GdiPlus::Region clip path.
   */
  NS_IMETHOD_(Region*) GetClipRegion()=0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsISVGGDIPlusCanvas, NS_ISVGGDIPLUSCANVAS_IID)

/** @} */

#endif //__NS_ISVGGDIPLUS_CANVAS_H__
