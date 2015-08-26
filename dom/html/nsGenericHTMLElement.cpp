/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/EventStates.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/Likely.h"

#include "nscore.h"
#include "nsGenericHTMLElement.h"
#include "nsAttrValueInlines.h"
#include "nsCOMPtr.h"
#include "nsIAtom.h"
#include "nsQueryObject.h"
#include "nsIContentInlines.h"
#include "nsIContentViewer.h"
#include "mozilla/css/StyleRule.h"
#include "nsIDocument.h"
#include "nsIDocumentEncoder.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMAttr.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMHTMLMenuElement.h"
#include "nsIDOMElementCSSInlineStyle.h"
#include "nsIDOMWindow.h"
#include "nsIDOMDocument.h"
#include "nsMappedAttributes.h"
#include "nsHTMLStyleSheet.h"
#include "nsIHTMLDocument.h"
#include "nsPIDOMWindow.h"
#include "nsIStyleRule.h"
#include "nsIURL.h"
#include "nsEscape.h"
#include "nsIFrameInlines.h"
#include "nsIScrollableFrame.h"
#include "nsView.h"
#include "nsViewManager.h"
#include "nsIWidget.h"
#include "nsRange.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIDocShell.h"
#include "nsNameSpaceManager.h"
#include "nsError.h"
#include "nsScriptLoader.h"
#include "nsRuleData.h"
#include "nsIPrincipal.h"
#include "nsContainerFrame.h"

#include "nsPresState.h"
#include "nsILayoutHistoryState.h"

#include "nsHTMLParts.h"
#include "nsContentUtils.h"
#include "mozilla/dom/DirectionalityUtils.h"
#include "nsString.h"
#include "nsUnicharUtils.h"
#include "nsGkAtoms.h"
#include "nsIDOMEvent.h"
#include "nsDOMCSSDeclaration.h"
#include "nsITextControlFrame.h"
#include "nsIForm.h"
#include "nsIFormControl.h"
#include "nsIDOMHTMLFormElement.h"
#include "mozilla/dom/HTMLFormElement.h"
#include "nsFocusManager.h"
#include "nsAttrValueOrString.h"

#include "mozilla/InternalMutationEvent.h"
#include "nsDOMStringMap.h"

#include "nsIEditor.h"
#include "nsIEditorIMESupport.h"
#include "nsLayoutUtils.h"
#include "mozAutoDocUpdate.h"
#include "nsHtml5Module.h"
#include "nsITextControlElement.h"
#include "mozilla/dom/Element.h"
#include "HTMLFieldSetElement.h"
#include "HTMLMenuElement.h"
#include "nsDOMMutationObserver.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/FromParser.h"
#include "mozilla/dom/Link.h"
#include "mozilla/dom/UndoManager.h"
#include "mozilla/BloomFilter.h"

#include "HTMLPropertiesCollection.h"
#include "nsVariant.h"
#include "nsDOMSettableTokenList.h"
#include "nsThreadUtils.h"
#include "nsTextFragment.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/TouchEvent.h"
#include "mozilla/ErrorResult.h"
#include "nsHTMLDocument.h"
#include "nsGlobalWindow.h"
#include "mozilla/dom/HTMLBodyElement.h"
#include "imgIContainer.h"

using namespace mozilla;
using namespace mozilla::dom;

/**
 * nsAutoFocusEvent is used to dispatch a focus event when a
 * nsGenericHTMLFormElement is binded to the tree with the autofocus attribute
 * enabled.
 */
class nsAutoFocusEvent : public nsRunnable
{
public:
  explicit nsAutoFocusEvent(nsGenericHTMLFormElement* aElement) : mElement(aElement) {}

  NS_IMETHOD Run() {
    nsFocusManager* fm = nsFocusManager::GetFocusManager();
    if (!fm) {
      return NS_ERROR_NULL_POINTER;
    }

    nsIDocument* document = mElement->OwnerDoc();

    nsPIDOMWindow* window = document->GetWindow();
    if (!window) {
      return NS_OK;
    }

    // Trying to found the top window (equivalent to window.top).
    nsCOMPtr<nsIDOMWindow> top;
    window->GetTop(getter_AddRefs(top));
    if (top) {
      window = static_cast<nsPIDOMWindow*>(top.get());
    }

    if (window->GetFocusedNode()) {
      return NS_OK;
    }

    nsCOMPtr<nsIDocument> topDoc = window->GetExtantDoc();
    if (topDoc && topDoc->GetReadyStateEnum() == nsIDocument::READYSTATE_COMPLETE) {
      return NS_OK;
    }

    // If something is focused in the same document, ignore autofocus.
    if (!fm->GetFocusedContent() ||
        fm->GetFocusedContent()->OwnerDoc() != document) {
      mozilla::ErrorResult rv;
      mElement->Focus(rv);
      return rv.StealNSResult();
    }

    return NS_OK;
  }
private:
  // NOTE: nsGenericHTMLFormElement is saved as a nsGenericHTMLElement
  // because AddRef/Release are ambiguous with nsGenericHTMLFormElement
  // and Focus() is declared (and defined) in nsGenericHTMLElement class.
  nsRefPtr<nsGenericHTMLElement> mElement;
};

class nsGenericHTMLElementTearoff : public nsIDOMElementCSSInlineStyle
{
  virtual ~nsGenericHTMLElementTearoff()
  {
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  explicit nsGenericHTMLElementTearoff(nsGenericHTMLElement* aElement)
    : mElement(aElement)
  {
  }

  NS_IMETHOD GetStyle(nsIDOMCSSStyleDeclaration** aStyle) override
  {
    NS_ADDREF(*aStyle = mElement->Style());
    return NS_OK;
  }

  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsGenericHTMLElementTearoff,
                                           nsIDOMElementCSSInlineStyle)

private:
  nsRefPtr<nsGenericHTMLElement> mElement;
};

NS_IMPL_CYCLE_COLLECTION(nsGenericHTMLElementTearoff, mElement)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsGenericHTMLElementTearoff)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsGenericHTMLElementTearoff)

NS_INTERFACE_TABLE_HEAD(nsGenericHTMLElementTearoff)
  NS_INTERFACE_TABLE_INHERITED(nsGenericHTMLElementTearoff,
                               nsIDOMElementCSSInlineStyle)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(nsGenericHTMLElementTearoff)
NS_INTERFACE_MAP_END_AGGREGATED(mElement)

NS_IMPL_ADDREF_INHERITED(nsGenericHTMLElement, nsGenericHTMLElementBase)
NS_IMPL_RELEASE_INHERITED(nsGenericHTMLElement, nsGenericHTMLElementBase)

NS_INTERFACE_MAP_BEGIN(nsGenericHTMLElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIDOMElementCSSInlineStyle,
                                 new nsGenericHTMLElementTearoff(this))
NS_INTERFACE_MAP_END_INHERITING(nsGenericHTMLElementBase)

nsresult
nsGenericHTMLElement::CopyInnerTo(Element* aDst)
{
  nsresult rv;
  int32_t i, count = GetAttrCount();
  for (i = 0; i < count; ++i) {
    const nsAttrName *name = mAttrsAndChildren.AttrNameAt(i);
    const nsAttrValue *value = mAttrsAndChildren.AttrAt(i);

    nsAutoString valStr;
    value->ToString(valStr);

    if (name->Equals(nsGkAtoms::style, kNameSpaceID_None) &&
        value->Type() == nsAttrValue::eCSSStyleRule) {
      // We can't just set this as a string, because that will fail
      // to reparse the string into style data until the node is
      // inserted into the document.  Clone the Rule instead.
      nsRefPtr<mozilla::css::Rule> ruleClone = value->GetCSSStyleRuleValue()->Clone();
      nsRefPtr<mozilla::css::StyleRule> styleRule = do_QueryObject(ruleClone);
      NS_ENSURE_TRUE(styleRule, NS_ERROR_UNEXPECTED);

      rv = aDst->SetInlineStyleRule(styleRule, &valStr, false);
      NS_ENSURE_SUCCESS(rv, rv);

      continue;
    }

    rv = aDst->SetAttr(name->NamespaceID(), name->LocalName(),
                       name->GetPrefix(), valStr, false);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

already_AddRefed<nsDOMStringMap>
nsGenericHTMLElement::Dataset()
{
  nsDOMSlots *slots = DOMSlots();

  if (!slots->mDataset) {
    // mDataset is a weak reference so assignment will not AddRef.
    // AddRef is called before returning the pointer.
    slots->mDataset = new nsDOMStringMap(this);
  }

  nsRefPtr<nsDOMStringMap> ret = slots->mDataset;
  return ret.forget();
}

NS_IMETHODIMP
nsGenericHTMLElement::GetDataset(nsISupports** aDataset)
{
  *aDataset = Dataset().take();
  return NS_OK;
}

nsresult
nsGenericHTMLElement::ClearDataset()
{
  nsDOMSlots *slots = GetExistingDOMSlots();

  NS_ASSERTION(slots && slots->mDataset,
               "Slots should exist and dataset should not be null.");
  slots->mDataset = nullptr;

  return NS_OK;
}

static const nsAttrValue::EnumTable kDirTable[] = {
  { "ltr", eDir_LTR },
  { "rtl", eDir_RTL },
  { "auto", eDir_Auto },
  { 0 }
};

void
nsGenericHTMLElement::GetAccessKeyLabel(nsString& aLabel)
{
  nsAutoString suffix;
  GetAccessKey(suffix);
  if (!suffix.IsEmpty()) {
    EventStateManager::GetAccessKeyLabelPrefix(this, aLabel);
    aLabel.Append(suffix);
  }
}

static bool IS_TABLE_CELL(nsIAtom* frameType) {
  return nsGkAtoms::tableCellFrame == frameType ||
    nsGkAtoms::bcTableCellFrame == frameType;
}

static bool
IsOffsetParent(nsIFrame* aFrame)
{
  nsIAtom* frameType = aFrame->GetType();
  
  if (IS_TABLE_CELL(frameType) || frameType == nsGkAtoms::tableFrame) {
    // Per the IDL for Element, only td, th, and table are acceptable offsetParents
    // apart from body or positioned elements; we need to check the content type as
    // well as the frame type so we ignore anonymous tables created by an element
    // with display: table-cell with no actual table
    nsIContent* content = aFrame->GetContent();

    return content->IsAnyOfHTMLElements(nsGkAtoms::table,
                                        nsGkAtoms::td,
                                        nsGkAtoms::th);
  }
  return false;
}

Element*
nsGenericHTMLElement::GetOffsetRect(CSSIntRect& aRect)
{
  aRect = CSSIntRect();

  nsIFrame* frame = GetStyledFrame();
  if (!frame) {
    return nullptr;
  }

  nsIFrame* parent = frame->GetParent();
  nsPoint origin(0, 0);

  if (parent && parent->GetType() == nsGkAtoms::tableOuterFrame &&
      frame->GetType() == nsGkAtoms::tableFrame) {
    origin = parent->GetPositionIgnoringScrolling();
    parent = parent->GetParent();
  }

  nsIContent* offsetParent = nullptr;
  Element* docElement = GetComposedDoc()->GetRootElement();
  nsIContent* content = frame->GetContent();

  if (content && (content->IsHTMLElement(nsGkAtoms::body) ||
                  content == docElement)) {
    parent = frame;
  }
  else {
    const bool isPositioned = frame->IsAbsPosContaininingBlock();
    const bool isAbsolutelyPositioned = frame->IsAbsolutelyPositioned();
    origin += frame->GetPositionIgnoringScrolling();

    for ( ; parent ; parent = parent->GetParent()) {
      content = parent->GetContent();

      // Stop at the first ancestor that is positioned.
      if (parent->IsAbsPosContaininingBlock()) {
        offsetParent = content;
        break;
      }

      // Add the parent's origin to our own to get to the
      // right coordinate system.
      const bool isOffsetParent = !isPositioned && IsOffsetParent(parent);
      if (!isAbsolutelyPositioned && !isOffsetParent) {
        origin += parent->GetPositionIgnoringScrolling();
      }

      if (content) {
        // If we've hit the document element, break here.
        if (content == docElement) {
          break;
        }

        // Break if the ancestor frame type makes it suitable as offset parent
        // and this element is *not* positioned or if we found the body element.
        if (isOffsetParent || content->IsHTMLElement(nsGkAtoms::body)) {
          offsetParent = content;
          break;
        }
      }
    }

    if (isAbsolutelyPositioned && !offsetParent) {
      // If this element is absolutely positioned, but we don't have
      // an offset parent it means this element is an absolutely
      // positioned child that's not nested inside another positioned
      // element, in this case the element's frame's parent is the
      // frame for the HTML element so we fail to find the body in the
      // parent chain. We want the offset parent in this case to be
      // the body, so we just get the body element from the document.

      nsCOMPtr<nsIDOMHTMLDocument> html_doc(do_QueryInterface(GetComposedDoc()));

      if (html_doc) {
        offsetParent = static_cast<nsHTMLDocument*>(html_doc.get())->GetBody();
      }
    }
  }

  // Subtract the parent border unless it uses border-box sizing.
  if (parent &&
      parent->StylePosition()->mBoxSizing != NS_STYLE_BOX_SIZING_BORDER) {
    const nsStyleBorder* border = parent->StyleBorder();
    origin.x -= border->GetComputedBorderWidth(NS_SIDE_LEFT);
    origin.y -= border->GetComputedBorderWidth(NS_SIDE_TOP);
  }

  // XXX We should really consider subtracting out padding for
  // content-box sizing, but we should see what IE does....

  // Get the union of all rectangles in this and continuation frames.
  // It doesn't really matter what we use as aRelativeTo here, since
  // we only care about the size. We just have to use something non-null.
  nsRect rcFrame = nsLayoutUtils::GetAllInFlowRectsUnion(frame, frame);
  rcFrame.MoveTo(origin);
  aRect = CSSIntRect::FromAppUnitsRounded(rcFrame);

  return offsetParent ? offsetParent->AsElement() : nullptr;
}

NS_IMETHODIMP
nsGenericHTMLElement::InsertAdjacentHTML(const nsAString& aPosition,
                                         const nsAString& aText)
{
  ErrorResult rv;
  Element::InsertAdjacentHTML(aPosition, aText, rv);
  return rv.StealNSResult();
}

bool
nsGenericHTMLElement::Spellcheck()
{
  // Has the state has been explicitly set?
  nsIContent* node;
  for (node = this; node; node = node->GetParent()) {
    if (node->IsHTMLElement()) {
      static nsIContent::AttrValuesArray strings[] =
        {&nsGkAtoms::_true, &nsGkAtoms::_false, nullptr};
      switch (node->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::spellcheck,
                                    strings, eCaseMatters)) {
        case 0:                         // spellcheck = "true"
          return true;
        case 1:                         // spellcheck = "false"
          return false;
      }
    }
  }

  // contenteditable/designMode are spellchecked by default
  if (IsEditable()) {
    return true;
  }

  // Is this a chrome element?
  if (nsContentUtils::IsChromeDoc(OwnerDoc())) {
    return false;                       // Not spellchecked by default
  }

  // Anything else that's not a form control is not spellchecked by default
  nsCOMPtr<nsIFormControl> formControl = do_QueryObject(this);
  if (!formControl) {
    return false;                       // Not spellchecked by default
  }

  // Is this a multiline plaintext input?
  int32_t controlType = formControl->GetType();
  if (controlType == NS_FORM_TEXTAREA) {
    return true;             // Spellchecked by default
  }

  // Is this anything other than an input text?
  // Other inputs are not spellchecked.
  if (controlType != NS_FORM_INPUT_TEXT) {
    return false;                       // Not spellchecked by default
  }

  // Does the user want input text spellchecked by default?
  // NOTE: Do not reflect a pref value of 0 back to the DOM getter.
  // The web page should not know if the user has disabled spellchecking.
  // We'll catch this in the editor itself.
  int32_t spellcheckLevel = Preferences::GetInt("layout.spellcheckDefault", 1);
  return spellcheckLevel == 2;           // "Spellcheck multi- and single-line"
}

bool
nsGenericHTMLElement::InNavQuirksMode(nsIDocument* aDoc)
{
  return aDoc && aDoc->GetCompatibilityMode() == eCompatibility_NavQuirks;
}

void
nsGenericHTMLElement::UpdateEditableState(bool aNotify)
{
  // XXX Should we do this only when in a document?
  ContentEditableTristate value = GetContentEditableValue();
  if (value != eInherit) {
    DoSetEditableFlag(!!value, aNotify);
    return;
  }

  nsStyledElement::UpdateEditableState(aNotify);
}

EventStates
nsGenericHTMLElement::IntrinsicState() const
{
  EventStates state = nsGenericHTMLElementBase::IntrinsicState();

  if (GetDirectionality() == eDir_RTL) {
    state |= NS_EVENT_STATE_RTL;
    state &= ~NS_EVENT_STATE_LTR;
  } else { // at least for HTML, directionality is exclusively LTR or RTL
    NS_ASSERTION(GetDirectionality() == eDir_LTR,
                 "HTML element's directionality must be either RTL or LTR");
    state |= NS_EVENT_STATE_LTR;
    state &= ~NS_EVENT_STATE_RTL;
  }

  return state;
}

uint32_t
nsGenericHTMLElement::EditableInclusiveDescendantCount()
{
  bool isEditable = IsInUncomposedDoc() && HasFlag(NODE_IS_EDITABLE) &&
    GetContentEditableValue() == eTrue;
  return EditableDescendantCount() + isEditable;
}

nsresult
nsGenericHTMLElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                                 nsIContent* aBindingParent,
                                 bool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLElementBase::BindToTree(aDocument, aParent,
                                                     aBindingParent,
                                                     aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aDocument) {
    if (HasProperties()) {
      HTMLPropertiesCollection* properties = 
        static_cast<HTMLPropertiesCollection*>(GetProperty(nsGkAtoms::microdataProperties));
      if (properties) {
        properties->SetDocument(aDocument);
      }
    }
    RegAccessKey();
    if (HasName()) {
      aDocument->
        AddToNameTable(this, GetParsedAttr(nsGkAtoms::name)->GetAtomValue());
    }

    if (HasFlag(NODE_IS_EDITABLE) && GetContentEditableValue() == eTrue) {
      nsCOMPtr<nsIHTMLDocument> htmlDocument = do_QueryInterface(aDocument);
      if (htmlDocument) {
        htmlDocument->ChangeContentEditableCount(this, +1);
      }
    }
  }

  return rv;
}

