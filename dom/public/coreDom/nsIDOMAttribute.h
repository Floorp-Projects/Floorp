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

#ifndef nsIDOMAttribute_h__
#define nsIDOMAttribute_h__

#include "nsDOM.h"
#include "nsISupports.h"

// forward declaration
class nsIDOMNode;

#define NS_IDOMATTRIBUTE_IID \
{ /* 8f6bca77-ce42-11d1-b724-00600891d8c9 */ \
0x8f6bca77, 0xce42, 0x11d1, \
  {0xb7, 0x24, 0x00, 0x60, 0x08, 0x91, 0xd8, 0xc9} }

/**
 * The Attribute object represents an attribute in an Element object. Typically the 
 * allowable values for the attribute are defined in a document type definition. 
 * <P>
 * The attribute's effective value is determined as follows: if this attribute has 
 * been explicitly assigned any value, that value is the attribute's effective value; 
 * otherwise, if there is a declaration for this attribute, and that declaration 
 * includes a default value, then that default value is the attribute's effective value; 
 * otherwise, the attribute has no effective value.) Note, in particular, that an
 * effective value of the null string would be returned as a Text node instance whose 
 * toString method will return a zero length string (as will toString invoked directly 
 * on this Attribute instance). If the attribute has no effective value, then this 
 * method will return null. Note the toString method on the Attribute instance can 
 * also be used to retrieve the string version of the attribute's value(s).
 */
class nsIDOMAttribute : public nsISupports {
public:
  /**
   * Return the name of this attribute. 
   *
   * @param aName [out]     The attribute name
   * @return <b>NS_OK</b>   iff the function succeeds, otherwise an error code
   */
  virtual nsresult            GetName(nsString &aName) = 0;

  /**
   * Return the value of this attribute. 
   *
   * @param aName [out]     The attribute value
   * @return <b>NS_OK</b>   iff the function succeeds, otherwise an error code
   */
  virtual nsresult            GetValue(nsString &aName) = 0;

  /**
   * Set the value of this attribute. 
   *
   * @param aName [in]      The attribute value
   * @return <b>NS_OK</b>   iff the function succeeds, otherwise an error code
   */
  virtual nsresult            SetValue(nsString &aName) = 0;

  /**
   * If this attribute was explicitly given a value in the original document, 
   * this will be true; otherwise, it will be false. 
   *
   * @return <b>NS_OK</b>   iff the function succeeds, otherwise an error code
   */
  virtual nsresult            GetSpecified() = 0;

  /**
   * Set the specified value
   *
   * @param specified [in]  The specified value
   * @return <b>NS_OK</b>   iff the function succeeds, otherwise an error code
   */
  virtual nsresult            SetSpecified(PRBool specified) = 0;

  /**
   * Returns the attribute as a string. Character and general entity 
   * references will have been replaced with their values in the returned string. 
   *
   * @param aString [out]   The attribute as a string
   * @return <b>NS_OK</b>   iff the function succeeds, otherwise an error code
   */
  virtual nsresult            ToString(nsString &aString) = 0;
};

#define NS_IDOMATTRIBUTELIST_IID \
{ /* 8f6bca78-ce42-11d1-b724-00600891d8c9 */ \
0x8f6bca78, 0xce42, 0x11d1, \
  {0xb7, 0x24, 0x00, 0x60, 0x08, 0x91, 0xd8, 0xc9} }

/**
 * AttributeList objects are used to represent collections of Attribute objects 
 * which can be accessed by name. The Attribute objects contained in a AttributeList 
 * may also be accessed by ordinal index. In most cases, AttributeList objects are 
 * created from Element objects. 
 */
class nsIDOMAttributeList : public nsISupports {
public:
  /**
   * Retrieve an Attribute instance from the list by its name. If it's not present, 
   * null is returned. 
   *
   * @param aAttrName [in]    The name of atrribute
   * @param aAttribute [out]  The returned attribute
   * @return <b>NS_OK</b>     iff the function succeeds, otherwise an error code
   */
  virtual nsresult            GetAttribute(nsString &aAttrName, nsIDOMAttribute** aAttribute) = 0;

  /**
   * Add a new attribute to the end of the list and associate it with the given name. 
   * If the name already exists, the previous Attribute object is replaced, and returned. 
   * If no object of the same name exists, null is returned, and the named Attribute 
   * is added to the end of the AttributeList object; that is, it is accessible via 
   * the item method using the index one less than the value returned by getLength. 
   *
   * @param aAttribute [in]   The attribute to set
   * @return <b>NS_OK</b>     iff the function succeeds, otherwise an error code
   */
  virtual nsresult            SetAttribute(nsIDOMAttribute *attr) = 0;

  /**
   * Removes the Attribute instance named name from the list and returns it.
   *
   * @param aAttrName [in]    The name of atrribute
   * @param aAttribute [out]  The removed attribute
   * @return <b>NS_OK</b>     iff the function succeeds, otherwise an error code
   */
  virtual nsresult            Remove(nsString &attrName, nsIDOMAttribute** aAttribute) = 0;

  /**
   * Returns the (zero-based) indexth Attribute item in the collection. 
   *
   * @param aIndex [in]       The index in the list of attributes
   * @param aAttribute [out]  The attribute at that index
   * @return <b>NS_OK</b>     iff the function succeeds, otherwise an error code
   */
  virtual nsresult            Item(PRUint32 aIndex, nsIDOMAttribute** aAttribute) = 0;

  /**
   * Returns the number of Attributes in the AttributeList instance. 
   *
   * @param aLength [out]   The attribute list length
   * @return <b>NS_OK</b>   iff the function succeeds, otherwise an error code
   */
  virtual nsresult            GetLength(PRUint32 *aLength) = 0;
};


#endif // nsIDOMAttribute_h__

