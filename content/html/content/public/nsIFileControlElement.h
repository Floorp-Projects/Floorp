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
 * Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonas Sicking <jonas@sicking.cc> (Original author)
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

#ifndef nsIFileControlElement_h___
#define nsIFileControlElement_h___

#include "nsISupports.h"
#include "nsTArray.h"
#include "nsString.h"
#include "nsCOMArray.h"

class nsIFile;

// IID for the nsIFileControl interface
#define NS_IFILECONTROLELEMENT_IID \
{ 0x1f6a32fd, 0x9cda, 0x43e9, \
  { 0x90, 0xef, 0x18, 0x0a, 0xd5, 0xe6, 0xcd, 0xa9 } }

/**
 * This interface is used for the file control frame to store its value away
 * into the content.
 */
class nsIFileControlElement : public nsISupports {
public:

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IFILECONTROLELEMENT_IID)

  /**
   * Gets a readable string representing the list of files currently
   * selected by this control. This value might not be a valid file name
   * and should not be used for anything but displaying the filename to the
   * user.
   */
  virtual void GetDisplayFileName(nsAString& aFileName) = 0;

  /**
   * Sets the list of filenames currently selected by this control.
   */
  virtual void SetFileNames(const nsTArray<nsString>& aFileNames) = 0;

  /**
   * Gets a list of nsIFile objects for the files currently selected by
   * this control.
   */
  virtual void GetFileArray(nsCOMArray<nsIFile>& aFiles) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIFileControlElement,
                              NS_IFILECONTROLELEMENT_IID)

#endif // nsIFileControlElement_h___
