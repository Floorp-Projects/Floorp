/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set tw=80 expandtab softtabstop=2 ts=2 sw=2: */
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
#ifndef nsGenericHTMLElement_h___
#define nsGenericHTMLElement_h___

#include "nsGenericElement.h"
#include "nsHTMLParts.h"
#include "nsIDOMHTMLElement.h"
#include "nsIContent.h"
#include "nsHTMLValue.h"
#include "nsVoidArray.h"
#include "nsINameSpaceManager.h"  // for kNameSpaceID_None
#include "nsIFormControl.h"

#include "nsIStatefulFrame.h"

class nsIDOMAttr;
class nsIDOMEventListener;
class nsIDOMNodeList;
class nsIFrame;
class nsHTMLAttributes;
class nsIHTMLMappedAttributes;
class nsIHTMLContent;
class nsIStyleRule;
class nsISupportsArray;
class nsChildContentList;
class nsDOMCSSDeclaration;
class nsIDOMCSSStyleDeclaration;
class nsIURI;
class nsIFormControlFrame;
class nsIForm;
class nsIPresState;
class nsIScrollableView;
class nsILayoutHistoryState;
struct nsRect;


/**
 * A common superclass for HTML elements
 */
class nsGenericHTMLElement : public nsGenericElement
{
public:
  nsGenericHTMLElement();
  virtual ~nsGenericHTMLElement();

#ifdef GATHER_ELEMENT_USEAGE_STATISTICS
  nsresult Init(nsINodeInfo *aNodeInfo);
#endif

  /** Call on shutdown to release globals */
  static void Shutdown();

  // nsISupports
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  /**
   * Handle QI for the standard DOM interfaces (DOMNode, DOMElement,
   * DOMHTMLElement) and handles tearoffs for other standard interfaces.
   * @param aElement the element as nsIDOMHTMLElement*
   * @param aIID the IID to QI to
   * @param aInstancePtr the QI'd method [OUT]
   * @see nsGenericHTMLElementTearoff
   */
  nsresult DOMQueryInterface(nsIDOMHTMLElement *aElement, REFNSIID aIID,
                             void **aInstancePtr);

  // From nsGenericElement
  NS_METHOD CopyInnerTo(nsIContent* aSrcContent,
                        nsGenericHTMLElement* aDest,
                        PRBool aDeep);

  // Implementation for nsIDOMNode
  NS_METHOD GetNodeName(nsAString& aNodeName);
  NS_METHOD GetLocalName(nsAString& aLocalName);

  // Implementation for nsIDOMElement
  NS_METHOD GetAttribute(const nsAString& aName,
                         nsAString& aReturn) 
  {
    return nsGenericElement::GetAttribute(aName, aReturn);
  }
  NS_METHOD SetAttribute(const nsAString& aName,
                         const nsAString& aValue)
  {
    return nsGenericElement::SetAttribute(aName, aValue);
  }
  NS_METHOD GetTagName(nsAString& aTagName);
  NS_METHOD GetElementsByTagName(const nsAString& aTagname,
                                 nsIDOMNodeList** aReturn);

  // Implementation for nsIDOMHTMLElement
  nsresult GetId(nsAString& aId);
  nsresult SetId(const nsAString& aId);
  nsresult GetTitle(nsAString& aTitle);
  nsresult SetTitle(const nsAString& aTitle);
  nsresult GetLang(nsAString& aLang);
  nsresult SetLang(const nsAString& aLang);
  nsresult GetDir(nsAString& aDir);
  nsresult SetDir(const nsAString& aDir);
  nsresult GetClassName(nsAString& aClassName);
  nsresult SetClassName(const nsAString& aClassName);
  nsresult GetStyle(nsIDOMCSSStyleDeclaration** aStyle);
  nsresult GetOffsetTop(PRInt32* aOffsetTop);
  nsresult GetOffsetLeft(PRInt32* aOffsetLeft);
  nsresult GetOffsetWidth(PRInt32* aOffsetWidth);
  nsresult GetOffsetHeight(PRInt32* aOffsetHeight);
  nsresult GetOffsetParent(nsIDOMElement** aOffsetParent);
  virtual nsresult GetInnerHTML(nsAString& aInnerHTML);
  virtual nsresult SetInnerHTML(const nsAString& aInnerHTML);
  nsresult GetScrollTop(PRInt32* aScrollTop);
  nsresult SetScrollTop(PRInt32 aScrollTop);
  nsresult GetScrollLeft(PRInt32* aScrollLeft);
  nsresult SetScrollLeft(PRInt32 aScrollLeft);
  nsresult GetScrollHeight(PRInt32* aScrollHeight);
  nsresult GetScrollWidth(PRInt32* aScrollWidth);
  nsresult GetClientHeight(PRInt32* aClientHeight);
  nsresult GetClientWidth(PRInt32* aClientWidth);
  nsresult ScrollIntoView(PRBool aTop);

  /**
   * Get the frame's offset information for offsetTop/Left/Width/Height.
   * @param aRect the offset information [OUT]
   * @param aOffsetParent the parent the offset is relative to (offsetParent)
   *        [OUT]
   */
  nsresult GetOffsetRect(nsRect& aRect,
                         nsIContent** aOffsetParent);
  nsresult GetScrollInfo(nsIScrollableView **aScrollableView, float *aP2T,
                         float *aT2P);

