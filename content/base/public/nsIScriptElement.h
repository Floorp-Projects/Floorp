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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vidur Apparao <vidur@netscape.com> (original author)
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

#ifndef nsIScriptElement_h___
#define nsIScriptElement_h___

#include "nsISupports.h"
#include "nsIURI.h"
#include "nsCOMPtr.h"

#define NS_ISCRIPTELEMENT_IID                      \
{ /*fa304da4-5e9c-45a6-a017-8cb011dc46b4 */        \
 0xfa304da4, 0x5e9c, 0x45a6,                       \
 {0xa0, 0x17, 0x8c, 0xb0, 0x11, 0xdc, 0x46, 0xb4}} \

/**
 * Internal interface implemented by script elements
 */
class nsIScriptElement : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISCRIPTELEMENT_IID)

  /**
   * Content type identifying the scripting language. Can be empty, in
   * which case javascript will be assumed.
   */
  virtual void GetScriptType(nsAString& type) = 0;
    
  /**
   * Location of script source text. Can return null, in which case
   * this is assumed to be an inline script element.
   */
  virtual already_AddRefed<nsIURI> GetScriptURI() = 0;
  
  /**
   * Script source text for inline script elements.
   */
  virtual void GetScriptText(nsAString& text) = 0;

  virtual void GetScriptCharset(nsAString& charset) = 0;
  
  virtual void SetScriptLineNumber(PRUint32 aLineNumber) = 0;
  virtual PRUint32 GetScriptLineNumber() = 0;
};

#endif // nsIScriptElement_h___
