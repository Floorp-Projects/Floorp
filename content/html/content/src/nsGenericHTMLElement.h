/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set tw=80 expandtab softtabstop=2 ts=2 sw=2: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsGenericHTMLElement_h___
#define nsGenericHTMLElement_h___

#include "nsMappedAttributeElement.h"
#include "nsIDOMHTMLElement.h"
#include "nsINameSpaceManager.h"  // for kNameSpaceID_None
#include "nsIFormControl.h"
#include "nsIDOMNSHTMLFrameElement.h"
#include "nsFrameLoader.h"
#include "nsGkAtoms.h"

class nsIDOMAttr;
class nsIDOMEventListener;
class nsIDOMNodeList;
class nsIFrame;
class nsIStyleRule;
class nsChildContentList;
class nsDOMCSSDeclaration;
class nsIDOMCSSStyleDeclaration;
class nsIURI;
class nsIFormControlFrame;
class nsIForm;
class nsPresState;
class nsILayoutHistoryState;
class nsIEditor;
struct nsRect;
struct nsSize;

typedef nsMappedAttributeElement nsGenericHTMLElementBase;

/**
 * A common superclass for HTML elements
 */
class nsGenericHTMLElement : public nsGenericHTMLElementBase
{
public:
  nsGenericHTMLElement(nsINodeInfo *aNodeInfo)
    : nsGenericHTMLElementBase(aNodeInfo)
  {
  }

  /** Typesafe, non-refcounting cast from nsIContent.  Cheaper than QI. **/
  static nsGenericHTMLElement* FromContent(nsIContent *aContent)
  {
    if (aContent->IsNodeOfType(eHTML))
      return static_cast<nsGenericHTMLElement*>(aContent);
    return nsnull;
  }

  /** Call on shutdown to release globals */
  static void Shutdown();

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
  nsresult CopyInnerTo(nsGenericElement* aDest) const;

  // Implementation for nsIDOMNode
  NS_METHOD GetNodeName(nsAString& aNodeName);
  NS_METHOD GetLocalName(nsAString& aLocalName);

  // Implementation for nsIDOMElement
  NS_METHOD SetAttribute(const nsAString& aName,
                         const nsAString& aValue);
  NS_METHOD GetTagName(nsAString& aTagName);
  NS_METHOD GetElementsByTagName(const nsAString& aTagname,
                                 nsIDOMNodeList** aReturn);
  NS_METHOD GetElementsByTagNameNS(const nsAString& aNamespaceURI,
                                   const nsAString& aLocalName,
                                   nsIDOMNodeList** aReturn);

  // nsIDOMHTMLElement methods. Note that these are non-virtual
  // methods, implementations are expected to forward calls to these
  // methods.
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

  // nsIDOMNSHTMLElement methods. Note that these are non-virtual
  // methods, implementations are expected to forward calls to these
  // methods.
  // Forward to GetStyle which is protected in the super-class
  inline nsresult GetStyle(nsIDOMCSSStyleDeclaration** aStyle)
  {
    return nsGenericHTMLElementBase::GetStyle(aStyle);
  }
  nsresult GetOffsetTop(PRInt32* aOffsetTop);
  nsresult GetOffsetLeft(PRInt32* aOffsetLeft);
  nsresult GetOffsetWidth(PRInt32* aOffsetWidth);
  nsresult GetOffsetHeight(PRInt32* aOffsetHeight);
  nsresult GetOffsetParent(nsIDOMElement** aOffsetParent);
  virtual nsresult GetInnerHTML(nsAString& aInnerHTML);
  virtual nsresult SetInnerHTML(const nsAString& aInnerHTML);
  nsresult ScrollIntoView(PRBool aTop);
  // Declare Focus(), Blur(), GetTabIndex(), SetTabIndex(), GetSpellcheck(),
  // SetSpellcheck(), and GetDraggable() such that classes that inherit interfaces
  // with those methods properly override them
  NS_IMETHOD Focus();
  NS_IMETHOD Blur();
  NS_IMETHOD GetTabIndex(PRInt32 *aTabIndex);
  NS_IMETHOD SetTabIndex(PRInt32 aTabIndex);
  NS_IMETHOD GetSpellcheck(PRBool* aSpellcheck);
  NS_IMETHOD SetSpellcheck(PRBool aSpellcheck);
  NS_IMETHOD GetDraggable(PRBool* aDraggable);
  NS_IMETHOD SetDraggable(PRBool aDraggable);
  nsresult GetContentEditable(nsAString &aContentEditable);
  nsresult SetContentEditable(const nsAString &aContentEditable);