void
nsGenericHTMLElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  if (IsInDoc()) {
    UnregAccessKey();
  }
  
  if(HasProperties()) {
    HTMLPropertiesCollection* properties = 
      static_cast<HTMLPropertiesCollection*>(GetProperty(nsGkAtoms::microdataProperties));
    if (properties) {
      properties->SetDocument(nullptr);
    }
  }

  RemoveFromNameTable();

  if (GetContentEditableValue() == eTrue) {
    //XXXsmaug Fix this for Shadow DOM, bug 1066965.
    nsCOMPtr<nsIHTMLDocument> htmlDocument = do_QueryInterface(GetUncomposedDoc());
    if (htmlDocument) {
      htmlDocument->ChangeContentEditableCount(this, -1);
    }
  }

  nsStyledElement::UnbindFromTree(aDeep, aNullParent);
}

HTMLFormElement*
nsGenericHTMLElement::FindAncestorForm(HTMLFormElement* aCurrentForm)
{
  NS_ASSERTION(!HasAttr(kNameSpaceID_None, nsGkAtoms::form) ||
               IsHTMLElement(nsGkAtoms::img),
               "FindAncestorForm should not be called if @form is set!");

  // Make sure we don't end up finding a form that's anonymous from
  // our point of view.
  nsIContent* bindingParent = GetBindingParent();

  nsIContent* content = this;
  while (content != bindingParent && content) {
    // If the current ancestor is a form, return it as our form
    if (content->IsHTMLElement(nsGkAtoms::form)) {
#ifdef DEBUG
      if (!nsContentUtils::IsInSameAnonymousTree(this, content)) {
        // It's possible that we started unbinding at |content| or
        // some ancestor of it, and |content| and |this| used to all be
        // anonymous.  Check for this the hard way.
        for (nsIContent* child = this; child != content;
             child = child->GetParent()) {
          NS_ASSERTION(child->GetParent()->IndexOf(child) != -1,
                       "Walked too far?");
        }
      }
#endif
      return static_cast<HTMLFormElement*>(content);
    }

    nsIContent *prevContent = content;
    content = prevContent->GetParent();

    if (!content && aCurrentForm) {
      // We got to the root of the subtree we're in, and we're being removed
      // from the DOM (the only time we get into this method with a non-null
      // aCurrentForm).  Check whether aCurrentForm is in the same subtree.  If
      // it is, we want to return aCurrentForm, since this case means that
      // we're one of those inputs-in-a-table that have a hacked mForm pointer
      // and a subtree containing both us and the form got removed from the
      // DOM.
      if (nsContentUtils::ContentIsDescendantOf(aCurrentForm, prevContent)) {
        return aCurrentForm;
      }
    }
  }

  return nullptr;
}

bool
nsGenericHTMLElement::CheckHandleEventForAnchorsPreconditions(
                        EventChainVisitor& aVisitor)
{
  NS_PRECONDITION(nsCOMPtr<Link>(do_QueryObject(this)),
                  "should be called only when |this| implements |Link|");

  if (!aVisitor.mPresContext) {
    // We need a pres context to do link stuff. Some events (e.g. mutation
    // events) don't have one.
    // XXX: ideally, shouldn't we be able to do what we need without one?
    return false; 
  }

  //Need to check if we hit an imagemap area and if so see if we're handling
  //the event on that map or on a link farther up the tree.  If we're on a
  //link farther up, do nothing.
  nsCOMPtr<nsIContent> target = aVisitor.mPresContext->EventStateManager()->
    GetEventTargetContent(aVisitor.mEvent);

  return !target || !target->IsHTMLElement(nsGkAtoms::area) ||
         IsHTMLElement(nsGkAtoms::area);
}

nsresult
nsGenericHTMLElement::PreHandleEventForAnchors(EventChainPreVisitor& aVisitor)
{
  nsresult rv = nsGenericHTMLElementBase::PreHandleEvent(aVisitor);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!CheckHandleEventForAnchorsPreconditions(aVisitor)) {
    return NS_OK;
  }

  return PreHandleEventForLinks(aVisitor);
}

nsresult
nsGenericHTMLElement::PostHandleEventForAnchors(EventChainPostVisitor& aVisitor)
{
  if (!CheckHandleEventForAnchorsPreconditions(aVisitor)) {
    return NS_OK;
  }

  return PostHandleEventForLinks(aVisitor);
}

bool
nsGenericHTMLElement::IsHTMLLink(nsIURI** aURI) const
{
  NS_PRECONDITION(aURI, "Must provide aURI out param");

  *aURI = GetHrefURIForAnchors().take();
  // We promise out param is non-null if we return true, so base rv on it
  return *aURI != nullptr;
}

already_AddRefed<nsIURI>
nsGenericHTMLElement::GetHrefURIForAnchors() const
{
  // This is used by the three Link implementations and
  // nsHTMLStyleElement.

  // Get href= attribute (relative URI).

  // We use the nsAttrValue's copy of the URI string to avoid copying.
  nsCOMPtr<nsIURI> uri;
  GetURIAttr(nsGkAtoms::href, nullptr, getter_AddRefs(uri));

  return uri.forget();
}

nsresult
nsGenericHTMLElement::AfterSetAttr(int32_t aNamespaceID, nsIAtom* aName,
                                   const nsAttrValue* aValue, bool aNotify)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (IsEventAttributeName(aName) && aValue) {
      MOZ_ASSERT(aValue->Type() == nsAttrValue::eString,
                 "Expected string value for script body");
      nsresult rv = SetEventHandler(aName, aValue->GetStringValue());
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else if (aNotify && aName == nsGkAtoms::spellcheck) {
      SyncEditorsOnSubtree(this);
    }
    else if (aName == nsGkAtoms::dir) {
      Directionality dir = eDir_LTR;
      if (aValue && aValue->Type() == nsAttrValue::eEnum) {
        SetHasValidDir();
        Directionality dirValue = (Directionality)aValue->GetEnumValue();
        if (dirValue == eDir_Auto) {
          SetHasDirAuto();
          ClearHasFixedDir();
        } else {
          dir = dirValue;
          SetDirectionality(dir, aNotify);
          ClearHasDirAuto();
          SetHasFixedDir();
        }
      } else {
        ClearHasValidDir();
        ClearHasFixedDir();
        if (NodeInfo()->Equals(nsGkAtoms::bdi)) {
          SetHasDirAuto();
        } else {
          ClearHasDirAuto();
          dir = RecomputeDirectionality(this, aNotify);
        }
      }
      SetDirectionalityOnDescendants(this, dir, aNotify);
    }
  }

  return nsGenericHTMLElementBase::AfterSetAttr(aNamespaceID, aName,
                                                aValue, aNotify);
}

EventListenerManager*
nsGenericHTMLElement::GetEventListenerManagerForAttr(nsIAtom* aAttrName,
                                                     bool* aDefer)
{
  // Attributes on the body and frameset tags get set on the global object
  if ((mNodeInfo->Equals(nsGkAtoms::body) ||
       mNodeInfo->Equals(nsGkAtoms::frameset)) &&
      // We only forward some event attributes from body/frameset to window
      (0
#define EVENT(name_, id_, type_, struct_) /* nothing */
#define FORWARDED_EVENT(name_, id_, type_, struct_) \
       || nsGkAtoms::on##name_ == aAttrName
#define WINDOW_EVENT FORWARDED_EVENT
#include "mozilla/EventNameList.h" // IWYU pragma: keep
#undef WINDOW_EVENT
#undef FORWARDED_EVENT
#undef EVENT
       )
      ) {
    nsPIDOMWindow *win;

    // If we have a document, and it has a window, add the event
    // listener on the window (the inner window). If not, proceed as
    // normal.
    // XXXbz sXBL/XBL2 issue: should we instead use GetComposedDoc() here,
    // override BindToTree for those classes and munge event listeners there?
    nsIDocument *document = OwnerDoc();

    *aDefer = false;
    if ((win = document->GetInnerWindow())) {
      nsCOMPtr<EventTarget> piTarget(do_QueryInterface(win));

      return piTarget->GetOrCreateListenerManager();
    }

    return nullptr;
  }

  return nsGenericHTMLElementBase::GetEventListenerManagerForAttr(aAttrName,
                                                                  aDefer);
}