  // Implementation for nsIContent
  NS_IMETHOD SetDocument(nsIDocument* aDocument, PRBool aDeep,
                         PRBool aCompileEventHandlers);
  NS_IMETHOD GetNameSpaceID(PRInt32& aID) const;
  NS_IMETHOD NormalizeAttrString(const nsAString& aStr,
                                 nsINodeInfo*& aNodeInfo);
  NS_IMETHOD SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                     const nsAString& aValue,
                          PRBool aNotify);
  NS_IMETHOD SetAttr(nsINodeInfo* aNodeInfo,
                     const nsAString& aValue,
                     PRBool aNotify);
  NS_IMETHOD GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                     nsAString& aResult) const;
  NS_IMETHOD GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                     nsIAtom*& aPrefix, nsAString& aResult) const;
  NS_IMETHOD_(PRBool) HasAttr(PRInt32 aNameSpaceID, nsIAtom* aName) const;
  NS_IMETHOD UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                       PRBool aNotify);
  NS_IMETHOD GetAttrNameAt(PRInt32 aIndex,
                           PRInt32& aNameSpaceID, 
                           nsIAtom*& aName,
                           nsIAtom*& aPrefix) const;
  NS_IMETHOD GetAttrCount(PRInt32& aResult) const;
#ifdef DEBUG
  NS_IMETHOD List(FILE* out, PRInt32 aIndent) const;
  NS_IMETHOD DumpContent(FILE* out, PRInt32 aIndent,PRBool aDumpAll) const;
#endif
  NS_IMETHOD_(PRBool) IsContentOfType(PRUint32 aFlags);

  /**
   * Standard anchor HandleDOMEvent, used by A, AREA and LINK (parameters
   * are the same as HandleDOMEvent)
   */
  nsresult HandleDOMEventForAnchors(nsIContent* aOuter,
                                    nsIPresContext* aPresContext,
                                    nsEvent* aEvent,
                                    nsIDOMEvent** aDOMEvent,
                                    PRUint32 aFlags,
                                    nsEventStatus* aEventStatus);

  // Implementation for nsIHTMLContent
  NS_IMETHOD SetHTMLAttribute(nsIAtom* aAttribute, const nsHTMLValue& aValue,
                              PRBool aNotify);
  NS_IMETHOD GetHTMLAttribute(nsIAtom* aAttribute, nsHTMLValue& aValue) const;
  NS_IMETHOD GetID(nsIAtom*& aResult) const;
  NS_IMETHOD GetClasses(nsVoidArray& aArray) const;
  NS_IMETHOD_(PRBool) HasClass(nsIAtom* aClass, PRBool aCaseSensitive) const;
  NS_IMETHOD WalkContentStyleRules(nsRuleWalker* aRuleWalker);
  NS_IMETHOD GetInlineStyleRule(nsIStyleRule** aStyleRule);
  NS_IMETHOD GetBaseURL(nsIURI*& aBaseURL) const;
  NS_IMETHOD GetBaseTarget(nsAString& aBaseTarget) const;

  //----------------------------------------
  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,
                               const nsHTMLValue& aValue,
                               nsAString& aResult) const;
  NS_IMETHOD GetMappedAttributeImpact(const nsIAtom* aAttribute, PRInt32 aModType,
                                      nsChangeHint& aHint) const;
  NS_IMETHOD GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const;

#ifdef DEBUG
  void ListAttributes(FILE* out) const;

  PRUint32 BaseSizeOf(nsISizeOfHandler* aSizer) const;