  // Implementation for nsIContent
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              PRBool aCompileEventHandlers);
  virtual void UnbindFromTree(PRBool aDeep = PR_TRUE,
                              PRBool aNullParent = PR_TRUE);
  nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, PRBool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nsnull, aValue, aNotify);
  }
  virtual nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           PRBool aNotify);
  virtual nsresult UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                             PRBool aNotify);
  virtual PRBool IsNodeOfType(PRUint32 aFlags) const;
  virtual void RemoveFocus(nsPresContext *aPresContext);
  virtual PRBool IsFocusable(PRInt32 *aTabIndex = nsnull)
  {
    PRBool isFocusable = PR_FALSE;
    IsHTMLFocusable(&isFocusable, aTabIndex);
    return isFocusable;
  }
  /**
   * Returns PR_TRUE if a subclass is not allowed to override the value returned
   * in aIsFocusable.
   */
  virtual PRBool IsHTMLFocusable(PRBool *aIsFocusable, PRInt32 *aTabIndex);
  virtual void PerformAccesskey(PRBool aKeyCausesActivation,
                                PRBool aIsTrustedEvent);

  /**
   * Check if an event for an anchor can be handled
   * @return PR_TRUE if the event can be handled, PR_FALSE otherwise
   */
  PRBool CheckHandleEventForAnchorsPreconditions(nsEventChainVisitor& aVisitor);
  nsresult PreHandleEventForAnchors(nsEventChainPreVisitor& aVisitor);
  nsresult PostHandleEventForAnchors(nsEventChainPostVisitor& aVisitor);
  PRBool IsHTMLLink(nsIURI** aURI) const;

  // Used by A, AREA, LINK, and STYLE.
  // Callers must hold a reference to nsHTMLUtils's global reference count.
  nsresult GetHrefURIForAnchors(nsIURI** aURI) const;

  // HTML element methods
  void Compact() { mAttrsAndChildren.Compact(); }
  const nsAttrValue* GetParsedAttr(nsIAtom* aAttr) const
  {
    return mAttrsAndChildren.GetAttr(aAttr);
  }

  virtual void UpdateEditableState();

  already_AddRefed<nsIURI> GetBaseURI() const;

  virtual PRBool ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);

  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const;

  /**
   * Get the base target for any links within this piece
   * of content. Generally, this is the document's base target,
   * but certain content carries a local base for backward
   * compatibility.
   *
   * @param aBaseTarget the base target [OUT]
   */
  void GetBaseTarget(nsAString& aBaseTarget) const;

  /**
   * Get the primary form control frame for this content (see
   * GetFormControlFrameFor)
   *
   * @param aFlushContent whether to flush the content sink
   * @return the primary form control frame (or null)
   */
  nsIFormControlFrame* GetFormControlFrame(PRBool aFlushContent)
  {
    nsIDocument* doc = GetCurrentDoc();
    if (!doc) {
      return nsnull;
    }

    return GetFormControlFrameFor(this, doc, aFlushContent);
  }

  //----------------------------------------

  /**
   * Parse an alignment attribute (top/middle/bottom/baseline)
   *
   * @param aString the string to parse
   * @param aResult the resulting HTMLValue
   * @return whether the value was parsed
   */
  static PRBool ParseAlignValue(const nsAString& aString,
                                nsAttrValue& aResult);

  /**
   * Parse a div align string to value (left/right/center/middle/justify)
   *
   * @param aString the string to parse
   * @param aResult the resulting HTMLValue
   * @return whether the value was parsed
   */
  PRBool ParseDivAlignValue(const nsAString& aString,
                            nsAttrValue& aResult) const;

  /**
   * Convert a table halign string to value (left/right/center/char/justify)
   *
   * @param aString the string to parse
   * @param aResult the resulting HTMLValue
   * @return whether the value was parsed
   */
  PRBool ParseTableHAlignValue(const nsAString& aString,
                               nsAttrValue& aResult) const;

  /**
   * Convert a table cell halign string to value
   *
   * @param aString the string to parse
   * @param aResult the resulting HTMLValue
   * @return whether the value was parsed
   */
  PRBool ParseTableCellHAlignValue(const nsAString& aString,
                                   nsAttrValue& aResult) const;

  /**
   * Convert a table valign string to value (left/right/center/char/justify/
   * abscenter/absmiddle/middle)
   *
   * @param aString the string to parse
   * @param aResult the resulting HTMLValue
   * @return whether the value was parsed
   */
  static PRBool ParseTableVAlignValue(const nsAString& aString,
                                      nsAttrValue& aResult);

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
                                    nsAttrValue& aResult);
  /**
   * Convert a frameborder string to value (yes/no/1/0)
   *
   * @param aString the string to parse
   * @param aResult the resulting HTMLValue
   * @return whether the value was parsed
   */
  static PRBool ParseFrameborderValue(const nsAString& aString,
                                      nsAttrValue& aResult);

  /**
   * Convert a scrolling string to value (yes/no/on/off/scroll/noscroll/auto)
   *
   * @param aString the string to parse
   * @param aResult the resulting HTMLValue
   * @return whether the value was parsed
   */
  static PRBool ParseScrollingValue(const nsAString& aString,
                                    nsAttrValue& aResult);

  /*
   * Attribute Mapping Helpers
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
  static void MapCommonAttributesInto(const nsMappedAttributes* aAttributes, 
                                      nsRuleData* aRuleData);
  static const MappedAttributeEntry sCommonAttributeMap[];
  static const MappedAttributeEntry sImageMarginSizeAttributeMap[];
  static const MappedAttributeEntry sImageBorderAttributeMap[];
  static const MappedAttributeEntry sImageAlignAttributeMap[];
  static const MappedAttributeEntry sDivAlignAttributeMap[];
  static const MappedAttributeEntry sBackgroundAttributeMap[];
  static const MappedAttributeEntry sBackgroundColorAttributeMap[];
  static const MappedAttributeEntry sScrollingAttributeMap[];
  
  /**
   * Helper to map the align attribute into a style struct.
   *
   * @param aAttributes the list of attributes to map
   * @param aData the returned rule data [INOUT]
   * @see GetAttributeMappingFunction
   */
  static void MapImageAlignAttributeInto(const nsMappedAttributes* aAttributes,
                                         nsRuleData* aData);

  /**
   * Helper to map the align attribute into a style struct for things
   * like <div>, <h1>, etc.
   *
   * @param aAttributes the list of attributes to map
   * @param aData the returned rule data [INOUT]
   * @see GetAttributeMappingFunction
   */
  static void MapDivAlignAttributeInto(const nsMappedAttributes* aAttributes,
                                       nsRuleData* aData);

  /**
   * Helper to map the image border attribute into a style struct.
   *
   * @param aAttributes the list of attributes to map
   * @param aData the returned rule data [INOUT]
   * @see GetAttributeMappingFunction
   */
  static void MapImageBorderAttributeInto(const nsMappedAttributes* aAttributes,
                                          nsRuleData* aData);
  /**
   * Helper to map the image margin attribute into a style struct.
   *
   * @param aAttributes the list of attributes to map
   * @param aData the returned rule data [INOUT]
   * @see GetAttributeMappingFunction
   */
  static void MapImageMarginAttributeInto(const nsMappedAttributes* aAttributes,
                                          nsRuleData* aData);
  /**
   * Helper to map the image position attribute into a style struct.
   *
   * @param aAttributes the list of attributes to map
   * @param aData the returned rule data [INOUT]
   * @see GetAttributeMappingFunction
   */
  static void MapImageSizeAttributesInto(const nsMappedAttributes* aAttributes,
                                         nsRuleData* aData);
  /**
   * Helper to map the background attribute
   * into a style struct.
   *
   * @param aAttributes the list of attributes to map
   * @param aData the returned rule data [INOUT]
   * @see GetAttributeMappingFunction
   */
  static void MapBackgroundInto(const nsMappedAttributes* aAttributes,
                                nsRuleData* aData);
  /**
   * Helper to map the bgcolor attribute
   * into a style struct.
   *
   * @param aAttributes the list of attributes to map
   * @param aData the returned rule data [INOUT]
   * @see GetAttributeMappingFunction
   */
  static void MapBGColorInto(const nsMappedAttributes* aAttributes,
                             nsRuleData* aData);
  /**
   * Helper to map the background attributes (currently background and bgcolor)
   * into a style struct.
   *
   * @param aAttributes the list of attributes to map
   * @param aData the returned rule data [INOUT]
   * @see GetAttributeMappingFunction
   */
  static void MapBackgroundAttributesInto(const nsMappedAttributes* aAttributes,
                                          nsRuleData* aData);
  /**
   * Helper to map the scrolling attribute on FRAME and IFRAME
   * into a style struct.
   *
   * @param aAttributes the list of attributes to map
   * @param aData the returned rule data [INOUT]
   * @see GetAttributeMappingFunction
   */
  static void MapScrollingAttributeInto(const nsMappedAttributes* aAttributes,
                                        nsRuleData* aData);
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
  static nsresult GetPrimaryPresState(nsGenericHTMLElement* aContent,
                                      nsPresState** aPresState);
  /**
   * Get the layout history object *and* generate the key for a particular
   * piece of content.
   *
   * @param aContent the content to generate the key for
   * @param aRead if true, won't return a layout history state (and won't
   *              generate a key) if the layout history state is empty.
   * @param aState the history state object (out param)
   * @param aKey the key (out param)
   */
  static nsresult GetLayoutHistoryAndKey(nsGenericHTMLElement* aContent,
                                         PRBool aRead,
                                         nsILayoutHistoryState** aState,
                                         nsACString& aKey);
  /**
   * Restore the state for a form control.  Ends up calling
   * nsIFormControl::RestoreState().
   *
   * @param aContent an nsGenericHTMLElement* pointing to the form control
   * @param aControl an nsIFormControl* pointing to the form control
   * @return PR_FALSE if RestoreState() was not called, the return
   *         value of RestoreState() otherwise.
   */
  static PRBool RestoreFormControlState(nsGenericHTMLElement* aContent,
                                        nsIFormControl* aControl);

  /**
   * Get the presentation context for this content node.
   * @return the presentation context
   */
  NS_HIDDEN_(nsPresContext*) GetPresContext();

  // Form Helper Routines
  /**
   * Find an ancestor of this content node which is a form (could be null)
   * @param aCurrentForm the current form for this node.  If this is
   *        non-null, and no ancestor form is found, and the current form is in
   *        a connected subtree with the node, the current form will be
   *        returned.  This is needed to handle cases when HTML elements have a
   *        current form that they're not descendants of.
   */
  already_AddRefed<nsIDOMHTMLFormElement> FindForm(nsIForm* aCurrentForm = nsnull);

  virtual void RecompileScriptEventListeners();

  /**
   * See if the document being tested has nav-quirks mode enabled.
   * @param doc the document
   */
  static PRBool InNavQuirksMode(nsIDocument* aDoc);

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

  /**
   * Locate an nsIEditor rooted at this content node, if there is one.
   */
  NS_HIDDEN_(nsresult) GetEditor(nsIEditor** aEditor);
  NS_HIDDEN_(nsresult) GetEditorInternal(nsIEditor** aEditor);

