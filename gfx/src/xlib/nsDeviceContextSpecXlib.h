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

#ifndef nsDeviceContextSpecXlib_h___
#define nsDeviceContextSpecXlib_h___

#include "nsIDeviceContextSpec.h"
#include "nsIPrintSettings.h"
#include "nsIPrintOptions.h" 
#include "nsVoidArray.h"
#include "nsCOMPtr.h"
#ifdef USE_POSTSCRIPT
#include "nsIDeviceContextSpecPS.h"
#endif /* USE_POSTSCRIPT */
#ifdef USE_XPRINT
#include "nsIDeviceContextSpecXPrint.h"
#endif /* USE_XPRINT */
#include "nsCRT.h" /* should be <limits.h>? */

#define NS_PORTRAIT  0
#define NS_LANDSCAPE 1

typedef enum
{
  pmInvalid = 0,
  pmXprint,
  pmPostScript
} PrintMethod;

class nsDeviceContextSpecXlib : public nsIDeviceContextSpec
#ifdef USE_POSTSCRIPT
                              , public nsIDeviceContextSpecPS
#endif /* USE_POSTSCRIPT */
#ifdef USE_XPRINT
                              , public nsIDeviceContextSpecXp
#endif /* USE_XPRINT */
{
public:
  nsDeviceContextSpecXlib();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init(nsIPrintSettings* aPS, PRBool aIsPrintPreview);
  NS_IMETHOD ClosePrintManager(); 

  NS_IMETHOD GetToPrinter(PRBool &aToPrinter); 
  NS_IMETHOD GetIsPrintPreview(PRBool &aIsPPreview);
  NS_IMETHOD GetPrinterName ( const char **aPrinter );
  NS_IMETHOD GetCopies ( int &aCopies );
  NS_IMETHOD GetFirstPageFirst(PRBool &aFpf);     
  NS_IMETHOD GetGrayscale(PRBool &aGrayscale);   
  NS_IMETHOD GetTopMargin(float &value); 
  NS_IMETHOD GetBottomMargin(float &value); 
  NS_IMETHOD GetLeftMargin(float &value); 
  NS_IMETHOD GetRightMargin(float &value); 
  NS_IMETHOD GetCommand(const char **aCommand);   
  NS_IMETHOD GetPath (const char **aPath);    
  NS_IMETHOD GetLandscape (PRBool &aLandscape);
  NS_IMETHOD GetUserCancelled(PRBool &aCancel);      
  NS_IMETHOD GetPrintMethod(PrintMethod &aMethod);
  static nsresult GetPrintMethod(const char *aPrinter, PrintMethod &aMethod);
  NS_IMETHOD GetPageSizeInTwips(PRInt32 *aWidth, PRInt32 *aHeight);
  NS_IMETHOD GetPaperName(const char **aPaperName);
  NS_IMETHOD GetPlexName(const char **aPlexName);
  virtual ~nsDeviceContextSpecXlib();
  
protected:
  nsCOMPtr<nsIPrintSettings> mPrintSettings;
  PRPackedBool mToPrinter : 1;      /* If PR_TRUE, print to printer */
  PRPackedBool mIsPPreview : 1;     /* If PR_TRUE, is print preview */
  PRPackedBool mFpf : 1;            /* If PR_TRUE, first page first */
  PRPackedBool mGrayscale : 1;      /* If PR_TRUE, print grayscale */
  PRPackedBool mCancel : 1;         /* If PR_TRUE, user cancelled */
  int    mOrientation;        /* Orientation e.g. Portrait */
  char   mCommand[PATH_MAX];  /* Print command e.g., lpr */
  char   mPath[PATH_MAX];     /* If toPrinter = PR_FALSE, dest file */
  char   mPrinter[256];       /* Printer name */
  char   mPaperName[256];     /* Printer name */
  char   mPlexName[256];      /* Plex mode name */
  int    mCopies;             /* number of copies */
  float  mLeft;               /* left margin */
  float  mRight;              /* right margin */
  float  mTop;                /* top margin */
  float  mBottom;             /* bottom margin */
};

//-------------------------------------------------------------------------
// Printer Enumerator
//-------------------------------------------------------------------------
class nsPrinterEnumeratorXlib : public nsIPrinterEnumerator
{
public:
  nsPrinterEnumeratorXlib();
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRINTERENUMERATOR
};

#endif /* !nsDeviceContextSpecXlib_h___ */
