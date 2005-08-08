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

#ifndef nsPDECommon_h___
#define nsPDECommon_h___


#define  kMozPDESignature       "MOZZ"
#define  kMozPDECreatorCode     'MOZZ'

// Our custom print settings data is stored in a CFDictionary. The dictionary
// is flattened to XML for storage in the print ticket. The following keys
// are currently supported:

// State info supplied by the printing app
#define kPDEKeyHaveSelection            CFSTR("HaveSelection")          // Value: CFBoolean
#define kPDEKeyHaveFrames               CFSTR("HaveFrames")             // Value: CFBoolean
#define kPDEKeyHaveFrameSelected        CFSTR("HaveFrameSelected")      // Value: CFBoolean

// The UI of the PDE allows control of these
#define kPDEKeyPrintSelection           CFSTR("PrintSelection")         // Value: CFBoolean
#define kPDEKeyPrintFrameType           CFSTR("PrintFrameType")         // Value: CFStringReference - one of the following:
    #define kPDEValueFramesAsIs         CFSTR("FramesAsIs")
    #define kPDEValueSelectedFrame      CFSTR("SelectedFrame")
    #define kPDEValueEachFrameSep       CFSTR("EachFrameSep")
#define kPDEKeyShrinkToFit              CFSTR("ShrinkToFit")            // Value: CFBoolean
#define kPDEKeyPrintBGColors            CFSTR("PrintBGColors")          // Value: CFBoolean
#define kPDEKeyPrintBGImages            CFSTR("PrintBGImages")          // Value: CFBoolean

// Header/Footer strings
//  The following (case-sensitive) codes are expanded by the print engine at print-time
//  &D  date
//  &PT page # of #
//  &P  page #
//  &T  title
//  &U  doc URL
#define kPDEKeyHeaderLeft               CFSTR("HeaderLeft")             // Value: CFStringReference
#define kPDEKeyHeaderCenter             CFSTR("HeaderCenter")           // Value: CFStringReference
#define kPDEKeyHeaderRight              CFSTR("HeaderRight")            // Value: CFStringReference
#define kPDEKeyFooterLeft               CFSTR("FooterLeft")             // Value: CFStringReference
#define kPDEKeyFooterCenter             CFSTR("FooterCenter")           // Value: CFStringReference
#define kPDEKeyFooterRight              CFSTR("FooterRight")            // Value: CFStringReference

// Our tag for the Print Settings ticket. The PDE uses Ticket Services to read and write our
// custom data, so its key is the standard string with our tag appended. The application
// uses PM[Get|Set]PrintSettingsExtendedData, which takes an OSType for the tag because the
// standard string prefix is implied. Short story: the last four bytes of the two must match.
#define kAppPrintDialogPDEOnlyKey       CFSTR("com.apple.print.PrintSettingsTicket." "GEKO")
#define kAppPrintDialogAppOnlyKey       'GEKO'


#endif
