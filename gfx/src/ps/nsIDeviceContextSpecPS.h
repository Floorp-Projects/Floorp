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

#ifndef nsIDeviceContextSpecPS_h___
#define nsIDeviceContextSpecPS_h___

#include "nsISupports.h"

/* dummy printer name for the gfx/src/ps driver */
#define NS_POSTSCRIPT_DRIVER_NAME "PostScript/"
/* strlen(NS_POSTSCRIPT_DRIVER_NAME)*/
#define NS_POSTSCRIPT_DRIVER_NAME_LEN (11) 

#define NS_IDEVICE_CONTEXT_SPEC_PS_IID { 0xa4ef8910, 0xdd65, 0x11d2, { 0xa8, 0x32, 0x0, 0x10, 0x5a, 0x18, 0x34, 0x19 } }

class nsIDeviceContextSpecPS : public nsISupports
{

public:
   NS_DEFINE_STATIC_IID_ACCESSOR(NS_IDEVICE_CONTEXT_SPEC_PS_IID)

  /*
   * If PR_TRUE, print to printer  
   * @update 
   * @param aToPrinter --  
   * @return 
   **/
   NS_IMETHOD GetToPrinter( PRBool &aToPrinter ) = 0; 

  /*
   * If PR_TRUE, print to printer  
   * @update 
   * @param aIsPPreview -- Set to PR_TRUE for print preview, PR_FALSE otherwise.
   * @return 
   **/
   NS_IMETHOD GetIsPrintPreview( PRBool &aIsPPreview ) = 0; 

  /*
   * If PR_TRUE, first page first 
   * @update 
   * @param aFpf -- 
   * @return 
   **/
   NS_IMETHOD GetFirstPageFirst (  PRBool &aFpf ) = 0;     

  /*
   * If PR_TRUE, print grayscale 
   * @update 
   * @param aGrayscale --
   * @return 
   **/
   NS_IMETHOD GetGrayscale(  PRBool &aGrayscale ) = 0;   

  /*
   * Top margin 
   * @update 
   * @param aValue --
   * @return 
   **/
   NS_IMETHOD GetTopMargin (  float &aValue ) = 0; 

  /*
   * Bottom margin 
   * @update 
   * @param aValue --
   * @return 
   **/
   NS_IMETHOD GetBottomMargin (  float &aValue ) = 0; 

  /*
   * Left margin 
   * @update 
   * @param aValue --
   * @return 
   **/
   NS_IMETHOD GetLeftMargin (  float &aValue ) = 0; 

  /*
   * Right margin 
   * @update 
   * @param aValue --
   * @return 
   **/
   NS_IMETHOD GetRightMargin (  float &aValue ) = 0; 

  /*
   * Print command e.g., lpr 
   * @update 
   * @param aCommand --
   * @return 
   **/
   NS_IMETHOD GetCommand ( const char **aCommand ) = 0;   

  /*
   * Printer name e.g., myprinter
   * @update 
   * @param aPrinter --
   * @return 
   **/
   NS_IMETHOD GetPrinterName ( const char **aPrinter ) = 0;   

  /*
   * If PR_TRUE, user chose Landscape
   * @update 
   * @param aLandscape
   * @return 
   **/
   NS_IMETHOD GetLandscape ( PRBool &aLandscape ) = 0;   

  /*
   * If toPrinter = PR_FALSE, dest file 
   * @update 
   * @param aPath --
   * @return 
   **/
   NS_IMETHOD GetPath ( const char **aPath ) = 0;    

  /*
   * If PR_TRUE, user cancelled 
   * @update 
   * @param aCancel -- 
   * @return 
   **/
   NS_IMETHOD GetUserCancelled(  PRBool &aCancel ) = 0;      

  /*
   * Paper name e.g., "din-a4" or "manual/din-a4"
   * @update 
   * @param aPaperName --
   * @return 
   **/
   NS_IMETHOD GetPaperName ( const char **aPaperName ) = 0;

   /*
    * Returns W/H in Twips from Paper Size H/W
    */
   NS_IMETHOD GetPageSizeInTwips(PRInt32 *aWidth, PRInt32 *aHeight) = 0;
 
   /*
    * Return number of copies to print
    * @update
    * @param aCopies
    * @return
    */
   NS_IMETHOD GetCopies ( int &aCopies ) = 0;

};

#endif /* !nsIDeviceContextSpecPS_h___ */

