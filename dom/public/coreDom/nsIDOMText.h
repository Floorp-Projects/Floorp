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

#ifndef nsIDOMText_h__
#define nsIDOMText_h__

#include "nsDOM.h"
#include "nsIDOMNode.h"

// forward declaration
class nsIDOMElement;

#define NS_IDOMTEXT_IID \
{ /* 8f6bca7b-ce42-11d1-b724-00600891d8c9 */ \
0x8f6bca7b, 0xce42, 0x11d1, \
  {0xb7, 0x24, 0x00, 0x60, 0x08, 0x91, 0xd8, 0xc9} }

/**
 * The Text object contains the non-markup content of an Element. If there is no 
 * markup inside an Element's content, the text will be contained in a single 
 * Text object that is the child of the Element. Any markup will parse into 
 * child elements that are siblings of the Text nodes on either side of it, and 
 * whose content will be represented as Text node children of the markup element. 
 */
class nsIDOMText : public nsIDOMNode {
public:
  /**
   * Return the actual content of the text node. Text nodes contain just plain text, 
   * without markup and without entities, both of which are represented as separate 
   * objects in the DOM. 
   *
   * @param aString [out]   The text data
   * @return <b>NS_OK</b>   iff the function succeeds, otherwise an error code
   */
  virtual nsresult            GetData(nsString &aString) = 0;

  /**
   * Set the actual content of the text node. Text nodes contain just plain text, 
   * without markup and without entities, both of which are represented as separate 
   * objects in the DOM. 
   *
   * @param aString [in]    The text data
   * @return <b>NS_OK</b>   iff the function succeeds, otherwise an error code
   */
  virtual nsresult            SetData(nsString &aString) = 0;

  /**
   * Append the string to the end of the character data in this Text node 
   *
   * @param aData [in]      The data to append
   * @return <b>NS_OK</b>   iff the function succeeds, otherwise an error code
   */
  virtual nsresult            Append(nsString &aData) = 0;

  /**
   * Insert the string at the specified character offset. 
   *
   * @param offset [in]     The offset
   * @param aData [in]      The data to insert
   * @return <b>NS_OK</b>   iff the function succeeds, otherwise an error code
   */
  virtual nsresult            Insert(int offset, nsString &aData) = 0;

  /**
   * Delete the specified characters range
   *
   * @param offset [in]     The offset where deletion starts
   * @param count [out]     The number of chracters to delete
   * @return <b>NS_OK</b>   iff the function succeeds, otherwise an error code
   */
  virtual nsresult            Delete(int offset, int count) = 0;

  /**
   * Replace the characters starting at the specified character offset with 
   * the specified string. 
   *
   * @param offset [in]     Offset at which to start replacing
   * @param count [in]      Number of characters to replace
   * @param aData [in]      String to replace previous content with.
   * @return <b>NS_OK</b>   iff the function succeeds, otherwise an error code
   */
  virtual nsresult            Replace(int offset, int count, nsString &aData) = 0;

  /**
   * Insert the specified element in the tree as a sibling of the Text node. 
   * The result of this operation may be the creation of up to 2 new Text nodes: 
   * the character data specified by the offset and count will form one Text node
   * that will become the child of the inserted element and the remainder of 
   * the character data (after the offset and count) will form another Text node 
   * become a sibling of the original Text node. 
   *
   * @param element [in]    Element to insert into the tree.
   * @param offset [in]     Offset at which to insert.
   * @param count [in]      Number of characters to copy to child Text node of element.
   * @return <b>NS_OK</b>   iff the function succeeds, otherwise an error code
   */
  virtual nsresult            Splice(nsIDOMElement *element, int offset, int count) = 0;
};

#endif // nsIDOMText_h__