protected:
  /**
   * Focus or blur the element.  This is what you should call if you want to
   * *cause* a focus or blur on your element.  SetFocus / SetBlur are the
   * methods where you want to catch what occurs on your element.
   * @param aDoFocus true to focus, false to blur
   */
  void SetElementFocus(PRBool aDoFocus);
  /**
   * Register or unregister an access key to this element based on the
   * accesskey attribute.
   * @param aDoReg true to register, false to unregister
   */
  void RegUnRegAccessKey(PRBool aDoReg);

  /**
   * Determine whether an attribute is an event (onclick, etc.)
   * @param aName the attribute
   * @return whether the name is an event handler name
   */
  PRBool IsEventName(nsIAtom* aName);

  virtual nsresult AfterSetAttr(PRInt32 aNamespaceID, nsIAtom* aName,
                                const nsAString* aValue, PRBool aNotify);

  virtual nsresult
    GetEventListenerManagerForAttr(nsIEventListenerManager** aManager,
                                   nsISupports** aTarget,
                                   PRBool* aDefer);

  virtual const nsAttrName* InternalGetExistingAttrNameFromQName(const nsAString& aStr) const;

  /**
   * Helper method for NS_IMPL_STRING_ATTR macro.
   * Gets the value of an attribute, returns empty string if
   * attribute isn't set. Only works for attributes in null namespace.
   *
   * @param aAttr    name of attribute.
   * @param aDefault default-value to return if attribute isn't set.
   * @param aResult  result value [out]
   * @result always NS_OK
   */
  NS_HIDDEN_(nsresult) GetAttrHelper(nsIAtom* aAttr, nsAString& aValue);

  /**
   * Helper method for NS_IMPL_STRING_ATTR macro.
   * Sets the value of an attribute, returns specified default value if the
   * attribute isn't set. Only works for attributes in null namespace.
   *
   * @param aAttr    name of attribute.
   * @param aDefault default-value to return if attribute isn't set.
   * @param aResult  result value [out]
   */
  NS_HIDDEN_(nsresult) SetAttrHelper(nsIAtom* aAttr, const nsAString& aValue);

  /**
   * Helper method for NS_IMPL_STRING_ATTR_DEFAULT_VALUE macro.
   * Gets the value of an attribute, returns specified default value if the
   * attribute isn't set. Only works for attributes in null namespace.
   *
   * @param aAttr    name of attribute.
   * @param aDefault default-value to return if attribute isn't set.
   * @param aResult  result value [out]
   */
  NS_HIDDEN_(nsresult) GetStringAttrWithDefault(nsIAtom* aAttr,
                                                const char* aDefault,
                                                nsAString& aResult);

  /**
   * Helper method for NS_IMPL_BOOL_ATTR macro.
   * Gets value of boolean attribute. Only works for attributes in null
   * namespace.
   *
   * @param aAttr    name of attribute.
   * @param aValue   Boolean value of attribute.
   */
  NS_HIDDEN_(nsresult) GetBoolAttr(nsIAtom* aAttr, PRBool* aValue) const;

  /**
   * Helper method for NS_IMPL_BOOL_ATTR macro.
   * Sets value of boolean attribute by removing attribute or setting it to
   * the empty string. Only works for attributes in null namespace.
   *
   * @param aAttr    name of attribute.
   * @param aValue   Boolean value of attribute.
   */
  NS_HIDDEN_(nsresult) SetBoolAttr(nsIAtom* aAttr, PRBool aValue);

  /**
   * Helper method for NS_IMPL_INT_ATTR macro.
   * Gets the integer-value of an attribute, returns specified default value
   * if the attribute isn't set or isn't set to an integer. Only works for
   * attributes in null namespace.
   *
   * @param aAttr    name of attribute.
   * @param aDefault default-value to return if attribute isn't set.
   * @param aResult  result value [out]
   */
  NS_HIDDEN_(nsresult) GetIntAttr(nsIAtom* aAttr, PRInt32 aDefault, PRInt32* aValue);

  /**
   * Helper method for NS_IMPL_INT_ATTR macro.
   * Sets value of attribute to specified integer. Only works for attributes
   * in null namespace.
   *
   * @param aAttr    name of attribute.
   * @param aValue   Integer value of attribute.
   */
  NS_HIDDEN_(nsresult) SetIntAttr(nsIAtom* aAttr, PRInt32 aValue);

  /**
   * Helper method for NS_IMPL_FLOAT_ATTR macro.
   * Gets the float-value of an attribute, returns specified default value
   * if the attribute isn't set or isn't set to a float. Only works for
   * attributes in null namespace.
   *
   * @param aAttr    name of attribute.
   * @param aDefault default-value to return if attribute isn't set.
   * @param aResult  result value [out]
   */
  NS_HIDDEN_(nsresult) GetFloatAttr(nsIAtom* aAttr, float aDefault, float* aValue);

  /**
   * Helper method for NS_IMPL_FLOAT_ATTR macro.
   * Sets value of attribute to specified float. Only works for attributes
   * in null namespace.
   *
   * @param aAttr    name of attribute.
   * @param aValue   Float value of attribute.
   */
  NS_HIDDEN_(nsresult) SetFloatAttr(nsIAtom* aAttr, float aValue);

  /**
   * Helper method for NS_IMPL_URI_ATTR macro.
   * Gets the absolute URI value of an attribute, by resolving any relative
   * URIs in the attribute against the baseuri of the element. If the attribute
   * isn't a relative URI the value of the attribute is returned as is. Only
   * works for attributes in null namespace.
   *
   * @param aAttr      name of attribute.
   * @param aBaseAttr  name of base attribute.
   * @param aResult    result value [out]
   */
  NS_HIDDEN_(nsresult) GetURIAttr(nsIAtom* aAttr, nsIAtom* aBaseAttr, nsAString& aResult);

  /**
   * This method works like GetURIAttr, except that it supports multiple
   * URIs separated by whitespace (one or more U+0020 SPACE characters).
   *
   * Gets the absolute URI values of an attribute, by resolving any relative
   * URIs in the attribute against the baseuri of the element. If a substring
   * isn't a relative URI, the substring is returned as is. Only works for
   * attributes in null namespace.
   *
   * @param aAttr    name of attribute.
   * @param aResult  result value [out]
   */
  NS_HIDDEN_(nsresult) GetURIListAttr(nsIAtom* aAttr, nsAString& aResult);

  /**
   * Locates the nsIEditor associated with this node.  In general this is
   * equivalent to GetEditorInternal(), but for designmode or contenteditable,
   * this may need to get an editor that's not actually on this element's
   * associated TextControlFrame.  This is used by the spellchecking routines
   * to get the editor affected by changing the spellcheck attribute on this
   * node.
   */
  virtual already_AddRefed<nsIEditor> GetAssociatedEditor();

  /**
   * Get the frame's offset information for offsetTop/Left/Width/Height.
   * @note This method flushes pending notifications (Flush_Layout).
   * @param aRect the offset information [OUT]
   * @param aOffsetParent the parent the offset is relative to (offsetParent)
   *        [OUT]
   */
  virtual void GetOffsetRect(nsRect& aRect, nsIContent** aOffsetParent);

  /**
   * Returns true if this is the current document's body element
   */
  PRBool IsCurrentBodyElement();

  /**
   * Ensures all editors associated with a subtree are synced, for purposes of
   * spellchecking.
   */
  static void SyncEditorsOnSubtree(nsIContent* content);

  enum ContentEditableTristate {
    eInherit = -1,
    eFalse = 0,
    eTrue = 1
  };

  /**
   * Returns eTrue if the element has a contentEditable attribute and its value
   * is "true" or an empty string. Returns eFalse if the element has a
   * contentEditable attribute and its value is "false". Otherwise returns
   * eInherit.
   */
  NS_HIDDEN_(ContentEditableTristate) GetContentEditableValue() const
  {
    static const nsIContent::AttrValuesArray values[] =
      { &nsGkAtoms::_false, &nsGkAtoms::_true, &nsGkAtoms::_empty, nsnull };

    PRInt32 value = FindAttrValueIn(kNameSpaceID_None,
                                    nsGkAtoms::contenteditable, values,
                                    eIgnoreCase);

    return value > 0 ? eTrue : (value == 0 ? eFalse : eInherit);
  }

