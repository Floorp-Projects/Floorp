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


#ifndef __NS_ISVGLIBART_BITMAP_H__
#define __NS_ISVGLIBART_BITMAP_H__

#include "nsISupports.h"

class nsIRenderingContext;
class nsPresContext;
struct nsRect;
typedef PRUint32 nscolor;

// {18E4F62F-60A4-42D1-BCE2-43445656096E}
#define NS_ISVGLIBARTBITMAP_IID \
{ 0x18e4f62f, 0x60a4, 0x42d1, { 0xbc, 0xe2, 0x43, 0x44, 0x56, 0x56, 0x09, 0x6e } }

/**
 * \addtogroup libart_renderer Libart Rendering Engine
 * @{
 */
////////////////////////////////////////////////////////////////////////
/**
 * 'Private' rendering engine interface
 *
 * Abstraction of a libart-compatible bitmap
 * hiding platform-specific implementation details.
 */
class nsISVGLibartBitmap : public nsISupports
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISVGLIBARTBITMAP_IID)

  enum PixelFormat {
    PIXEL_FORMAT_24_RGB  = 1, // linux
    PIXEL_FORMAT_24_BGR  = 2, // windows
    PIXEL_FORMAT_32_ABGR = 3, // mac
    PIXEL_FORMAT_32_RGBA = 4, // mac/linux + compositing
    PIXEL_FORMAT_32_BGRA = 5  // windows + compositing
  };
  
  NS_IMETHOD_(PRUint8 *) GetBits()=0;
  NS_IMETHOD_(PixelFormat) GetPixelFormat()=0;

  /**
   * Linestride in bytes.
   */
  NS_IMETHOD_(int) GetLineStride()=0;

  /**
   * Width in pixels.
   */
  NS_IMETHOD_(int) GetWidth()=0;

  /**
   * Height in pixels.
   */
  NS_IMETHOD_(int) GetHeight()=0;

  /**
   * Obtain a rendering context for part of the bitmap. In general
   * this will be different to the RC passed at initialization time.
   */
  NS_IMETHOD_(void) LockRenderingContext(const nsRect& rect, nsIRenderingContext**ctx)=0;
  
  NS_IMETHOD_(void) UnlockRenderingContext()=0;

  /**
   * Flush changes to the rendering context passed at initialization time.
   */
  NS_IMETHOD_(void) Flush()=0;
};

/** @} */


// The libart backend expects to find a statically linked class
// implementing the above interface and instantiable with the
// following method:
nsresult
NS_NewSVGLibartBitmap(nsISVGLibartBitmap **result,
                      nsIRenderingContext *ctx,
                      nsPresContext* presContext,
                      const nsRect & rect);

#endif // __NS_ISVGLIBART_BITMAP_H__
