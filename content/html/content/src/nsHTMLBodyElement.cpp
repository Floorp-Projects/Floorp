/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Daniel Glazman <glazman@netscape.com>
 *   Ms2ger <ms2ger@gmail.com>
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
#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsIDOMHTMLBodyElement.h"
#include "nsIDOMEventTarget.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"
#include "nsIHTMLDocument.h"
#include "nsHTMLStyleSheet.h"
#include "nsIContentViewer.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsMappedAttributes.h"
#include "nsRuleData.h"
#include "nsIFrame.h"
#include "nsIDocShell.h"
#include "nsIEditorDocShell.h"
#include "nsCOMPtr.h"
#include "nsRuleWalker.h"
#include "jsapi.h"

//----------------------------------------------------------------------

class nsHTMLBodyElement;

class BodyRule: public nsIStyleRule {
public:
  BodyRule(nsHTMLBodyElement* aPart);
  virtual ~BodyRule();

  NS_DECL_ISUPPORTS

  // nsIStyleRule interface
  virtual void MapRuleInfoInto(nsRuleData* aRuleData);
#ifdef DEBUG
  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const;
#endif

  nsHTMLBodyElement*  mPart;  // not ref-counted, cleared by content 
};

//----------------------------------------------------------------------

class nsHTMLBodyElement : public nsGenericHTMLElement,
                          public nsIDOMHTMLBodyElement
{
public:
  using nsGenericElement::GetText;
  using nsGenericElement::SetText;

  nsHTMLBodyElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~nsHTMLBodyElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLBodyElement
  NS_DECL_NSIDOMHTMLBODYELEMENT

  // Event listener stuff; we need to declare only the ones we need to
  // forward to window that don't come from nsIDOMHTMLBodyElement.
#define EVENT(name_, id_, type_, struct_) /* nothing; handled by the shim */
#define FORWARDED_EVENT(name_, id_, type_, struct_)               \
    NS_IMETHOD GetOn##name_(JSContext *cx, jsval *vp);            \
    NS_IMETHOD SetOn##name_(JSContext *cx, const jsval &v);
#include "nsEventNameList.h"
#undef FORWARDED_EVENT
#undef EVENT

  virtual bool ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true);
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const;
  NS_IMETHOD WalkContentStyleRules(nsRuleWalker* aRuleWalker);
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;
  virtual already_AddRefed<nsIEditor> GetAssociatedEditor();
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;
  virtual nsXPCClassInfo* GetClassInfo();
private:
  nsresult GetColorHelper(nsIAtom* aAtom, nsAString& aColor);

protected:
  BodyRule* mContentStyleRule;
};

//----------------------------------------------------------------------

BodyRule::BodyRule(nsHTMLBodyElement* aPart)
{
  mPart = aPart;
}

BodyRule::~BodyRule()
{
}

NS_IMPL_ISUPPORTS1(BodyRule, nsIStyleRule)

