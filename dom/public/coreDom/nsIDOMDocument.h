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

/**
 * The DocumentContext object represents information that is not strictly related 
 * to a document's content; rather, it provides the information about where the 
 * document came from, and any additional meta-data about the document
 */
class nsIDOMDocumentContext : public nsISupports {
public:
  /**
   * Return the root node of a Document Object Model. 
   *
   * @param aDocument [out] The root node of a Document Object Model
   * @return <b>NS_OK</b>   iff the function succeeds, otherwise an error code
   */
  virtual nsresult                GetDocument(nsIDOMDocument **aDocument) = 0;

  /**
   * Set the root node of a Document Object Model. 
   *
   * @param aDocument [in]  The root node of a Document Object Model
   * @return <b>NS_OK</b>   iff the function succeeds, otherwise an error code
   */
  virtual nsresult                SetDocument(nsIDOMDocument *aDocument) = 0;
};

#define NS_IDOMDOCUMENTFRAGMENT_IID \
{ /* 8f6bca72-ce42-11d1-b724-00600891d8c9 */ \
0x8f6bca72, 0xce42, 0x11d1, \
  {0xb7, 0x24, 0x00, 0x60, 0x08, 0x91, 0xd8, 0xc9} }

/**
 * DocumentFragment is the "lightweight" or "minimal" document object, or the 
 * handle for a "fragment" as returned by the Range operations, and it 
 * (as the superclass of Document) anchors the XML/HTML tree in a full-fledged document
 */
class nsIDOMDocumentFragment : public nsIDOMNode {
public:
  /**
   * Return the Document object associated with this DocumentFragment 
   *
   * @param aDocument [out] The main Document
   * @return <b>NS_OK</b>   iff the function succeeds, otherwise an error code
   */
  virtual nsresult                GetMasterDoc(nsIDOMDocument **aDocument) = 0;

  /**
   * Set the Document object associated with this DocumentFragment 
   *
   * @param aDocument [in]  The main Document
   * @return <b>NS_OK</b>   iff the function succeeds, otherwise an error code
   */
  virtual nsresult                SetMasterDoc(nsIDOMDocument *aDocument) = 0;
};

#define NS_IDOMDOCUMENT_IID \
{ /* 8f6bca73-ce42-11d1-b724-00600891d8c9 */ \
0x8f6bca73, 0xce42, 0x11d1, \
  {0xb7, 0x24, 0x00, 0x60, 0x08, 0x91, 0xd8, 0xc9} }

/**
 * The Document object represents the entire HTML or XML document. Conceptually, 
 * it is the root of the document tree, and provides the primary access to the 
 * document's data. Since Document inherits from DocumentFragment, its children are
 * contents of the Document, e.g., the root Element, the XML prolog, any processing 
 * instructions and or comments, etc. 
 *<P> Since Elements, Text nodes, Comments, PIs, etc. cannot exist outside a Document, 
 * the Document interface also anchors the "factory" methods that create these objects. 
 */
class nsIDOMDocument : public nsIDOMDocumentFragment {
public:    
  /**
   * For XML, this provides access to the Document Type Definition (see DocumentType) 
   * associated with this XML document. For HTML documents and XML documents without 
   * a document type definition this returns the value null. 
   *
   * @param aDocType [out]  The document type
   * @return <b>NS_OK</b>   iff the function succeeds, otherwise an error code
   */
  virtual nsresult                GetDocumentType(nsIDOMNode **aDocType) = 0;            

  /**
   * For XML, this sets the Document Type Definition (see DocumentType) 
   * associated with this XML document. For HTML documents and XML documents without 
   * a document type definition this is a noop.
   *
   * @param aDocType [in]   The document type
   * @return <b>NS_OK</b>   iff the function succeeds, otherwise an error code
   */
  virtual nsresult                SetDocumentType(nsIDOMNode *aNode) = 0;            
  
  /**
   * This is a "convenience" function to jump directly to the child node 
   * that is the root element of the document 
   *
   * @param aElement [out]  The root element
   * @return <b>NS_OK</b>   iff the function succeeds, otherwise an error code
   */
  virtual nsresult                GetDocumentElement(nsIDOMElement **aElement) = 0;            

  /**
   * This is a "convenience" function to set the child node 
   * that is the root element of the document 
   *
   * @param aElement [out]  The root element
   * @return <b>NS_OK</b>   iff the function succeeds, otherwise an error code
   */
  virtual nsresult                SetDocumentElement(nsIDOMElement *aElement) = 0;            