private:
  /**
   * Returns whether this element is an editable root. There are two types of
   * editable roots:
   *   1) the documentElement if the whole document is editable (for example for
   *      desginMode=on)
   *   2) an element that is marked editable with contentEditable=true and that
   *      doesn't have a parent or whose parent is not editable.
   * Note that this doesn't return input and textarea elements that haven't been
   * made editable through contentEditable or designMode.
   */
  PRBool IsEditableRoot() const;

  void ChangeEditableState(PRInt32 aChange);
};


//----------------------------------------------------------------------

/**
 * A helper class for form elements that can contain children
 */
class nsGenericHTMLFormElement : public nsGenericHTMLElement,
                                 public nsIFormControl
{
public:
  nsGenericHTMLFormElement(nsINodeInfo *aNodeInfo);
  virtual ~nsGenericHTMLFormElement();

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  virtual PRBool IsNodeOfType(PRUint32 aFlags) const;
  virtual void SaveSubtreeState();

  // nsIFormControl
  NS_IMETHOD GetForm(nsIDOMHTMLFormElement** aForm);
  virtual void SetForm(nsIDOMHTMLFormElement* aForm);
  virtual void ClearForm(PRBool aRemoveFromForm, PRBool aNotify);

  NS_IMETHOD SaveState()
  {
    return NS_OK;
  }
  
  virtual PRBool RestoreState(nsPresState* aState)
  {
    return PR_FALSE;
  }
  virtual PRBool AllowDrop()
  {
    return PR_TRUE;
  }
  
  virtual PRBool IsSubmitControl() const;

  // nsIContent
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              PRBool aCompileEventHandlers);
  virtual void UnbindFromTree(PRBool aDeep = PR_TRUE,
                              PRBool aNullParent = PR_TRUE);
  virtual PRUint32 GetDesiredIMEState();
  virtual PRInt32 IntrinsicState() const;