/* virtual */ void
BodyRule::MapRuleInfoInto(nsRuleData* aData)
{
  if (!(aData->mSIDs & NS_STYLE_INHERIT_BIT(Margin)) || !mPart)
    return; // We only care about margins.

  PRInt32 bodyMarginWidth  = -1;
  PRInt32 bodyMarginHeight = -1;
  PRInt32 bodyTopMargin = -1;
  PRInt32 bodyBottomMargin = -1;
  PRInt32 bodyLeftMargin = -1;
  PRInt32 bodyRightMargin = -1;

  // check the mode (fortunately, the ruleData has a presContext for us to use!)
  NS_ASSERTION(aData->mPresContext, "null presContext in ruleNode was unexpected");
  nsCompatibility mode = aData->mPresContext->CompatibilityMode();


  const nsAttrValue* value;
  if (mPart->GetAttrCount() > 0) {
    // if marginwidth/marginheight are set, reflect them as 'margin'
    value = mPart->GetParsedAttr(nsGkAtoms::marginwidth);
    if (value && value->Type() == nsAttrValue::eInteger) {
      bodyMarginWidth = value->GetIntegerValue();
      if (bodyMarginWidth < 0) bodyMarginWidth = 0;
      nsCSSValue* marginLeft = aData->ValueForMarginLeftValue();
      if (marginLeft->GetUnit() == eCSSUnit_Null)
        marginLeft->SetFloatValue((float)bodyMarginWidth, eCSSUnit_Pixel);
      nsCSSValue* marginRight = aData->ValueForMarginRightValue();
      if (marginRight->GetUnit() == eCSSUnit_Null)
        marginRight->SetFloatValue((float)bodyMarginWidth, eCSSUnit_Pixel);
    }

    value = mPart->GetParsedAttr(nsGkAtoms::marginheight);
    if (value && value->Type() == nsAttrValue::eInteger) {
      bodyMarginHeight = value->GetIntegerValue();
      if (bodyMarginHeight < 0) bodyMarginHeight = 0;
      nsCSSValue* marginTop = aData->ValueForMarginTop();
      if (marginTop->GetUnit() == eCSSUnit_Null)
        marginTop->SetFloatValue((float)bodyMarginHeight, eCSSUnit_Pixel);
      nsCSSValue* marginBottom = aData->ValueForMarginBottom();
      if (marginBottom->GetUnit() == eCSSUnit_Null)
        marginBottom->SetFloatValue((float)bodyMarginHeight, eCSSUnit_Pixel);
    }

    if (eCompatibility_NavQuirks == mode){
      // topmargin (IE-attribute)
      value = mPart->GetParsedAttr(nsGkAtoms::topmargin);
      if (value && value->Type() == nsAttrValue::eInteger) {
        bodyTopMargin = value->GetIntegerValue();
        if (bodyTopMargin < 0) bodyTopMargin = 0;
        nsCSSValue* marginTop = aData->ValueForMarginTop();
        if (marginTop->GetUnit() == eCSSUnit_Null)
          marginTop->SetFloatValue((float)bodyTopMargin, eCSSUnit_Pixel);
      }

      // bottommargin (IE-attribute)
      value = mPart->GetParsedAttr(nsGkAtoms::bottommargin);
      if (value && value->Type() == nsAttrValue::eInteger) {
        bodyBottomMargin = value->GetIntegerValue();
        if (bodyBottomMargin < 0) bodyBottomMargin = 0;
        nsCSSValue* marginBottom = aData->ValueForMarginBottom();
        if (marginBottom->GetUnit() == eCSSUnit_Null)
          marginBottom->SetFloatValue((float)bodyBottomMargin, eCSSUnit_Pixel);
      }

      // leftmargin (IE-attribute)
      value = mPart->GetParsedAttr(nsGkAtoms::leftmargin);
      if (value && value->Type() == nsAttrValue::eInteger) {
        bodyLeftMargin = value->GetIntegerValue();
        if (bodyLeftMargin < 0) bodyLeftMargin = 0;
        nsCSSValue* marginLeft = aData->ValueForMarginLeftValue();
        if (marginLeft->GetUnit() == eCSSUnit_Null)
          marginLeft->SetFloatValue((float)bodyLeftMargin, eCSSUnit_Pixel);
      }

      // rightmargin (IE-attribute)
      value = mPart->GetParsedAttr(nsGkAtoms::rightmargin);
      if (value && value->Type() == nsAttrValue::eInteger) {
        bodyRightMargin = value->GetIntegerValue();
        if (bodyRightMargin < 0) bodyRightMargin = 0;
        nsCSSValue* marginRight = aData->ValueForMarginRightValue();
        if (marginRight->GetUnit() == eCSSUnit_Null)
          marginRight->SetFloatValue((float)bodyRightMargin, eCSSUnit_Pixel);
      }
    }

  }

  // if marginwidth or marginheight is set in the <frame> and not set in the <body>
  // reflect them as margin in the <body>
  if (bodyMarginWidth == -1 || bodyMarginHeight == -1) {
    nsCOMPtr<nsISupports> container = aData->mPresContext->GetContainer();
    if (container) {
      nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(container));
      if (docShell) {
        nscoord frameMarginWidth=-1;  // default value
        nscoord frameMarginHeight=-1; // default value
        docShell->GetMarginWidth(&frameMarginWidth); // -1 indicates not set   
        docShell->GetMarginHeight(&frameMarginHeight); 
        if ((frameMarginWidth >= 0) && (bodyMarginWidth == -1)) { // set in <frame> & not in <body> 
          if (eCompatibility_NavQuirks == mode) {
            if ((bodyMarginHeight == -1) && (0 > frameMarginHeight)) // nav quirk 
              frameMarginHeight = 0;
          }
        }
        if ((frameMarginHeight >= 0) && (bodyMarginHeight == -1)) { // set in <frame> & not in <body> 
          if (eCompatibility_NavQuirks == mode) {
            if ((bodyMarginWidth == -1) && (0 > frameMarginWidth)) // nav quirk
              frameMarginWidth = 0;
          }
        }

        if ((bodyMarginWidth == -1) && (frameMarginWidth >= 0)) {
          nsCSSValue* marginLeft = aData->ValueForMarginLeftValue();
          if (marginLeft->GetUnit() == eCSSUnit_Null)
            marginLeft->SetFloatValue((float)frameMarginWidth, eCSSUnit_Pixel);
          nsCSSValue* marginRight = aData->ValueForMarginRightValue();
          if (marginRight->GetUnit() == eCSSUnit_Null)
            marginRight->SetFloatValue((float)frameMarginWidth, eCSSUnit_Pixel);
        }

        if ((bodyMarginHeight == -1) && (frameMarginHeight >= 0)) {
          nsCSSValue* marginTop = aData->ValueForMarginTop();
          if (marginTop->GetUnit() == eCSSUnit_Null)
            marginTop->SetFloatValue((float)frameMarginHeight, eCSSUnit_Pixel);
          nsCSSValue* marginBottom = aData->ValueForMarginBottom();
          if (marginBottom->GetUnit() == eCSSUnit_Null)
            marginBottom->SetFloatValue((float)frameMarginHeight, eCSSUnit_Pixel);
        }
      }
    }
  }
}