#endif

  /**
   * Get the primary form control frame for this content (see
   * GetFormControlFrameFor)
   *
   * @param aFlushContent whether to flush the content sink
   * @return the primary form control frame (or null)
   */
  nsIFormControlFrame* GetFormControlFrame(PRBool aFlushContent)
  {
    if (!mDocument) {
      return nsnull;
    }

    return GetFormControlFrameFor(this, mDocument, aFlushContent);
  }

  /**
   * Get the primary frame for this content (see GetPrimaryFrameFor)
   *
   * @param aFlushContent whether to flush the content sink
   * @return the primary frame
   */
  nsIFrame* GetPrimaryFrame(PRBool aFlushContent)
  {
    if (!mDocument) {
      return nsnull;
    }

    return GetPrimaryFrameFor(this, mDocument, aFlushContent);
  }

  //----------------------------------------

  /**
   * Parse common attributes (currently dir and lang, may be more)
   *
   * @param aAttribute the attr to parse
   * @param aValue the value to parse
   * @param aResult the resulting HTMLValue
   * @return whether the value was parsed
   */
  static PRBool ParseCommonAttribute(nsIAtom* aAttribute, 
                                     const nsAString& aValue, 
                                     nsHTMLValue& aResult);
  /**
   * Parse an alignment attribute (top/middle/bottom/baseline)
   *
   * @param aString the string to parse
   * @param aResult the resulting HTMLValue
   * @return whether the value was parsed
   */
  static PRBool ParseAlignValue(const nsAString& aString,
                                nsHTMLValue& aResult);

  /**
   * Parse a div align string to value (left/right/center/middle/justify)
   *
   * @param aString the string to parse
   * @param aResult the resulting HTMLValue
   * @return whether the value was parsed
   */
  PRBool ParseDivAlignValue(const nsAString& aString,
                            nsHTMLValue& aResult) const;
  /**
   * Convert a div align value to string
   *
   * @param aValue the value to convert
   * @param aResult the resulting string
   * @return whether the value was converted
   */
  PRBool DivAlignValueToString(const nsHTMLValue& aValue,
                               nsAString& aResult) const;

  /**
   * Convert a table halign string to value (left/right/center/char/justify)
   *
   * @param aString the string to parse
   * @param aResult the resulting HTMLValue
   * @return whether the value was parsed
   */
  PRBool ParseTableHAlignValue(const nsAString& aString,
                               nsHTMLValue& aResult) const;
  /**
   * Convert a table halign value to string
   *
   * @param aValue the value to convert
   * @param aResult the resulting string
   * @return whether the value was converted
   */
  PRBool TableHAlignValueToString(const nsHTMLValue& aValue,
                                  nsAString& aResult) const;

  /**
   * Convert a table cell halign string to value
   *
   * @param aString the string to parse
   * @param aResult the resulting HTMLValue
   * @return whether the value was parsed
   */
  PRBool ParseTableCellHAlignValue(const nsAString& aString,
                                   nsHTMLValue& aResult) const;
  /**
   * Convert a table cell halign value to string
   *
   * @param aValue the value to convert
   * @param aResult the resulting string
   * @return whether the value was converted
   */
  PRBool TableCellHAlignValueToString(const nsHTMLValue& aValue,
                                      nsAString& aResult) const;

  /**
   * Convert a table valign string to value (left/right/center/char/justify/
   * abscenter/absmiddle/middle)
   *
   * @param aString the string to parse
   * @param aResult the resulting HTMLValue
   * @return whether the value was parsed
   */
  static PRBool ParseTableVAlignValue(const nsAString& aString,
                                      nsHTMLValue& aResult);
  /**
   * Convert a table valign value to string
   *
   * @param aValue the value to convert
   * @param aResult the resulting string
   * @return whether the value was converted
   */
  static PRBool TableVAlignValueToString(const nsHTMLValue& aValue,
                                         nsAString& aResult);

  /**
   * Convert an align value to string (left/right/texttop/baseline/center/
   * bottom/top/middle/absbottom/abscenter/absmiddle)
   *
   * @param aValue the value to convert
   * @param aResult the resulting string
   * @return whether the value was converted
   */
  static PRBool AlignValueToString(const nsHTMLValue& aValue,
                                   nsAString& aResult);

  /**
   * Convert a valign value to string
   *
   * @param aValue the value to convert
   * @param aResult the resulting string
   * @return whether the value was converted
   */
  static PRBool VAlignValueToString(const nsHTMLValue& aValue,
                                    nsAString& aResult);

  /**
   * Convert an image attribute to value (width, height, hspace, vspace, border)
   *
   * @param aAttribute the attribute to parse
   * @param aString the string to parse
   * @param aResult the resulting HTMLValue
   * @return whether the value was parsed
   */
  static PRBool ParseImageAttribute(nsIAtom* aAttribute,
                                    const nsAString& aString,
                                    nsHTMLValue& aResult);
  /**
   * Convert an image attribute to string
   *
   * @param aAttribute the attribute to parse
   * @param aValue the value to convert
   * @param aResult the resulting string
   * @return whether the value was converted
   */
  static PRBool ImageAttributeToString(nsIAtom* aAttribute,
                                       const nsHTMLValue& aValue,
                                       nsAString& aResult);

  /**
   * Convert a frameborder string to value (yes/no/1/0)
   *
   * @param aString the string to parse
   * @param aResult the resulting HTMLValue
   * @return whether the value was parsed
   */
  static PRBool ParseFrameborderValue(const nsAString& aString,
                                      nsHTMLValue& aResult);
  /**
   * Convert a frameborder value to string
   *
   * @param aValue the value to convert
   * @param aResult the resulting string
   * @return whether the value was converted
   */
  static PRBool FrameborderValueToString(const nsHTMLValue& aValue,
                                         nsAString& aResult);

  /**
   * Convert a scrolling string to value (yes/no/on/off/scroll/noscroll/auto)
   *
   * @param aString the string to parse
   * @param aResult the resulting HTMLValue
   * @return whether the value was parsed
   */
  static PRBool ParseScrollingValue(const nsAString& aString,
                                    nsHTMLValue& aResult);
  /**
   * Convert a scrolling value to string
   *
   * @param aValue the value to convert
   * @param aResult the resulting string
   * @return whether the value was converted
   */
  static PRBool ScrollingValueToString(const nsHTMLValue& aValue,
                                       nsAString& aResult);

  /**
   * Create the style struct from the style attr.  Used when an element is first
   * put into a document.  Only has an effect if the old value is a string.
   */
  nsresult  ReparseStyleAttribute(void);
  /**
   * Parse a style attr value into a CSS rulestruct (or, if there is no
   * document, leave it as a string) and return as HTMLValue.
   *
   * @param aValue the value to parse
   * @param aResult the resulting HTMLValue [OUT]
   */
  nsresult  ParseStyleAttribute(const nsAString& aValue,
                                nsHTMLValue& aResult);

  /*
   * Attribute Mapping Helpers
   *
   * All attributes that are mapped into style contexts must have a 
   * matched set of mapping function and impact getter
   */

  /**
   * A style attribute mapping function for the most common attributes, to be
   * called by subclasses' attribute mapping functions.  Currently handles
   * dir and lang, could handle others.
   *
   * @param aAttributes the list of attributes to map
   * @param aData the returned rule data [INOUT]
   * @see GetAttributeMappingFunction
   */
  static void MapCommonAttributesInto(const nsIHTMLMappedAttributes* aAttributes, 
                                      nsRuleData* aRuleData);
  /**
   * Get the style impact of common attributes (dir, lang and _baseHref
   * currently).  To be called from GetMappedAttributeImpact().
   *
   * @param aAttribute the attribute to test for impact
   * @param aHint the impact (NS_STYLE_HINT_*)
   * @return whether the impact was set
   */
  static PRBool GetCommonMappedAttributesImpact(const nsIAtom* aAttribute,
                                                nsChangeHint& aHint);

  /**
   * Get the style impact of image attributes (width, height, hspace and vspace
   * currently).  To be called from GetMappedAttributeImpact().
   *
   * @param aAttribute the attribute to test for impact
   * @param aHint the impact (NS_STYLE_HINT_*)
   * @return whether the impact was set
   */
  static PRBool GetImageMappedAttributesImpact(const nsIAtom* aAttribute,
                                               nsChangeHint& aHint);
  /**
   * Get the style impact of image align attributes (align currently).  To be
   * called from GetMappedAttributeImpact().
   *
   * @param aAttribute the attribute to test for impact
   * @param aHint the impact (NS_STYLE_HINT_*)
   * @return whether the impact was set
   */
  static PRBool GetImageAlignAttributeImpact(const nsIAtom* aAttribute,
                                             nsChangeHint& aHint);

  /**
   * Helper to map the align attribute into a style struct.
   *
   * @param aAttributes the list of attributes to map
   * @param aData the returned rule data [INOUT]
   * @see GetAttributeMappingFunction
   */
  static void MapAlignAttributeInto(const nsIHTMLMappedAttributes* aAttributes,
                                    nsRuleData* aData);

  /**
   * Helper to map the align attribute into a style struct for things
   * like <div>, <h1>, etc.
   *
   * @param aAttributes the list of attributes to map
   * @param aData the returned rule data [INOUT]
   * @see GetAttributeMappingFunction
   */
  static void MapDivAlignAttributeInto(const nsIHTMLMappedAttributes* aAttributes,
                                       nsRuleData* aData);

  /**
   * Helper to map the image border attribute into a style struct.
   *
   * @param aAttributes the list of attributes to map
   * @param aData the returned rule data [INOUT]
   * @see GetAttributeMappingFunction
   */
  static void MapImageBorderAttributeInto(const nsIHTMLMappedAttributes* aAttributes,
                                          nsRuleData* aData);
  /**
   * Helper to map the image margin attribute into a style struct.
   *
   * @param aAttributes the list of attributes to map
   * @param aData the returned rule data [INOUT]
   * @see GetAttributeMappingFunction
   */
  static void MapImageMarginAttributeInto(const nsIHTMLMappedAttributes* aAttributes,
                                          nsRuleData* aData);
  /**
   * Helper to map the image position attribute into a style struct.
   *
   * @param aAttributes the list of attributes to map
   * @param aData the returned rule data [INOUT]
   * @see GetAttributeMappingFunction
   */
  static void MapImagePositionAttributeInto(const nsIHTMLMappedAttributes* aAttributes,
                                            nsRuleData* aData);
  /**
   * Get the style impact of image border attributes (border currently).  To be
   * called from GetMappedAttributeImpact().
   *
   * @param aAttribute the attribute to test for impact
   * @param aHint the impact (NS_STYLE_HINT_*)
   * @return whether the impact was set
   */
  static PRBool GetImageBorderAttributeImpact(const nsIAtom* aAttribute,
                                              nsChangeHint& aHint);

  /**
   * Helper to map the background attributes (currently background and bgcolor)
   * into a style struct.
   *
   * @param aAttributes the list of attributes to map
   * @param aData the returned rule data [INOUT]
   * @see GetAttributeMappingFunction
   */
  static void MapBackgroundAttributesInto(const nsIHTMLMappedAttributes* aAttributes,
                                          nsRuleData* aData);
  /**
   * Get the style impact of background attributes (background and bgcolor
   * currently).  To be called from GetMappedAttributeImpact().
   *
   * @param aAttribute the attribute to test for impact
   * @param aHint the impact (NS_STYLE_HINT_*)
   * @return whether the impact was set
   */
  static PRBool GetBackgroundAttributesImpact(const nsIAtom* aAttribute,
                                              nsChangeHint& aHint);

  /**
   * Get the primary frame for a piece of content.
   *
   * @param aContent the content to get the primary frame for
   * @param aDocument the document for this content
   * @param aFlushContent whether to flush the content sink, which creates
   *        frames for content that do not already have it.  EXPENSIVE.
   * @return the primary frame
   */
  static nsIFrame* GetPrimaryFrameFor(nsIContent* aContent,
                                      nsIDocument* aDocument,
                                      PRBool aFlushContent);

  /**
   * Get the primary form control frame for a piece of content.  Same as
   * GetPrimaryFrameFor, except it QI's to nsIFormControlFrame.
   *
   * @param aContent the content to get the primary frame for
   * @param aDocument the document for this content
   * @param aFlushContent whether to flush the content sink, which creates
   *        frames for content that do not already have it.  EXPENSIVE.
   * @return the primary frame as nsIFormControlFrame
   */
  static nsIFormControlFrame* GetFormControlFrameFor(nsIContent* aContent,
                                                     nsIDocument* aDocument,
                                                     PRBool aFlushContent);
  /**
   * Get the presentation state for a piece of content, or create it if it does
   * not exist.  Generally used by SaveState().
   *
   * @param aContent the content to get presentation state for.
   * @param aPresState the presentation state (out param)
   */
  static nsresult GetPrimaryPresState(nsIHTMLContent* aContent,
                                      nsIPresState** aPresState);
  /**
   * Get the layout history object *and* generate the key for a particular
   * piece of content.
   *
   * @param aContent the content to generate the key for
   * @param aState the history state object (out param)
   * @param aKey the key (out param)
   */
  static nsresult GetLayoutHistoryAndKey(nsIHTMLContent* aContent,
                                         nsILayoutHistoryState** aState,
                                         nsACString& aKey);
  /**
   * Restore the state for a form control.  Ends up calling
   * nsIFormControl::RestoreState().
   *
   * @param aContent an nsIHTMLContent* pointing to the form control
   * @param aControl an nsIFormControl* pointing to the form control
   * @return whether or not the RestoreState() was called and exited
   *         successfully.
   */
  static PRBool RestoreFormControlState(nsIHTMLContent* aContent,
                                        nsIFormControl* aControl);

  /**
   * Get the presentation context for a content node.
   * @param aContent the content node
   * @param aPresContext the presentation context [OUT]
   */
  static nsresult GetPresContext(nsIHTMLContent* aContent,
                                 nsIPresContext** aPresContext);

  /**
   * Resolve the base URL from a _baseHref attribute (could be empty) and
   * from the document.
   *
   * @param aBaseHref the _baseHref attribute
   * @param aDocument the document
   * @param aResult the base URL
   */
  static nsresult GetBaseURL(const nsHTMLValue& aBaseHref,
                             nsIDocument* aDocument,
                             nsIURI** aResult);

  // Form Helper Routines
  /**
   * Find an ancestor of this content node which is a form (could be null)
   * @param aForm the form ancestore [OUT]
   */
  nsresult FindForm(nsIDOMHTMLFormElement **aForm);

  /**
   * Find the form for this element and set aFormControl's form to it
   * (aFormControl is passed in to avoid QI)
   *
   * @param aFormControl the form control to set the form for
   */
  nsresult FindAndSetForm(nsIFormControl *aFormControl);

  /**
   * See if the document being tested has nav-quirks mode enabled.
   * @param doc the document
   */
  static PRBool InNavQuirksMode(nsIDocument* aDoc);

  /**
   * Helper for the form subclasses of nsGenericHTMLElement to take care
   * of fixing up form.elements when name, id and type change
   *
   * @param aForm the form the control is in
   * @param aNameSpaceID the namespace of the attr
   * @param aName the name of the attr
   * @param aValue the value of the attr
   * @param aNotify whether to notify the document of the attribute change
   */
  nsresult SetFormControlAttribute(nsIForm* aForm, PRInt32 aNameSpaceID,
                                   nsIAtom* aName,
                                   const nsAString& aValue,
                                   PRBool aNotify);

  /** The list of attributes */
  nsHTMLAttributes* mAttributes;

  // Helper functions for <a> and <area>
  static nsresult SetProtocolInHrefString(const nsAString &aHref,
                                          const nsAString &aProtocol,
                                          nsAString &aResult);

  static nsresult SetHostInHrefString(const nsAString &aHref,
                                      const nsAString &aHost,
                                      nsAString &aResult);

  static nsresult SetHostnameInHrefString(const nsAString &aHref,
                                          const nsAString &aHostname,
                                          nsAString &aResult);

  static nsresult SetPathnameInHrefString(const nsAString &aHref,
                                          const nsAString &aHostname,
                                          nsAString &aResult);

  static nsresult SetSearchInHrefString(const nsAString &aHref,
                                        const nsAString &aSearch,
                                        nsAString &aResult);
  
  static nsresult SetHashInHrefString(const nsAString &aHref,
                                      const nsAString &aHash,
                                      nsAString &aResult);

  static nsresult SetPortInHrefString(const nsAString &aHref,
                                      const nsAString &aPort,
                                      nsAString &aResult);

  static nsresult GetProtocolFromHrefString(const nsAString &aHref,
                                            nsAString& aProtocol,
                                            nsIDocument *aDocument);

  static nsresult GetHostFromHrefString(const nsAString &aHref,
                                        nsAString& aHost);

  static nsresult GetHostnameFromHrefString(const nsAString &aHref,
                                            nsAString& aHostname);

  static nsresult GetPathnameFromHrefString(const nsAString &aHref,
                                            nsAString& aPathname);

  static nsresult GetSearchFromHrefString(const nsAString &aHref,
                                          nsAString& aSearch);

  static nsresult GetPortFromHrefString(const nsAString &aHref,
                                        nsAString& aPort);

  static nsresult GetHashFromHrefString(const nsAString &aHref,
                                        nsAString& aHash);
