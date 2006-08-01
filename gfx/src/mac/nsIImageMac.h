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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mike Pinkerton (pinkerton@netscape.com)
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

#ifndef nsIImageMac_h__
#define nsIImageMac_h__


#include "nsISupports.h"
#include "nsRect.h"
#include <Quickdraw.h>


// IID for the nsIImage interface
// {DE2628F9-6023-4443-A4C9-CE0CE6DA0628}
#define NS_IIMAGEMAC_IID \
 { 0xDE2628F9, 0x6023, 0x4443, \
   { 0xA4, 0xC9, 0xCE, 0x0C, 0xE6, 0xDA, 0x06, 0x28 } };


// 
// nsIImageMac
//
  
// MacOS-specific Interface to Images
//
class nsIImageMac : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IIMAGEMAC_IID)

    // Convert to the os-native PICT format. Most likely
    // used for clipboard. The caller is responsible for
    // using ::KillPicture() to dispose of |outPicture|
    // when it's done.
  NS_IMETHOD ConvertToPICT ( PicHandle* outPicture ) = 0;
  
    // Convert from the os-native PICT format. Most likely
    // used for clipboard.  
  NS_IMETHOD ConvertFromPICT ( PicHandle inPicture ) = 0;

    // Get the underlying CGImageRef.  The object that is returned is
    // not retained.
  NS_IMETHOD GetCGImageRef(CGImageRef* aCGImageRef) = 0;

}; // nsIImageMac

NS_DEFINE_STATIC_IID_ACCESSOR(nsIImageMac, NS_IIMAGEMAC_IID)

#endif
