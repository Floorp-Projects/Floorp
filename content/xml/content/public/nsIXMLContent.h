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
 *   Peter Annema <disttsc@bart.nl>
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

#ifndef nsIXMLContent_h___
#define nsIXMLContent_h___

#include "nsISupports.h"
#include "nsIStyledContent.h"

class nsINameSpace;
class nsINodeInfo;
class nsIDocShell;
class nsIURI;

#define NS_IXMLCONTENT_IID \
 { 0xa6cf90cb, 0x15b3, 0x11d2, \
 { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } }

/**
 * XML content extensions to nsIContent
 */
class nsIXMLContent : public nsIStyledContent {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IXMLCONTENT_IID)
  
  /**
   * Give this element a chance to fire links that should be fired
   * automatically when loaded. If the element was an autoloading link
   * and it was succesfully handled, we will throw special nsresult values.
   *
   * @param aShell the current doc shell (to possibly load the link on)
   * @throws NS_OK if nothing happened
   * @throws NS_XML_AUTOLINK_EMBED if the caller is loading the link embedded
   * @throws NS_XML_AUTOLINK_NEW if the caller is loading the link in a new
   *         window
   * @throws NS_XML_AUTOLINK_REPLACE if it is loading a link that will replace
   *         the current window (and thus the caller must stop parsing)
   * @throws NS_XML_AUTOLINK_UNDEFINED if it is loading in any other way--in
   *         which case, the caller should stop parsing as well.
   */
  NS_IMETHOD MaybeTriggerAutoLink(nsIDocShell *aShell) = 0;
};

#endif // nsIXMLContent_h___