protected:
  /**
   * Focus or blur the element.  This is what you should call if you want to
   * *cause* a focus or blur on your element.  SetFocus / SetBlur are the
   * methods where you want to catch what occurs on your element.
   * @param aDoFocus true to focus, false to blur
   */
  nsresult SetElementFocus(PRBool aDoFocus);
  /**
   * Register or unregister an access key to this element based on the
   * accesskey attribute.
   * @param aDoReg true to register, false to unregister
   */
  nsresult RegUnRegAccessKey(PRBool aDoReg);

  /**
   * Determine whether an attribute is an event (onclick, etc.)
   * @param aName the attribute
   * @return whether the name is an event handler name
   */
  PRBool IsEventName(nsIAtom* aName);
};


//----------------------------------------------------------------------

/**
 * A helper class to subclass for leaf elements.
 */
class nsGenericHTMLLeafElement : public nsGenericHTMLElement {
public:
  nsGenericHTMLLeafElement();
  virtual ~nsGenericHTMLLeafElement();

  NS_METHOD CopyInnerTo(nsIContent* aSrcContent,
                        nsGenericHTMLLeafElement* aDest,
                        PRBool aDeep);

  // Remainder of nsIDOMHTMLElement (and nsIDOMNode)
  NS_METHOD GetChildNodes(nsIDOMNodeList** aChildNodes);
  NS_METHOD HasChildNodes(PRBool* aHasChildNodes) {
    *aHasChildNodes = PR_FALSE;
    return NS_OK;
  }
  NS_METHOD GetFirstChild(nsIDOMNode** aFirstChild) {
    *aFirstChild = nsnull;
    return NS_OK;
  }
  NS_METHOD GetLastChild(nsIDOMNode** aLastChild) {
    *aLastChild = nsnull;
    return NS_OK;
  }
  NS_METHOD InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild,
                        nsIDOMNode** aReturn) {
    return NS_ERROR_FAILURE;
  }
  NS_METHOD ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild,
                        nsIDOMNode** aReturn) {
    return NS_ERROR_FAILURE;
  }
  NS_METHOD RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn) {
    return NS_ERROR_FAILURE;
  }
  NS_METHOD AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn) {
    return NS_ERROR_FAILURE;
  }

  // Remainder of nsIHTMLContent (and nsIContent)
  NS_IMETHOD Compact() {
    return NS_OK;
  }
  NS_IMETHOD CanContainChildren(PRBool& aResult) const {
    aResult = PR_FALSE;
    return NS_OK;
  }
  NS_IMETHOD ChildCount(PRInt32& aResult) const {
    aResult = 0;
    return NS_OK;
  }
  NS_IMETHOD ChildAt(PRInt32 aIndex, nsIContent*& aResult) const {
    aResult = nsnull;
    return NS_OK;
  }
  NS_IMETHOD IndexOf(nsIContent* aPossibleChild, PRInt32& aResult) const {
    aResult = -1;
    return NS_OK;
  }
  NS_IMETHOD InsertChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify,
                           PRBool aDeepSetDocument) {
    return NS_OK;
  }
  NS_IMETHOD ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify,
                            PRBool aDeepSetDocument) {
    return NS_OK;
  }
  NS_IMETHOD AppendChildTo(nsIContent* aKid, PRBool aNotify,
                           PRBool aDeepSetDocument) {
    return NS_OK;
  }
  NS_IMETHOD RemoveChildAt(PRInt32 aIndex, PRBool aNotify) {
    return NS_OK;
  }
};

