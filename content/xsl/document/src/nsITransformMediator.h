/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsITransformMediator_h___
#define nsITransformMediator_h___

#include "nsISupports.h"
#include "nsAString.h"

class nsIDOMNode;
class nsIDOMDocument;
class nsIObserver;
class nsIXMLContentSink;

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
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ITRANSFORM_MEDIATOR_IID);

  NS_IMETHOD SetEnabled(PRBool aValue)=0;
  NS_IMETHOD SetSourceContentModel(nsIDOMNode* aSource)=0;
  NS_IMETHOD SetStyleSheetContentModel(nsIDOMNode* aStyle)=0;
  NS_IMETHOD SetResultDocument(nsIDOMDocument* aDoc)=0;
  NS_IMETHOD GetResultDocument(nsIDOMDocument** aDoc)=0;
  NS_IMETHOD SetTransformObserver(nsIObserver* aObserver)=0;

};

extern nsresult NS_NewTransformMediator(nsITransformMediator** aInstancePtrResult,                                        
                                        const nsACString& aMimeType);

#endif // nsITransformMediator_h___