#define EVENT(name_, id_, type_, struct_) /* nothing; handled by nsINode */
#define FORWARDED_EVENT(name_, id_, type_, struct_)                           \
EventHandlerNonNull*                                                          \
nsGenericHTMLElement::GetOn##name_()                                          \
{                                                                             \
  if (IsAnyOfHTMLElements(nsGkAtoms::body, nsGkAtoms::frameset)) {            \
    /* XXXbz note to self: add tests for this! */                             \
    nsPIDOMWindow* win = OwnerDoc()->GetInnerWindow();                        \
    if (win) {                                                                \
      nsCOMPtr<nsISupports> supports = do_QueryInterface(win);                \
      nsGlobalWindow* globalWin = nsGlobalWindow::FromSupports(supports);     \
      return globalWin->GetOn##name_();                                       \
    }                                                                         \
    return nullptr;                                                           \
  }                                                                           \
                                                                              \
  return nsINode::GetOn##name_();                                             \
}                                                                             \
void                                                                          \
nsGenericHTMLElement::SetOn##name_(EventHandlerNonNull* handler)              \
{                                                                             \
  if (IsAnyOfHTMLElements(nsGkAtoms::body, nsGkAtoms::frameset)) {            \
    nsPIDOMWindow* win = OwnerDoc()->GetInnerWindow();                        \
    if (!win) {                                                               \
      return;                                                                 \
    }                                                                         \
                                                                              \
    nsCOMPtr<nsISupports> supports = do_QueryInterface(win);                  \
    nsGlobalWindow* globalWin = nsGlobalWindow::FromSupports(supports);       \
    return globalWin->SetOn##name_(handler);                                  \
  }                                                                           \
                                                                              \
  return nsINode::SetOn##name_(handler);                                      \
}
#define ERROR_EVENT(name_, id_, type_, struct_)                               \
already_AddRefed<EventHandlerNonNull>                                         \
nsGenericHTMLElement::GetOn##name_()                                          \
{                                                                             \
  if (IsAnyOfHTMLElements(nsGkAtoms::body, nsGkAtoms::frameset)) {            \
    /* XXXbz note to self: add tests for this! */                             \
    nsPIDOMWindow* win = OwnerDoc()->GetInnerWindow();                        \
    if (win) {                                                                \
      nsCOMPtr<nsISupports> supports = do_QueryInterface(win);                \
      nsGlobalWindow* globalWin = nsGlobalWindow::FromSupports(supports);     \
      OnErrorEventHandlerNonNull* errorHandler = globalWin->GetOn##name_();   \
      if (errorHandler) {                                                     \
        nsRefPtr<EventHandlerNonNull> handler =                               \
          new EventHandlerNonNull(errorHandler);                              \
        return handler.forget();                                              \
      }                                                                       \
    }                                                                         \
    return nullptr;                                                           \
  }                                                                           \
                                                                              \
  nsRefPtr<EventHandlerNonNull> handler = nsINode::GetOn##name_();            \
  return handler.forget();                                                    \
}                                                                             \
void                                                                          \
nsGenericHTMLElement::SetOn##name_(EventHandlerNonNull* handler)              \
{                                                                             \
  if (IsAnyOfHTMLElements(nsGkAtoms::body, nsGkAtoms::frameset)) {            \
    nsPIDOMWindow* win = OwnerDoc()->GetInnerWindow();                        \
    if (!win) {                                                               \
      return;                                                                 \
    }                                                                         \
                                                                              \
    nsCOMPtr<nsISupports> supports = do_QueryInterface(win);                  \
    nsGlobalWindow* globalWin = nsGlobalWindow::FromSupports(supports);       \
    nsRefPtr<OnErrorEventHandlerNonNull> errorHandler;                        \
    if (handler) {                                                            \
      errorHandler = new OnErrorEventHandlerNonNull(handler);                 \
    }                                                                         \
    return globalWin->SetOn##name_(errorHandler);                             \
  }                                                                           \
                                                                              \
  return nsINode::SetOn##name_(handler);                                      \
}
#include "mozilla/EventNameList.h" // IWYU pragma: keep
#undef ERROR_EVENT
#undef FORWARDED_EVENT
#undef EVENT

nsresult
nsGenericHTMLElement::SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                              nsIAtom* aPrefix, const nsAString& aValue,
                              bool aNotify)
{
  bool contentEditable = aNameSpaceID == kNameSpaceID_None &&
                           aName == nsGkAtoms::contenteditable;
  bool undoScope = aNameSpaceID == kNameSpaceID_None &&
                           aName == nsGkAtoms::undoscope;
  bool accessKey = aName == nsGkAtoms::accesskey && 
                     aNameSpaceID == kNameSpaceID_None;

  int32_t change = 0;
  if (contentEditable) {
    change = GetContentEditableValue() == eTrue ? -1 : 0;
    SetMayHaveContentEditableAttr();
  }

  if (accessKey) {
    UnregAccessKey();
  }

  nsresult rv = nsStyledElement::SetAttr(aNameSpaceID, aName, aPrefix, aValue,
                                         aNotify);
  NS_ENSURE_SUCCESS(rv, rv);

  if (contentEditable) {
    if (aValue.IsEmpty() || aValue.LowerCaseEqualsLiteral("true")) {
      change += 1;
    }

    ChangeEditableState(change);
  }

  if (undoScope) {
    rv = SetUndoScopeInternal(true);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (accessKey && !aValue.IsEmpty()) {
    SetFlags(NODE_HAS_ACCESSKEY);
    RegAccessKey();
  }

  return NS_OK;
}

nsresult
nsGenericHTMLElement::UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                                bool aNotify)
{
  bool contentEditable = false;
  int32_t contentEditableChange = 0;

  // Check for event handlers
  if (aNameSpaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::name) {
      // Have to do this before clearing flag. See RemoveFromNameTable
      RemoveFromNameTable();
      ClearHasName();
    }
    else if (aAttribute == nsGkAtoms::contenteditable) {
      contentEditable = true;
      contentEditableChange = GetContentEditableValue() == eTrue ? -1 : 0;
    }
    else if (aAttribute == nsGkAtoms::undoscope) {
      nsresult rv = SetUndoScopeInternal(false);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else if (aAttribute == nsGkAtoms::accesskey) {
      // Have to unregister before clearing flag. See UnregAccessKey
      UnregAccessKey();
      UnsetFlags(NODE_HAS_ACCESSKEY);
    }
    else if (IsEventAttributeName(aAttribute)) {
      if (EventListenerManager* manager = GetExistingListenerManager()) {
        manager->RemoveEventHandler(aAttribute, EmptyString());
      }
    }
  }

  nsresult rv = nsGenericHTMLElementBase::UnsetAttr(aNameSpaceID, aAttribute,
                                                    aNotify);
  NS_ENSURE_SUCCESS(rv, rv);

  if (contentEditable) {
    ChangeEditableState(contentEditableChange);
  }

  return NS_OK;
}

void
nsGenericHTMLElement::GetBaseTarget(nsAString& aBaseTarget) const
{
  OwnerDoc()->GetBaseTarget(aBaseTarget);
}

//----------------------------------------------------------------------

bool
nsGenericHTMLElement::ParseAttribute(int32_t aNamespaceID,
                                     nsIAtom* aAttribute,
                                     const nsAString& aValue,
                                     nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::dir) {
      return aResult.ParseEnumValue(aValue, kDirTable, false);
    }
  
    if (aAttribute == nsGkAtoms::tabindex) {
      return aResult.ParseIntValue(aValue);
    }

    if (aAttribute == nsGkAtoms::referrer) {
      return ParseReferrerAttribute(aValue, aResult);
    }

    if (aAttribute == nsGkAtoms::name) {
      // Store name as an atom.  name="" means that the element has no name,
      // not that it has an emptystring as the name.
      RemoveFromNameTable();
      if (aValue.IsEmpty()) {
        ClearHasName();
        return false;
      }

      aResult.ParseAtom(aValue);

      if (CanHaveName(NodeInfo()->NameAtom())) {
        SetHasName();
        AddToNameTable(aResult.GetAtomValue());
      }
      
      return true;
    }

    if (aAttribute == nsGkAtoms::contenteditable) {
      aResult.ParseAtom(aValue);
      return true;
    }

    if (aAttribute == nsGkAtoms::itemref ||
        aAttribute == nsGkAtoms::itemprop ||
        aAttribute == nsGkAtoms::itemtype ||
        aAttribute == nsGkAtoms::rel) {
      aResult.ParseAtomArray(aValue);
      return true;
    }
  }

  return nsGenericHTMLElementBase::ParseAttribute(aNamespaceID, aAttribute,
                                                  aValue, aResult);
}

bool
nsGenericHTMLElement::ParseBackgroundAttribute(int32_t aNamespaceID,
                                               nsIAtom* aAttribute,
                                               const nsAString& aValue,
                                               nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None &&
      aAttribute == nsGkAtoms::background &&
      !aValue.IsEmpty()) {
    // Resolve url to an absolute url
    nsIDocument* doc = OwnerDoc();
    nsCOMPtr<nsIURI> baseURI = GetBaseURI();
    nsCOMPtr<nsIURI> uri;
    nsresult rv = nsContentUtils::NewURIWithDocumentCharset(
        getter_AddRefs(uri), aValue, doc, baseURI);
    if (NS_FAILED(rv)) {
      return false;
    }

    nsString value(aValue);
    nsRefPtr<nsStringBuffer> buffer = nsCSSValue::BufferFromString(value);
    if (MOZ_UNLIKELY(!buffer)) {
      return false;
    }

    mozilla::css::URLValue *url =
      new mozilla::css::URLValue(uri, buffer, doc->GetDocumentURI(),
                                 NodePrincipal());
    aResult.SetTo(url, &aValue);
    return true;
  }

  return false;
}

bool
nsGenericHTMLElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry* const map[] = {
    sCommonAttributeMap
  };
  
  return FindAttributeDependence(aAttribute, map);
}

nsMapRuleToAttributesFunc
nsGenericHTMLElement::GetAttributeMappingFunction() const
{
  return &MapCommonAttributesInto;
}

nsIFormControlFrame*
nsGenericHTMLElement::GetFormControlFrame(bool aFlushFrames)
{
  if (aFlushFrames && IsInComposedDoc()) {
    // Cause a flush of the frames, so we get up-to-date frame information
    GetComposedDoc()->FlushPendingNotifications(Flush_Frames);
  }
  nsIFrame* frame = GetPrimaryFrame();
  if (frame) {
    nsIFormControlFrame* form_frame = do_QueryFrame(frame);
    if (form_frame) {
      return form_frame;
    }

    // If we have generated content, the primary frame will be a
    // wrapper frame..  out real frame will be in its child list.
    for (frame = frame->GetFirstPrincipalChild();
         frame;
         frame = frame->GetNextSibling()) {
      form_frame = do_QueryFrame(frame);
      if (form_frame) {
        return form_frame;
      }
    }
  }

  return nullptr;
}

nsPresContext*
nsGenericHTMLElement::GetPresContext(PresContextFor aFor)
{
  // Get the document
  nsIDocument* doc = (aFor == eForComposedDoc) ?
    GetComposedDoc() : GetUncomposedDoc();
  if (doc) {
    // Get presentation shell.
    nsIPresShell *presShell = doc->GetShell();
    if (presShell) {
      return presShell->GetPresContext();
    }
  }

  return nullptr;
}

static const nsAttrValue::EnumTable kDivAlignTable[] = {
  { "left", NS_STYLE_TEXT_ALIGN_MOZ_LEFT },
  { "right", NS_STYLE_TEXT_ALIGN_MOZ_RIGHT },
  { "center", NS_STYLE_TEXT_ALIGN_MOZ_CENTER },
  { "middle", NS_STYLE_TEXT_ALIGN_MOZ_CENTER },
  { "justify", NS_STYLE_TEXT_ALIGN_JUSTIFY },
  { 0 }
};

static const nsAttrValue::EnumTable kFrameborderTable[] = {
  { "yes", NS_STYLE_FRAME_YES },
  { "no", NS_STYLE_FRAME_NO },
  { "1", NS_STYLE_FRAME_1 },
  { "0", NS_STYLE_FRAME_0 },
  { 0 }
};

static const nsAttrValue::EnumTable kScrollingTable[] = {
  { "yes", NS_STYLE_FRAME_YES },
  { "no", NS_STYLE_FRAME_NO },
  { "on", NS_STYLE_FRAME_ON },
  { "off", NS_STYLE_FRAME_OFF },
  { "scroll", NS_STYLE_FRAME_SCROLL },
  { "noscroll", NS_STYLE_FRAME_NOSCROLL },
  { "auto", NS_STYLE_FRAME_AUTO },
  { 0 }
};

static const nsAttrValue::EnumTable kTableVAlignTable[] = {
  { "top",     NS_STYLE_VERTICAL_ALIGN_TOP },
  { "middle",  NS_STYLE_VERTICAL_ALIGN_MIDDLE },
  { "bottom",  NS_STYLE_VERTICAL_ALIGN_BOTTOM },
  { "baseline",NS_STYLE_VERTICAL_ALIGN_BASELINE },
  { 0 }
};