#ifdef DEBUG
/* virtual */ void
BodyRule::List(FILE* out, PRInt32 aIndent) const
{
}
#endif

//----------------------------------------------------------------------


NS_IMPL_NS_NEW_HTML_ELEMENT(Body)


nsHTMLBodyElement::nsHTMLBodyElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo),
    mContentStyleRule(nsnull)
{
}

nsHTMLBodyElement::~nsHTMLBodyElement()
{
  if (mContentStyleRule) {
    mContentStyleRule->mPart = nsnull;
    NS_RELEASE(mContentStyleRule);
  }
}


NS_IMPL_ADDREF_INHERITED(nsHTMLBodyElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLBodyElement, nsGenericElement) 

DOMCI_NODE_DATA(HTMLBodyElement, nsHTMLBodyElement)

// QueryInterface implementation for nsHTMLBodyElement
NS_INTERFACE_TABLE_HEAD(nsHTMLBodyElement)
  NS_HTML_CONTENT_INTERFACE_TABLE1(nsHTMLBodyElement, nsIDOMHTMLBodyElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLBodyElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLBodyElement)

NS_IMPL_ELEMENT_CLONE(nsHTMLBodyElement)


NS_IMPL_STRING_ATTR(nsHTMLBodyElement, Background, background)
NS_IMPL_STRING_ATTR(nsHTMLBodyElement, VLink, vlink)
NS_IMPL_STRING_ATTR(nsHTMLBodyElement, ALink, alink)
NS_IMPL_STRING_ATTR(nsHTMLBodyElement, Link, link)
NS_IMPL_STRING_ATTR(nsHTMLBodyElement, Text, text)
NS_IMPL_STRING_ATTR(nsHTMLBodyElement, BgColor, bgcolor)

