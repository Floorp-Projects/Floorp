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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */


#ifndef nsIHTMLEditor_h__
#define nsIHTMLEditor_h__


#define NS_IHTMLEDITOR_IID                       \
{ /* 4805e683-49b9-11d3-9ce4-ed60bd6cb5bc} */    \
0x4805e683, 0x49b9, 0x11d3,                      \
{ 0x9c, 0xe4, 0xed, 0x60, 0xbd, 0x6c, 0xb5, 0xbc } }

#define NS_EDITOR_ELEMENT_NOT_FOUND \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_EDITOR, 1)

class nsString;
class nsStringArray;

class nsIAtom;
class nsIDOMNode;
class nsIDOMElement;
class nsIDOMKeyEvent;
class nsIDOMEvent;


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
    eEditorFilterInputBit,                // text input is limited to certain character types, use mFilter
    eEditorMailBit,                       // use mail-compose editting rules
    eEditorDisableForcedUpdatesBit,       // prevent immediate view refreshes
    eEditorDisableForcedReflowsBit        // prevent immediate reflows
    
    // max 32 bits
  };
  
  enum {
    eEditorPlaintextMask            = (1 << eEditorPlaintextBit),
    eEditorSingleLineMask           = (1 << eEditorSingleLineBit),
    eEditorPasswordMask             = (1 << eEditorPasswordBit),
    eEditorReadonlyMask             = (1 << eEditorReadonlyBit),
    eEditorDisabledMask             = (1 << eEditorDisabledBit),
    eEditorFilterInputMask          = (1 << eEditorFilterInputBit),
    eEditorMailMask                 = (1 << eEditorMailBit),
    eEditorDisableForcedUpdatesMask = (1 << eEditorDisableForcedUpdatesBit),
    eEditorDisableForcedReflowsMask = (1 << eEditorDisableForcedReflowsBit)
  };
  
  // below used by TypedText()
  enum {
    eTypedText,  // user typed text
    eTypedBR,    // user typed shift-enter to get a br
    eTypedBreak  // user typed enter
  };
  
  // used by GetAlignment()
  typedef enum {
    eLeft,
    eCenter,
    eRight
  } EAlignment;
  
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

  NS_IMETHOD GetInlinePropertyWithAttrValue(nsIAtom *aProperty, 
                             const nsString *aAttribute,
                             const nsString *aValue,
                             PRBool &aFirst, PRBool &aAny, PRBool &aAll,
                             nsString *outValue)=0;
                             
  /**
   * RemoveAllInlineProperties() deletes all the inline properties from all 
   * text in the current selection.
   */
  NS_IMETHOD RemoveAllInlineProperties()=0;


  /**
   * RemoveInlineProperty() deletes the properties from all text in the current selection.
   * If aProperty is not set on the selection, nothing is done.
   *
   * @param aProperty   the property to remove from the selection 
   *                    All atoms are for normal HTML tags (e.g.: nsIEditorProptery::font)
   *                      except when you want to remove just links and not named anchors
   *                      For that, use nsIEditorProperty::href
   * @param aAttribute  the attribute of the property, if applicable.  May be null.
   *                    Example: aProperty=nsIEditorProptery::font, aAttribute="color"
   *                    nsIEditProperty::allAttributes is special.  It indicates that
   *                    all content-based text properties are to be removed from the selection.
   */
  NS_IMETHOD RemoveInlineProperty(nsIAtom *aProperty, const nsString *aAttribute)=0;

  /**
   *  Increase font size for text in selection by 1 HTML unit
   *  All existing text is scanned for existing <FONT SIZE> attributes
   *    so they will be incremented instead of inserting new <FONT> tag
   */
  NS_IMETHOD IncreaseFontSize()=0;

  /**
   *  Decrease font size for text in selection by 1 HTML unit
   *  All existing text is scanned for existing <FONT SIZE> attributes
   *    so they will be decreased instead of inserting new <FONT> tag
   */
  NS_IMETHOD DecreaseFontSize()=0;


  /* ------------ HTML content methods -------------- */

  /** 
   * EditorKeyPress consumes a keyevent.
   * @param aKeyEvent    key event to consume
   */
  NS_IMETHOD EditorKeyPress(nsIDOMKeyEvent* aKeyEvent)=0;

  /** 
   * TypedText responds to user typing.  Provides a logging bottleneck for typing.
   * @param aString    string to type
   * @param aAction    action to take: insert text, insert BR, insert break
   */
  NS_IMETHOD TypedText(const nsString& aString, PRInt32 aAction)=0;

  /** 
   * CanDrag decides if a drag should be started (for example, based on the current selection and mousepoint).
   */
  NS_IMETHOD CanDrag(nsIDOMEvent *aEvent, PRBool &aCanDrag)=0;

  /** 
   * DoDrag transfers the relevant data (as appropriate) to a transferable so it can later be dropped.
   */
  NS_IMETHOD DoDrag(nsIDOMEvent *aEvent)=0;

  /** 
   * InsertFromDrop looks for a dragsession and inserts the relevant data in response to a drop.
   */
  NS_IMETHOD InsertFromDrop(nsIDOMEvent *aEvent)=0;

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
   * Insert some HTML source at the current location
   *
   * @param aInputString   the string to be inserted
   */
  NS_IMETHOD InsertHTML(const nsString &aInputString)=0;


  /** Rebuild the entire document from source HTML
   *  Needed to be able to edit HEAD and other outside-of-BODY content
   *
   *  @param aSourceString   HTML source string of the entire new document
   */
  NS_IMETHOD RebuildDocumentFromSource(const nsString& aSourceString)=0;

  /**
   * Insert some HTML source, interpreting
   * the string argument according to the given charset.
   *
   * @param aInputString   the string to be inserted
   * @param aCharset       Charset of string
   * @param aParentNode    Parent to insert under.
   *                       If null, insert at the current location.
   */
  NS_IMETHOD InsertHTMLWithCharset(const nsString &aInputString,
                                   const nsString& aCharset)=0;


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
  NS_IMETHOD InsertElementAtSelection(nsIDOMElement* aElement, PRBool aDeleteSelection)=0;

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
    * Set selection to start of document
    */
  NS_IMETHOD SetCaretToDocumentStart()=0;

  /**
   * SetParagraphFormat       Insert a block paragraph tag around selection
   * @param aParagraphFormat  "p", "h1" to "h6", "address", "pre", or "blockquote"
   */
  NS_IMETHOD SetParagraphFormat(const nsString& aParagraphFormat)=0;

  /**
   * GetParagraphState returns what block tag paragraph format is in the selection.
   * @param aMixed     True if there is more than one format
   * @param outState   Name of block tag. "" is returned for none.
   */
  NS_IMETHOD GetParagraphState(PRBool &aMixed, nsString &outState)=0;

  /** 
   * GetFontFaceState returns what font face is in the selection.
   * @param aMixed    True if there is more than one font face
   * @param outFace   Name of face.  Note: "tt" is returned for
   *                  tt tag.  "" is returned for none.
   */
  NS_IMETHOD GetFontFaceState(PRBool &aMixed, nsString &outFont)=0;
  
  /** 
   * GetFontColorState returns what font face is in the selection.
   * @param aMixed     True if there is more than one font color
   * @param outColor   Color string. "" is returned for none.
   */
  NS_IMETHOD GetFontColorState(PRBool &aMixed, nsString &outColor)=0;

  /** 
   * GetFontColorState returns what font face is in the selection.
   * @param aMixed     True if there is more than one font color
   * @param outColor   Color string. "" is returned for none.
   */
  NS_IMETHOD GetBackgroundColorState(PRBool &aMixed, nsString &outColor)=0;

  /** 
   * GetListState returns what list type is in the selection.
   * @param aMixed    True if there is more than one type of list, or
   *                  if there is some list and non-list
   * @param aOL       The company that employs me.  No, really, it's 
   *                  true if an "ol" list is selected.
   * @param aUL       true if an "ul" list is selected.
   * @param aDL       true if a "dl" list is selected.
   */
  NS_IMETHOD GetListState(PRBool &aMixed, PRBool &aOL, PRBool &aUL, PRBool &aDL)=0;
  
  /** 
   * GetListItemState returns what list item type is in the selection.
   * @param aMixed    True if there is more than one type of list item, or
   *                  if there is some list and non-list
   * @param aLI       true if "li" list items are selected.
   * @param aDT       true if "dt" list items are selected.
   * @param aDD       true if "dd" list items are selected.
   */
  NS_IMETHOD GetListItemState(PRBool &aMixed, PRBool &aLI, PRBool &aDT, PRBool &aDD)=0;
  
  /** 
   * GetAlignment     returns what alignment is in the selection.
   * @param aMixed    True if there is more than one type of list item, or
   *                  if there is some list and non-list
   * @param aAlign    enum value for first encountered alignment (left/center/right)
   */
  NS_IMETHOD GetAlignment(PRBool &aMixed, EAlignment &aAlign)=0;
  
  /**
   * Document me!
   * 
   */
  NS_IMETHOD GetIndentState(PRBool &aCanIndent, PRBool &aCanOutdent)=0;

  /**
   * Document me!
   * 
   */
  NS_IMETHOD MakeOrChangeList(const nsString& aListType)=0;

  /**
   * Document me!
   * 
   */
  NS_IMETHOD RemoveList(const nsString& aListType)=0;

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
    *  Special input values:
    *    Use "href" to get a link node 
    *      (an "A" tag with the "href" attribute set)
    *    Use "anchor" or "namedanchor" to get a named anchor node
    *      (an "A" tag with the "name" attribute set)
    *    Use "list" to get an OL, UL, or DL list node
    *    Use "td" to get either a TD or TH cell node
    *
    * @param aNode    The node in the document to start the search
    *     If it is null, the anchor node of the current selection is used
    * Returns NS_EDITOR_ELEMENT_NOT_FOUND if an element is not found (passes NS_SUCCEEDED macro)
    */
  NS_IMETHOD GetElementOrParentByTagName(const nsString& aTagName, nsIDOMNode *aNode, nsIDOMElement** aReturn)=0;

  /** Return an element only if it is the only node selected,
    *    such as an image, horizontal rule, etc.
    * The exception is a link, which is more like a text attribute:
    *    The Anchor tag is returned if the selection is within the textnode(s)
    *    that are children of the "A" node.
    *    This could be a collapsed selection, i.e., a caret within the link text.
    *
    * @param aTagName  The HTML tagname or and empty string 
    *       to get any element (but only if it is the only element selected)
    *    Special input values for Links and Named anchors:
    *    Use "href" to get a link node
    *      (an "A" tag with the "href" attribute set)
    *    Use "anchor" or "namedanchor" to get a named anchor node
    *      (an "A" tag with the "name" attribute set)
    * Returns NS_EDITOR_ELEMENT_NOT_FOUND if an element is not found (passes NS_SUCCEEDED macro)
    */
  NS_IMETHOD GetSelectedElement(const nsString& aTagName, nsIDOMElement** aReturn)=0;

  /** Output the contents of the <HEAD> section as text/HTML format
    */
  NS_IMETHOD GetHeadContentsAsHTML(nsString& aOutputString)=0;

  /** Replace all children of <HEAD> with string of HTML source
    */
  NS_IMETHOD ReplaceHeadContentsWithHTML(const nsString &aSourceToInsert)=0;

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
    *    Use "href" to get a link node
    *      (an "A" tag with the "href" attribute set)
    *    Use "anchor" or "namedanchor" to get a named anchor node
    *      (an "A" tag with the "name" attribute set)
    */
  NS_IMETHOD CreateElementWithDefaults(const nsString& aTagName, nsIDOMElement** aReturn)=0;

  /** Insert an link element as the parent of the current selection
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
