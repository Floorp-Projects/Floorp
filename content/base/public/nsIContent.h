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
#ifndef nsIContent_h___
#define nsIContent_h___

#include <stdio.h>
#include "nslayout.h"
#include "nsISupports.h"
#include "nsGUIEvent.h"

// Forward declarations
class nsIAtom;
class nsIContentDelegate;
class nsIDocument;
class nsIPresContext;
class nsString;
class nsString;
class nsVoidArray;
class nsXIFConverter;
class nsIDOMEvent;
class nsIContent;
class nsISupportsArray;
class nsIDOMRange;

// IID for the nsIContent interface
#define NS_ICONTENT_IID       \
{ 0x78030220, 0x9447, 0x11d1, \
  {0x93, 0x23, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }

// A node of content in a documents content model. This interface
// is supported by all content objects.
class nsIContent : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_ICONTENT_IID; return iid; }

  NS_IMETHOD GetDocument(nsIDocument*& aResult) const = 0;

  NS_IMETHOD SetDocument(nsIDocument* aDocument, PRBool aDeep) = 0;

  NS_IMETHOD GetParent(nsIContent*& aResult) const = 0;

  NS_IMETHOD SetParent(nsIContent* aParent) = 0;

  /**
   * Get the namespace that this tag is defined in
   */
  NS_IMETHOD GetNameSpaceID(PRInt32& aResult) const = 0;

  NS_IMETHOD GetTag(nsIAtom*& aResult) const = 0;

  NS_IMETHOD CanContainChildren(PRBool& aResult) const = 0;

  NS_IMETHOD ChildCount(PRInt32& aResult) const = 0;

  NS_IMETHOD ChildAt(PRInt32 aIndex, nsIContent*& aResult) const = 0;

  NS_IMETHOD IndexOf(nsIContent* aPossibleChild, PRInt32& aIndex) const = 0;

  NS_IMETHOD InsertChildAt(nsIContent* aKid, PRInt32 aIndex,
                           PRBool aNotify) = 0;

  NS_IMETHOD ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex,
                            PRBool aNotify) = 0;

  NS_IMETHOD AppendChildTo(nsIContent* aKid, PRBool aNotify) = 0;

  NS_IMETHOD RemoveChildAt(PRInt32 aIndex, PRBool aNotify) = 0;

  /**
   * Test and see if this piece of content is synthetic. Synthetic content
   * is content generated stylistically and should normally be ignored
   * during content saving.
   */
  NS_IMETHOD IsSynthetic(PRBool& aResult) = 0;

  /**
   * Parses an attribute string into an atom that represents the
   * attribute name and an identifier that represents the namespace
   * of the attribute. The namespace identifier may be computed
   * from a namespace prefix that must be interpreted in the context
   * of the content itself.
   *
   * @param aStr the unparsed attribute string
   * @param aName out parameter representing the name of the attribute
   * @param aNameSpaceID out parameter reprsenting the namespace 
   *                     of the attribute
   */
  NS_IMETHOD ParseAttributeString(const nsString& aStr, 
                                  nsIAtom*& aName,
                                  PRInt32& aNameSpaceID) = 0;

  /**
   * Returns the prefix for the specified name space identifier in 
   * the context of the content element itself.
   *
   * @param aNameSpaceID identifier of the namespace
   * @param aPrefix out parameter representing the prefix for the namespace
   */
  NS_IMETHOD GetNameSpacePrefixFromId(PRInt32 aNameSpaceID,
                                      nsIAtom*& aPrefix) = 0;

  /**
   * Set attribute values. All attribute values are assumed to have a
   * canonical String representation that can be used for these
   * methods. The setAttribute method is assumed to perform a translation
   * of the canonical form into the underlying content specific
   * form.
   *
   * @param aName the name of the attribute

   * @param aValue may legitimately be the empty string.
   *
   * @param aUpdateMask specifies how whether or not the document should be
   * notified of the attribute change.
   */
  NS_IMETHOD SetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,
                          const nsString& aValue,
                          PRBool aNotify) = 0;

  /**
   * Get the current value of the attribute. This returns a form that is
   * suitable for passing back into setAttribute.
   *
   * <UL>
   *
   * <LI>If the attribute is not set and has no default value, return
   * NS_CONTENT_ATTR_NOT_THERE.
   *
   * <LI>If the attribute exists, but has no value, return
   * NS_CONTENT_ATTR_NO_VALUE.
   *
   * <LI>If the attribute has a value, empty or otherwise, set ret to
   * be the value, and return NS_CONTENT_ATTR_HAS_VALUE (== NS_OK).
   *
   * </UL>
   */
  NS_IMETHOD GetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, 
                          nsString& aResult) const = 0;

  /**
   * Remove an attribute so that it is no longer explicitly specified.
   *
   * @param aAttribute the name of the attribute to unset
   * 
   * @param aNotify specifies whether or not the document should be
   * notified of the attribute change
   *
   */
  NS_IMETHOD UnsetAttribute(PRInt32 aNameSpaceID, nsIAtom* aAttribute, 
                            PRBool aNotify) = 0;


  /**
   * Get the namespace & name of a given attribute.
   * 
   * @param aIndex the index of the attribute name
   * @param aNameSpace an out param of the name space ID of the attribute name
   * @param aName an out param if the attribute name
   *
   */
  NS_IMETHOD GetAttributeNameAt(PRInt32 aIndex,
                                PRInt32& aNameSpaceID, 
                                nsIAtom*& aName) const = 0;

  /**
   * Get the number of all specified attributes.
   *
   * @param aCountResult an out parameter to be filled in with
   *        the number of attributes
   *
   */
  NS_IMETHOD GetAttributeCount(PRInt32& aCountResult) const = 0;

  /**
   * List the content (and anything it contains) out to the given
   * file stream. Use aIndent as the base indent during formatting.
   * Returns NS_OK unless a file error occurs.
   */
  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const = 0;

  /**
   * Translate the content object into the (XIF) XML Interchange Format
   * XIF is an intermediate form of the content model, the buffer
   * will then be parsed into any number of formats including HTML, TXT, etc.

   * BeginConvertToXIF -- opens a container and writes out the attributes
   * ConvertContentToXIF -- typically does nothing unless there is text content
   * FinishConvertToXIF -- closes a container
   */
  NS_IMETHOD BeginConvertToXIF(nsXIFConverter& aConverter) const = 0;
  NS_IMETHOD ConvertContentToXIF(nsXIFConverter& aConverter) const = 0;
  NS_IMETHOD FinishConvertToXIF(nsXIFConverter& aConverter) const = 0;

  /**
   * Inform content of range ownership changes.  This allows content
   * to do the right thing to ranges in the face of changes to the content
   * model.

   * RangeAdd -- informs content that it owns one or both range endpoints
   * RangeRemove -- informs content that it no longer owns a range endpoint
   * GetRangeList -- returns the list of ranges that have one or both endpoints
   *                 within this content item
   */
  NS_IMETHOD RangeAdd(nsIDOMRange& aRange) = 0;
  NS_IMETHOD RangeRemove(nsIDOMRange& aRange) = 0;
  NS_IMETHOD GetRangeList(nsVoidArray*& aResult) const = 0;
  
  /**
   * Handle a DOM event for this piece of content.
   */
  NS_IMETHOD HandleDOMEvent(nsIPresContext& aPresContext,
                            nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent,
                            PRUint32 aFlags,
                            nsEventStatus& aEventStatus) = 0;
};

// nsresult codes for GetAttribute
#define NS_CONTENT_ATTR_HAS_VALUE NS_OK

#define NS_CONTENT_ATTR_NO_VALUE \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_LAYOUT,0)

#define NS_CONTENT_ATTR_NOT_THERE \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_LAYOUT,1)

#endif /* nsIContent_h___ */