bool
nsHTMLBodyElement::ParseAttribute(PRInt32 aNamespaceID,
                                  nsIAtom* aAttribute,
                                  const nsAString& aValue,
                                  nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::bgcolor ||
        aAttribute == nsGkAtoms::text ||
        aAttribute == nsGkAtoms::link ||
        aAttribute == nsGkAtoms::alink ||
        aAttribute == nsGkAtoms::vlink) {
      return aResult.ParseColor(aValue);
    }
    if (aAttribute == nsGkAtoms::marginwidth ||
        aAttribute == nsGkAtoms::marginheight ||
        aAttribute == nsGkAtoms::topmargin ||
        aAttribute == nsGkAtoms::bottommargin ||
        aAttribute == nsGkAtoms::leftmargin ||
        aAttribute == nsGkAtoms::rightmargin) {
      return aResult.ParseIntWithBounds(aValue, 0);
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

void
nsHTMLBodyElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  if (mContentStyleRule) {
    mContentStyleRule->mPart = nsnull;

    // destroy old style rule
    NS_RELEASE(mContentStyleRule);
  }

  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);  
}

static 
void MapAttributesIntoRule(const nsMappedAttributes* aAttributes, nsRuleData* aData)
{
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Display)) {
    // When display if first asked for, go ahead and get our colors set up.
    nsIPresShell *presShell = aData->mPresContext->GetPresShell();
    if (presShell) {
      nsIDocument *doc = presShell->GetDocument();
      if (doc) {
        nsHTMLStyleSheet* styleSheet = doc->GetAttributeStyleSheet();
        if (styleSheet) {
          const nsAttrValue* value;
          nscolor color;
          value = aAttributes->GetAttr(nsGkAtoms::link);
          if (value && value->GetColorValue(color)) {
            styleSheet->SetLinkColor(color);
          }

          value = aAttributes->GetAttr(nsGkAtoms::alink);
          if (value && value->GetColorValue(color)) {
            styleSheet->SetActiveLinkColor(color);
          }

          value = aAttributes->GetAttr(nsGkAtoms::vlink);
          if (value && value->GetColorValue(color)) {
            styleSheet->SetVisitedLinkColor(color);
          }
        }
      }
    }
  }

  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Color)) {
    nsCSSValue *colorValue = aData->ValueForColor();
    if (colorValue->GetUnit() == eCSSUnit_Null &&
        aData->mPresContext->UseDocumentColors()) {
      // color: color
      nscolor color;
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::text);
      if (value && value->GetColorValue(color))
        colorValue->SetColorValue(color);
    }
  }

  nsGenericHTMLElement::MapBackgroundAttributesInto(aAttributes, aData);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

nsMapRuleToAttributesFunc
nsHTMLBodyElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}

NS_IMETHODIMP
nsHTMLBodyElement::WalkContentStyleRules(nsRuleWalker* aRuleWalker)
{
  nsGenericHTMLElement::WalkContentStyleRules(aRuleWalker);

  if (!mContentStyleRule && IsInDoc()) {
    // XXXbz should this use GetOwnerDoc() or GetCurrentDoc()?
    // sXBL/XBL2 issue!
    mContentStyleRule = new BodyRule(this);
    NS_IF_ADDREF(mContentStyleRule);
  }
  if (aRuleWalker && mContentStyleRule) {
    aRuleWalker->Forward(mContentStyleRule);
  }
  return NS_OK;
}

NS_IMETHODIMP_(bool)
nsHTMLBodyElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry attributes[] = {
    { &nsGkAtoms::link },
    { &nsGkAtoms::vlink },
    { &nsGkAtoms::alink },
    { &nsGkAtoms::text },
    // These aren't mapped through attribute mapping, but they are
    // mapped through a style rule, so it is attribute dependent style.
    // XXXldb But we don't actually replace the body rule when we have
    // dynamic changes...
    { &nsGkAtoms::marginwidth },
    { &nsGkAtoms::marginheight },
    { nsnull },
  };

  static const MappedAttributeEntry* const map[] = {
    attributes,
    sCommonAttributeMap,
    sBackgroundAttributeMap,
  };

  return FindAttributeDependence(aAttribute, map, NS_ARRAY_LENGTH(map));
}