protected:
  virtual nsresult BeforeSetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                                 const nsAString* aValue, PRBool aNotify);

  virtual nsresult AfterSetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                                const nsAString* aValue, PRBool aNotify);

  /**
   * Returns true if the control can be disabled
   */
  PRBool CanBeDisabled() const;

  void UpdateEditableFormControlState();

  void SetFocusAndScrollIntoView(nsPresContext* aPresContext);

  // A sane SetFocus implementation for focusable form controls
  void DoSetFocus(nsPresContext* aPresContext);

  // The focusability state of this form control.  eUnfocusable means that it
  // shouldn't be focused at all, eInactiveWindow means it's in an inactive
  // window, eActiveWindow means it's in an active window.
  enum FocusTristate {
    eUnfocusable,
    eInactiveWindow,
    eActiveWindow
  };

  // Get our focus state.  If this returns eInactiveWindow, it will set this
  // element as the focused element for that window.
  FocusTristate FocusState();

  /** The form that contains this control */
  nsIForm* mForm;
};

// If this flag is set on an nsGenericHTMLFormElement, that means that we have
// added ourselves to our mForm.  It's possible to have a non-null mForm, but
// not have this flag set.  That happens when the form is set via the content
// sink.
#define ADDED_TO_FORM (1 << NODE_TYPE_SPECIFIC_BITS_OFFSET)