//----------------------------------------------------------------------

/**
 * A helper class to subclass for elements that can contain children
 */
class nsGenericHTMLContainerElement : public nsGenericHTMLElement
{
public:
  nsGenericHTMLContainerElement();
  virtual ~nsGenericHTMLContainerElement();

  NS_METHOD CopyInnerTo(nsIContent* aSrcContent,
                       nsGenericHTMLContainerElement* aDest,
                       PRBool aDeep);

  // Remainder of nsIDOMHTMLElement (and nsIDOMNode)
  NS_METHOD GetChildNodes(nsIDOMNodeList** aChildNodes);
  NS_METHOD HasChildNodes(PRBool* aHasChildNodes);
  NS_METHOD GetFirstChild(nsIDOMNode** aFirstChild);
  NS_METHOD GetLastChild(nsIDOMNode** aLastChild);
  
  NS_METHOD InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild,
                         nsIDOMNode** aReturn)
  {
    return nsGenericElement::doInsertBefore(aNewChild, aRefChild, aReturn);
  }
  NS_METHOD ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild,
                         nsIDOMNode** aReturn)
  {
    return nsGenericElement::doReplaceChild(aNewChild, aOldChild, aReturn);
  }
  NS_METHOD RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
  {
    return nsGenericElement::doRemoveChild(aOldChild, aReturn);
  }
  NS_METHOD AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
  {
    return nsGenericElement::doInsertBefore(aNewChild, nsnull, aReturn);
  }

  // Remainder of nsIHTMLContent (and nsIContent)
  NS_IMETHOD Compact();
  NS_IMETHOD CanContainChildren(PRBool& aResult) const;
  NS_IMETHOD ChildCount(PRInt32& aResult) const;
  NS_IMETHOD ChildAt(PRInt32 aIndex, nsIContent*& aResult) const;
  NS_IMETHOD IndexOf(nsIContent* aPossibleChild, PRInt32& aResult) const;
  NS_IMETHOD InsertChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify,
                           PRBool aDeepSetDocument);
  NS_IMETHOD ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify,
                            PRBool aDeepSetDocument);
  NS_IMETHOD AppendChildTo(nsIContent* aKid, PRBool aNotify,
                           PRBool aDeepSetDocument);
  NS_IMETHOD RemoveChildAt(PRInt32 aIndex, PRBool aNotify);

  /** The list of children */
  nsSmallVoidArray mChildren;