  /**
   * Return the DocumentContext 
   *
   * @param aContext [out]  The DocumentContext
   * @return <b>NS_OK</b>   iff the function succeeds, otherwise an error code
   */
  virtual nsresult                GetDocumentContext(nsIDOMDocumentContext **aDocContext) = 0;            

  /**
   * Set the DocumentContext 
   *
   * @param aContext [in]   The DocumentContext
   * @return <b>NS_OK</b>   iff the function succeeds, otherwise an error code
   */
  virtual nsresult                SetDocumentContext(nsIDOMDocumentContext *aContext) = 0;            

  /**
   * Create and return a new DocumentContext. 
   *
   * @param aDocContext [out]   The new context
   * @return <b>NS_OK</b>       iff the function succeeds, otherwise an error code
   */
  virtual nsresult                CreateDocumentContext(nsIDOMDocumentContext **aDocContext) = 0;

  /**
   * Create an element based on the tagName. Note that the instance returned may 
   * implement an interface derived from Element. The attributes parameter can be 
   * null if no attributes are specified for the new Element 
   *
   * @param aTagName [in]       The name of the element type to instantiate
   * @param aAttributes [in]    The set of attributes for the element
   * @param aElement [out]      The new element
   * @return <b>NS_OK</b>       iff the function succeeds, otherwise an error code
   */
  virtual nsresult                CreateElement(nsString &aTagName, 
                                                nsIDOMAttributeList *aAttributes, 
                                                nsIDOMElement **aElement) = 0;

  /**
   * Create and return a new Text node. 
   *
   * @param aData [in]          The data for the node
   * @param aTextNode [out]     The new text node
   * @return <b>NS_OK</b>       iff the function succeeds, otherwise an error code
   */
  virtual nsresult                CreateTextNode(nsString &aData, nsIDOMText** aTextNode) = 0;

  /**
   * Create a Comment node given the specified string 
   *
   * @param aData [in]          The data for the node
   * @param aComment [out]      The new Comment object.
   * @return <b>NS_OK</b>       iff the function succeeds, otherwise an error code
   */
  virtual nsresult                CreateComment(nsString &aData, nsIDOMComment **aComment) = 0;

  /**
   * Create a PI node given the specified name and data strings. 
   *
   * @param aName [in]          The name part of the PI.
   * @param aData [in]          The data for the node.
   * @param aPI [out]           The new PI object
   * @return <b>NS_OK</b>       iff the function succeeds, otherwise an error code
   */
  virtual nsresult                CreatePI(nsString &aName, nsString &aData, nsIDOMPI **aPI) = 0;

  /**
   * Create an Attribute of the given name and specified value. Note that the 
   * Attribute instance can then be set on an Element using the setAttribute method 
   *
   * @param aName [in]          The name of the attribute
   * @param value [in]          The value of the attribute.
   * @param aAttribute [out]    A new Attribute object.
   * @return <b>NS_OK</b>       iff the function succeeds, otherwise an error code
   */
  virtual nsresult                CreateAttribute(nsString &aName, 
                                                  nsIDOMNode *value, 
                                                  nsIDOMAttribute **aAttribute) = 0;
  
  /**
   * Create an empty AttributeList
   *
   * @param aAttributesList [out]     The new AttributeList
   * @return <b>NS_OK</b>             iff the function succeeds, otherwise an error code
   */
  virtual nsresult                CreateAttributeList(nsIDOMAttributeList **aAttributesList) = 0;

  /**
   * Create a TreeIterator anchored on a given node.  
   *
   * @param aNode [in]            The "root" node of this tree iterator.
   * @param aTreeIterator [out]   A new TreeIterator object.
   * @return <b>NS_OK</b>         iff the function succeeds, otherwise an error code
   */
  virtual nsresult                CreateTreeIterator(nsIDOMNode **aNode, nsIDOMTreeIterator **aTreeIterator) = 0;

  /**
   * Returns an iterator through all subordinate Elements with a given tag name. 
   *
   * @param aTagname [in]         The name of the tag to match on
   * @param aIterator [out]       A new NodeIterator object.
   * @return <b>NS_OK</b>         iff the function succeeds, otherwise an error code
   */
  virtual nsresult                GetElementsByTagName(nsString &aTagname, nsIDOMNodeIterator **aIterator) = 0;
};

#endif // nsIDOMDocument_h__