// If this flag is set on an nsGenericHTMLFormElement, that means that its form
// is in the process of being unbound from the tree, and this form element
// hasn't re-found its form in nsGenericHTMLFormElement::UnbindFromTree yet.
#define MAYBE_ORPHAN_FORM_ELEMENT (1 << (NODE_TYPE_SPECIFIC_BITS_OFFSET+1))

// NOTE: I don't think it's possible to have the above two flags set at the
// same time, so if it becomes an issue we can probably merge them into the
// same bit.  --bz

//----------------------------------------------------------------------

/**
 * A helper class for frame elements
 */

class nsGenericHTMLFrameElement : public nsGenericHTMLElement,
                                  public nsIDOMNSHTMLFrameElement,
                                  public nsIFrameLoaderOwner
{
public:
  nsGenericHTMLFrameElement(nsINodeInfo *aNodeInfo)
    : nsGenericHTMLElement(aNodeInfo)
  {
  }
  virtual ~nsGenericHTMLFrameElement();

  // nsISupports
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  // nsIDOMNSHTMLFrameElement
  NS_DECL_NSIDOMNSHTMLFRAMEELEMENT

  // nsIFrameLoaderOwner
  NS_DECL_NSIFRAMELOADEROWNER

  // nsIContent
  virtual PRBool IsHTMLFocusable(PRBool *aIsFocusable, PRInt32 *aTabIndex);
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              PRBool aCompileEventHandlers);
  virtual void UnbindFromTree(PRBool aDeep = PR_TRUE,
                              PRBool aNullParent = PR_TRUE);
  nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, PRBool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nsnull, aValue, aNotify);
  }
  virtual nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           PRBool aNotify);
  virtual void DestroyContent();

  // nsIDOMNSHTMLElement 
  NS_IMETHOD GetTabIndex(PRInt32 *aTabIndex);
  NS_IMETHOD SetTabIndex(PRInt32 aTabIndex);

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED_NO_UNLINK(nsGenericHTMLFrameElement,
                                                     nsGenericHTMLElement)

protected:
  // This doesn't really ensure a frame loade in all cases, only when
  // it makes sense.
  nsresult EnsureFrameLoader();
  nsresult LoadSrc();
  nsresult GetContentDocument(nsIDOMDocument** aContentDocument);

  nsCOMPtr<nsIFrameLoader> mFrameLoader;
};

//----------------------------------------------------------------------

/**
 * A macro to implement the NS_NewHTMLXXXElement() functions.
 */
#define NS_IMPL_NS_NEW_HTML_ELEMENT(_elementName)                            \
nsGenericHTMLElement*                                                        \
NS_NewHTML##_elementName##Element(nsINodeInfo *aNodeInfo, PRBool aFromParser)\
{                                                                            \
  return new nsHTML##_elementName##Element(aNodeInfo);                       \
}

#define NS_IMPL_NS_NEW_HTML_ELEMENT_CHECK_PARSER(_elementName)               \
nsGenericHTMLElement*                                                        \
NS_NewHTML##_elementName##Element(nsINodeInfo *aNodeInfo, PRBool aFromParser)\
{                                                                            \
  return new nsHTML##_elementName##Element(aNodeInfo, aFromParser);          \
}


/**
 * A macro to implement the getter and setter for a given string
 * valued content property. The method uses the generic GetAttr and
 * SetAttr methods.
 */
#define NS_IMPL_STRING_ATTR(_class, _method, _atom)                  \
  NS_IMETHODIMP                                                      \
  _class::Get##_method(nsAString& aValue)                            \
  {                                                                  \
    return GetAttrHelper(nsGkAtoms::_atom, aValue);                \
  }                                                                  \
  NS_IMETHODIMP                                                      \
  _class::Set##_method(const nsAString& aValue)                      \
  {                                                                  \
    return SetAttrHelper(nsGkAtoms::_atom, aValue);                \
  }

/**
 * A macro to implement the getter and setter for a given string
 * valued content property with a default value.
 * The method uses the generic GetAttr and SetAttr methods.
 */
#define NS_IMPL_STRING_ATTR_DEFAULT_VALUE(_class, _method, _atom, _default) \
  NS_IMETHODIMP                                                      \
  _class::Get##_method(nsAString& aValue)                            \
  {                                                                  \
    return GetStringAttrWithDefault(nsGkAtoms::_atom, _default, aValue);\
  }                                                                  \
  NS_IMETHODIMP                                                      \
  _class::Set##_method(const nsAString& aValue)                      \
  {                                                                  \
    return SetAttrHelper(nsGkAtoms::_atom, aValue);                \
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
    return GetBoolAttr(nsGkAtoms::_atom, aValue);                   \
  }                                                                   \
  NS_IMETHODIMP                                                       \
  _class::Set##_method(PRBool aValue)                                 \
  {                                                                   \
    return SetBoolAttr(nsGkAtoms::_atom, aValue);                   \
  }

/**
 * A macro to implement the getter and setter for a given integer
 * valued content property. The method uses the generic GetAttr and
 * SetAttr methods.
 */
#define NS_IMPL_INT_ATTR(_class, _method, _atom)                    \
  NS_IMPL_INT_ATTR_DEFAULT_VALUE(_class, _method, _atom, -1)