protected:
  /**
   * ReplaceContentsWithText will take the aText string and make sure
   * that the only child of |this| is a textnode which corresponds to
   * that string.
   *
   * @param aText the text to put in
   * @param aNotify whether to notify the document of child adds/removes
   */
  nsresult ReplaceContentsWithText(const nsAString& aText, PRBool aNotify);
  /**
   * GetContentsAsText will take all the textnodes that are children
   * of |this| and concatenate the text in them into aText.  It
   * completely ignores any non-text-node children of |this|; in
   * particular it does not descend into any children of |this| that
   * happen to be container elements.
   *
   * @param aText the resulting text [OUT]
   */
  nsresult GetContentsAsText(nsAString& aText);
};

//----------------------------------------------------------------------

/**
 * A helper class for form elements that can contain children
 */
class nsGenericHTMLContainerFormElement : public nsGenericHTMLContainerElement,
                                          public nsIFormControl
{
public:
  nsGenericHTMLContainerFormElement();
  virtual ~nsGenericHTMLContainerFormElement();

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  NS_IMETHOD_(PRBool) IsContentOfType(PRUint32 aFlags);

  // nsIFormControl
  NS_IMETHOD GetForm(nsIDOMHTMLFormElement** aForm);
  NS_IMETHOD SetForm(nsIDOMHTMLFormElement* aForm,
                     PRBool aRemoveFromForm = PR_TRUE);
  NS_IMETHOD SaveState() { return NS_OK; }
  NS_IMETHOD RestoreState(nsIPresState* aState) { return NS_OK; }

  // nsIContent
  NS_IMETHOD SetParent(nsIContent *aParent);
  NS_IMETHOD SetDocument(nsIDocument* aDocument, PRBool aDeep,
                         PRBool aCompileEventHandlers);

  NS_IMETHOD SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                     const nsAString& aValue, PRBool aNotify);

  NS_IMETHOD SetAttr(nsINodeInfo* aNodeInfo,
                     const nsAString& aValue,
                     PRBool aNotify);


  NS_METHOD SetAttribute(const nsAString& aName,
                         const nsAString& aValue)
  {
    return nsGenericHTMLElement::SetAttribute(aName, aValue);
  }

protected:
  /** The form that contains this control */
  nsIForm* mForm;
};

//----------------------------------------------------------------------

/**
 * A helper class for form elements that can contain children
 */
class nsGenericHTMLLeafFormElement : public nsGenericHTMLLeafElement,
                                     public nsIFormControl
{
public:
  nsGenericHTMLLeafFormElement();
  ~nsGenericHTMLLeafFormElement();

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  NS_IMETHOD_(PRBool) IsContentOfType(PRUint32 aFlags);

  // nsIFormControl
  NS_IMETHOD GetForm(nsIDOMHTMLFormElement** aForm);
  NS_IMETHOD SetForm(nsIDOMHTMLFormElement* aForm,
                     PRBool aRemoveFromForm = PR_TRUE);
  NS_IMETHOD SaveState() { return NS_OK; }
  NS_IMETHOD RestoreState(nsIPresState* aState) { return NS_OK; }

  // nsIContent
  NS_IMETHOD SetParent(nsIContent *aParent);
  NS_IMETHOD SetDocument(nsIDocument* aDocument, PRBool aDeep,
                         PRBool aCompileEventHandlers);

  NS_IMETHOD SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                     const nsAString& aValue, PRBool aNotify);

  NS_IMETHOD SetAttr(nsINodeInfo* aNodeInfo,
                     const nsAString& aValue,
                     PRBool aNotify);
  NS_IMETHOD DoneCreatingElement();

  NS_METHOD SetAttribute(const nsAString& aName,
                         const nsAString& aValue)
  {
    return nsGenericHTMLElement::SetAttribute(aName, aValue);
  }

