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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Johnny Stenback <jst@netscape.com> (original author)
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
#ifndef nsIFrameLoader_h___
#define nsIFrameLoader_h___

#include "nsISupports.h"
#include "nsAString.h"

// Forward declarations
class nsIContent;
class nsIDocShell;
class nsIURI;

// IID for the nsIFrameLoader interface
#define NS_IFRAMELOADER_IID   \
{ 0x51e2b6df, 0xdaf2, 0x4a2f, \
  {0x80, 0xe5, 0xed, 0x69, 0x5b, 0x8c, 0x67, 0x4f} }

// IID for the nsIFrameLoaderOwner interface
#define NS_IFRAMELOADEROWNER_IID   \
{ 0x0080d493, 0x96b4, 0x4606, \
  {0xa7, 0x43, 0x0f, 0x47, 0xee, 0x87, 0x14, 0xd1} }

// CID for the nsIFrameLoader implementation
#define NS_FRAMELOADER_CID   \
{ 0x712603da, 0xf245, 0x4503, \
  {0xa5, 0x41, 0xb0, 0x49, 0xcb, 0x06, 0x81, 0xae} }

#define NS_FRAMELOADER_CONTRACTID "@mozilla.org/content/frameloader"

class nsIFrameLoader : public nsISupports
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IFRAMELOADER_IID)

  /**
   * Initialize the frame loader, hand it the owner content. Note that
   * the owner content reference is a weak reference, if the owner
   * content is destroyed before the frame loader goes away the owner
   * content must call the Destroy() method to clear the owner content
   * reference.
   */
  NS_IMETHOD Init(nsIContent *aOwner) = 0;

  /**
   * Start loading the frame. This method figures out what to load
   * from the owner content in the frame loader.
   */
  NS_IMETHOD LoadFrame() = 0;

  /**
   * Get the docshell from the frame loader.
   */
  NS_IMETHOD GetDocShell(nsIDocShell **aDocShell) = 0;

  /**
   * Destroy the frame loader and everything inside it. This will
   * clear the weak owner content reference.  */
  NS_IMETHOD Destroy() = 0;
};


class nsIFrameLoaderOwner : public nsISupports
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IFRAMELOADEROWNER_IID)

  /**
   * Get the frame loader from the frame loader owner.
   */
  NS_IMETHOD GetFrameLoader(nsIFrameLoader **aFrameLoader) = 0;

  /**
   * Sets (or clears) the frame loader for this content.
   */
  NS_IMETHOD SetFrameLoader(nsIFrameLoader *aFrameLoader) = 0;
};


nsresult
NS_NewFrameLoader(nsIFrameLoader **aFrameLoader);

#endif /* nsIFrameLoader_h___ */
