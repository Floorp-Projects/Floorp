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
 *   Julien Lafon <julien.lafon@gmail.com>
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

#ifndef nsIDeviceContextSpecXP_h___
#define nsIDeviceContextSpecXP_h___

#include "nsISupports.h"

/* UUID=12ab7845-a341-41ba-bc12-6025e0b11e0e */
#define NS_IDEVICE_CONTEXT_SPEC_XP_IID { 0x12ab7845, 0xa341, 0x41ba, { 0x60, 0x25, 0xe0, 0xb1, 0x1e, 0x0e } }

class nsIDeviceContextSpecXp : public nsISupports
{

public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IDEVICE_CONTEXT_SPEC_XP_IID);

  /*
   * If PR_TRUE, print to printer  
   * @update 
   * @param aToPrinter --  
   * @return 
   **/
   NS_IMETHOD GetToPrinter( PRBool &aToPrinter ) = 0; 

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
   * Printer name e.g., myprinter or myprinter@myxprt:5 
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
   * Plex name e.g., "simplex", "duplex", "tumble" or any 
   * driver/printer-specific custom value
   * @update 
   * @param aPlexName --
   * @return 
   **/
   NS_IMETHOD GetPlexName ( const char **aPlexName ) = 0;   

  /*
   * Resolution/quality name e.g., "600", "600x300", "high-res",
   * "med-res"
   * driver/printer-specific custom value
   * @update 
   * @param aResolutionName --
   * @return 
   **/
   NS_IMETHOD GetResolutionName ( const char **aResolutionName ) = 0;   
   
  /*
   * Colorspace name e.g., "TrueColor", "Grayscale/10bit", "b/w",
   * "CYMK"
   * driver/printer-specific custom value
   * @update 
   * @param aColorspace --
   * @return 
   **/
   NS_IMETHOD GetColorspace ( const char **aColorspace ) = 0;   

  /*
   * If PR_TRUE, enable font download to printer
   * @update 
   * @param aDownloadFonts --
   * @return 
   **/
   NS_IMETHOD GetDownloadFonts( PRBool &aDownloadFonts ) = 0;   

  /*
   * Return number of copies to print
   * @update 
   * @param aCopies
   * @return 
   **/   
   NS_IMETHOD GetCopies ( int &aCopies ) = 0;
};

#endif /* !nsIDeviceContextSpecXP_h___ */

