/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#ifndef nsIDocumentTransformer_h___
#define nsIDocumentTransformer_h___

#include "nsISupports.h"

class nsIDOMElement;
class nsIDOMDocument;
class nsIObserver;

/* 3fbff728-2d20-11d3-aef3-00108300ff91 */
#define NS_IDOCUMENT_TRANSFORMER_IID \
{ 0x3fbff728, 0x2d20, 0x11d3, {0xae, 0xf3, 0x00, 0x10, 0x83, 0x00, 0xff, 0x91} }

/**
 * This interface should be implemented by any object that wants to
 * transform the content model of the current document before the
 * document is displayed.  One possible implementor of this interface
 * is an XSL processor. 
 */
class nsIDocumentTransformer : public nsISupports {
public:  
  NS_IMETHOD TransformDocument(nsIDOMElement* aSourceDOM, 
                               nsIDOMElement* aStyleDOM,
                               nsIDOMDocument* aOutputDoc,
                               nsIObserver* aObserver)=0;
};

#endif // nsIDocumentTransformer_h___
