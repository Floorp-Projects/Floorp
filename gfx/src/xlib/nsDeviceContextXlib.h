/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Smith <david@igelaus.com.au>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *
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
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsDeviceContextXlib_h__
#define nsDeviceContextXlib_h__

#include "nsDeviceContextX.h"
#include "nsRenderingContextXlib.h"

class nsDeviceContextXlib : public nsDeviceContextX
{
public:
  nsDeviceContextXlib();

  NS_IMETHOD  Init(nsNativeWidget aNativeWidget);

  NS_IMETHOD  CreateRenderingContext(nsIRenderingContext *&aContext);
  NS_IMETHOD  CreateRenderingContext(nsIView *aView, nsIRenderingContext *&aContext)
    {return (DeviceContextImpl::CreateRenderingContext(aView, aContext)); }
  NS_IMETHOD  CreateRenderingContext(nsIWidget *aWidget, nsIRenderingContext *&aContext)
    {return (DeviceContextImpl::CreateRenderingContext(aWidget, aContext)); }
  NS_IMETHOD  SupportsNativeWidgets(PRBool &aSupportsWidgets);

  NS_IMETHOD  GetScrollBarDimensions(float &aWidth, float &aHeight) const;
  NS_IMETHOD  GetSystemFont(nsSystemFontID anID, nsFont *aFont) const;

  NS_IMETHOD GetDrawingSurface(nsIRenderingContext &aContext, nsDrawingSurface &aSurface);

  NS_IMETHOD ConvertPixel(nscolor aColor, PRUint32 & aPixel);
  NS_IMETHOD CheckFontExistence(const nsString& aFontName);

  NS_IMETHOD GetDeviceSurfaceDimensions(PRInt32 &aWidth, PRInt32 &aHeight);
  NS_IMETHOD GetRect(nsRect &aRect);
  NS_IMETHOD GetClientRect(nsRect &aRect);

  NS_IMETHOD GetDeviceContextFor(nsIDeviceContextSpec *aDevice,
                                 nsIDeviceContext *&aContext);

  NS_IMETHOD BeginDocument(PRUnichar * aTitle);
  NS_IMETHOD EndDocument(void);

  NS_IMETHOD BeginPage(void);
  NS_IMETHOD EndPage(void);

  NS_IMETHOD CreateFontCache();

  NS_IMETHOD GetXlibRgbHandle(XlibRgbHandle *&aHandle) 
             { aHandle =  mXlibRgbHandle; return NS_OK; }
  XlibRgbHandle *GetXlibRgbHandle() { return mXlibRgbHandle; }
  NS_IMETHOD GetDepth( PRUint32 &depth ) { depth = (PRUint32)mDepth; return NS_OK; }

protected:
  virtual ~nsDeviceContextXlib();

private:
  void                 CommonInit(void);
  PRBool               mWriteable;
  PRUint32             mNumCells;
  nsDrawingSurface     mSurface;
  XlibRgbHandle       *mXlibRgbHandle;
  Display *            mDisplay;
  Screen *             mScreen;
  Visual *             mVisual;
  int                  mDepth;

  float                mWidthFloat;
  float                mHeightFloat;
  PRInt32              mWidth;
  PRInt32              mHeight;
};

#endif /* !nsDeviceContextXlib_h__ */