protected:
  /** The form that contains this control */
  nsIForm* mForm;
};


//----------------------------------------------------------------------

/**
 * A macro to implement the getter and setter for a given string
 * valued content property. The method uses the generic GetAttr and
 * SetAttr methods.
 */
#define NS_IMPL_STRING_ATTR(_class, _method, _atom)                  \
  NS_IMPL_STRING_ATTR_DEFAULT_VALUE(_class, _method, _atom, "")

/**
 * A macro to implement the getter and setter for a given string
 * valued content property with a default value.
 * The method uses the generic GetAttr and SetAttr methods.
 */
#define NS_IMPL_STRING_ATTR_DEFAULT_VALUE(_class, _method, _atom, _default) \
  NS_IMETHODIMP                                                      \
  _class::Get##_method(nsAString& aValue)                            \
  {                                                                  \
    nsresult rv = GetAttr(kNameSpaceID_None,                         \
                          nsHTMLAtoms::_atom, aValue);               \
    if (rv == NS_CONTENT_ATTR_NOT_THERE) {                           \
      aValue.Assign(NS_LITERAL_STRING(_default));                    \
    }                                                                \
    return NS_OK;                                                    \
  }                                                                  \
  NS_IMETHODIMP                                                      \
  _class::Set##_method(const nsAString& aValue)                      \
  {                                                                  \
    return SetAttr(kNameSpaceID_None, nsHTMLAtoms::_atom, aValue,    \
                   PR_TRUE);                                         \
  }

/**
 * A macro to implement the getter and setter for a given boolean
 * valued content property. The method uses the generic GetAttr and
 * SetAttr methods.
 */
#define NS_IMPL_BOOL_ATTR(_class, _method, _atom)                     \
  NS_IMETHODIMP                                                       \
  _class::Get##_method(PRBool* aValue)                                \
  {                                                                   \
    nsHTMLValue val;                                                  \
    nsresult rv = GetHTMLAttribute(nsHTMLAtoms::_atom, val);          \
    *aValue = NS_CONTENT_ATTR_NOT_THERE != rv;                        \
    return NS_OK;                                                     \
  }                                                                   \
  NS_IMETHODIMP                                                       \
  _class::Set##_method(PRBool aValue)                                 \
  {                                                                   \
    nsHTMLValue empty(eHTMLUnit_Empty);                               \
    if (aValue) {                                                     \
      return SetHTMLAttribute(nsHTMLAtoms::_atom, empty, PR_TRUE);    \
    }                                                                 \
    else {                                                            \
      UnsetAttr(kNameSpaceID_None, nsHTMLAtoms::_atom, PR_TRUE);      \
      return NS_OK;                                                   \
    }                                                                 \
  }

/**
 * A macro to implement the getter and setter for a given integer
 * valued content property. The method uses the generic GetAttr and
 * SetAttr methods.
 */
#define NS_IMPL_INT_ATTR(_class, _method, _atom)                    \
  NS_IMPL_INT_ATTR_DEFAULT_VALUE(_class, _method, _atom, -1)

/**
 * A macro to implement the getter and setter for a given integer
 * valued content property with a default value.
 * The method uses the generic GetAttr and SetAttr methods.
 */
#define NS_IMPL_INT_ATTR_DEFAULT_VALUE(_class, _method, _atom, _default) \
  NS_IMETHODIMP                                                     \
  _class::Get##_method(PRInt32* aValue)                             \
  {                                                                 \
    nsHTMLValue value;                                              \
    *aValue = _default;                                             \
    if (NS_CONTENT_ATTR_HAS_VALUE ==                                \
        GetHTMLAttribute(nsHTMLAtoms::_atom, value)) {              \
      if (value.GetUnit() == eHTMLUnit_Integer) {                   \
        *aValue = value.GetIntValue();                              \
      }                                                             \
    }                                                               \
    return NS_OK;                                                   \
  }                                                                 \
  NS_IMETHODIMP                                                     \
  _class::Set##_method(PRInt32 aValue)                              \
  {                                                                 \
    nsHTMLValue value(aValue, eHTMLUnit_Integer);                   \
    return SetHTMLAttribute(nsHTMLAtoms::_atom, value, PR_TRUE);    \
  }

/**
 * A macro to implement the getter and the setter for a given pixel
 * valued content property. The method uses the generic GetAttr and
 * SetAttr methods.
 */
#define NS_IMPL_PIXEL_ATTR(_class, _method, _atom)                  \
  NS_IMPL_PIXEL_ATTR_DEFAULT_VALUE(_class, _method, _atom, -1)

/**
 * A macro to implement the getter and the setter for a given pixel
 * valued content property with a default value.
 * The method uses the generic GetAttr and SetAttr methods.
 */
