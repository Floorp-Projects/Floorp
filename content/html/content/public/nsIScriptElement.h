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

#define NS_ISCRIPTELEMENT_IID                     \
{ /* c9cbf78e-b7c1-48b0-a933-78d62b83a675 */      \
 0xc9cbf78e, 0xb7c1, 0x48b0,                      \
{0xa9, 0x33, 0x78, 0xd6, 0x2b, 0x83, 0xa6, 0x75}} \

/**
 * Internal interface so that the content sink can let a
 * script element know about its origin line number.
 */
class nsIScriptElement : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISCRIPTELEMENT_IID)

  NS_IMETHOD SetLineNumber(PRUint32 aLineNumber) = 0;
  NS_IMETHOD GetLineNumber(PRUint32* aLineNumber) = 0;
};

#endif // nsIScriptElement_h___
