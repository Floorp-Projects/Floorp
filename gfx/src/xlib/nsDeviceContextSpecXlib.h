/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *
 */

#ifndef nsDeviceContextSpecXlib_h___
#define nsDeviceContextSpecXlib_h___

#include "nsIDeviceContextSpec.h"
#include "nsDeviceContextSpecXlib.h"
#include "nsIDeviceContextSpecPS.h"
#ifdef USE_XPRINT
#include "nsIDeviceContextSpecXPrint.h"
#endif /* USE_XPRINT */
#include "nsPrintdXlib.h"

typedef enum
{
  pmAuto = 0, /* default */
  pmXprint,
  pmPostScript
} PrintMethod;

/* make Xprint the default print system if user/admin has set the XPSERVERLIST"
 * env var. See Xprt config README (/usr/openwin/server/etc/XpConfig/README) 
 * for details.
 */
#define NS_DEFAULT_PRINT_METHOD ((PR_GetEnv("XPSERVERLIST")!=nsnull)?(pmXprint):(pmPostScript))

class nsDeviceContextSpecXlib : public nsIDeviceContextSpec,
                                public nsIDeviceContextSpecPS
#ifdef USE_XPRINT
                              , public nsIDeviceContextSpecXp
#endif /* USE_XPRINT */
{
public:
  nsDeviceContextSpecXlib();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init(PRBool        aQuiet);
  NS_IMETHOD ClosePrintManager(); 

  NS_IMETHOD GetToPrinter(PRBool &aToPrinter); 
  NS_IMETHOD GetFirstPageFirst(PRBool &aFpf);     
  NS_IMETHOD GetGrayscale(PRBool &aGrayscale);   
  NS_IMETHOD GetSize(int &aSize); 
  NS_IMETHOD GetTopMargin(float &value); 
  NS_IMETHOD GetBottomMargin(float &value); 
  NS_IMETHOD GetLeftMargin(float &value); 
  NS_IMETHOD GetRightMargin(float &value); 
  NS_IMETHOD GetCommand(char **aCommand);   
  NS_IMETHOD GetPath (char **aPath);    
  NS_IMETHOD GetPageDimensions(float &aWidth, float &aHeight);
  NS_IMETHOD GetLandscape (PRBool &aLandscape);
  NS_IMETHOD GetUserCancelled(PRBool &aCancel);      
  NS_IMETHOD GetPrintMethod(PrintMethod &aMethod); 
  virtual ~nsDeviceContextSpecXlib();
protected:
  UnixPrData mPrData;
};


#endif
