/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsITransformMediator_h___
#define nsITransformMediator_h___

#include "nsISupports.h"

class nsIDOMNode;
class nsIDOMDocument;
class nsIObserver;
class nsIXMLContentSink;
class nsString;

/* 6f4c2d0e-2cdf-11d3-aef3-00108300ff91 */
#define NS_ITRANSFORM_MEDIATOR_IID \
{ 0x6f4c2d0e, 0x2cdf, 0x11d3, {0xae, 0xf3, 0x00, 0x10, 0x83, 0x00, 0xff, 0x91} }

/**
 * This interface represents a mediator between raptor and an external
 * transformation engine.  The following process of document transformation 
 * is assumed : a source document, a stylesheet specifying the transform, 
 * and an output document are passed to a transformation engine.  All three
 * documents expose nsIDOMDocument interfaces.  The transformation engine 
 * uses the nsIDOMDocument interfaces to read the source document and 
 * stylesheet, and write the output document.
 */

class nsITransformMediator : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_ITRANSFORM_MEDIATOR_IID; return iid; }

  NS_IMETHOD SetEnabled(PRBool aValue)=0;
  NS_IMETHOD SetSourceContentModel(nsIDOMNode* aSource)=0;
  NS_IMETHOD SetStyleSheetContentModel(nsIDOMNode* aStyle)=0;
  NS_IMETHOD SetResultDocument(nsIDOMDocument* aDoc)=0;
  NS_IMETHOD GetResultDocument(nsIDOMDocument** aDoc)=0;
  NS_IMETHOD SetTransformObserver(nsIObserver* aObserver)=0;

};

extern nsresult NS_NewTransformMediator(nsITransformMediator** aInstancePtrResult,                                        
                                        const nsString& aMimeType);

#endif // nsITransformMediator_h___
