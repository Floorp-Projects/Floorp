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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Ken Herron <kherron@fastmail.us>
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

#ifndef nsDeviceContextPS_h___
#define nsDeviceContextPS_h___

#include "nsDeviceContext.h"
#include "nsUnitConversion.h"
#include "nsIWidget.h"
#include "nsIView.h"
#include "nsIRenderingContext.h"
#include "nsVoidArray.h"
#include "nsIDeviceContextPS.h"
#include "nsFontMetricsPS.h"
#include "nsIPrintJobPS.h"

class nsPostScriptObj;
class nsDeviceContextWin;       // need to be a friend of the class using us.

class nsDeviceContextPS : public DeviceContextImpl,
                          public nsIDeviceContextPS
{
public:
  nsDeviceContextPS();

  NS_DECL_ISUPPORTS_INHERITED

  /**
   * This method does nothing since a postscript devicecontext will never be created
   * with a NativeWidget.
   * @update 12/21/98 dwc
   */
  NS_IMETHOD  InitDeviceContextPS(nsIDeviceContext *aCreatingDeviceContext,nsIDeviceContext *aPrinterContext); 

  NS_IMETHOD  CreateRenderingContext(nsIRenderingContext *&aContext);
  NS_IMETHOD  CreateRenderingContext(nsIView *aView, nsIRenderingContext *&aContext) {return (DeviceContextImpl::CreateRenderingContext(aView,aContext));}
  NS_IMETHOD  CreateRenderingContext(nsIWidget *aWidget, nsIRenderingContext *&aContext) {return (DeviceContextImpl::CreateRenderingContext(aWidget,aContext));}
  /* PostScript module does not support offscreen drawing surfaces */
  NS_IMETHOD  CreateRenderingContext(nsIDrawingSurface* aSurface, nsIRenderingContext *&aContext) {return NS_ERROR_NOT_IMPLEMENTED;}
  NS_IMETHOD  CreateRenderingContextInstance(nsIRenderingContext *&aContext);

  NS_IMETHOD  SupportsNativeWidgets(PRBool &aSupportsWidgets);

  NS_IMETHOD  GetScrollBarDimensions(float &aWidth, float &aHeight) const;

  NS_IMETHOD  CheckFontExistence(const nsString& aFontName);
  NS_IMETHOD  GetDepth(PRUint32& aDepth);

  NS_IMETHOD  GetDeviceSurfaceDimensions(PRInt32 &aWidth, PRInt32 &aHeight);
  NS_IMETHOD  GetClientRect(nsRect &aRect);
  NS_IMETHOD  GetRect(nsRect &aRect);

  NS_IMETHOD  GetDeviceContextFor(nsIDeviceContextSpec *aDevice,nsIDeviceContext *&aContext);
  NS_IMETHOD  GetSystemFont(nsSystemFontID anID, nsFont *aFont) const;
  NS_IMETHOD  BeginDocument(PRUnichar * aTitle, PRUnichar* aPrintToFileName, PRInt32 aStartPage, PRInt32 aEndPage);
  NS_IMETHOD  EndDocument(void);
  NS_IMETHOD  AbortDocument(void);
  NS_IMETHOD  BeginPage(void);
  NS_IMETHOD  EndPage(void);
  NS_IMETHOD  CreateFontCache();
  
  NS_IMETHOD  SetSpec(nsIDeviceContextSpec *aSpec);

  nsPostScriptObj*    GetPrintContext() { return mPSObj; }
  nsHashtable*        GetPSFontGeneratorList() { return mPSFontGeneratorList; }
  PRBool               mFTPEnable;

protected:
  virtual     ~nsDeviceContextPS();
  
  nsIDrawingSurface*       mSurface;
  PRUint32               mDepth;
  nsCOMPtr<nsIDeviceContextSpec>  mSpec;
  nsCOMPtr<nsIDeviceContext>      mParentDeviceContext;
  nsIPrintJobPS         *mPrintJob;
  nsPostScriptObj       *mPSObj;
  nsHashtable           *mPSFontGeneratorList;
};

#endif /* nsDeviceContextPS_h___ */