bool
nsGenericHTMLElement::ParseAlignValue(const nsAString& aString,
                                      nsAttrValue& aResult)
{
  static const nsAttrValue::EnumTable kAlignTable[] = {
    { "left",      NS_STYLE_TEXT_ALIGN_LEFT },
    { "right",     NS_STYLE_TEXT_ALIGN_RIGHT },

    { "top",       NS_STYLE_VERTICAL_ALIGN_TOP },
    { "middle",    NS_STYLE_VERTICAL_ALIGN_MIDDLE_WITH_BASELINE },
    { "bottom",    NS_STYLE_VERTICAL_ALIGN_BASELINE },

    { "center",    NS_STYLE_VERTICAL_ALIGN_MIDDLE_WITH_BASELINE },
    { "baseline",  NS_STYLE_VERTICAL_ALIGN_BASELINE },

    { "texttop",   NS_STYLE_VERTICAL_ALIGN_TEXT_TOP },
    { "absmiddle", NS_STYLE_VERTICAL_ALIGN_MIDDLE },
    { "abscenter", NS_STYLE_VERTICAL_ALIGN_MIDDLE },
    { "absbottom", NS_STYLE_VERTICAL_ALIGN_BOTTOM },
    { 0 }
  };

  return aResult.ParseEnumValue(aString, kAlignTable, false);
}

//----------------------------------------

static const nsAttrValue::EnumTable kTableHAlignTable[] = {
  { "left",   NS_STYLE_TEXT_ALIGN_LEFT },
  { "right",  NS_STYLE_TEXT_ALIGN_RIGHT },
  { "center", NS_STYLE_TEXT_ALIGN_CENTER },
  { "char",   NS_STYLE_TEXT_ALIGN_CHAR },
  { "justify",NS_STYLE_TEXT_ALIGN_JUSTIFY },
  { 0 }
};

bool
nsGenericHTMLElement::ParseTableHAlignValue(const nsAString& aString,
                                            nsAttrValue& aResult)
{
  return aResult.ParseEnumValue(aString, kTableHAlignTable, false);
}

//----------------------------------------

// This table is used for td, th, tr, col, thead, tbody and tfoot.
static const nsAttrValue::EnumTable kTableCellHAlignTable[] = {
  { "left",   NS_STYLE_TEXT_ALIGN_MOZ_LEFT },
  { "right",  NS_STYLE_TEXT_ALIGN_MOZ_RIGHT },
  { "center", NS_STYLE_TEXT_ALIGN_MOZ_CENTER },
  { "char",   NS_STYLE_TEXT_ALIGN_CHAR },
  { "justify",NS_STYLE_TEXT_ALIGN_JUSTIFY },
  { "middle", NS_STYLE_TEXT_ALIGN_MOZ_CENTER },
  { "absmiddle", NS_STYLE_TEXT_ALIGN_CENTER },
  { 0 }
};

bool
nsGenericHTMLElement::ParseTableCellHAlignValue(const nsAString& aString,
                                                nsAttrValue& aResult)
{
  return aResult.ParseEnumValue(aString, kTableCellHAlignTable, false);
}

//----------------------------------------

bool
nsGenericHTMLElement::ParseTableVAlignValue(const nsAString& aString,
                                            nsAttrValue& aResult)
{
  return aResult.ParseEnumValue(aString, kTableVAlignTable, false);
}

bool
nsGenericHTMLElement::ParseDivAlignValue(const nsAString& aString,
                                         nsAttrValue& aResult)
{
  return aResult.ParseEnumValue(aString, kDivAlignTable, false);
}

bool
nsGenericHTMLElement::ParseImageAttribute(nsIAtom* aAttribute,
                                          const nsAString& aString,
                                          nsAttrValue& aResult)
{
  if ((aAttribute == nsGkAtoms::width) ||
      (aAttribute == nsGkAtoms::height)) {
    return aResult.ParseSpecialIntValue(aString);
  }
  if ((aAttribute == nsGkAtoms::hspace) ||
      (aAttribute == nsGkAtoms::vspace) ||
      (aAttribute == nsGkAtoms::border)) {
    return aResult.ParseIntWithBounds(aString, 0);
  }
  return false;
}

bool
nsGenericHTMLElement::ParseReferrerAttribute(const nsAString& aString,
                                             nsAttrValue& aResult)
{
  static const nsAttrValue::EnumTable kReferrerTable[] = {
    { "no-referrer", net::RP_No_Referrer },
    { "origin", net::RP_Origin },
    { "unsafe-url", net::RP_Unsafe_URL },
    { 0 }
  };
  return aResult.ParseEnumValue(aString, kReferrerTable, false);
}

bool
nsGenericHTMLElement::ParseFrameborderValue(const nsAString& aString,
                                            nsAttrValue& aResult)
{
  return aResult.ParseEnumValue(aString, kFrameborderTable, false);
}

bool
nsGenericHTMLElement::ParseScrollingValue(const nsAString& aString,
                                          nsAttrValue& aResult)
{
  return aResult.ParseEnumValue(aString, kScrollingTable, false);
}

/**
 * Handle attributes common to all html elements
 */
void
nsGenericHTMLElement::MapCommonAttributesIntoExceptHidden(const nsMappedAttributes* aAttributes,
                                                          nsRuleData* aData)
{
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(UserInterface)) {
    nsCSSValue* userModify = aData->ValueForUserModify();
    if (userModify->GetUnit() == eCSSUnit_Null) {
      const nsAttrValue* value =
        aAttributes->GetAttr(nsGkAtoms::contenteditable);
      if (value) {
        if (value->Equals(nsGkAtoms::_empty, eCaseMatters) ||
            value->Equals(nsGkAtoms::_true, eIgnoreCase)) {
          userModify->SetIntValue(NS_STYLE_USER_MODIFY_READ_WRITE,
                                  eCSSUnit_Enumerated);
        }
        else if (value->Equals(nsGkAtoms::_false, eIgnoreCase)) {
            userModify->SetIntValue(NS_STYLE_USER_MODIFY_READ_ONLY,
                                    eCSSUnit_Enumerated);
        }
      }
    }
  }

  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Font)) {
    nsCSSValue* lang = aData->ValueForLang();
    if (lang->GetUnit() == eCSSUnit_Null) {
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::lang);
      if (value && value->Type() == nsAttrValue::eString) {
        lang->SetStringValue(value->GetStringValue(), eCSSUnit_Ident);
      }
    }
  }
}

void
nsGenericHTMLElement::MapCommonAttributesInto(const nsMappedAttributes* aAttributes,
                                              nsRuleData* aData)
{
  MapCommonAttributesIntoExceptHidden(aAttributes, aData);

  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Display)) {
    nsCSSValue* display = aData->ValueForDisplay();
    if (display->GetUnit() == eCSSUnit_Null) {
      if (aAttributes->IndexOfAttr(nsGkAtoms::hidden) >= 0) {
        display->SetIntValue(NS_STYLE_DISPLAY_NONE, eCSSUnit_Enumerated);
      }
    }
  }
}

/* static */ const nsGenericHTMLElement::MappedAttributeEntry
nsGenericHTMLElement::sCommonAttributeMap[] = {
  { &nsGkAtoms::contenteditable },
  { &nsGkAtoms::lang },
  { &nsGkAtoms::hidden },
  { nullptr }
};

/* static */ const Element::MappedAttributeEntry
nsGenericHTMLElement::sImageMarginSizeAttributeMap[] = {
  { &nsGkAtoms::width },
  { &nsGkAtoms::height },
  { &nsGkAtoms::hspace },
  { &nsGkAtoms::vspace },
  { nullptr }
};

/* static */ const Element::MappedAttributeEntry
nsGenericHTMLElement::sImageAlignAttributeMap[] = {
  { &nsGkAtoms::align },
  { nullptr }
};

/* static */ const Element::MappedAttributeEntry
nsGenericHTMLElement::sDivAlignAttributeMap[] = {
  { &nsGkAtoms::align },
  { nullptr }
};

/* static */ const Element::MappedAttributeEntry
nsGenericHTMLElement::sImageBorderAttributeMap[] = {
  { &nsGkAtoms::border },
  { nullptr }
};

/* static */ const Element::MappedAttributeEntry
nsGenericHTMLElement::sBackgroundAttributeMap[] = {
  { &nsGkAtoms::background },
  { &nsGkAtoms::bgcolor },
  { nullptr }
};

/* static */ const Element::MappedAttributeEntry
nsGenericHTMLElement::sBackgroundColorAttributeMap[] = {
  { &nsGkAtoms::bgcolor },
  { nullptr }
};

void
nsGenericHTMLElement::MapImageAlignAttributeInto(const nsMappedAttributes* aAttributes,
                                                 nsRuleData* aRuleData)
{
  if (aRuleData->mSIDs & (NS_STYLE_INHERIT_BIT(Display) |
                          NS_STYLE_INHERIT_BIT(TextReset))) {
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::align);
    if (value && value->Type() == nsAttrValue::eEnum) {
      int32_t align = value->GetEnumValue();
      if (aRuleData->mSIDs & NS_STYLE_INHERIT_BIT(Display)) {
        nsCSSValue* cssFloat = aRuleData->ValueForFloat();
        if (cssFloat->GetUnit() == eCSSUnit_Null) {
          if (align == NS_STYLE_TEXT_ALIGN_LEFT) {
            cssFloat->SetIntValue(NS_STYLE_FLOAT_LEFT, eCSSUnit_Enumerated);
          } else if (align == NS_STYLE_TEXT_ALIGN_RIGHT) {
            cssFloat->SetIntValue(NS_STYLE_FLOAT_RIGHT, eCSSUnit_Enumerated);
          }
        }
      }
      if (aRuleData->mSIDs & NS_STYLE_INHERIT_BIT(TextReset)) {
        nsCSSValue* verticalAlign = aRuleData->ValueForVerticalAlign();
        if (verticalAlign->GetUnit() == eCSSUnit_Null) {
          switch (align) {
          case NS_STYLE_TEXT_ALIGN_LEFT:
          case NS_STYLE_TEXT_ALIGN_RIGHT:
            break;
          default:
            verticalAlign->SetIntValue(align, eCSSUnit_Enumerated);
            break;
          }
        }
      }
    }
  }
}

void
nsGenericHTMLElement::MapDivAlignAttributeInto(const nsMappedAttributes* aAttributes,
                                               nsRuleData* aRuleData)
{
  if (aRuleData->mSIDs & NS_STYLE_INHERIT_BIT(Text)) {
    nsCSSValue* textAlign = aRuleData->ValueForTextAlign();
    if (textAlign->GetUnit() == eCSSUnit_Null) {
      // align: enum
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::align);
      if (value && value->Type() == nsAttrValue::eEnum)
        textAlign->SetIntValue(value->GetEnumValue(), eCSSUnit_Enumerated);
    }
  }
}


void
nsGenericHTMLElement::MapImageMarginAttributeInto(const nsMappedAttributes* aAttributes,
                                                  nsRuleData* aData)
{
  if (!(aData->mSIDs & NS_STYLE_INHERIT_BIT(Margin)))
    return;

  const nsAttrValue* value;

  // hspace: value
  value = aAttributes->GetAttr(nsGkAtoms::hspace);
  if (value) {
    nsCSSValue hval;
    if (value->Type() == nsAttrValue::eInteger)
      hval.SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Pixel);
    else if (value->Type() == nsAttrValue::ePercent)
      hval.SetPercentValue(value->GetPercentValue());

    if (hval.GetUnit() != eCSSUnit_Null) {
      nsCSSValue* left = aData->ValueForMarginLeft();
      if (left->GetUnit() == eCSSUnit_Null)
        *left = hval;
      nsCSSValue* right = aData->ValueForMarginRight();
      if (right->GetUnit() == eCSSUnit_Null)
        *right = hval;
    }
  }

  // vspace: value
  value = aAttributes->GetAttr(nsGkAtoms::vspace);
  if (value) {
    nsCSSValue vval;
    if (value->Type() == nsAttrValue::eInteger)
      vval.SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Pixel);
    else if (value->Type() == nsAttrValue::ePercent)
      vval.SetPercentValue(value->GetPercentValue());
  
    if (vval.GetUnit() != eCSSUnit_Null) {
      nsCSSValue* top = aData->ValueForMarginTop();
      if (top->GetUnit() == eCSSUnit_Null)
        *top = vval;
      nsCSSValue* bottom = aData->ValueForMarginBottom();
      if (bottom->GetUnit() == eCSSUnit_Null)
        *bottom = vval;
    }
  }
}

void
nsGenericHTMLElement::MapImageSizeAttributesInto(const nsMappedAttributes* aAttributes,
                                                 nsRuleData* aData)
{
  if (!(aData->mSIDs & NS_STYLE_INHERIT_BIT(Position)))
    return;

  // width: value
  nsCSSValue* width = aData->ValueForWidth();
  if (width->GetUnit() == eCSSUnit_Null) {
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::width);
    if (value && value->Type() == nsAttrValue::eInteger)
      width->SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Pixel);
    else if (value && value->Type() == nsAttrValue::ePercent)
      width->SetPercentValue(value->GetPercentValue());
  }

  // height: value
  nsCSSValue* height = aData->ValueForHeight();
  if (height->GetUnit() == eCSSUnit_Null) {
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::height);
    if (value && value->Type() == nsAttrValue::eInteger)
      height->SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Pixel); 
    else if (value && value->Type() == nsAttrValue::ePercent)
      height->SetPercentValue(value->GetPercentValue());
  }
}

