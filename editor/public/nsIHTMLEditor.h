/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL")=0; you may not use this file except in
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


#ifndef nsIHTMLEditor_h__
#define nsIHTMLEditor_h__


#define NS_IHTMLEDITOR_IID                       \
{ /* 4805e683-49b9-11d3-9ce4-ed60bd6cb5bc} */    \
0x4805e683, 0x49b9, 0x11d3,                      \
{ 0x9c, 0xe4, 0xed, 0x60, 0xbd, 0x6c, 0xb5, 0xbc } }


class nsString;
class nsStringArray;

class nsIAtom;
class nsIDOMNode;
class nsIDOMElement;


class nsIHTMLEditor : public nsISupports
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IHTMLEDITOR_IID; return iid; }


  // the bits in an editor behavior mask.
  enum {
    eEditorPlaintextBit       = 0,        // only plain text entry is allowed via events
    eEditorSingleLineBit,                 // enter key and CR-LF handled specially
    eEditorPasswordBit,                   // text is not entered into content, only a representative character
    eEditorReadonlyBit,                   // editing events are disabled.  Editor may still accept focus.
    eEditorDisabledBit,                   // all events are disabled (like scrolling).  Editor will not accept focus.
    eEditorFilterInputBit                 // text input is limited to certain character types, use mFilter
    
    // max 32 bits
  };
  
  enum {
    eEditorPlaintextMask      = (1 << eEditorPlaintextBit),
    eEditorSingleLineMask     = (1 << eEditorSingleLineBit),
    eEditorPasswordMask       = (1 << eEditorPasswordBit),
    eEditorReadonlyMask       = (1 << eEditorReadonlyBit),
    eEditorDisabledMask       = (1 << eEditorDisabledBit),
    eEditorFilterInputMask    = (1 << eEditorFilterInputBit)
  };
  
  
  /* ------------ Document info methods -------------- */

  /** get the length of the document in characters */
  NS_IMETHOD GetDocumentLength(PRInt32 *aCount)=0;

  NS_IMETHOD SetMaxTextLength(PRInt32 aMaxTextLength)=0;
  NS_IMETHOD GetMaxTextLength(PRInt32& aMaxTextLength)=0;

  /* ------------ Inline property methods -------------- */


  /**
   * SetInlineProperty() sets the aggregate properties on the current selection
   *
   * @param aProperty   the property to set on the selection 
   * @param aAttribute  the attribute of the property, if applicable.  May be null.
   *                    Example: aProperty="font", aAttribute="color"
   * @param aValue      if aAttribute is not null, the value of the attribute.  May be null.
   *                    Example: aProperty="font", aAttribute="color", aValue="0x00FFFF"
   */
  NS_IMETHOD SetInlineProperty(nsIAtom *aProperty, 
                             const nsString *aAttribute,
                             const nsString *aValue)=0;

  /**
   * GetInlineProperty() gets the aggregate properties of the current selection.
   * All object in the current selection are scanned and their attributes are
   * represented in a list of Property object.
   *
   * @param aProperty   the property to get on the selection 
   * @param aAttribute  the attribute of the property, if applicable.  May be null.
   *                    Example: aProperty="font", aAttribute="color"
   * @param aValue      if aAttribute is not null, the value of the attribute.  May be null.
   *                    Example: aProperty="font", aAttribute="color", aValue="0x00FFFF"
   * @param aFirst      [OUT] PR_TRUE if the first text node in the selection has the property
   * @param aAny        [OUT] PR_TRUE if any of the text nodes in the selection have the property
   * @param aAll        [OUT] PR_TRUE if all of the text nodes in the selection have the property
   */
  NS_IMETHOD GetInlineProperty(nsIAtom *aProperty, 
                             const nsString *aAttribute,
                             const nsString *aValue,
                             PRBool &aFirst, PRBool &aAny, PRBool &aAll)=0;

  /**
   * RemoveInlineProperty() deletes the properties from all text in the current selection.
   * If aProperty is not set on the selection, nothing is done.
   *
   * @param aProperty   the property to reomve from the selection 
   * @param aAttribute  the attribute of the property, if applicable.  May be null.
   *                    Example: aProperty="font", aAttribute="color"
   *                    nsIEditProperty::allAttributes is special.  It indicates that
   *                    all content-based text properties are to be removed from the selection.
   */
  NS_IMETHOD RemoveInlineProperty(nsIAtom *aProperty, const nsString *aAttribute)=0;


  /* ------------ HTML content methods -------------- */

  /**
   * Insert a break into the content model.<br>
   * The interpretation of a break is up to the rule system:
   * it may enter a character, split a node in the tree, etc.
   */
  NS_IMETHOD InsertBreak()=0;

  /**
   * InsertText() Inserts a string at the current location, given by the selection.
   * If the selection is not collapsed, the selection is deleted and the insertion
   * takes place at the resulting collapsed selection.
   *
   * NOTE: what happens if the string contains a CR?
   *
   * @param aString   the string to be inserted
   */
  NS_IMETHOD InsertText(const nsString& aStringToInsert)=0;

  /**
   * document me!
   */
  NS_IMETHOD InsertHTML(const nsString &aInputString)=0;


  /** Insert an element, which may have child nodes, at the selection
    * Used primarily to insert a new element for various insert element dialogs,
    *   but it enforces the HTML 4.0 DTD "CanContain" rules, so it should
    *   be useful for other elements.
    *
    * @param aElement           The element to insert
    * @param aDeleteSelection   Delete the selection before inserting
    *     If aDeleteSelection is PR_FALSE, then the element is inserted 
    *     after the end of the selection for all element except
    *     Named Anchors, which insert before the selection
    */  
  NS_IMETHOD InsertElement(nsIDOMElement* aElement, PRBool aDeleteSelection)=0;



  /** 
   * DeleteSelectionAndCreateNode combines DeleteSelection and CreateNode
   * It deletes only if there is something selected (doesn't do DEL, BACKSPACE action)   
   * @param aTag      The type of object to create
   * @param aNewNode  [OUT] The node created.  Caller must release aNewNode.
   */
  NS_IMETHOD DeleteSelectionAndCreateNode(const nsString& aTag, nsIDOMNode ** aNewNode)=0;

  /* ------------ Selection manipulation -------------- */
  /* Should these be moved to nsIDOMSelection? */
  
  /** Set the selection at the suppled element
    *
    * @param aElement   An element in the document
    */
  NS_IMETHOD SelectElement(nsIDOMElement* aElement)=0;

  /** Create a collapsed selection just after aElement
    * 
    * XXX could we parameterize SelectElement(before/select/after>?
    *
    * The selection is set to parent-of-aElement with an
    *   offset 1 greater than aElement's offset
    *   but it enforces the HTML 4.0 DTD "CanContain" rules, so it should
    *   be useful for other elements.
    *
    * @param aElement  An element in the document
    */
  NS_IMETHOD SetCaretAfterElement(nsIDOMElement* aElement)=0;


  /**
   * Document me!
   * 
   */
  NS_IMETHOD GetParagraphFormat(nsString& aParagraphFormat)=0;

  /**
   * Document me!
   * 
   */
  NS_IMETHOD SetParagraphFormat(const nsString& aParagraphFormat)=0;

  /**
   * Document me!
   * 
   */
  NS_IMETHOD GetParagraphStyle(nsStringArray *aTagList)=0;

  /** Add a block parent node around the selected content.
    * Only legal nestings are allowed.
    * An example of use is for indenting using blockquote nodes.
    *
    * @param aParentTag  The tag from which the new parent is created.
    */
  NS_IMETHOD AddBlockParent(nsString& aParentTag)=0;

  /** Replace the block parent node around the selected content with a new block
    * parent node of type aParentTag.
    * Only legal replacements are allowed.
    * An example of use are is transforming H1 to LI ("paragraph type transformations").
    * For containing block transformations (transforming UL to OL, for example),
    * the caller should RemoveParent("UL"), set the selection appropriately,
    * and call AddBlockParent("OL").
    *
    * @param aParentTag  The tag from which the new parent is created.
    */
  NS_IMETHOD ReplaceBlockParent(nsString& aParentTag)=0;

  /** remove the paragraph style from the selection */
  NS_IMETHOD RemoveParagraphStyle()=0;

  /** remove block parent of type aTagToRemove from the selection.
    * if aTagToRemove is null, the nearest enclosing block that 
    * is <B>not</B> a root is removed.
    */
  NS_IMETHOD RemoveParent(const nsString &aParentTag)=0;

  /**
   * Document me!
   * 
   */
  NS_IMETHOD InsertList(const nsString& aListType)=0;

  /**
   * Document me!
   * 
   */
  NS_IMETHOD Indent(const nsString& aIndent)=0;

  /**
   * Document me!
   * 
   */
  NS_IMETHOD Align(const nsString& aAlign)=0;

  /** Return the input node or a parent matching the given aTagName,
    *   starting the search at the supplied node.
    * An example of use is for testing if a node is in a table cell
    *   given a selection anchor node.
    *
    * @param aTagName  The HTML tagname
    *    Special input values for Links and Named anchors:
    *    Use "link" or "href" to get a link node 
    *      (an "A" tag with the "href" attribute set)
    *    Use "anchor" or "namedanchor" to get a named anchor node
    *      (an "A" tag with the "name" attribute set)
    *
    * @param aNode    The node in the document to start the search
    *     If it is null, the anchor node of the current selection is used
    */
  NS_IMETHOD GetElementOrParentByTagName(const nsString& aTagName, nsIDOMNode *aNode, nsIDOMElement** aReturn)=0;

  /** Return an element only if it is the only node selected,
    *    such as an image, horizontal rule, etc.
    * The exception is a link, which is more like a text attribute:
    *    The Ancho tag is returned if the selection is within the textnode(s)
    *    that are children of the "A" node.
    *    This could be a collapsed selection, i.e., a caret within the link text.
    *
    * @param aTagName  The HTML tagname
    *    Special input values for Links and Named anchors:
    *    Use "link" or "href" to get a link node
    *      (an "A" tag with the "href" attribute set)
    *    Use "anchor" or "namedanchor" to get a named anchor node
    *      (an "A" tag with the "name" attribute set)
    */
  NS_IMETHOD GetSelectedElement(const nsString& aTagName, nsIDOMElement** aReturn)=0;

  /** Return a new element with default attribute values
    * 
    * This does not rely on the selection, and is not sensitive to context.
    * 
    * Used primarily to supply new element for various insert element dialogs
    *  (Image, Link, NamedAnchor, Table, and HorizontalRule 
    *   are the only returned elements as of 7/25/99)
    *
    * @param aTagName  The HTML tagname
    *    Special input values for Links and Named anchors:
    *    Use "link" or "href" to get a link node
    *      (an "A" tag with the "href" attribute set)
    *    Use "anchor" or "namedanchor" to get a named anchor node
    *      (an "A" tag with the "name" attribute set)
    */
  NS_IMETHOD CreateElementWithDefaults(const nsString& aTagName, nsIDOMElement** aReturn)=0;

  /** Insert an link element as the parent of the current selection
    *   be useful for other elements.
    *
    * @param aElement   An "A" element with a non-empty "href" attribute
    */
  NS_IMETHOD InsertLinkAroundSelection(nsIDOMElement* aAnchorElement)=0;

  /**
   * Document me!
   * 
   */
  NS_IMETHOD SetBackgroundColor(const nsString& aColor)=0;


  /**
   * Document me!
   * 
   */
  NS_IMETHOD SetBodyAttribute(const nsString& aAttr, const nsString& aValue)=0;

};

#endif // nsIHTMLEditor_h__
