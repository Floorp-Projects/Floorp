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
 * John B. Keiser.
 * Portions created by the Initial Developer are Copyright (C) 2001
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

#ifndef nsIListControlFrame_h___
#define nsIListControlFrame_h___

#include "nsISupports.h"
class nsAString;

// IID for the nsIImageControlFrame class
#define NS_ILISTCONTROLFRAME_IID    \
{ 0x85603274, 0x1dd2, 0x11b2,  \
  { 0xba, 0x7a, 0xec, 0x7d, 0x65, 0x8d, 0x98, 0xe2 } }

/**
  * nsIImageControlFrame is the external interface for ImageControlFrame
  */
class nsIImageControlFrame : public nsISupports {

public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ILISTCONTROLFRAME_IID)

  /**
   * Get the place the user last clicked on the image (esp. for submits)
   */
  NS_IMETHOD GetClickedX(PRInt32* aX) = 0;

  /**
   * Get the place the user last clicked on the image (esp. for submits)
   */
  NS_IMETHOD GetClickedY(PRInt32* aY) = 0;

};

#endif