void
nsGenericHTMLElement::MapImageBorderAttributeInto(const nsMappedAttributes* aAttributes,
                                                  nsRuleData* aData)
{
  if (!(aData->mSIDs & NS_STYLE_INHERIT_BIT(Border)))
    return;

  // border: pixels
  const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::border);
  if (!value)
    return;
  
  nscoord val = 0;
  if (value->Type() == nsAttrValue::eInteger)
    val = value->GetIntegerValue();

  nsCSSValue* borderLeftWidth = aData->ValueForBorderLeftWidth();
  if (borderLeftWidth->GetUnit() == eCSSUnit_Null)
    borderLeftWidth->SetFloatValue((float)val, eCSSUnit_Pixel);
  nsCSSValue* borderTopWidth = aData->ValueForBorderTopWidth();
  if (borderTopWidth->GetUnit() == eCSSUnit_Null)
    borderTopWidth->SetFloatValue((float)val, eCSSUnit_Pixel);
  nsCSSValue* borderRightWidth = aData->ValueForBorderRightWidth();
  if (borderRightWidth->GetUnit() == eCSSUnit_Null)
    borderRightWidth->SetFloatValue((float)val, eCSSUnit_Pixel);
  nsCSSValue* borderBottomWidth = aData->ValueForBorderBottomWidth();
  if (borderBottomWidth->GetUnit() == eCSSUnit_Null)
    borderBottomWidth->SetFloatValue((float)val, eCSSUnit_Pixel);

  nsCSSValue* borderLeftStyle = aData->ValueForBorderLeftStyle();
  if (borderLeftStyle->GetUnit() == eCSSUnit_Null)
    borderLeftStyle->SetIntValue(NS_STYLE_BORDER_STYLE_SOLID, eCSSUnit_Enumerated);
  nsCSSValue* borderTopStyle = aData->ValueForBorderTopStyle();
  if (borderTopStyle->GetUnit() == eCSSUnit_Null)
    borderTopStyle->SetIntValue(NS_STYLE_BORDER_STYLE_SOLID, eCSSUnit_Enumerated);
  nsCSSValue* borderRightStyle = aData->ValueForBorderRightStyle();
  if (borderRightStyle->GetUnit() == eCSSUnit_Null)
    borderRightStyle->SetIntValue(NS_STYLE_BORDER_STYLE_SOLID, eCSSUnit_Enumerated);
  nsCSSValue* borderBottomStyle = aData->ValueForBorderBottomStyle();
  if (borderBottomStyle->GetUnit() == eCSSUnit_Null)
    borderBottomStyle->SetIntValue(NS_STYLE_BORDER_STYLE_SOLID, eCSSUnit_Enumerated);

  nsCSSValue* borderLeftColor = aData->ValueForBorderLeftColor();
  if (borderLeftColor->GetUnit() == eCSSUnit_Null)
    borderLeftColor->SetIntValue(NS_STYLE_COLOR_MOZ_USE_TEXT_COLOR, eCSSUnit_Enumerated);
  nsCSSValue* borderTopColor = aData->ValueForBorderTopColor();
  if (borderTopColor->GetUnit() == eCSSUnit_Null)
    borderTopColor->SetIntValue(NS_STYLE_COLOR_MOZ_USE_TEXT_COLOR, eCSSUnit_Enumerated);
  nsCSSValue* borderRightColor = aData->ValueForBorderRightColor();
  if (borderRightColor->GetUnit() == eCSSUnit_Null)
    borderRightColor->SetIntValue(NS_STYLE_COLOR_MOZ_USE_TEXT_COLOR, eCSSUnit_Enumerated);
  nsCSSValue* borderBottomColor = aData->ValueForBorderBottomColor();
  if (borderBottomColor->GetUnit() == eCSSUnit_Null)
    borderBottomColor->SetIntValue(NS_STYLE_COLOR_MOZ_USE_TEXT_COLOR, eCSSUnit_Enumerated);
}

void
nsGenericHTMLElement::MapBackgroundInto(const nsMappedAttributes* aAttributes,
                                        nsRuleData* aData)
{
  if (!(aData->mSIDs & NS_STYLE_INHERIT_BIT(Background)))
    return;

  nsPresContext* presContext = aData->mPresContext;
  nsCSSValue* backImage = aData->ValueForBackgroundImage();
  if (backImage->GetUnit() == eCSSUnit_Null &&
      presContext->UseDocumentColors()) {
    // background
    nsAttrValue* value =
      const_cast<nsAttrValue*>(aAttributes->GetAttr(nsGkAtoms::background));
    // If the value is an image, or it is a URL and we attempted a load,
    // put it in the style tree.
    if (value) {
      if (value->Type() == nsAttrValue::eURL) {
        value->LoadImage(presContext->Document());
      }
      if (value->Type() == nsAttrValue::eImage) {
        nsCSSValueList* list = backImage->SetListValue();
        list->mValue.SetImageValue(value->GetImageValue());
      }
    }
  }
}

void
nsGenericHTMLElement::MapBGColorInto(const nsMappedAttributes* aAttributes,
                                     nsRuleData* aData)
{
  if (!(aData->mSIDs & NS_STYLE_INHERIT_BIT(Background)))
    return;

  nsCSSValue* backColor = aData->ValueForBackgroundColor();
  if (backColor->GetUnit() == eCSSUnit_Null &&
      aData->mPresContext->UseDocumentColors()) {
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::bgcolor);
    nscolor color;
    if (value && value->GetColorValue(color)) {
      backColor->SetColorValue(color);
    }
  }
}

void
nsGenericHTMLElement::MapBackgroundAttributesInto(const nsMappedAttributes* aAttributes,
                                                  nsRuleData* aData)
{
  MapBackgroundInto(aAttributes, aData);
  MapBGColorInto(aAttributes, aData);
}

//----------------------------------------------------------------------

nsresult
nsGenericHTMLElement::SetAttrHelper(nsIAtom* aAttr, const nsAString& aValue)
{
  return SetAttr(kNameSpaceID_None, aAttr, aValue, true);
}

int32_t
nsGenericHTMLElement::GetIntAttr(nsIAtom* aAttr, int32_t aDefault) const
{
  const nsAttrValue* attrVal = mAttrsAndChildren.GetAttr(aAttr);
  if (attrVal && attrVal->Type() == nsAttrValue::eInteger) {
    return attrVal->GetIntegerValue();
  }
  return aDefault;
}

nsresult
nsGenericHTMLElement::SetIntAttr(nsIAtom* aAttr, int32_t aValue)
{
  nsAutoString value;
  value.AppendInt(aValue);

  return SetAttr(kNameSpaceID_None, aAttr, value, true);
}

uint32_t
nsGenericHTMLElement::GetUnsignedIntAttr(nsIAtom* aAttr,
                                         uint32_t aDefault) const
{
  const nsAttrValue* attrVal = mAttrsAndChildren.GetAttr(aAttr);
  if (!attrVal || attrVal->Type() != nsAttrValue::eInteger) {
    return aDefault;
  }

  return attrVal->GetIntegerValue();
}

void
nsGenericHTMLElement::GetURIAttr(nsIAtom* aAttr, nsIAtom* aBaseAttr,
                                 nsAString& aResult) const
{
  nsCOMPtr<nsIURI> uri;
  bool hadAttr = GetURIAttr(aAttr, aBaseAttr, getter_AddRefs(uri));
  if (!hadAttr) {
    aResult.Truncate();
    return;
  }

  if (!uri) {
    // Just return the attr value
    GetAttr(kNameSpaceID_None, aAttr, aResult);
    return;
  }

  nsAutoCString spec;
  uri->GetSpec(spec);
  CopyUTF8toUTF16(spec, aResult);
}

bool
nsGenericHTMLElement::GetURIAttr(nsIAtom* aAttr, nsIAtom* aBaseAttr, nsIURI** aURI) const
{
  *aURI = nullptr;

  const nsAttrValue* attr = mAttrsAndChildren.GetAttr(aAttr);
  if (!attr) {
    return false;
  }

  nsCOMPtr<nsIURI> baseURI = GetBaseURI();

  if (aBaseAttr) {
    nsAutoString baseAttrValue;
    if (GetAttr(kNameSpaceID_None, aBaseAttr, baseAttrValue)) {
      nsCOMPtr<nsIURI> baseAttrURI;
      nsresult rv =
        nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(baseAttrURI),
                                                  baseAttrValue, OwnerDoc(),
                                                  baseURI);
      if (NS_FAILED(rv)) {
        return true;
      }
      baseURI.swap(baseAttrURI);
    }
  }

  // Don't care about return value.  If it fails, we still want to
  // return true, and *aURI will be null.
  nsContentUtils::NewURIWithDocumentCharset(aURI,
                                            attr->GetStringValue(),
                                            OwnerDoc(), baseURI);
  return true;
}

/* static */ bool
nsGenericHTMLElement::IsScrollGrabAllowed(JSContext*, JSObject*)
{
  // Only allow scroll grabbing in chrome and certified apps.
  nsIPrincipal* prin = nsContentUtils::SubjectPrincipal();
  return nsContentUtils::IsSystemPrincipal(prin) ||
    prin->GetAppStatus() == nsIPrincipal::APP_STATUS_CERTIFIED;
}

nsresult
nsGenericHTMLElement::GetURIListAttr(nsIAtom* aAttr, nsAString& aResult)
{
  aResult.Truncate();

  nsAutoString value;
  if (!GetAttr(kNameSpaceID_None, aAttr, value))
    return NS_OK;

  nsIDocument* doc = OwnerDoc(); 
  nsCOMPtr<nsIURI> baseURI = GetBaseURI();

  nsString::const_iterator end;
  value.EndReading(end);

  nsAString::const_iterator iter;
  value.BeginReading(iter);

  while (iter != end) {
    while (*iter == ' ' && iter != end) {
      ++iter;
    }

    if (iter == end) {
      break;
    }

    nsAString::const_iterator start = iter;

    while (iter != end && *iter != ' ') {
      ++iter;
    }

    if (!aResult.IsEmpty()) {
      aResult.Append(NS_LITERAL_STRING(" "));
    }

    const nsSubstring& uriPart = Substring(start, iter);
    nsCOMPtr<nsIURI> attrURI;
    nsresult rv =
      nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(attrURI),
                                                uriPart, doc, baseURI);
    if (NS_FAILED(rv)) {
      aResult.Append(uriPart);
      continue;
    }

    MOZ_ASSERT(attrURI);

    nsAutoCString spec;
    rv = attrURI->GetSpec(spec);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aResult.Append(uriPart);
      continue;
    }

    AppendUTF8toUTF16(spec, aResult);
  }

  return NS_OK;
}

HTMLMenuElement*
nsGenericHTMLElement::GetContextMenu() const
{
  nsAutoString value;
  GetHTMLAttr(nsGkAtoms::contextmenu, value);
  if (!value.IsEmpty()) {
    //XXXsmaug How should this work in Shadow DOM?
    nsIDocument* doc = GetUncomposedDoc();
    if (doc) {
      return HTMLMenuElement::FromContentOrNull(doc->GetElementById(value));
    }
  }
  return nullptr;
}

NS_IMETHODIMP
nsGenericHTMLElement::GetContextMenu(nsIDOMHTMLMenuElement** aContextMenu)
{
  NS_IF_ADDREF(*aContextMenu = GetContextMenu());
  return NS_OK;
}

bool
nsGenericHTMLElement::IsLabelable() const
{
  return IsAnyOfHTMLElements(nsGkAtoms::progress, nsGkAtoms::meter);
}

bool
nsGenericHTMLElement::IsInteractiveHTMLContent(bool aIgnoreTabindex) const
{
  return IsAnyOfHTMLElements(nsGkAtoms::details, nsGkAtoms::embed,
                             nsGkAtoms::keygen) ||
         (!aIgnoreTabindex && HasAttr(kNameSpaceID_None, nsGkAtoms::tabindex));
}

already_AddRefed<UndoManager>
nsGenericHTMLElement::GetUndoManager()
{
  nsDOMSlots* slots = GetExistingDOMSlots();
  if (slots && slots->mUndoManager) {
    nsRefPtr<UndoManager> undoManager = slots->mUndoManager;
    return undoManager.forget();
  } else {
    return nullptr;
  }
}

bool
nsGenericHTMLElement::UndoScope()
{
  nsDOMSlots* slots = GetExistingDOMSlots();
  return slots && slots->mUndoManager;
}

void
nsGenericHTMLElement::SetUndoScope(bool aUndoScope, mozilla::ErrorResult& aError)
{
  nsresult rv = SetUndoScopeInternal(aUndoScope);
  if (NS_FAILED(rv)) {
    aError.Throw(rv);
    return;
  }

  // The undoScope property must reflect the undoscope boolean attribute.
  if (aUndoScope) {
    rv = SetAttr(kNameSpaceID_None, nsGkAtoms::undoscope,
                 NS_LITERAL_STRING(""), true);
  } else {
    rv = UnsetAttr(kNameSpaceID_None, nsGkAtoms::undoscope, true);
  }

  if (NS_FAILED(rv)) {
    aError.Throw(rv);
    return;
  }
}

nsresult
nsGenericHTMLElement::SetUndoScopeInternal(bool aUndoScope)
{
  if (aUndoScope) {
    nsDOMSlots* slots = DOMSlots();
    if (!slots->mUndoManager) {
      slots->mUndoManager = new UndoManager(this);
    }
  } else {
    nsDOMSlots* slots = GetExistingDOMSlots();
    if (slots && slots->mUndoManager) {
      // Clear transaction history and disconnect.
      ErrorResult rv;
      slots->mUndoManager->ClearRedo(rv);
      if (rv.Failed()) {
        return rv.StealNSResult();
      }

      slots->mUndoManager->ClearUndo(rv);
      if (rv.Failed()) {
        return rv.StealNSResult();
      }

      slots->mUndoManager->Disconnect();
      slots->mUndoManager = nullptr;
    }
  }
  return NS_OK;
}

// static
bool
nsGenericHTMLElement::TouchEventsEnabled(JSContext* /* unused */, JSObject* /* unused */)
{
  return TouchEvent::PrefEnabled();
}