already_AddRefed<nsIEditor>
nsHTMLBodyElement::GetAssociatedEditor()
{
  nsIEditor* editor = nsnull;
  if (NS_SUCCEEDED(GetEditorInternal(&editor)) && editor) {
    return editor;
  }

  // Make sure this is the actual body of the document
  if (!IsCurrentBodyElement()) {
    return nsnull;
  }

  // For designmode, try to get document's editor
  nsPresContext* presContext = GetPresContext();
  if (!presContext) {
    return nsnull;
  }

  nsCOMPtr<nsISupports> container = presContext->GetContainer();
  nsCOMPtr<nsIEditorDocShell> editorDocShell = do_QueryInterface(container);
  if (!editorDocShell) {
    return nsnull;
  }

  editorDocShell->GetEditor(&editor);
  return editor;
}

// Event listener stuff
// FIXME (https://bugzilla.mozilla.org/show_bug.cgi?id=431767)
// nsDocument::GetInnerWindow can return an outer window in some
// cases.  We don't want to stick an event listener on an outer
// window, so bail if it does.  See also similar code in
// nsGenericHTMLElement::GetEventListenerManagerForAttr.
#define EVENT(name_, id_, type_, struct_) /* nothing; handled by the superclass */
#define FORWARDED_EVENT(name_, id_, type_, struct_)                 \
  NS_IMETHODIMP nsHTMLBodyElement::GetOn##name_(JSContext *cx,      \
                                           jsval *vp) {             \
    /* XXXbz note to self: add tests for this! */                   \
    nsPIDOMWindow* win = GetOwnerDoc()->GetInnerWindow();           \
    if (win && win->IsInnerWindow()) {                              \
      nsCOMPtr<nsIInlineEventHandlers> ev = do_QueryInterface(win); \
      return ev->GetOn##name_(cx, vp);                              \
    }                                                               \
    *vp = JSVAL_NULL;                                               \
    return NS_OK;                                                   \
  }                                                                 \
  NS_IMETHODIMP nsHTMLBodyElement::SetOn##name_(JSContext *cx,      \
                                           const jsval &v) {        \
    nsPIDOMWindow* win = GetOwnerDoc()->GetInnerWindow();           \
    if (win && win->IsInnerWindow()) {                              \
      nsCOMPtr<nsIInlineEventHandlers> ev = do_QueryInterface(win); \
      return ev->SetOn##name_(cx, v);                               \
    }                                                               \
    return NS_OK;                                                   \
  }
#define WINDOW_EVENT(name_, id_, type_, struct_)                  \
  NS_IMETHODIMP nsHTMLBodyElement::GetOn##name_(JSContext *cx,    \
                                                jsval *vp) {      \
    nsPIDOMWindow* win = GetOwnerDoc()->GetInnerWindow();         \
    if (win && win->IsInnerWindow()) {                            \
      return win->GetOn##name_(cx, vp);                           \
    }                                                             \
    *vp = JSVAL_NULL;                                             \
    return NS_OK;                                                 \
  }                                                               \
  NS_IMETHODIMP nsHTMLBodyElement::SetOn##name_(JSContext *cx,    \
                                                const jsval &v) { \
    nsPIDOMWindow* win = GetOwnerDoc()->GetInnerWindow();         \
    if (win && win->IsInnerWindow()) {                            \
      return win->SetOn##name_(cx, v);                            \
    }                                                             \
    return NS_OK;                                                 \
  }
#include "nsEventNameList.h"
#undef WINDOW_EVENT
#undef FORWARDED_EVENT
#undef EVENT