#define NS_IMPL_INT_ATTR_DEFAULT_VALUE(_class, _method, _atom, _default)  \
  NS_IMETHODIMP                                                           \
  _class::Get##_method(PRInt32* aValue)                                   \
  {                                                                       \
    return GetIntAttr(nsGkAtoms::_atom, _default, aValue);              \
  }                                                                       \
  NS_IMETHODIMP                                                           \
  _class::Set##_method(PRInt32 aValue)                                    \
  {                                                                       \
    return SetIntAttr(nsGkAtoms::_atom, aValue);                        \
  }

/**
 * A macro to implement the getter and setter for a given float
 * valued content property. The method uses the generic GetAttr and
 * SetAttr methods.
 */
#define NS_IMPL_FLOAT_ATTR(_class, _method, _atom)                    \
  NS_IMPL_FLOAT_ATTR_DEFAULT_VALUE(_class, _method, _atom, 0.0)

#define NS_IMPL_FLOAT_ATTR_DEFAULT_VALUE(_class, _method, _atom, _default)  \
  NS_IMETHODIMP                                                             \
  _class::Get##_method(float* aValue)                                   \
  {                                                                         \
    return GetFloatAttr(nsGkAtoms::_atom, _default, aValue);                \
  }                                                                         \
  NS_IMETHODIMP                                                             \
  _class::Set##_method(float aValue)                                    \
  {                                                                         \
    return SetFloatAttr(nsGkAtoms::_atom, aValue);                          \
  }

/**
 * A macro to implement the getter and setter for a given content
 * property that needs to return a URI in string form.  The method
 * uses the generic GetAttr and SetAttr methods.  This macro is much
 * like the NS_IMPL_STRING_ATTR macro, except we make sure the URI is
 * absolute.
 */
#define NS_IMPL_URI_ATTR(_class, _method, _atom)                    \
  NS_IMETHODIMP                                                     \
  _class::Get##_method(nsAString& aValue)                           \
  {                                                                 \
    return GetURIAttr(nsGkAtoms::_atom, nsnull, aValue);          \
  }                                                                 \
  NS_IMETHODIMP                                                     \
  _class::Set##_method(const nsAString& aValue)                     \
  {                                                                 \
    return SetAttrHelper(nsGkAtoms::_atom, aValue);               \
  }

#define NS_IMPL_URI_ATTR_WITH_BASE(_class, _method, _atom, _base_atom)       \
  NS_IMETHODIMP                                                              \
  _class::Get##_method(nsAString& aValue)                                    \
  {                                                                          \
    return GetURIAttr(nsGkAtoms::_atom, nsGkAtoms::_base_atom, aValue);  \
  }                                                                          \
  NS_IMETHODIMP                                                              \
  _class::Set##_method(const nsAString& aValue)                              \
  {                                                                          \
    return SetAttrHelper(nsGkAtoms::_atom, aValue);                        \
  }

/**
 * QueryInterface() implementation helper macros
 */

#define NS_HTML_CONTENT_INTERFACE_TABLE_AMBIGUOUS_BEGIN(_class, _base)        \
  NS_NODE_OFFSET_AND_INTERFACE_TABLE_BEGIN(_class)                            \
    NS_INTERFACE_TABLE_ENTRY_AMBIGUOUS(_class, nsIDOMNode, _base)             \
    NS_INTERFACE_TABLE_ENTRY_AMBIGUOUS(_class, nsIDOMElement, _base)          \
    NS_INTERFACE_TABLE_ENTRY_AMBIGUOUS(_class, nsIDOMHTMLElement, _base)

#define NS_HTML_CONTENT_INTERFACE_TABLE_BEGIN(_class)                         \
  NS_HTML_CONTENT_INTERFACE_TABLE_AMBIGUOUS_BEGIN(_class, nsIDOMHTMLElement)

#define NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE_AMBIGUOUS(_class, _base, \
                                                               _base_if)      \
  rv = _base::QueryInterface(aIID, aInstancePtr);                             \
  if (NS_SUCCEEDED(rv))                                                       \
    return rv;                                                                \
                                                                              \
  rv = DOMQueryInterface(static_cast<_base_if *>(this), aIID, aInstancePtr);  \
  if (NS_SUCCEEDED(rv))                                                       \
    return rv;                                                                \
                                                                              \
  NS_OFFSET_AND_INTERFACE_TABLE_TO_MAP_SEGUE

#define NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(_class, _base)           \
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE_AMBIGUOUS(_class, _base,       \
                                                         nsIDOMHTMLElement)

#define NS_HTML_CONTENT_INTERFACE_MAP_END                                     \
  NS_ELEMENT_INTERFACE_MAP_END

#define NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(_class)                \
    NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(_class)                          \
  NS_HTML_CONTENT_INTERFACE_MAP_END

#define NS_INTERFACE_MAP_ENTRY_IF_TAG(_interface, _tag)                       \
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(_interface,                              \
                                     mNodeInfo->Equals(nsGkAtoms::_tag))


#define NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO_IF_TAG(_class, _tag)         \
  if (mNodeInfo->Equals(nsGkAtoms::_tag) &&                                   \
      aIID.Equals(NS_GET_IID(nsIClassInfo))) {                                \
    foundInterface = NS_GetDOMClassInfoInstance(eDOMClassInfo_##_class##_id); \
    if (!foundInterface) {                                                    \
      *aInstancePtr = nsnull;                                                 \
      return NS_ERROR_OUT_OF_MEMORY;                                          \
    }                                                                         \
  } else

#define NS_HTML_CONTENT_INTERFACE_TABLE0(_class)                              \
  NS_HTML_CONTENT_INTERFACE_TABLE_BEGIN(_class)                               \
  NS_OFFSET_AND_INTERFACE_TABLE_END

#define NS_HTML_CONTENT_INTERFACE_TABLE1(_class, _i1)                         \
  NS_HTML_CONTENT_INTERFACE_TABLE_BEGIN(_class)                               \
    NS_INTERFACE_TABLE_ENTRY(_class, _i1)                                     \
  NS_OFFSET_AND_INTERFACE_TABLE_END

#define NS_HTML_CONTENT_INTERFACE_TABLE2(_class, _i1, _i2)                    \
  NS_HTML_CONTENT_INTERFACE_TABLE_BEGIN(_class)                               \
    NS_INTERFACE_TABLE_ENTRY(_class, _i1)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i2)                                     \
  NS_OFFSET_AND_INTERFACE_TABLE_END