//----------------------------------------------------------------------

nsGenericHTMLFormElement::nsGenericHTMLFormElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
  , mForm(nullptr)
  , mFieldSet(nullptr)
{
  // We should add the NS_EVENT_STATE_ENABLED bit here as needed, but
  // that depends on our type, which is not initialized yet.  So we
  // have to do this in subclasses.
}

nsGenericHTMLFormElement::~nsGenericHTMLFormElement()
{
  if (mFieldSet) {
    mFieldSet->RemoveElement(this);
  }

  // Check that this element doesn't know anything about its form at this point.
  NS_ASSERTION(!mForm, "mForm should be null at this point!");
}

NS_IMPL_ISUPPORTS_INHERITED(nsGenericHTMLFormElement,
                            nsGenericHTMLElement,
                            nsIFormControl)

nsINode*
nsGenericHTMLFormElement::GetScopeChainParent() const
{
  return mForm ? mForm : nsGenericHTMLElement::GetScopeChainParent();
}

bool
nsGenericHTMLFormElement::IsNodeOfType(uint32_t aFlags) const
{
  return !(aFlags & ~(eCONTENT | eHTML_FORM_CONTROL));
}

void
nsGenericHTMLFormElement::SaveSubtreeState()
{
  SaveState();

  nsGenericHTMLElement::SaveSubtreeState();
}

void
nsGenericHTMLFormElement::SetForm(nsIDOMHTMLFormElement* aForm)
{
  NS_PRECONDITION(aForm, "Don't pass null here");
  NS_ASSERTION(!mForm,
               "We don't support switching from one non-null form to another.");

  // keep a *weak* ref to the form here
  mForm = static_cast<HTMLFormElement*>(aForm);
}

void
nsGenericHTMLFormElement::ClearForm(bool aRemoveFromForm)
{
  NS_ASSERTION((mForm != nullptr) == HasFlag(ADDED_TO_FORM),
               "Form control should have had flag set correctly");

  if (!mForm) {
    return;
  }
  
  if (aRemoveFromForm) {
    nsAutoString nameVal, idVal;
    GetAttr(kNameSpaceID_None, nsGkAtoms::name, nameVal);
    GetAttr(kNameSpaceID_None, nsGkAtoms::id, idVal);

    mForm->RemoveElement(this, true);

    if (!nameVal.IsEmpty()) {
      mForm->RemoveElementFromTable(this, nameVal,
                                    HTMLFormElement::ElementRemoved);
    }

    if (!idVal.IsEmpty()) {
      mForm->RemoveElementFromTable(this, idVal,
                                    HTMLFormElement::ElementRemoved);
    }
  }

  UnsetFlags(ADDED_TO_FORM);
  mForm = nullptr;
}

Element*
nsGenericHTMLFormElement::GetFormElement()
{
  return mForm;
}

HTMLFieldSetElement*
nsGenericHTMLFormElement::GetFieldSet()
{
  return mFieldSet;
}

nsresult
nsGenericHTMLFormElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  NS_ENSURE_ARG_POINTER(aForm);
  NS_IF_ADDREF(*aForm = mForm);
  return NS_OK;
}

nsIContent::IMEState
nsGenericHTMLFormElement::GetDesiredIMEState()
{
  nsIEditor* editor = GetEditorInternal();
  if (!editor)
    return nsGenericHTMLElement::GetDesiredIMEState();
  nsCOMPtr<nsIEditorIMESupport> imeEditor = do_QueryInterface(editor);
  if (!imeEditor)
    return nsGenericHTMLElement::GetDesiredIMEState();
  IMEState state;
  nsresult rv = imeEditor->GetPreferredIMEState(&state);
  if (NS_FAILED(rv))
    return nsGenericHTMLElement::GetDesiredIMEState();
  return state;
}

nsresult
nsGenericHTMLFormElement::BindToTree(nsIDocument* aDocument,
                                     nsIContent* aParent,
                                     nsIContent* aBindingParent,
                                     bool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument, aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  // An autofocus event has to be launched if the autofocus attribute is
  // specified and the element accept the autofocus attribute. In addition,
  // the document should not be already loaded and the "browser.autofocus"
  // preference should be 'true'.
  if (IsAutofocusable() && HasAttr(kNameSpaceID_None, nsGkAtoms::autofocus) &&
      Preferences::GetBool("browser.autofocus", true)) {
    nsCOMPtr<nsIRunnable> event = new nsAutoFocusEvent(this);
    rv = NS_DispatchToCurrentThread(event);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // If @form is set, the element *has* to be in a document, otherwise it
  // wouldn't be possible to find an element with the corresponding id.
  // If @form isn't set, the element *has* to have a parent, otherwise it
  // wouldn't be possible to find a form ancestor.
  // We should not call UpdateFormOwner if none of these conditions are
  // fulfilled.
  if (HasAttr(kNameSpaceID_None, nsGkAtoms::form) ? !!GetUncomposedDoc()
                                                  : !!aParent) {
    UpdateFormOwner(true, nullptr);
  }

  // Set parent fieldset which should be used for the disabled state.
  UpdateFieldSet(false);

  return NS_OK;
}

void
nsGenericHTMLFormElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  // Save state before doing anything
  SaveState();
  
  if (mForm) {
    // Might need to unset mForm
    if (aNullParent) {
      // No more parent means no more form
      ClearForm(true);
    } else {
      // Recheck whether we should still have an mForm.
      if (HasAttr(kNameSpaceID_None, nsGkAtoms::form) ||
          !FindAncestorForm(mForm)) {
        ClearForm(true);
      } else {
        UnsetFlags(MAYBE_ORPHAN_FORM_ELEMENT);
      }
    }

    if (!mForm) {
      // Our novalidate state might have changed
      UpdateState(false);
    }
  }

  // We have to remove the form id observer if there was one.
  // We will re-add one later if needed (during bind to tree).
  if (nsContentUtils::HasNonEmptyAttr(this, kNameSpaceID_None,
                                      nsGkAtoms::form)) {
    RemoveFormIdObserver();
  }

  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);

  // The element might not have a fieldset anymore.
  UpdateFieldSet(false);
}

nsresult
nsGenericHTMLFormElement::BeforeSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                        nsAttrValueOrString* aValue,
                                        bool aNotify)
{
  if (aNameSpaceID == kNameSpaceID_None) {
    nsAutoString tmp;

    // remove the control from the hashtable as needed

    if (mForm && (aName == nsGkAtoms::name || aName == nsGkAtoms::id)) {
      GetAttr(kNameSpaceID_None, aName, tmp);

      if (!tmp.IsEmpty()) {
        mForm->RemoveElementFromTable(this, tmp,
                                      HTMLFormElement::AttributeUpdated);
      }
    }

    if (mForm && aName == nsGkAtoms::type) {
      GetAttr(kNameSpaceID_None, nsGkAtoms::name, tmp);

      if (!tmp.IsEmpty()) {
        mForm->RemoveElementFromTable(this, tmp,
                                      HTMLFormElement::AttributeUpdated);
      }

      GetAttr(kNameSpaceID_None, nsGkAtoms::id, tmp);

      if (!tmp.IsEmpty()) {
        mForm->RemoveElementFromTable(this, tmp,
                                      HTMLFormElement::AttributeUpdated);
      }

      mForm->RemoveElement(this, false);

      // Removing the element from the form can make it not be the default
      // control anymore.  Go ahead and notify on that change, though we might
      // end up readding and becoming the default control again in
      // AfterSetAttr.
      // FIXME: Bug 656197
      UpdateState(aNotify);
    }

    if (aName == nsGkAtoms::form) {
      // If @form isn't set or set to the empty string, there were no observer
      // so we don't have to remove it.
      if (nsContentUtils::HasNonEmptyAttr(this, kNameSpaceID_None,
                                          nsGkAtoms::form)) {
        // The current form id observer is no longer needed.
        // A new one may be added in AfterSetAttr.
        RemoveFormIdObserver();
      }
    }
  }

  return nsGenericHTMLElement::BeforeSetAttr(aNameSpaceID, aName,
                                             aValue, aNotify);
}

nsresult
nsGenericHTMLFormElement::AfterSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                       const nsAttrValue* aValue, bool aNotify)
{
  if (aNameSpaceID == kNameSpaceID_None) {
    // add the control to the hashtable as needed

    if (mForm && (aName == nsGkAtoms::name || aName == nsGkAtoms::id) &&
        aValue && !aValue->IsEmptyString()) {
      MOZ_ASSERT(aValue->Type() == nsAttrValue::eAtom,
                 "Expected atom value for name/id");
      mForm->AddElementToTable(this,
        nsDependentAtomString(aValue->GetAtomValue()));
    }

    if (mForm && aName == nsGkAtoms::type) {
      nsAutoString tmp;

      GetAttr(kNameSpaceID_None, nsGkAtoms::name, tmp);

      if (!tmp.IsEmpty()) {
        mForm->AddElementToTable(this, tmp);
      }

      GetAttr(kNameSpaceID_None, nsGkAtoms::id, tmp);

      if (!tmp.IsEmpty()) {
        mForm->AddElementToTable(this, tmp);
      }

      mForm->AddElement(this, false, aNotify);

      // Adding the element to the form can make it be the default control .
      // Go ahead and notify on that change.
      // Note: no need to notify on CanBeDisabled(), since type attr
      // changes can't affect that.
      UpdateState(aNotify);
    }

    if (aName == nsGkAtoms::form) {
      // We need a new form id observer.
      //XXXsmaug How should this work in Shadow DOM?
      nsIDocument* doc = GetUncomposedDoc();
      if (doc) {
        Element* formIdElement = nullptr;
        if (aValue && !aValue->IsEmptyString()) {
          formIdElement = AddFormIdObserver();
        }

        // Because we have a new @form value (or no more @form), we have to
        // update our form owner.
        UpdateFormOwner(false, formIdElement);
      }
    }
  }

  return nsGenericHTMLElement::AfterSetAttr(aNameSpaceID, aName,
                                            aValue, aNotify);
}

nsresult
nsGenericHTMLFormElement::PreHandleEvent(EventChainPreVisitor& aVisitor)
{
  if (aVisitor.mEvent->mFlags.mIsTrusted) {
    switch (aVisitor.mEvent->mMessage) {
      case NS_FOCUS_CONTENT:
      {
        // Check to see if focus has bubbled up from a form control's
        // child textfield or button.  If that's the case, don't focus
        // this parent file control -- leave focus on the child.
        nsIFormControlFrame* formControlFrame = GetFormControlFrame(true);
        if (formControlFrame &&
            aVisitor.mEvent->originalTarget == static_cast<nsINode*>(this))
          formControlFrame->SetFocus(true, true);
        break;
      }
      case NS_BLUR_CONTENT:
      {
        nsIFormControlFrame* formControlFrame = GetFormControlFrame(true);
        if (formControlFrame)
          formControlFrame->SetFocus(false, false);
        break;
      }
      default:
        break;
    }
  }

  return nsGenericHTMLElement::PreHandleEvent(aVisitor);
}

/* virtual */
bool
nsGenericHTMLFormElement::IsDisabled() const
{
  return HasAttr(kNameSpaceID_None, nsGkAtoms::disabled) ||
         (mFieldSet && mFieldSet->IsDisabled());
}

void
nsGenericHTMLFormElement::ForgetFieldSet(nsIContent* aFieldset)
{
  if (mFieldSet == aFieldset) {
    mFieldSet = nullptr;
  }
}

bool
nsGenericHTMLFormElement::CanBeDisabled() const
{
  int32_t type = GetType();
  // It's easier to test the types that _cannot_ be disabled
  return
    type != NS_FORM_LABEL &&
    type != NS_FORM_OBJECT &&
    type != NS_FORM_OUTPUT;
}

bool
nsGenericHTMLFormElement::IsHTMLFocusable(bool aWithMouse,
                                          bool* aIsFocusable,
                                          int32_t* aTabIndex)
{
  if (nsGenericHTMLElement::IsHTMLFocusable(aWithMouse, aIsFocusable, aTabIndex)) {
    return true;
  }

#ifdef XP_MACOSX
  *aIsFocusable =
    (!aWithMouse || nsFocusManager::sMouseFocusesFormControl) && *aIsFocusable;
#endif
  return false;
}

EventStates
nsGenericHTMLFormElement::IntrinsicState() const
{
  // If you add attribute-dependent states here, you need to add them them to
  // AfterSetAttr too.  And add them to AfterSetAttr for all subclasses that
  // implement IntrinsicState() and are affected by that attribute.
  EventStates state = nsGenericHTMLElement::IntrinsicState();

  if (CanBeDisabled()) {
    // :enabled/:disabled
    if (IsDisabled()) {
      state |= NS_EVENT_STATE_DISABLED;
      state &= ~NS_EVENT_STATE_ENABLED;
    } else {
      state &= ~NS_EVENT_STATE_DISABLED;
      state |= NS_EVENT_STATE_ENABLED;
    }
  }
  
  if (mForm && mForm->IsDefaultSubmitElement(this)) {
      NS_ASSERTION(IsSubmitControl(),
                   "Default submit element that isn't a submit control.");
      // We are the default submit element (:default)
      state |= NS_EVENT_STATE_DEFAULT;
  }

  // Make the text controls read-write
  if (!state.HasState(NS_EVENT_STATE_MOZ_READWRITE) &&
      IsTextControl(false)) {
    bool roState = GetBoolAttr(nsGkAtoms::readonly);

    if (!roState) {
      state |= NS_EVENT_STATE_MOZ_READWRITE;
      state &= ~NS_EVENT_STATE_MOZ_READONLY;
    }
  }

  return state;
}