#define NS_IMPL_PIXEL_ATTR_DEFAULT_VALUE(_class, _method, _atom, _default) \
  NS_IMETHODIMP                                                     \
  _class::Get##_method(PRInt32* aValue)                             \
  {                                                                 \
    nsHTMLValue value;                                              \
    *aValue = _default;                                                   \
    if (NS_CONTENT_ATTR_HAS_VALUE ==                                \
        GetHTMLAttribute(nsHTMLAtoms::_atom, value)) {              \
      if (value.GetUnit() == eHTMLUnit_Pixel) {                     \
        *aValue = value.GetPixelValue();                            \
      }                                                             \
    }                                                               \
    return NS_OK;                                                   \
  }                                                                 \
  NS_IMETHODIMP                                                     \
  _class::Set##_method(PRInt32 aValue)                              \
  {                                                                 \
    nsHTMLValue value(aValue, eHTMLUnit_Pixel);                     \
    return SetHTMLAttribute(nsHTMLAtoms::_atom, value, PR_TRUE);    \
  }

/**
 * QueryInterface() implementation helper macros
 */

#define NS_HTML_CONTENT_INTERFACE_MAP_AMBIGOUS_BEGIN(_class, _base, _base_if) \
  NS_IMETHODIMP _class::QueryInterface(REFNSIID aIID, void** aInstancePtr)    \
  {                                                                           \
    NS_ENSURE_ARG_POINTER(aInstancePtr);                                      \
                                                                              \
    *aInstancePtr = nsnull;                                                   \
                                                                              \
    nsresult rv;                                                              \
                                                                              \
    rv = _base::QueryInterface(aIID, aInstancePtr);                           \
                                                                              \
    if (NS_SUCCEEDED(rv))                                                     \
      return rv;                                                              \
                                                                              \
    rv = DOMQueryInterface(NS_STATIC_CAST(_base_if *, this), aIID,            \
                           aInstancePtr);                                     \
                                                                              \
    if (NS_SUCCEEDED(rv))                                                     \
      return rv;                                                              \
                                                                              \
    nsISupports *foundInterface = nsnull;


#define NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(_class, _base)                    \
  NS_HTML_CONTENT_INTERFACE_MAP_AMBIGOUS_BEGIN(_class, _base,                 \
                                               nsIDOMHTMLElement)


#define NS_HTML_CONTENT_INTERFACE_MAP_END                                     \
    {                                                                         \
      return PostQueryInterface(aIID, aInstancePtr);                          \
    }                                                                         \
                                                                              \
    NS_ADDREF(foundInterface);                                                \
                                                                              \
    *aInstancePtr = foundInterface;                                           \
                                                                              \
    return NS_OK;                                                             \
  }


#define NS_INTERFACE_MAP_ENTRY_IF_TAG(_interface, _tag)                       \
  if (mNodeInfo->Equals(nsHTMLAtoms::_tag) &&                                 \
      aIID.Equals(NS_GET_IID(_interface)))                                    \
    foundInterface = NS_STATIC_CAST(_interface *, this);                      \
  else


#define NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO_IF_TAG(_class, _tag)         \
  if (mNodeInfo->Equals(nsHTMLAtoms::_tag) &&                                 \
      aIID.Equals(NS_GET_IID(nsIClassInfo))) {                                \
    foundInterface =                                                          \
      nsContentUtils::GetClassInfoInstance(eDOMClassInfo_##_class##_id);      \
    NS_ENSURE_TRUE(foundInterface, NS_ERROR_OUT_OF_MEMORY);                   \
                                                                              \
    *aInstancePtr = foundInterface;                                           \
                                                                              \
    return NS_OK;                                                             \
  } else



// Element class factory methods

nsresult
NS_NewHTMLSharedLeafElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLAnchorElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLAppletElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLAreaElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLBRElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

inline nsresult
NS_NewHTMLBaseElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo)
{
  return NS_NewHTMLSharedLeafElement(aResult, aNodeInfo);
}

nsresult
NS_NewHTMLBaseFontElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLBodyElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLButtonElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLDListElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLDelElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLDirectoryElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLDivElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

inline nsresult
NS_NewHTMLEmbedElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo)
{
  return NS_NewHTMLSharedLeafElement(aResult, aNodeInfo);
}

nsresult
NS_NewHTMLFieldSetElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLFontElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLFormElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLFrameElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLFrameSetElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLHRElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLHeadElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLHeadingElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLHtmlElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLIFrameElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLImageElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLInputElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo,
                       PRBool aFromParser);

nsresult
NS_NewHTMLInsElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

inline nsresult
NS_NewHTMLIsIndexElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo)
{
  return NS_NewHTMLSharedLeafElement(aResult, aNodeInfo);
}

nsresult
NS_NewHTMLLIElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLLabelElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLLegendElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLLinkElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLMapElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLMenuElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLMetaElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLModElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLOListElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLObjectElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLOptGroupElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLOptionElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLParagraphElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

inline nsresult
NS_NewHTMLParamElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo)
{
  return NS_NewHTMLSharedLeafElement(aResult, aNodeInfo);
}

nsresult
NS_NewHTMLPreElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLQuoteElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLScriptElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLSelectElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo,
                        PRBool aFromParser);

inline nsresult
NS_NewHTMLSpacerElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo)
{
  return NS_NewHTMLSharedLeafElement(aResult, aNodeInfo);
}

nsresult
NS_NewHTMLSpanElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLStyleElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLTableCaptionElement(nsIHTMLContent** aResult,nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLTableCellElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLTableColElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

inline nsresult
NS_NewHTMLTableColGroupElement(nsIHTMLContent** aResult,
                               nsINodeInfo *aNodeInfo)
{
  return NS_NewHTMLTableColElement(aResult, aNodeInfo);
}

nsresult
NS_NewHTMLTableElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLTableRowElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLTableSectionElement(nsIHTMLContent** aResult,nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLTbodyElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLTextAreaElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLTfootElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLTheadElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLTitleElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLUListElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

nsresult
NS_NewHTMLUnknownElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo);

inline nsresult
NS_NewHTMLWBRElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo)
{
  return NS_NewHTMLSharedLeafElement(aResult, aNodeInfo);
}


#endif /* nsGenericHTMLElement_h___ */
