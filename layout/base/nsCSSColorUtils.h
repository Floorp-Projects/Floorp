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


#ifndef __nsCSSColorUtils_h
#define __nsCSSColorUtils_h

#include "nsColor.h"

// Weird color computing code stolen from winfe which was stolen
// from the xfe which was written originally by Eric Bina. So there.
// To determin colors based on the background brightness
void NS_Get3DColors(nscolor aResult[2], nscolor aBackgroundColor);

// To determine colors based on the background brightness and border color
void NS_GetSpecial3DColors(nscolor aResult[2],
											   nscolor aBackgroundColor,
											   nscolor aBorderColor);

// Determins brightness for a specific color
int NS_GetBrightness(PRUint8 aRed, PRUint8 aGreen, PRUint8 aBlue);

// function to convert from RGB color space to HSV color space 
void NS_RGB2HSV(nscolor aColor,PRUint16 &aHue,PRUint16 &aSat,PRUint16 &aValue);
// function to convert from HSV color space to RGB color space 
void NS_HSV2RGB(nscolor &aColor,PRUint16 aHue,PRUint16 aSat,PRUint16 aValue);

#endif