nsGenericHTMLFormElement::FocusTristate
nsGenericHTMLFormElement::FocusState()
{
  // We can't be focused if we aren't in a (composed) document
  nsIDocument* doc = GetComposedDoc();
  if (!doc)
    return eUnfocusable;

  // first see if we are disabled or not. If disabled then do nothing.
  if (IsDisabled()) {
    return eUnfocusable;
  }

  // If the window is not active, do not allow the focus to bring the
  // window to the front.  We update the focus controller, but do
  // nothing else.
  nsPIDOMWindow* win = doc->GetWindow();
  if (win) {
    nsCOMPtr<nsIDOMWindow> rootWindow = do_QueryInterface(win->GetPrivateRoot());

    nsCOMPtr<nsIFocusManager> fm = do_GetService(FOCUSMANAGER_CONTRACTID);
    if (fm && rootWindow) {
      nsCOMPtr<nsIDOMWindow> activeWindow;
      fm->GetActiveWindow(getter_AddRefs(activeWindow));
      if (activeWindow == rootWindow) {
        return eActiveWindow;
      }
    }
  }

  return eInactiveWindow;
}

Element*
nsGenericHTMLFormElement::AddFormIdObserver()
{
  NS_ASSERTION(GetUncomposedDoc(), "When adding a form id observer, "
                                   "we should be in a document!");

  nsAutoString formId;
  nsIDocument* doc = OwnerDoc();
  GetAttr(kNameSpaceID_None, nsGkAtoms::form, formId);
  NS_ASSERTION(!formId.IsEmpty(),
               "@form value should not be the empty string!");
  nsCOMPtr<nsIAtom> atom = do_GetAtom(formId);

  return doc->AddIDTargetObserver(atom, FormIdUpdated, this, false);
}

void
nsGenericHTMLFormElement::RemoveFormIdObserver()
{
  /**
   * We are using OwnerDoc() because we don't really care about having the
   * element actually being in the tree. If it is not and @form value changes,
   * this method will be called for nothing but removing an observer which does
   * not exist doesn't cost so much (no entry in the hash table) so having a
   * boolean for GetUncomposedDoc()/GetOwnerDoc() would make everything look
   * more complex for nothing.
   */

  nsIDocument* doc = OwnerDoc();

  // At this point, we may not have a document anymore. In that case, we can't
  // remove the observer. The document did that for us.
  if (!doc) {
    return;
  }

  nsAutoString formId;
  GetAttr(kNameSpaceID_None, nsGkAtoms::form, formId);
  NS_ASSERTION(!formId.IsEmpty(),
               "@form value should not be the empty string!");
  nsCOMPtr<nsIAtom> atom = do_GetAtom(formId);

  doc->RemoveIDTargetObserver(atom, FormIdUpdated, this, false);
}


/* static */
bool
nsGenericHTMLFormElement::FormIdUpdated(Element* aOldElement,
                                        Element* aNewElement,
                                        void* aData)
{
  nsGenericHTMLFormElement* element =
    static_cast<nsGenericHTMLFormElement*>(aData);

  NS_ASSERTION(element->IsHTMLElement(), "aData should be an HTML element");

  element->UpdateFormOwner(false, aNewElement);

  return true;
}

bool 
nsGenericHTMLFormElement::IsElementDisabledForEvents(EventMessage aMessage,
                                                     nsIFrame* aFrame)
{
  bool disabled = IsDisabled();
  if (!disabled && aFrame) {
    const nsStyleUserInterface* uiStyle = aFrame->StyleUserInterface();
    disabled = uiStyle->mUserInput == NS_STYLE_USER_INPUT_NONE ||
      uiStyle->mUserInput == NS_STYLE_USER_INPUT_DISABLED;

  }
  return disabled && aMessage != NS_MOUSE_MOVE;
}

void
nsGenericHTMLFormElement::UpdateFormOwner(bool aBindToTree,
                                          Element* aFormIdElement)
{
  NS_PRECONDITION(!aBindToTree || !aFormIdElement,
                  "aFormIdElement shouldn't be set if aBindToTree is true!");

  bool needStateUpdate = false;
  if (!aBindToTree) {
    needStateUpdate = mForm && mForm->IsDefaultSubmitElement(this);
    ClearForm(true);
  }

  HTMLFormElement *oldForm = mForm;

  if (!mForm) {
    // If @form is set, we have to use that to find the form.
    nsAutoString formId;
    if (GetAttr(kNameSpaceID_None, nsGkAtoms::form, formId)) {
      if (!formId.IsEmpty()) {
        Element* element = nullptr;

        if (aBindToTree) {
          element = AddFormIdObserver();
        } else {
          element = aFormIdElement;
        }

        NS_ASSERTION(GetUncomposedDoc(), "The element should be in a document "
                                         "when UpdateFormOwner is called!");
        NS_ASSERTION(!GetUncomposedDoc() ||
                     element == GetUncomposedDoc()->GetElementById(formId),
                     "element should be equals to the current element "
                     "associated with the id in @form!");

        if (element && element->IsHTMLElement(nsGkAtoms::form)) {
          mForm = static_cast<HTMLFormElement*>(element);
        }
      }
     } else {
      // We now have a parent, so we may have picked up an ancestor form.  Search
      // for it.  Note that if mForm is already set we don't want to do this,
      // because that means someone (probably the content sink) has already set
      // it to the right value.  Also note that even if being bound here didn't
      // change our parent, we still need to search, since our parent chain
      // probably changed _somewhere_.
      mForm = FindAncestorForm();
    }
  }

  if (mForm && !HasFlag(ADDED_TO_FORM)) {
    // Now we need to add ourselves to the form
    nsAutoString nameVal, idVal;
    GetAttr(kNameSpaceID_None, nsGkAtoms::name, nameVal);
    GetAttr(kNameSpaceID_None, nsGkAtoms::id, idVal);

    SetFlags(ADDED_TO_FORM);

    // Notify only if we just found this mForm.
    mForm->AddElement(this, true, oldForm == nullptr);

    if (!nameVal.IsEmpty()) {
      mForm->AddElementToTable(this, nameVal);
    }

    if (!idVal.IsEmpty()) {
      mForm->AddElementToTable(this, idVal);
    }
  }

  if (mForm != oldForm || needStateUpdate) {
    UpdateState(true);
  }
}

void
nsGenericHTMLFormElement::UpdateFieldSet(bool aNotify)
{
  nsIContent* parent = nullptr;
  nsIContent* prev = nullptr;

  for (parent = GetParent(); parent;
       prev = parent, parent = parent->GetParent()) {
    HTMLFieldSetElement* fieldset =
      HTMLFieldSetElement::FromContent(parent);
    if (fieldset &&
        (!prev || fieldset->GetFirstLegend() != prev)) {
      if (mFieldSet == fieldset) {
        // We already have the right fieldset;
        return;
      }

      if (mFieldSet) {
        mFieldSet->RemoveElement(this);
      }
      mFieldSet = fieldset;
      fieldset->AddElement(this);

      // The disabled state may have changed
      FieldSetDisabledChanged(aNotify);
      return;
    }
  }

  // No fieldset found.
  if (mFieldSet) {
    mFieldSet->RemoveElement(this);
    mFieldSet = nullptr;
    // The disabled state may have changed
    FieldSetDisabledChanged(aNotify);
  }
}

void
nsGenericHTMLFormElement::FieldSetDisabledChanged(bool aNotify)
{
  UpdateState(aNotify);
}

bool
nsGenericHTMLFormElement::IsLabelable() const
{
  // TODO: keygen should be in that list, see bug 101019.
  uint32_t type = GetType();
  return (type & NS_FORM_INPUT_ELEMENT && type != NS_FORM_INPUT_HIDDEN) ||
         type & NS_FORM_BUTTON_ELEMENT ||
         // type == NS_FORM_KEYGEN ||
         type == NS_FORM_OUTPUT ||
         type == NS_FORM_SELECT ||
         type == NS_FORM_TEXTAREA;
}

//----------------------------------------------------------------------

void
nsGenericHTMLElement::Blur(mozilla::ErrorResult& aError)
{
  if (!ShouldBlur(this)) {
    return;
  }

  nsIDocument* doc = GetComposedDoc();
  if (!doc) {
    return;
  }

  nsIDOMWindow* win = doc->GetWindow();
  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (win && fm) {
    aError = fm->ClearFocus(win);
  }
}

void
nsGenericHTMLElement::Focus(ErrorResult& aError)
{
  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm) {
    aError = fm->SetFocus(this, 0);
  }
}

void
nsGenericHTMLElement::Click()
{
  if (HandlingClick())
    return;

  // Strong in case the event kills it
  nsCOMPtr<nsIDocument> doc = GetComposedDoc();

  nsCOMPtr<nsIPresShell> shell;
  nsRefPtr<nsPresContext> context;
  if (doc) {
    shell = doc->GetShell();
    if (shell) {
      context = shell->GetPresContext();
    }
  }

  SetHandlingClick();

  // Click() is never called from native code, but it may be
  // called from chrome JS. Mark this event trusted if Click()
  // is called from chrome code.
  WidgetMouseEvent event(nsContentUtils::IsCallerChrome(),
                         NS_MOUSE_CLICK, nullptr, WidgetMouseEvent::eReal);
  event.inputSource = nsIDOMMouseEvent::MOZ_SOURCE_UNKNOWN;

  EventDispatcher::Dispatch(static_cast<nsIContent*>(this), context, &event);

  ClearHandlingClick();
}

bool
nsGenericHTMLElement::IsHTMLFocusable(bool aWithMouse,
                                      bool *aIsFocusable,
                                      int32_t *aTabIndex)
{
  nsIDocument* doc = GetComposedDoc();
  if (!doc || doc->HasFlag(NODE_IS_EDITABLE)) {
    // In designMode documents we only allow focusing the document.
    if (aTabIndex) {
      *aTabIndex = -1;
    }

    *aIsFocusable = false;

    return true;
  }

  int32_t tabIndex = TabIndex();

  bool override, disabled = false;
  if (IsEditableRoot()) {
    // Editable roots should always be focusable.
    override = true;

    // Ignore the disabled attribute in editable contentEditable/designMode
    // roots.
    if (!HasAttr(kNameSpaceID_None, nsGkAtoms::tabindex)) {
      // The default value for tabindex should be 0 for editable
      // contentEditable roots.
      tabIndex = 0;
    }
  }
  else {
    override = false;

    // Just check for disabled attribute on form controls
    disabled = IsDisabled();
    if (disabled) {
      tabIndex = -1;
    }
  }

  if (aTabIndex) {
    *aTabIndex = tabIndex;
  }

  // If a tabindex is specified at all, or the default tabindex is 0, we're focusable
  *aIsFocusable = 
    (tabIndex >= 0 || (!disabled && HasAttr(kNameSpaceID_None, nsGkAtoms::tabindex)));

  return override;
}

void
nsGenericHTMLElement::RegUnRegAccessKey(bool aDoReg)
{
  // first check to see if we have an access key
  nsAutoString accessKey;
  GetAttr(kNameSpaceID_None, nsGkAtoms::accesskey, accessKey);
  if (accessKey.IsEmpty()) {
    return;
  }

  // We have an access key, so get the ESM from the pres context.
  nsPresContext* presContext = GetPresContext(eForUncomposedDoc);

  if (presContext) {
    EventStateManager* esm = presContext->EventStateManager();

    // Register or unregister as appropriate.
    if (aDoReg) {
      esm->RegisterAccessKey(this, (uint32_t)accessKey.First());
    } else {
      esm->UnregisterAccessKey(this, (uint32_t)accessKey.First());
    }
  }
}

void
nsGenericHTMLElement::PerformAccesskey(bool aKeyCausesActivation,
                                       bool aIsTrustedEvent)
{
  nsPresContext* presContext = GetPresContext(eForUncomposedDoc);
  if (!presContext)
    return;

  // It's hard to say what HTML4 wants us to do in all cases.
  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm) {
    fm->SetFocus(this, nsIFocusManager::FLAG_BYKEY);
  }

  if (aKeyCausesActivation) {
    // Click on it if the users prefs indicate to do so.
    WidgetMouseEvent event(aIsTrustedEvent, NS_MOUSE_CLICK, nullptr,
                           WidgetMouseEvent::eReal);
    event.inputSource = nsIDOMMouseEvent::MOZ_SOURCE_KEYBOARD;

    nsAutoPopupStatePusher popupStatePusher(aIsTrustedEvent ?
                                            openAllowed : openAbused);

    EventDispatcher::Dispatch(static_cast<nsIContent*>(this),
                              presContext, &event);
  }
}

const nsAttrName*
nsGenericHTMLElement::InternalGetExistingAttrNameFromQName(const nsAString& aStr) const
{
  if (IsInHTMLDocument()) {
    nsAutoString lower;
    nsContentUtils::ASCIIToLower(aStr, lower);
    return mAttrsAndChildren.GetExistingAttrNameFromQName(lower);
  }

  return mAttrsAndChildren.GetExistingAttrNameFromQName(aStr);
}

