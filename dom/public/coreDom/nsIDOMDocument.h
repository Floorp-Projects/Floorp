/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsIDOMDocument_h__
#define nsIDOMDocument_h__

#include "nsDOM.h"
#include "nsISupports.h"
#include "nsIDOMNode.h"

// forward declaration
class nsIDOMDocument;
class nsIDOMElement;
class nsIDOMText;
class nsIDOMComment;
class nsIDOMPI;
class nsIDOMAttribute;
class nsIDOMAttributeList;
class nsIDOMNodeIterator;
class nsIDOMTreeIterator;

#define NS_IDOMDOCUMENTCONTEXT_IID \
{ /* 8f6bca71-ce42-11d1-b724-00600891d8c9 */ \
0x8f6bca71, 0xce42, 0x11d1, \
  {0xb7, 0x24, 0x00, 0x60, 0x08, 0x91, 0xd8, 0xc9} }

class nsIDOMDocumentContext : public nsISupports {
public:
  //attribute Document  document;
  virtual nsresult                GetDocument(nsIDOMDocument **aDocument) = 0;
  virtual nsresult                SetDocument(nsIDOMDocument *aDocument) = 0;
};

#define NS_IDOMDOCUMENTFRAGMENT_IID \
{ /* 8f6bca72-ce42-11d1-b724-00600891d8c9 */ \
0x8f6bca72, 0xce42, 0x11d1, \
  {0xb7, 0x24, 0x00, 0x60, 0x08, 0x91, 0xd8, 0xc9} }

class nsIDOMDocumentFragment : public nsIDOMNode {
public:
  //attribute Document       masterDoc;
  virtual nsresult                GetMasterDoc(nsIDOMDocument **aDocument) = 0;
  virtual nsresult                SetMasterDoc(nsIDOMDocument *aDocument) = 0;
};

#define NS_IDOMDOCUMENT_IID \
{ /* 8f6bca73-ce42-11d1-b724-00600891d8c9 */ \
0x8f6bca73, 0xce42, 0x11d1, \
  {0xb7, 0x24, 0x00, 0x60, 0x08, 0x91, 0xd8, 0xc9} }

class nsIDOMDocument : public nsIDOMDocumentFragment {
public:    
  //attribute Node           documentType;
  virtual nsresult                GetDocumentType(nsIDOMNode **aDocType) = 0;            
  virtual nsresult                SetDocumentType(nsIDOMNode *aNode) = 0;            
  
  //attribute Element        documentElement;
  virtual nsresult                GetDocumentElement(nsIDOMElement **aElement) = 0;            
  virtual nsresult                SetDocumentElement(nsIDOMElement *aElement) = 0;            

  //attribute DocumentContext contextInfo;
  virtual nsresult                GetDocumentContext(nsIDOMDocumentContext **aDocContext) = 0;            
  virtual nsresult                SetDocumentContext(nsIDOMDocumentContext *aContext) = 0;            

  virtual nsresult                CreateDocumentContext(nsIDOMDocumentContext **aDocContext) = 0;
  virtual nsresult                CreateElement(nsString &aTagName, 
                                                nsIDOMAttributeList *aAttributes, 
                                                nsIDOMElement **aElement) = 0;
  virtual nsresult                CreateTextNode(nsString &aData, nsIDOMText** aTextNode) = 0;
  virtual nsresult                CreateComment(nsString &aData, nsIDOMComment **aComment) = 0;
  virtual nsresult                CreatePI(nsString &aName, nsString &aData, nsIDOMPI **aPI) = 0;
  virtual nsresult                CreateAttribute(nsString &aName, 
                                                  nsIDOMNode *value, 
                                                  nsIDOMAttribute **aAttribute) = 0;
  virtual nsresult                CreateAttributeList(nsIDOMAttributeList **aAttributesList) = 0;
  virtual nsresult                CreateTreeIterator(nsIDOMNode **aNode, nsIDOMTreeIterator **aTreeIterator) = 0;
  virtual nsresult                GetElementsByTagName(nsIDOMNodeIterator **aIterator) = 0;
};

#endif // nsIDOMDocument_h__

