/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications.  Portions created by Netscape Communications are
 * Copyright (C) 2001 by Netscape Communications.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Vidur Apparao <vidur@netscape.com> (original author)
 */

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