nsresult
nsGenericHTMLElement::GetEditor(nsIEditor** aEditor)
{
  *aEditor = nullptr;

  if (!nsContentUtils::IsCallerChrome()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  NS_IF_ADDREF(*aEditor = GetEditorInternal());
  return NS_OK;
}

already_AddRefed<nsIEditor>
nsGenericHTMLElement::GetAssociatedEditor()
{
  // If contenteditable is ever implemented, it might need to do something different here?

  nsCOMPtr<nsIEditor> editor = GetEditorInternal();
  return editor.forget();
}

bool
nsGenericHTMLElement::IsCurrentBodyElement()
{
  // TODO Bug 698498: Should this handle the case where GetBody returns a
  //                  frameset?
  if (!IsHTMLElement(nsGkAtoms::body)) {
    return false;
  }

  nsCOMPtr<nsIDOMHTMLDocument> htmlDocument =
    do_QueryInterface(GetUncomposedDoc());
  if (!htmlDocument) {
    return false;
  }

  nsCOMPtr<nsIDOMHTMLElement> htmlElement;
  htmlDocument->GetBody(getter_AddRefs(htmlElement));
  return htmlElement == static_cast<HTMLBodyElement*>(this);
}

// static
void
nsGenericHTMLElement::SyncEditorsOnSubtree(nsIContent* content)
{
  /* Sync this node */
  nsGenericHTMLElement* element = FromContent(content);
  if (element) {
    nsCOMPtr<nsIEditor> editor = element->GetAssociatedEditor();
    if (editor) {
      editor->SyncRealTimeSpell();
    }
  }

  /* Sync all children */
  for (nsIContent* child = content->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    SyncEditorsOnSubtree(child);
  }
}

void
nsGenericHTMLElement::RecompileScriptEventListeners()
{
    int32_t i, count = mAttrsAndChildren.AttrCount();
    for (i = 0; i < count; ++i) {
        const nsAttrName *name = mAttrsAndChildren.AttrNameAt(i);

        // Eventlistenener-attributes are always in the null namespace
        if (!name->IsAtom()) {
            continue;
        }

        nsIAtom *attr = name->Atom();
        if (!IsEventAttributeName(attr)) {
            continue;
        }

        nsAutoString value;
        GetAttr(kNameSpaceID_None, attr, value);
        SetEventHandler(attr, value, true);
    }
}

bool
nsGenericHTMLElement::IsEditableRoot() const
{
  nsIDocument *document = GetComposedDoc();
  if (!document) {
    return false;
  }

  if (document->HasFlag(NODE_IS_EDITABLE)) {
    return false;
  }

  if (GetContentEditableValue() != eTrue) {
    return false;
  }

  nsIContent *parent = GetParent();

  return !parent || !parent->HasFlag(NODE_IS_EDITABLE);
}

static void
MakeContentDescendantsEditable(nsIContent *aContent, nsIDocument *aDocument)
{
  // If aContent is not an element, we just need to update its
  // internal editable state and don't need to notify anyone about
  // that.  For elements, we need to send a ContentStateChanged
  // notification.
  if (!aContent->IsElement()) {
    aContent->UpdateEditableState(false);
    return;
  }

  Element *element = aContent->AsElement();

  element->UpdateEditableState(true);

  for (nsIContent *child = aContent->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    if (!child->HasAttr(kNameSpaceID_None, nsGkAtoms::contenteditable)) {
      MakeContentDescendantsEditable(child, aDocument);
    }
  }
}

void
nsGenericHTMLElement::ChangeEditableState(int32_t aChange)
{
  //XXXsmaug Fix this for Shadow DOM, bug 1066965.
  nsIDocument* document = GetUncomposedDoc();
  if (!document) {
    return;
  }

  if (aChange != 0) {
    nsCOMPtr<nsIHTMLDocument> htmlDocument =
      do_QueryInterface(document);
    if (htmlDocument) {
      htmlDocument->ChangeContentEditableCount(this, aChange);
    }

    nsIContent* parent = GetParent();
    while (parent) {
      parent->ChangeEditableDescendantCount(aChange);
      parent = parent->GetParent();
    }
  }

  if (document->HasFlag(NODE_IS_EDITABLE)) {
    document = nullptr;
  }

  // MakeContentDescendantsEditable is going to call ContentStateChanged for
  // this element and all descendants if editable state has changed.
  // We might as well wrap it all in one script blocker.
  nsAutoScriptBlocker scriptBlocker;
  MakeContentDescendantsEditable(this, document);
}


//----------------------------------------------------------------------

nsGenericHTMLFormElementWithState::nsGenericHTMLFormElementWithState(
    already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo
  )
  : nsGenericHTMLFormElement(aNodeInfo)
{
  mStateKey.SetIsVoid(true);
}

nsresult
nsGenericHTMLFormElementWithState::GenerateStateKey()
{
  // Keep the key if already computed
  if (!mStateKey.IsVoid()) {
    return NS_OK;
  }

  nsIDocument* doc = GetUncomposedDoc();
  if (!doc) {
    return NS_OK;
  }

  // Generate the state key
  nsresult rv = nsContentUtils::GenerateStateKey(this, doc, mStateKey);

  if (NS_FAILED(rv)) {
    mStateKey.SetIsVoid(true);
    return rv;
  }

  // If the state key is blank, this is anonymous content or for whatever
  // reason we are not supposed to save/restore state: keep it as such.
  if (!mStateKey.IsEmpty()) {
    // Add something unique to content so layout doesn't muck us up.
    mStateKey += "-C";
  }
  return NS_OK;
}

nsPresState*
nsGenericHTMLFormElementWithState::GetPrimaryPresState()
{
  if (mStateKey.IsEmpty()) {
    return nullptr;
  }

  nsCOMPtr<nsILayoutHistoryState> history = GetLayoutHistory(false);

  if (!history) {
    return nullptr;
  }

  // Get the pres state for this key, if it doesn't exist, create one.
  nsPresState* result = history->GetState(mStateKey);
  if (!result) {
    result = new nsPresState();
    history->AddState(mStateKey, result);
  }

  return result;
}

already_AddRefed<nsILayoutHistoryState>
nsGenericHTMLFormElementWithState::GetLayoutHistory(bool aRead)
{
  nsCOMPtr<nsIDocument> doc = GetUncomposedDoc();
  if (!doc) {
    return nullptr;
  }

  //
  // Get the history
  //
  nsCOMPtr<nsILayoutHistoryState> history = doc->GetLayoutHistoryState();
  if (!history) {
    return nullptr;
  }

  if (aRead && !history->HasStates()) {
    return nullptr;
  }

  return history.forget();
}

bool
nsGenericHTMLFormElementWithState::RestoreFormControlState()
{
  if (mStateKey.IsEmpty()) {
    return false;
  }

  nsCOMPtr<nsILayoutHistoryState> history =
    GetLayoutHistory(true);
  if (!history) {
    return false;
  }

  nsPresState *state;
  // Get the pres state for this key
  state = history->GetState(mStateKey);
  if (state) {
    bool result = RestoreState(state);
    history->RemoveState(mStateKey);
    return result;
  }

  return false;
}

void
nsGenericHTMLFormElementWithState::NodeInfoChanged(mozilla::dom::NodeInfo* aOldNodeInfo)
{
  mStateKey.SetIsVoid(true);
}

void
nsGenericHTMLElement::GetItemValue(JSContext* aCx, JSObject* aScope,
                                   JS::MutableHandle<JS::Value> aRetval,
                                   ErrorResult& aError)
{
  JS::Rooted<JSObject*> scope(aCx, aScope);
  if (!HasAttr(kNameSpaceID_None, nsGkAtoms::itemprop)) {
    aRetval.setNull();
    return;
  }

  if (ItemScope()) {
    JS::Rooted<JS::Value> v(aCx);
    JSAutoCompartment ac(aCx, scope);
    if (!mozilla::dom::WrapObject(aCx, this, aRetval)) {
      aError.Throw(NS_ERROR_FAILURE);
    }
    return;
  }

  DOMString string;
  GetItemValueText(string);
  if (!xpc::NonVoidStringToJsval(aCx, string, aRetval)) {
    aError.Throw(NS_ERROR_FAILURE);
  }
}

NS_IMETHODIMP
nsGenericHTMLElement::GetItemValue(nsIVariant** aValue)
{
  nsCOMPtr<nsIWritableVariant> out = new nsVariant();

  if (!HasAttr(kNameSpaceID_None, nsGkAtoms::itemprop)) {
    out->SetAsEmpty();
    out.forget(aValue);
    return NS_OK;
  }

  if (ItemScope()) {
    out->SetAsISupports(static_cast<nsIContent*>(this));
  } else {
    DOMString string;
    GetItemValueText(string);
    nsString xpcomString;
    string.ToString(xpcomString);
    out->SetAsAString(xpcomString);
  }

  out.forget(aValue);
  return NS_OK;
}

void
nsGenericHTMLElement::SetItemValue(JSContext* aCx, JS::Value aValue,
                                   ErrorResult& aError)
{
  if (!HasAttr(kNameSpaceID_None, nsGkAtoms::itemprop) ||
      HasAttr(kNameSpaceID_None, nsGkAtoms::itemscope)) {
    aError.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return;
  }

  nsAutoString string;
  JS::Rooted<JS::Value> value(aCx, aValue);
  if (!ConvertJSValueToString(aCx, value, eStringify, eStringify, string)) {
    aError.Throw(NS_ERROR_UNEXPECTED);
    return;
  }
  SetItemValueText(string);
}

NS_IMETHODIMP
nsGenericHTMLElement::SetItemValue(nsIVariant* aValue)
{
  if (!HasAttr(kNameSpaceID_None, nsGkAtoms::itemprop) ||
      HasAttr(kNameSpaceID_None, nsGkAtoms::itemscope)) {
    return NS_ERROR_DOM_INVALID_ACCESS_ERR;
  }

  nsAutoString string;
  aValue->GetAsAString(string);
  SetItemValueText(string);
  return NS_OK;
}

void
nsGenericHTMLElement::GetItemValueText(DOMString& text)
{
  ErrorResult rv;
  GetTextContentInternal(text, rv);
}

void
nsGenericHTMLElement::SetItemValueText(const nsAString& text)
{
  mozilla::ErrorResult rv;
  SetTextContentInternal(text, rv);
}

static void
HTMLPropertiesCollectionDestructor(void *aObject, nsIAtom *aProperty,
                                   void *aPropertyValue, void *aData)
{
  HTMLPropertiesCollection* properties =
    static_cast<HTMLPropertiesCollection*>(aPropertyValue);
  NS_IF_RELEASE(properties);
}

HTMLPropertiesCollection*
nsGenericHTMLElement::Properties()
{
  HTMLPropertiesCollection* properties =
    static_cast<HTMLPropertiesCollection*>(GetProperty(nsGkAtoms::microdataProperties));
  if (!properties) {
     properties = new HTMLPropertiesCollection(this);
     NS_ADDREF(properties);
     SetProperty(nsGkAtoms::microdataProperties, properties, HTMLPropertiesCollectionDestructor);
  }
  return properties;
}

NS_IMETHODIMP
nsGenericHTMLElement::GetProperties(nsISupports** aProperties)
{
  NS_ADDREF(*aProperties = static_cast<nsIHTMLCollection*>(Properties()));
  return NS_OK;
}

nsSize
nsGenericHTMLElement::GetWidthHeightForImage(nsRefPtr<imgRequestProxy>& aImageRequest)
{
  nsSize size(0,0);

  nsIFrame* frame = GetPrimaryFrame(Flush_Layout);

  if (frame) {
    size = frame->GetContentRect().Size();

    size.width = nsPresContext::AppUnitsToIntCSSPixels(size.width);
    size.height = nsPresContext::AppUnitsToIntCSSPixels(size.height);
  } else {
    const nsAttrValue* value;
    nsCOMPtr<imgIContainer> image;
    if (aImageRequest) {
      aImageRequest->GetImage(getter_AddRefs(image));
    }

    if ((value = GetParsedAttr(nsGkAtoms::width)) &&
        value->Type() == nsAttrValue::eInteger) {
      size.width = value->GetIntegerValue();
    } else if (image) {
      image->GetWidth(&size.width);
    }

    if ((value = GetParsedAttr(nsGkAtoms::height)) &&
        value->Type() == nsAttrValue::eInteger) {
      size.height = value->GetIntegerValue();
    } else if (image) {
      image->GetHeight(&size.height);
    }
  }

  NS_ASSERTION(size.width >= 0, "negative width");
  NS_ASSERTION(size.height >= 0, "negative height");
  return size;
}

bool
nsGenericHTMLElement::IsEventAttributeName(nsIAtom *aName)
{
  return nsContentUtils::IsEventAttributeName(aName, EventNameType_HTML);
}

/**
 * Construct a URI from a string, as an element.src attribute
 * would be set to. Helper for the media elements.
 */
nsresult
nsGenericHTMLElement::NewURIFromString(const nsAString& aURISpec,
                                       nsIURI** aURI)
{
  NS_ENSURE_ARG_POINTER(aURI);

  *aURI = nullptr;

  nsCOMPtr<nsIDocument> doc = OwnerDoc();

  nsCOMPtr<nsIURI> baseURI = GetBaseURI();
  nsresult rv = nsContentUtils::NewURIWithDocumentCharset(aURI, aURISpec,
                                                          doc, baseURI);
  NS_ENSURE_SUCCESS(rv, rv);

  bool equal;
  if (aURISpec.IsEmpty() &&
      doc->GetDocumentURI() &&
      NS_SUCCEEDED(doc->GetDocumentURI()->Equals(*aURI, &equal)) &&
      equal) {
    // Assume an element can't point to a fragment of its embedding
    // document. Fail here instead of returning the recursive URI
    // and waiting for the subsequent load to fail.
    NS_RELEASE(*aURI);
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  return NS_OK;
}