#define NS_HTML_CONTENT_INTERFACE_TABLE4(_class, _i1, _i2, _i3, _i4)          \
  NS_HTML_CONTENT_INTERFACE_TABLE_BEGIN(_class)                               \
    NS_INTERFACE_TABLE_ENTRY(_class, _i1)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i2)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i3)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i4)                                     \
  NS_OFFSET_AND_INTERFACE_TABLE_END

#define NS_HTML_CONTENT_INTERFACE_TABLE5(_class, _i1, _i2, _i3, _i4, _i5)     \
  NS_HTML_CONTENT_INTERFACE_TABLE_BEGIN(_class)                               \
    NS_INTERFACE_TABLE_ENTRY(_class, _i1)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i2)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i3)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i4)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i5)                                     \
  NS_OFFSET_AND_INTERFACE_TABLE_END

#define NS_HTML_CONTENT_INTERFACE_TABLE6(_class, _i1, _i2, _i3, _i4, _i5,     \
                                         _i6)                                 \
  NS_HTML_CONTENT_INTERFACE_TABLE_BEGIN(_class)                               \
    NS_INTERFACE_TABLE_ENTRY(_class, _i1)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i2)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i3)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i4)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i5)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i6)                                     \
  NS_OFFSET_AND_INTERFACE_TABLE_END

#define NS_HTML_CONTENT_INTERFACE_TABLE9(_class, _i1, _i2, _i3, _i4, _i5,     \
                                         _i6, _i7, _i8, _i9)                  \
  NS_HTML_CONTENT_INTERFACE_TABLE_BEGIN(_class)                               \
    NS_INTERFACE_TABLE_ENTRY(_class, _i1)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i2)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i3)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i4)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i5)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i6)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i7)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i8)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i9)                                     \
  NS_OFFSET_AND_INTERFACE_TABLE_END

#define NS_HTML_CONTENT_INTERFACE_TABLE10(_class, _i1, _i2, _i3, _i4, _i5,    \
                                          _i6, _i7, _i8, _i9, _i10)           \
  NS_HTML_CONTENT_INTERFACE_TABLE_BEGIN(_class)                               \
    NS_INTERFACE_TABLE_ENTRY(_class, _i1)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i2)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i3)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i4)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i5)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i6)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i7)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i8)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i9)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i10)                                    \
  NS_OFFSET_AND_INTERFACE_TABLE_END


// Element class factory methods

#define NS_DECLARE_NS_NEW_HTML_ELEMENT(_elementName)              \
nsGenericHTMLElement*                                             \
NS_NewHTML##_elementName##Element(nsINodeInfo *aNodeInfo,         \
                                  PRBool aFromParser = PR_FALSE);

NS_DECLARE_NS_NEW_HTML_ELEMENT(Shared)
NS_DECLARE_NS_NEW_HTML_ELEMENT(SharedList)
NS_DECLARE_NS_NEW_HTML_ELEMENT(SharedObject)

NS_DECLARE_NS_NEW_HTML_ELEMENT(Anchor)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Area)
#if defined(MOZ_MEDIA)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Audio)
#endif
NS_DECLARE_NS_NEW_HTML_ELEMENT(BR)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Body)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Button)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Canvas)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Mod)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Div)
NS_DECLARE_NS_NEW_HTML_ELEMENT(FieldSet)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Font)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Form)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Frame)
NS_DECLARE_NS_NEW_HTML_ELEMENT(FrameSet)
NS_DECLARE_NS_NEW_HTML_ELEMENT(HR)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Head)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Heading)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Html)
NS_DECLARE_NS_NEW_HTML_ELEMENT(IFrame)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Image)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Input)
NS_DECLARE_NS_NEW_HTML_ELEMENT(LI)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Label)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Legend)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Link)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Map)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Meta)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Object)
NS_DECLARE_NS_NEW_HTML_ELEMENT(OptGroup)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Option)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Paragraph)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Pre)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Script)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Select)
#if defined(MOZ_MEDIA)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Source)
#endif
NS_DECLARE_NS_NEW_HTML_ELEMENT(Span)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Style)
NS_DECLARE_NS_NEW_HTML_ELEMENT(TableCaption)
NS_DECLARE_NS_NEW_HTML_ELEMENT(TableCell)
NS_DECLARE_NS_NEW_HTML_ELEMENT(TableCol)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Table)
NS_DECLARE_NS_NEW_HTML_ELEMENT(TableRow)
NS_DECLARE_NS_NEW_HTML_ELEMENT(TableSection)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Tbody)
NS_DECLARE_NS_NEW_HTML_ELEMENT(TextArea)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Tfoot)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Thead)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Title)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Unknown)
#if defined(MOZ_MEDIA)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Video)
#endif

#endif /* nsGenericHTMLElement_h___ */
