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

#ifndef nsIDOMElement_h__
#define nsIDOMElement_h__

#include "nsDOM.h"
#include "nsIDOMNode.h"

// forward declaration
class nsIDOMAttribute;
class nsIDOMAttributeList;
class nsIDOMNodeIterator;

#define NS_IDOMELEMENT_IID \
{ /* 8f6bca79-ce42-11d1-b724-00600891d8c9 */ \
0x8f6bca79, 0xce42, 0x11d1, \
  {0xb7, 0x24, 0x00, 0x60, 0x08, 0x91, 0xd8, 0xc9} }

/**
 * By far the vast majority (apart from text) of node types that authors will generally 
 * encounter when traversing a document will be Element nodes. These objects represent 
 * both the element itself, as well as any contained nodes. 
 */
class nsIDOMElement : public nsIDOMNode {
public:
  /**
   * This method returns the string that is the element's name 
   *
   * @param newChild [out]  The tag name
   * @return <b>NS_OK</b>   iff the function succeeds, otherwise an error code
   */
  virtual nsresult              GetTagName(nsString &aName) = 0;

  /**
   * The attributes for this element. 
   *
   * @param aAttributeList [out]  The AttributeList
   * @return <b>NS_OK</b>         iff the function succeeds, otherwise an error code
   */
  virtual nsresult              GetAttributes(nsIDOMAttributeList **aAttributeList) = 0;

  /**
   * Retrieves an attribute value by name from an Element object. 
   * <I> NOTE: the name of this function will change to GetAttribute in a subsequent
   * release </I>
   *
   * @param aName [in]      The attribute name
   * @param aValue [out]    The attribute value as a string
   * @return <b>NS_OK</b>   iff the function succeeds, otherwise an error code
   */
  virtual nsresult              GetDOMAttribute(nsString &aName, nsString &aValue) = 0;

  /**
   * Set an attribute value from an Element object. 
   * <I> NOTE: the name of this function will change to SetAttribute in a subsequent
   * release </I>
   *
   * @param aName [in]      The attribute name
   * @param aValue [in]     The attribute value as a string
   * @return <b>NS_OK</b>   iff the function succeeds, otherwise an error code
   */
  virtual nsresult              SetDOMAttribute(nsString &aName, nsString &aValue) = 0;

  /**
   * Remove the specified attribute 
   *
   * @param aName [in]      The attribute name
   * @return <b>NS_OK</b>   iff the function succeeds, otherwise an error code
   */
  virtual nsresult              RemoveAttribute(nsString &aName) = 0;

  /**
   * Retrieves an Attribute node by name from an Element object.  
   *
   * @param aName [in]        The name of the attribute to retrieve
   * @param aAttribute [out]  The attribute with the given name
   * @return <b>NS_OK</b>     iff the function succeeds, otherwise an error code
   */
  virtual nsresult              GetAttributeNode(nsString &aName, nsIDOMAttribute **aAttribute) = 0;

  /**
   * Set an Attribute node by name from an Element object.  
   *
   * @param aAttribute [in] The attribute to set
   * @return <b>NS_OK</b>   iff the function succeeds, otherwise an error code
   */
  virtual nsresult              SetAttributeNode(nsIDOMAttribute *aAttribute) = 0;

  /**
   * Removes the specified attribute/value pair from an Element node object. 
   *
   * @param aAttribute [in] The attribute to remove
   * @return <b>NS_OK</b>   iff the function succeeds, otherwise an error code
   */
  virtual nsresult              RemoveAttributeNode(nsIDOMAttribute *aAttribute) = 0;

  /**
   * Returns an iterator through all subordinate elements with a given tag name.
   *
   * @param aName [in]      The name of the tag to match on
   * @param aIterator [out] The iterator
   * @return <b>NS_OK</b>   iff the function succeeds, otherwise an error code
   */
  virtual nsresult              GetElementsByTagName(nsString &aName,nsIDOMNodeIterator **aIterator) = 0;

  /**
   * Puts all Tet nodes in the sub-tree underneath this Element into a "normal" 
   * form where only markup (e.g., tags, comments, PIs, CDATASections, and and 
   * entity references separates Text nodes. 
   *
   * @return <b>NS_OK</b>   iff the function succeeds, otherwise an error code
   */
  virtual nsresult              Normalize() = 0;
};

#endif // nsIDOMElement_h__

