/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/DeclarationBlock.h"
#include "mozilla/EditorBase.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/HTMLEditor.h"
#include "mozilla/IMEContentObserver.h"
#include "mozilla/IMEStateManager.h"
#include "mozilla/MappedDeclarations.h"
#include "mozilla/Maybe.h"
#include "mozilla/Likely.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/PresShell.h"
#include "mozilla/TextEditor.h"
#include "mozilla/TextEvents.h"
#include "mozilla/StaticPrefs_html5.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/StaticPrefs_accessibility.h"

#include "nscore.h"
#include "nsGenericHTMLElement.h"
#include "nsAttrValueInlines.h"
#include "nsCOMPtr.h"
#include "nsAtom.h"
#include "nsQueryObject.h"
#include "nsIContentInlines.h"
#include "mozilla/dom/BindContext.h"
#include "mozilla/dom/Document.h"
#include "nsMappedAttributes.h"
#include "nsHTMLStyleSheet.h"
#include "nsPIDOMWindow.h"
#include "nsEscape.h"
#include "nsIFrameInlines.h"
#include "nsIScrollableFrame.h"
#include "nsView.h"
#include "nsViewManager.h"
#include "nsIWidget.h"
#include "nsRange.h"
#include "nsPresContext.h"
#include "nsNameSpaceManager.h"
#include "nsError.h"
#include "nsIPrincipal.h"
#include "nsContainerFrame.h"
#include "nsStyleUtil.h"
#include "ReferrerInfo.h"

#include "mozilla/PresState.h"
#include "nsILayoutHistoryState.h"

#include "nsHTMLParts.h"
#include "nsContentUtils.h"
#include "mozilla/dom/DirectionalityUtils.h"
#include "mozilla/dom/DocumentOrShadowRoot.h"
#include "nsString.h"
#include "nsUnicharUtils.h"
#include "nsGkAtoms.h"
#include "nsDOMCSSDeclaration.h"
#include "nsITextControlFrame.h"
#include "nsIFormControl.h"
#include "mozilla/dom/HTMLFormElement.h"
#include "nsFocusManager.h"
#include "nsAttrValueOrString.h"

#include "mozilla/InternalMutationEvent.h"
#include "nsDOMStringMap.h"

#include "nsLayoutUtils.h"
#include "mozAutoDocUpdate.h"
#include "nsHtml5Module.h"
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/dom/ElementInlines.h"
#include "HTMLFieldSetElement.h"
#include "nsTextNode.h"
#include "HTMLBRElement.h"
#include "nsDOMMutationObserver.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/FromParser.h"
#include "mozilla/dom/Link.h"
#include "mozilla/BloomFilter.h"
#include "mozilla/dom/ScriptLoader.h"

#include "nsVariant.h"
#include "nsDOMTokenList.h"
#include "nsThreadUtils.h"
#include "nsTextFragment.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/MouseEventBinding.h"
#include "mozilla/dom/TouchEvent.h"
#include "mozilla/ErrorResult.h"
#include "nsHTMLDocument.h"
#include "nsGlobalWindow.h"
#include "mozilla/dom/HTMLBodyElement.h"
#include "imgIContainer.h"
#include "nsComputedDOMStyle.h"
#include "mozilla/dom/HTMLLabelElement.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/dom/CustomElementRegistry.h"
#include "mozilla/dom/ElementInternals.h"

using namespace mozilla;
using namespace mozilla::dom;

static const uint8_t NS_INPUTMODE_NONE = 1;
static const uint8_t NS_INPUTMODE_TEXT = 2;
static const uint8_t NS_INPUTMODE_TEL = 3;
static const uint8_t NS_INPUTMODE_URL = 4;
static const uint8_t NS_INPUTMODE_EMAIL = 5;
static const uint8_t NS_INPUTMODE_NUMERIC = 6;
static const uint8_t NS_INPUTMODE_DECIMAL = 7;
static const uint8_t NS_INPUTMODE_SEARCH = 8;

static const nsAttrValue::EnumTable kInputmodeTable[] = {
    {"none", NS_INPUTMODE_NONE},
    {"text", NS_INPUTMODE_TEXT},
    {"tel", NS_INPUTMODE_TEL},
    {"url", NS_INPUTMODE_URL},
    {"email", NS_INPUTMODE_EMAIL},
    {"numeric", NS_INPUTMODE_NUMERIC},
    {"decimal", NS_INPUTMODE_DECIMAL},
    {"search", NS_INPUTMODE_SEARCH},
    {nullptr, 0}};

static const uint8_t NS_ENTERKEYHINT_ENTER = 1;
static const uint8_t NS_ENTERKEYHINT_DONE = 2;
static const uint8_t NS_ENTERKEYHINT_GO = 3;
static const uint8_t NS_ENTERKEYHINT_NEXT = 4;
static const uint8_t NS_ENTERKEYHINT_PREVIOUS = 5;
static const uint8_t NS_ENTERKEYHINT_SEARCH = 6;
static const uint8_t NS_ENTERKEYHINT_SEND = 7;

static const nsAttrValue::EnumTable kEnterKeyHintTable[] = {
    {"enter", NS_ENTERKEYHINT_ENTER},
    {"done", NS_ENTERKEYHINT_DONE},
    {"go", NS_ENTERKEYHINT_GO},
    {"next", NS_ENTERKEYHINT_NEXT},
    {"previous", NS_ENTERKEYHINT_PREVIOUS},
    {"search", NS_ENTERKEYHINT_SEARCH},
    {"send", NS_ENTERKEYHINT_SEND},
    {nullptr, 0}};

static const uint8_t NS_AUTOCAPITALIZE_NONE = 1;
static const uint8_t NS_AUTOCAPITALIZE_SENTENCES = 2;
static const uint8_t NS_AUTOCAPITALIZE_WORDS = 3;
static const uint8_t NS_AUTOCAPITALIZE_CHARACTERS = 4;

static const nsAttrValue::EnumTable kAutocapitalizeTable[] = {
    {"none", NS_AUTOCAPITALIZE_NONE},
    {"sentences", NS_AUTOCAPITALIZE_SENTENCES},
    {"words", NS_AUTOCAPITALIZE_WORDS},
    {"characters", NS_AUTOCAPITALIZE_CHARACTERS},
    {"off", NS_AUTOCAPITALIZE_NONE},
    {"on", NS_AUTOCAPITALIZE_SENTENCES},
    {"", 0},
    {nullptr, 0}};

static const nsAttrValue::EnumTable* kDefaultAutocapitalize =
    &kAutocapitalizeTable[1];

nsresult nsGenericHTMLElement::CopyInnerTo(Element* aDst) {
  MOZ_ASSERT(!aDst->GetUncomposedDoc(),
             "Should not CopyInnerTo an Element in a document");

  auto reparse = aDst->OwnerDoc() == OwnerDoc() ? ReparseAttributes::No
                                                : ReparseAttributes::Yes;
  nsresult rv = Element::CopyInnerTo(aDst, reparse);
  NS_ENSURE_SUCCESS(rv, rv);

  // cloning a node must retain its internal nonce slot
  nsString* nonce = static_cast<nsString*>(GetProperty(nsGkAtoms::nonce));
  if (nonce) {
    static_cast<nsGenericHTMLElement*>(aDst)->SetNonce(*nonce);
  }
  return NS_OK;
}

static const nsAttrValue::EnumTable kDirTable[] = {
    {"ltr", eDir_LTR}, {"rtl", eDir_RTL}, {"auto", eDir_Auto}, {nullptr, 0}};

void nsGenericHTMLElement::AddToNameTable(nsAtom* aName) {
  MOZ_ASSERT(HasName(), "Node doesn't have name?");
  Document* doc = GetUncomposedDoc();
  if (doc && !IsInNativeAnonymousSubtree()) {
    doc->AddToNameTable(this, aName);
  }
}

void nsGenericHTMLElement::RemoveFromNameTable() {
  if (HasName() && CanHaveName(NodeInfo()->NameAtom())) {
    if (Document* doc = GetUncomposedDoc()) {
      doc->RemoveFromNameTable(this,
                               GetParsedAttr(nsGkAtoms::name)->GetAtomValue());
    }
  }
}

void nsGenericHTMLElement::GetAccessKeyLabel(nsString& aLabel) {
  nsAutoString suffix;
  GetAccessKey(suffix);
  if (!suffix.IsEmpty()) {
    EventStateManager::GetAccessKeyLabelPrefix(this, aLabel);
    aLabel.Append(suffix);
  }
}

static bool IsOffsetParent(nsIFrame* aFrame) {
  LayoutFrameType frameType = aFrame->Type();

  if (frameType == LayoutFrameType::TableCell ||
      frameType == LayoutFrameType::Table) {
    // Per the IDL for Element, only td, th, and table are acceptable
    // offsetParents apart from body or positioned elements; we need to check
    // the content type as well as the frame type so we ignore anonymous tables
    // created by an element with display: table-cell with no actual table
    nsIContent* content = aFrame->GetContent();

    return content->IsAnyOfHTMLElements(nsGkAtoms::table, nsGkAtoms::td,
                                        nsGkAtoms::th);
  }
  return false;
}

struct OffsetResult {
  Element* mParent = nullptr;
  CSSIntRect mRect;
};

static OffsetResult GetUnretargetedOffsetsFor(const Element& aElement) {
  nsIFrame* frame = aElement.GetPrimaryFrame();
  if (!frame) {
    return {};
  }

  nsIFrame* styleFrame = nsLayoutUtils::GetStyleFrame(frame);

  nsIFrame* parent = frame->GetParent();
  nsPoint origin(0, 0);

  nsIContent* offsetParent = nullptr;
  Element* docElement = aElement.GetComposedDoc()->GetRootElement();
  nsIContent* content = frame->GetContent();

  if (content &&
      (content->IsHTMLElement(nsGkAtoms::body) || content == docElement)) {
    parent = frame;
  } else {
    const bool isPositioned = styleFrame->IsAbsPosContainingBlock();
    const bool isAbsolutelyPositioned = frame->IsAbsolutelyPositioned();
    origin += frame->GetPositionIgnoringScrolling();

    for (; parent; parent = parent->GetParent()) {
      content = parent->GetContent();

      // Stop at the first ancestor that is positioned.
      if (parent->IsAbsPosContainingBlock()) {
        offsetParent = content;
        break;
      }

      // Add the parent's origin to our own to get to the
      // right coordinate system.
      const bool isOffsetParent = !isPositioned && IsOffsetParent(parent);
      if (!isOffsetParent) {
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
      //
      // We use GetBodyElement() here, not GetBody(), because we don't want to
      // end up with framesets here.
      offsetParent = aElement.GetComposedDoc()->GetBodyElement();
    }
  }

  // Make the position relative to the padding edge.
  if (parent) {
    const nsStyleBorder* border = parent->StyleBorder();
    origin.x -= border->GetComputedBorderWidth(eSideLeft);
    origin.y -= border->GetComputedBorderWidth(eSideTop);
  }

  // Get the union of all rectangles in this and continuation frames.
  // It doesn't really matter what we use as aRelativeTo here, since
  // we only care about the size. We just have to use something non-null.
  nsRect rcFrame = nsLayoutUtils::GetAllInFlowRectsUnion(frame, frame);
  rcFrame.MoveTo(origin);
  return {Element::FromNodeOrNull(offsetParent),
          CSSIntRect::FromAppUnitsRounded(rcFrame)};
}

static bool ShouldBeRetargeted(const Element& aReferenceElement,
                               const Element& aElementToMaybeRetarget) {
  ShadowRoot* shadow = aElementToMaybeRetarget.GetContainingShadow();
  if (!shadow) {
    return false;
  }
  for (ShadowRoot* scope = aReferenceElement.GetContainingShadow(); scope;
       scope = scope->Host()->GetContainingShadow()) {
    if (scope == shadow) {
      return false;
    }
  }

  return true;
}

Element* nsGenericHTMLElement::GetOffsetRect(CSSIntRect& aRect) {
  aRect = CSSIntRect();

  if (!GetPrimaryFrame(FlushType::Layout)) {
    return nullptr;
  }

  OffsetResult thisResult = GetUnretargetedOffsetsFor(*this);
  aRect = thisResult.mRect;

  Element* parent = thisResult.mParent;
  while (parent && ShouldBeRetargeted(*this, *parent)) {
    OffsetResult result = GetUnretargetedOffsetsFor(*parent);
    aRect += result.mRect.TopLeft();
    parent = result.mParent;
  }

  return parent;
}

bool nsGenericHTMLElement::Spellcheck() {
  // Has the state has been explicitly set?
  nsIContent* node;
  for (node = this; node; node = node->GetParent()) {
    if (node->IsHTMLElement()) {
      static Element::AttrValuesArray strings[] = {nsGkAtoms::_true,
                                                   nsGkAtoms::_false, nullptr};
      switch (node->AsElement()->FindAttrValueIn(
          kNameSpaceID_None, nsGkAtoms::spellcheck, strings, eCaseMatters)) {
        case 0:  // spellcheck = "true"
          return true;
        case 1:  // spellcheck = "false"
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
    return false;  // Not spellchecked by default
  }

  // Anything else that's not a form control is not spellchecked by default
  nsCOMPtr<nsIFormControl> formControl = do_QueryObject(this);
  if (!formControl) {
    return false;  // Not spellchecked by default
  }

  // Is this a multiline plaintext input?
  auto controlType = formControl->ControlType();
  if (controlType == FormControlType::Textarea) {
    return true;  // Spellchecked by default
  }

  // Is this anything other than an input text?
  // Other inputs are not spellchecked.
  if (controlType != FormControlType::InputText) {
    return false;  // Not spellchecked by default
  }

  // Does the user want input text spellchecked by default?
  // NOTE: Do not reflect a pref value of 0 back to the DOM getter.
  // The web page should not know if the user has disabled spellchecking.
  // We'll catch this in the editor itself.
  int32_t spellcheckLevel = Preferences::GetInt("layout.spellcheckDefault", 1);
  return spellcheckLevel == 2;  // "Spellcheck multi- and single-line"
}

bool nsGenericHTMLElement::InNavQuirksMode(Document* aDoc) {
  return aDoc && aDoc->GetCompatibilityMode() == eCompatibility_NavQuirks;
}

void nsGenericHTMLElement::UpdateEditableState(bool aNotify) {
  // XXX Should we do this only when in a document?
  ContentEditableTristate value = GetContentEditableValue();
  if (value != eInherit) {
    DoSetEditableFlag(!!value, aNotify);
    return;
  }

  nsStyledElement::UpdateEditableState(aNotify);
}

ElementState nsGenericHTMLElement::IntrinsicState() const {
  ElementState state = nsGenericHTMLElementBase::IntrinsicState();

  if (GetDirectionality() == eDir_RTL) {
    state |= ElementState::RTL;
    state &= ~ElementState::LTR;
  } else {  // at least for HTML, directionality is exclusively LTR or RTL
    NS_ASSERTION(GetDirectionality() == eDir_LTR,
                 "HTML element's directionality must be either RTL or LTR");
    state |= ElementState::LTR;
    state &= ~ElementState::RTL;
  }

  return state;
}

nsresult nsGenericHTMLElement::BindToTree(BindContext& aContext,
                                          nsINode& aParent) {
  nsresult rv = nsGenericHTMLElementBase::BindToTree(aContext, aParent);
  NS_ENSURE_SUCCESS(rv, rv);

  if (IsInComposedDoc()) {
    RegUnRegAccessKey(true);
  }

  if (IsInUncomposedDoc()) {
    if (HasName() && CanHaveName(NodeInfo()->NameAtom())) {
      aContext.OwnerDoc().AddToNameTable(
          this, GetParsedAttr(nsGkAtoms::name)->GetAtomValue());
    }
  }

  if (HasFlag(NODE_IS_EDITABLE) && GetContentEditableValue() == eTrue &&
      IsInComposedDoc()) {
    aContext.OwnerDoc().ChangeContentEditableCount(this, +1);
  }

  // Hide any nonce from the DOM, but keep the internal value of the
  // nonce by copying and resetting the internal nonce value.
  if (HasFlag(NODE_HAS_NONCE_AND_HEADER_CSP) && IsInComposedDoc() &&
      OwnerDoc()->GetBrowsingContext()) {
    nsContentUtils::AddScriptRunner(NS_NewRunnableFunction(
        "nsGenericHTMLElement::ResetNonce::Runnable",
        [self = RefPtr<nsGenericHTMLElement>(this)]() {
          nsAutoString nonce;
          self->GetNonce(nonce);
          self->SetAttr(kNameSpaceID_None, nsGkAtoms::nonce, u""_ns, true);
          self->SetNonce(nonce);
        }));
  }

  // We need to consider a labels element is moved to another subtree
  // with different root, it needs to update labels list and its root
  // as well.
  nsExtendedDOMSlots* slots = GetExistingExtendedDOMSlots();
  if (slots && slots->mLabelsList) {
    slots->mLabelsList->MaybeResetRoot(SubtreeRoot());
  }

  return rv;
}

void nsGenericHTMLElement::UnbindFromTree(bool aNullParent) {
  if (IsInComposedDoc()) {
    RegUnRegAccessKey(false);
  }

  RemoveFromNameTable();

  if (GetContentEditableValue() == eTrue) {
    Document* doc = GetComposedDoc();
    if (doc) {
      doc->ChangeContentEditableCount(this, -1);
    }
  }

  nsStyledElement::UnbindFromTree(aNullParent);

  // Invalidate .labels list. It will be repopulated when used the next time.
  nsExtendedDOMSlots* slots = GetExistingExtendedDOMSlots();
  if (slots && slots->mLabelsList) {
    slots->mLabelsList->MaybeResetRoot(SubtreeRoot());
  }
}

HTMLFormElement* nsGenericHTMLElement::FindAncestorForm(
    HTMLFormElement* aCurrentForm) {
  NS_ASSERTION(!HasAttr(kNameSpaceID_None, nsGkAtoms::form) ||
                   IsHTMLElement(nsGkAtoms::img),
               "FindAncestorForm should not be called if @form is set!");
  if (IsInNativeAnonymousSubtree()) {
    return nullptr;
  }

  nsIContent* content = this;
  while (content) {
    // If the current ancestor is a form, return it as our form
    if (content->IsHTMLElement(nsGkAtoms::form)) {
#ifdef DEBUG
      if (!nsContentUtils::IsInSameAnonymousTree(this, content)) {
        // It's possible that we started unbinding at |content| or
        // some ancestor of it, and |content| and |this| used to all be
        // anonymous.  Check for this the hard way.
        for (nsIContent* child = this; child != content;
             child = child->GetParent()) {
          NS_ASSERTION(child->ComputeIndexInParentContent().isSome(),
                       "Walked too far?");
        }
      }
#endif
      return static_cast<HTMLFormElement*>(content);
    }

    nsIContent* prevContent = content;
    content = prevContent->GetParent();

    if (!content && aCurrentForm) {
      // We got to the root of the subtree we're in, and we're being removed
      // from the DOM (the only time we get into this method with a non-null
      // aCurrentForm).  Check whether aCurrentForm is in the same subtree.  If
      // it is, we want to return aCurrentForm, since this case means that
      // we're one of those inputs-in-a-table that have a hacked mForm pointer
      // and a subtree containing both us and the form got removed from the
      // DOM.
      if (aCurrentForm->IsInclusiveDescendantOf(prevContent)) {
        return aCurrentForm;
      }
    }
  }

  return nullptr;
}

bool nsGenericHTMLElement::CheckHandleEventForAnchorsPreconditions(
    EventChainVisitor& aVisitor) {
  MOZ_ASSERT(nsCOMPtr<Link>(do_QueryObject(this)),
             "should be called only when |this| implements |Link|");
  // When disconnected, only <a> should navigate away per
  // https://html.spec.whatwg.org/#cannot-navigate
  return IsInComposedDoc() || IsHTMLElement(nsGkAtoms::a);
}

void nsGenericHTMLElement::GetEventTargetParentForAnchors(
    EventChainPreVisitor& aVisitor) {
  nsGenericHTMLElementBase::GetEventTargetParent(aVisitor);

  if (!CheckHandleEventForAnchorsPreconditions(aVisitor)) {
    return;
  }

  GetEventTargetParentForLinks(aVisitor);
}

nsresult nsGenericHTMLElement::PostHandleEventForAnchors(
    EventChainPostVisitor& aVisitor) {
  if (!CheckHandleEventForAnchorsPreconditions(aVisitor)) {
    return NS_OK;
  }

  return PostHandleEventForLinks(aVisitor);
}

bool nsGenericHTMLElement::IsHTMLLink(nsIURI** aURI) const {
  MOZ_ASSERT(aURI, "Must provide aURI out param");

  *aURI = GetHrefURIForAnchors().take();
  // We promise out param is non-null if we return true, so base rv on it
  return *aURI != nullptr;
}

already_AddRefed<nsIURI> nsGenericHTMLElement::GetHrefURIForAnchors() const {
  // This is used by the three Link implementations and
  // nsHTMLStyleElement.

  // Get href= attribute (relative URI).

  // We use the nsAttrValue's copy of the URI string to avoid copying.
  nsCOMPtr<nsIURI> uri;
  GetURIAttr(nsGkAtoms::href, nullptr, getter_AddRefs(uri));

  return uri.forget();
}

nsresult nsGenericHTMLElement::BeforeSetAttr(int32_t aNamespaceID,
                                             nsAtom* aName,
                                             const nsAttrValueOrString* aValue,
                                             bool aNotify) {
  if (aNamespaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::accesskey) {
      // Have to unregister before clearing flag. See UnregAccessKey
      RegUnRegAccessKey(false);
      if (!aValue) {
        UnsetFlags(NODE_HAS_ACCESSKEY);
      }
    } else if (aName == nsGkAtoms::name) {
      // Have to do this before clearing flag. See RemoveFromNameTable
      RemoveFromNameTable();
      if (!aValue || aValue->IsEmpty()) {
        ClearHasName();
      }
    } else if (aName == nsGkAtoms::contenteditable) {
      if (aValue) {
        // Set this before the attribute is set so that any subclass code that
        // runs before the attribute is set won't think we're missing a
        // contenteditable attr when we actually have one.
        SetMayHaveContentEditableAttr();
      }
    }
    if (!aValue && IsEventAttributeName(aName)) {
      if (EventListenerManager* manager = GetExistingListenerManager()) {
        manager->RemoveEventHandler(GetEventNameForAttr(aName));
      }
    }
  }

  return nsGenericHTMLElementBase::BeforeSetAttr(aNamespaceID, aName, aValue,
                                                 aNotify);
}

nsresult nsGenericHTMLElement::AfterSetAttr(
    int32_t aNamespaceID, nsAtom* aName, const nsAttrValue* aValue,
    const nsAttrValue* aOldValue, nsIPrincipal* aMaybeScriptedPrincipal,
    bool aNotify) {
  if (aNamespaceID == kNameSpaceID_None) {
    if (IsEventAttributeName(aName) && aValue) {
      MOZ_ASSERT(aValue->Type() == nsAttrValue::eString,
                 "Expected string value for script body");
      SetEventHandler(GetEventNameForAttr(aName), aValue->GetStringValue());
    } else if (aNotify && aName == nsGkAtoms::spellcheck) {
      SyncEditorsOnSubtree(this);
    } else if (aName == nsGkAtoms::dir) {
      Directionality dir = eDir_LTR;
      // A boolean tracking whether we need to recompute our directionality.
      // This needs to happen after we update our internal "dir" attribute
      // state but before we call SetDirectionalityOnDescendants.
      bool recomputeDirectionality = false;
      // We don't want to have to keep getting the "dir" attribute in
      // IntrinsicState, so we manually recompute our dir-related event states
      // here and send the relevant update notifications.
      ElementState dirStates;
      if (aValue && aValue->Type() == nsAttrValue::eEnum) {
        SetHasValidDir();
        dirStates |= ElementState::HAS_DIR_ATTR;
        Directionality dirValue = (Directionality)aValue->GetEnumValue();
        if (dirValue == eDir_Auto) {
          dirStates |= ElementState::HAS_DIR_ATTR_LIKE_AUTO;
        } else {
          dir = dirValue;
          SetDirectionality(dir, aNotify);
          if (dirValue == eDir_LTR) {
            dirStates |= ElementState::HAS_DIR_ATTR_LTR;
          } else {
            MOZ_ASSERT(dirValue == eDir_RTL);
            dirStates |= ElementState::HAS_DIR_ATTR_RTL;
          }
        }
      } else {
        if (aValue) {
          // We have a value, just not a valid one.
          dirStates |= ElementState::HAS_DIR_ATTR;
        }
        ClearHasValidDir();
        if (NodeInfo()->Equals(nsGkAtoms::bdi)) {
          dirStates |= ElementState::HAS_DIR_ATTR_LIKE_AUTO;
        } else {
          recomputeDirectionality = true;
        }
      }
      // Now figure out what's changed about our dir states.
      ElementState oldDirStates = State() & ElementState::DIR_ATTR_STATES;
      ElementState changedStates = dirStates ^ oldDirStates;
      ToggleStates(changedStates, aNotify);
      if (recomputeDirectionality) {
        dir = RecomputeDirectionality(this, aNotify);
      }
      SetDirectionalityOnDescendants(this, dir, aNotify);
    } else if (aName == nsGkAtoms::contenteditable) {
      int32_t editableCountDelta = 0;
      if (aOldValue && (aOldValue->Equals(u"true"_ns, eIgnoreCase) ||
                        aOldValue->Equals(u""_ns, eIgnoreCase))) {
        editableCountDelta = -1;
      }
      if (aValue && (aValue->Equals(u"true"_ns, eIgnoreCase) ||
                     aValue->Equals(u""_ns, eIgnoreCase))) {
        ++editableCountDelta;
      }
      ChangeEditableState(editableCountDelta);
    } else if (aName == nsGkAtoms::accesskey) {
      if (aValue && !aValue->Equals(u""_ns, eIgnoreCase)) {
        SetFlags(NODE_HAS_ACCESSKEY);
        RegUnRegAccessKey(true);
      }
    } else if (aName == nsGkAtoms::inert &&
               StaticPrefs::html5_inert_enabled()) {
      if (aValue) {
        AddStates(ElementState::INERT);
      } else {
        RemoveStates(ElementState::INERT);
      }
    } else if (aName == nsGkAtoms::name) {
      if (aValue && !aValue->Equals(u""_ns, eIgnoreCase)) {
        // This may not be quite right because we can have subclass code run
        // before here. But in practice subclasses don't care about this flag,
        // and in particular selector matching does not care.  Otherwise we'd
        // want to handle it like we handle id attributes (in PreIdMaybeChange
        // and PostIdMaybeChange).
        SetHasName();
        if (CanHaveName(NodeInfo()->NameAtom())) {
          AddToNameTable(aValue->GetAtomValue());
        }
      }
    } else if ((aName == nsGkAtoms::inputmode &&
                StaticPrefs::dom_forms_inputmode()) ||
               (aName == nsGkAtoms::enterkeyhint &&
                StaticPrefs::dom_forms_enterkeyhint())) {
      nsPIDOMWindowOuter* window = OwnerDoc()->GetWindow();
      if (window && window->GetFocusedElement() == this) {
        if (IMEContentObserver* observer =
                IMEStateManager::GetActiveContentObserver()) {
          if (const nsPresContext* presContext =
                  GetPresContext(eForComposedDoc)) {
            if (observer->IsManaging(*presContext, this)) {
              if (RefPtr<EditorBase> editor =
                      nsContentUtils::GetActiveEditor(window)) {
                IMEState newState;
                editor->GetPreferredIMEState(&newState);
                OwningNonNull<nsGenericHTMLElement> kungFuDeathGrip(*this);
                IMEStateManager::UpdateIMEState(
                    newState, kungFuDeathGrip, *editor,
                    {IMEStateManager::UpdateIMEStateOption::ForceUpdate,
                     IMEStateManager::UpdateIMEStateOption::
                         DontCommitComposition});
              }
            }
          }
        }
      }
    }

    // The nonce will be copied over to an internal slot and cleared from the
    // Element within BindToTree to avoid CSS Selector nonce exfiltration if
    // the CSP list contains a header-delivered CSP.
    if (nsGkAtoms::nonce == aName) {
      if (aValue) {
        SetNonce(aValue->GetStringValue());
        if (OwnerDoc()->GetHasCSPDeliveredThroughHeader()) {
          SetFlags(NODE_HAS_NONCE_AND_HEADER_CSP);
        }
      } else {
        RemoveNonce();
      }
    }
  }

  return nsGenericHTMLElementBase::AfterSetAttr(
      aNamespaceID, aName, aValue, aOldValue, aMaybeScriptedPrincipal, aNotify);
}

EventListenerManager* nsGenericHTMLElement::GetEventListenerManagerForAttr(
    nsAtom* aAttrName, bool* aDefer) {
  // Attributes on the body and frameset tags get set on the global object
  if ((mNodeInfo->Equals(nsGkAtoms::body) ||
       mNodeInfo->Equals(nsGkAtoms::frameset)) &&
      // We only forward some event attributes from body/frameset to window
      (0
#define EVENT(name_, id_, type_, struct_) /* nothing */
#define FORWARDED_EVENT(name_, id_, type_, struct_) \
  || nsGkAtoms::on##name_ == aAttrName
#define WINDOW_EVENT FORWARDED_EVENT
#include "mozilla/EventNameList.h"  // IWYU pragma: keep
#undef WINDOW_EVENT
#undef FORWARDED_EVENT
#undef EVENT
       )) {
    nsPIDOMWindowInner* win;

    // If we have a document, and it has a window, add the event
    // listener on the window (the inner window). If not, proceed as
    // normal.
    // XXXbz sXBL/XBL2 issue: should we instead use GetComposedDoc() here,
    // override BindToTree for those classes and munge event listeners there?
    Document* document = OwnerDoc();

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
#define FORWARDED_EVENT(name_, id_, type_, struct_)                       \
  EventHandlerNonNull* nsGenericHTMLElement::GetOn##name_() {             \
    if (IsAnyOfHTMLElements(nsGkAtoms::body, nsGkAtoms::frameset)) {      \
      /* XXXbz note to self: add tests for this! */                       \
      if (nsPIDOMWindowInner* win = OwnerDoc()->GetInnerWindow()) {       \
        nsGlobalWindowInner* globalWin = nsGlobalWindowInner::Cast(win);  \
        return globalWin->GetOn##name_();                                 \
      }                                                                   \
      return nullptr;                                                     \
    }                                                                     \
                                                                          \
    return nsINode::GetOn##name_();                                       \
  }                                                                       \
  void nsGenericHTMLElement::SetOn##name_(EventHandlerNonNull* handler) { \
    if (IsAnyOfHTMLElements(nsGkAtoms::body, nsGkAtoms::frameset)) {      \
      nsPIDOMWindowInner* win = OwnerDoc()->GetInnerWindow();             \
      if (!win) {                                                         \
        return;                                                           \
      }                                                                   \
                                                                          \
      nsGlobalWindowInner* globalWin = nsGlobalWindowInner::Cast(win);    \
      return globalWin->SetOn##name_(handler);                            \
    }                                                                     \
                                                                          \
    return nsINode::SetOn##name_(handler);                                \
  }
#define ERROR_EVENT(name_, id_, type_, struct_)                                \
  already_AddRefed<EventHandlerNonNull> nsGenericHTMLElement::GetOn##name_() { \
    if (IsAnyOfHTMLElements(nsGkAtoms::body, nsGkAtoms::frameset)) {           \
      /* XXXbz note to self: add tests for this! */                            \
      if (nsPIDOMWindowInner* win = OwnerDoc()->GetInnerWindow()) {            \
        nsGlobalWindowInner* globalWin = nsGlobalWindowInner::Cast(win);       \
        OnErrorEventHandlerNonNull* errorHandler = globalWin->GetOn##name_();  \
        if (errorHandler) {                                                    \
          RefPtr<EventHandlerNonNull> handler =                                \
              new EventHandlerNonNull(errorHandler);                           \
          return handler.forget();                                             \
        }                                                                      \
      }                                                                        \
      return nullptr;                                                          \
    }                                                                          \
                                                                               \
    RefPtr<EventHandlerNonNull> handler = nsINode::GetOn##name_();             \
    return handler.forget();                                                   \
  }                                                                            \
  void nsGenericHTMLElement::SetOn##name_(EventHandlerNonNull* handler) {      \
    if (IsAnyOfHTMLElements(nsGkAtoms::body, nsGkAtoms::frameset)) {           \
      nsPIDOMWindowInner* win = OwnerDoc()->GetInnerWindow();                  \
      if (!win) {                                                              \
        return;                                                                \
      }                                                                        \
                                                                               \
      nsGlobalWindowInner* globalWin = nsGlobalWindowInner::Cast(win);         \
      RefPtr<OnErrorEventHandlerNonNull> errorHandler;                         \
      if (handler) {                                                           \
        errorHandler = new OnErrorEventHandlerNonNull(handler);                \
      }                                                                        \
      return globalWin->SetOn##name_(errorHandler);                            \
    }                                                                          \
                                                                               \
    return nsINode::SetOn##name_(handler);                                     \
  }
#include "mozilla/EventNameList.h"  // IWYU pragma: keep
#undef ERROR_EVENT
#undef FORWARDED_EVENT
#undef EVENT

void nsGenericHTMLElement::GetBaseTarget(nsAString& aBaseTarget) const {
  OwnerDoc()->GetBaseTarget(aBaseTarget);
}

//----------------------------------------------------------------------

bool nsGenericHTMLElement::ParseAttribute(int32_t aNamespaceID,
                                          nsAtom* aAttribute,
                                          const nsAString& aValue,
                                          nsIPrincipal* aMaybeScriptedPrincipal,
                                          nsAttrValue& aResult) {
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::dir) {
      return aResult.ParseEnumValue(aValue, kDirTable, false);
    }

    if (aAttribute == nsGkAtoms::tabindex) {
      return aResult.ParseIntValue(aValue);
    }

    if (aAttribute == nsGkAtoms::referrerpolicy) {
      return ParseReferrerAttribute(aValue, aResult);
    }

    if (aAttribute == nsGkAtoms::name) {
      // Store name as an atom.  name="" means that the element has no name,
      // not that it has an empty string as the name.
      if (aValue.IsEmpty()) {
        return false;
      }
      aResult.ParseAtom(aValue);
      return true;
    }

    if (aAttribute == nsGkAtoms::contenteditable) {
      aResult.ParseAtom(aValue);
      return true;
    }

    if (aAttribute == nsGkAtoms::rel) {
      aResult.ParseAtomArray(aValue);
      return true;
    }

    if (aAttribute == nsGkAtoms::inputmode) {
      return aResult.ParseEnumValue(aValue, kInputmodeTable, false);
    }

    if (aAttribute == nsGkAtoms::enterkeyhint) {
      return aResult.ParseEnumValue(aValue, kEnterKeyHintTable, false);
    }

    if (aAttribute == nsGkAtoms::autocapitalize) {
      return aResult.ParseEnumValue(aValue, kAutocapitalizeTable, false);
    }
  }

  return nsGenericHTMLElementBase::ParseAttribute(
      aNamespaceID, aAttribute, aValue, aMaybeScriptedPrincipal, aResult);
}

bool nsGenericHTMLElement::ParseBackgroundAttribute(int32_t aNamespaceID,
                                                    nsAtom* aAttribute,
                                                    const nsAString& aValue,
                                                    nsAttrValue& aResult) {
  if (aNamespaceID == kNameSpaceID_None &&
      aAttribute == nsGkAtoms::background && !aValue.IsEmpty()) {
    // Resolve url to an absolute url
    Document* doc = OwnerDoc();
    nsCOMPtr<nsIURI> uri;
    nsresult rv = nsContentUtils::NewURIWithDocumentCharset(
        getter_AddRefs(uri), aValue, doc, GetBaseURI());
    if (NS_FAILED(rv)) {
      return false;
    }
    aResult.SetTo(uri, &aValue);
    return true;
  }

  return false;
}

bool nsGenericHTMLElement::IsAttributeMapped(const nsAtom* aAttribute) const {
  static const MappedAttributeEntry* const map[] = {sCommonAttributeMap};

  return FindAttributeDependence(aAttribute, map);
}

nsMapRuleToAttributesFunc nsGenericHTMLElement::GetAttributeMappingFunction()
    const {
  return &MapCommonAttributesInto;
}

nsIFormControlFrame* nsGenericHTMLElement::GetFormControlFrame(
    bool aFlushFrames) {
  auto flushType = aFlushFrames ? FlushType::Frames : FlushType::None;
  nsIFrame* frame = GetPrimaryFrame(flushType);
  if (!frame) {
    return nullptr;
  }

  if (nsIFormControlFrame* f = do_QueryFrame(frame)) {
    return f;
  }

  // If we have generated content, the primary frame will be a wrapper frame...
  // Our real frame will be in its child list.
  //
  // FIXME(emilio): I don't think that's true... See bug 155957 for test-cases
  // though, we should figure out whether this is still needed.
  for (nsIFrame* kid : frame->PrincipalChildList()) {
    if (nsIFormControlFrame* f = do_QueryFrame(kid)) {
      return f;
    }
  }

  return nullptr;
}

static const nsAttrValue::EnumTable kDivAlignTable[] = {
    {"left", StyleTextAlign::MozLeft},
    {"right", StyleTextAlign::MozRight},
    {"center", StyleTextAlign::MozCenter},
    {"middle", StyleTextAlign::MozCenter},
    {"justify", StyleTextAlign::Justify},
    {nullptr, 0}};

static const nsAttrValue::EnumTable kFrameborderTable[] = {
    {"yes", NS_STYLE_FRAME_YES},
    {"no", NS_STYLE_FRAME_NO},
    {"1", NS_STYLE_FRAME_1},
    {"0", NS_STYLE_FRAME_0},
    {nullptr, 0}};

// TODO(emilio): Nobody uses the parsed attribute here.
static const nsAttrValue::EnumTable kScrollingTable[] = {
    {"yes", NS_STYLE_FRAME_YES},       {"no", NS_STYLE_FRAME_NO},
    {"on", NS_STYLE_FRAME_ON},         {"off", NS_STYLE_FRAME_OFF},
    {"scroll", NS_STYLE_FRAME_SCROLL}, {"noscroll", NS_STYLE_FRAME_NOSCROLL},
    {"auto", NS_STYLE_FRAME_AUTO},     {nullptr, 0}};

static const nsAttrValue::EnumTable kTableVAlignTable[] = {
    {"top", StyleVerticalAlignKeyword::Top},
    {"middle", StyleVerticalAlignKeyword::Middle},
    {"bottom", StyleVerticalAlignKeyword::Bottom},
    {"baseline", StyleVerticalAlignKeyword::Baseline},
    {nullptr, 0}};

bool nsGenericHTMLElement::ParseAlignValue(const nsAString& aString,
                                           nsAttrValue& aResult) {
  static const nsAttrValue::EnumTable kAlignTable[] = {
      {"left", StyleTextAlign::Left},
      {"right", StyleTextAlign::Right},

      {"top", StyleVerticalAlignKeyword::Top},
      {"middle", StyleVerticalAlignKeyword::MozMiddleWithBaseline},

      // Intentionally not bottom.
      {"bottom", StyleVerticalAlignKeyword::Baseline},

      {"center", StyleVerticalAlignKeyword::MozMiddleWithBaseline},
      {"baseline", StyleVerticalAlignKeyword::Baseline},

      {"texttop", StyleVerticalAlignKeyword::TextTop},
      {"absmiddle", StyleVerticalAlignKeyword::Middle},
      {"abscenter", StyleVerticalAlignKeyword::Middle},
      {"absbottom", StyleVerticalAlignKeyword::Bottom},
      {nullptr, 0}};

  static_assert(uint8_t(StyleTextAlign::Left) !=
                    uint8_t(StyleVerticalAlignKeyword::Top) &&
                uint8_t(StyleTextAlign::Left) !=
                    uint8_t(StyleVerticalAlignKeyword::MozMiddleWithBaseline) &&
                uint8_t(StyleTextAlign::Left) !=
                    uint8_t(StyleVerticalAlignKeyword::Baseline) &&
                uint8_t(StyleTextAlign::Left) !=
                    uint8_t(StyleVerticalAlignKeyword::TextTop) &&
                uint8_t(StyleTextAlign::Left) !=
                    uint8_t(StyleVerticalAlignKeyword::Middle) &&
                uint8_t(StyleTextAlign::Left) !=
                    uint8_t(StyleVerticalAlignKeyword::Bottom));

  static_assert(uint8_t(StyleTextAlign::Right) !=
                    uint8_t(StyleVerticalAlignKeyword::Top) &&
                uint8_t(StyleTextAlign::Right) !=
                    uint8_t(StyleVerticalAlignKeyword::MozMiddleWithBaseline) &&
                uint8_t(StyleTextAlign::Right) !=
                    uint8_t(StyleVerticalAlignKeyword::Baseline) &&
                uint8_t(StyleTextAlign::Right) !=
                    uint8_t(StyleVerticalAlignKeyword::TextTop) &&
                uint8_t(StyleTextAlign::Right) !=
                    uint8_t(StyleVerticalAlignKeyword::Middle) &&
                uint8_t(StyleTextAlign::Right) !=
                    uint8_t(StyleVerticalAlignKeyword::Bottom));

  return aResult.ParseEnumValue(aString, kAlignTable, false);
}

//----------------------------------------

static const nsAttrValue::EnumTable kTableHAlignTable[] = {
    {"left", StyleTextAlign::Left},       {"right", StyleTextAlign::Right},
    {"center", StyleTextAlign::Center},   {"char", StyleTextAlign::Char},
    {"justify", StyleTextAlign::Justify}, {nullptr, 0}};

bool nsGenericHTMLElement::ParseTableHAlignValue(const nsAString& aString,
                                                 nsAttrValue& aResult) {
  return aResult.ParseEnumValue(aString, kTableHAlignTable, false);
}

//----------------------------------------

// This table is used for td, th, tr, col, thead, tbody and tfoot.
static const nsAttrValue::EnumTable kTableCellHAlignTable[] = {
    {"left", StyleTextAlign::MozLeft},
    {"right", StyleTextAlign::MozRight},
    {"center", StyleTextAlign::MozCenter},
    {"char", StyleTextAlign::Char},
    {"justify", StyleTextAlign::Justify},
    {"middle", StyleTextAlign::MozCenter},
    {"absmiddle", StyleTextAlign::Center},
    {nullptr, 0}};

bool nsGenericHTMLElement::ParseTableCellHAlignValue(const nsAString& aString,
                                                     nsAttrValue& aResult) {
  return aResult.ParseEnumValue(aString, kTableCellHAlignTable, false);
}

//----------------------------------------

bool nsGenericHTMLElement::ParseTableVAlignValue(const nsAString& aString,
                                                 nsAttrValue& aResult) {
  return aResult.ParseEnumValue(aString, kTableVAlignTable, false);
}

bool nsGenericHTMLElement::ParseDivAlignValue(const nsAString& aString,
                                              nsAttrValue& aResult) {
  return aResult.ParseEnumValue(aString, kDivAlignTable, false);
}

bool nsGenericHTMLElement::ParseImageAttribute(nsAtom* aAttribute,
                                               const nsAString& aString,
                                               nsAttrValue& aResult) {
  if (aAttribute == nsGkAtoms::width || aAttribute == nsGkAtoms::height ||
      aAttribute == nsGkAtoms::hspace || aAttribute == nsGkAtoms::vspace) {
    return aResult.ParseHTMLDimension(aString);
  }
  if (aAttribute == nsGkAtoms::border) {
    return aResult.ParseNonNegativeIntValue(aString);
  }
  return false;
}

bool nsGenericHTMLElement::ParseReferrerAttribute(const nsAString& aString,
                                                  nsAttrValue& aResult) {
  using mozilla::dom::ReferrerInfo;
  static const nsAttrValue::EnumTable kReferrerPolicyTable[] = {
      {ReferrerInfo::ReferrerPolicyToString(ReferrerPolicy::No_referrer),
       static_cast<int16_t>(ReferrerPolicy::No_referrer)},
      {ReferrerInfo::ReferrerPolicyToString(ReferrerPolicy::Origin),
       static_cast<int16_t>(ReferrerPolicy::Origin)},
      {ReferrerInfo::ReferrerPolicyToString(
           ReferrerPolicy::Origin_when_cross_origin),
       static_cast<int16_t>(ReferrerPolicy::Origin_when_cross_origin)},
      {ReferrerInfo::ReferrerPolicyToString(
           ReferrerPolicy::No_referrer_when_downgrade),
       static_cast<int16_t>(ReferrerPolicy::No_referrer_when_downgrade)},
      {ReferrerInfo::ReferrerPolicyToString(ReferrerPolicy::Unsafe_url),
       static_cast<int16_t>(ReferrerPolicy::Unsafe_url)},
      {ReferrerInfo::ReferrerPolicyToString(ReferrerPolicy::Strict_origin),
       static_cast<int16_t>(ReferrerPolicy::Strict_origin)},
      {ReferrerInfo::ReferrerPolicyToString(ReferrerPolicy::Same_origin),
       static_cast<int16_t>(ReferrerPolicy::Same_origin)},
      {ReferrerInfo::ReferrerPolicyToString(
           ReferrerPolicy::Strict_origin_when_cross_origin),
       static_cast<int16_t>(ReferrerPolicy::Strict_origin_when_cross_origin)},
      {nullptr, ReferrerPolicy::_empty}};
  return aResult.ParseEnumValue(aString, kReferrerPolicyTable, false);
}

bool nsGenericHTMLElement::ParseFrameborderValue(const nsAString& aString,
                                                 nsAttrValue& aResult) {
  return aResult.ParseEnumValue(aString, kFrameborderTable, false);
}

bool nsGenericHTMLElement::ParseScrollingValue(const nsAString& aString,
                                               nsAttrValue& aResult) {
  return aResult.ParseEnumValue(aString, kScrollingTable, false);
}

static inline void MapLangAttributeInto(const nsMappedAttributes* aAttributes,
                                        MappedDeclarations& aDecls) {
  const nsAttrValue* langValue = aAttributes->GetAttr(nsGkAtoms::lang);
  if (!langValue) {
    return;
  }
  MOZ_ASSERT(langValue->Type() == nsAttrValue::eAtom);
  aDecls.SetIdentAtomValueIfUnset(eCSSProperty__x_lang,
                                  langValue->GetAtomValue());
  if (!aDecls.PropertyIsSet(eCSSProperty_text_emphasis_position)) {
    const nsAtom* lang = langValue->GetAtomValue();
    if (nsStyleUtil::MatchesLanguagePrefix(lang, u"zh")) {
      aDecls.SetKeywordValue(eCSSProperty_text_emphasis_position,
                             NS_STYLE_TEXT_EMPHASIS_POSITION_DEFAULT_ZH);
    } else if (nsStyleUtil::MatchesLanguagePrefix(lang, u"ja") ||
               nsStyleUtil::MatchesLanguagePrefix(lang, u"mn")) {
      // This branch is currently no part of the spec.
      // See bug 1040668 comment 69 and comment 75.
      aDecls.SetKeywordValue(eCSSProperty_text_emphasis_position,
                             NS_STYLE_TEXT_EMPHASIS_POSITION_DEFAULT);
    }
  }
}

/**
 * Handle attributes common to all html elements
 */
void nsGenericHTMLElement::MapCommonAttributesIntoExceptHidden(
    const nsMappedAttributes* aAttributes, MappedDeclarations& aDecls) {
  if (!aDecls.PropertyIsSet(eCSSProperty__moz_user_modify)) {
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::contenteditable);
    if (value) {
      if (value->Equals(nsGkAtoms::_empty, eCaseMatters) ||
          value->Equals(nsGkAtoms::_true, eIgnoreCase)) {
        aDecls.SetKeywordValue(eCSSProperty__moz_user_modify,
                               StyleUserModify::ReadWrite);
      } else if (value->Equals(nsGkAtoms::_false, eIgnoreCase)) {
        aDecls.SetKeywordValue(eCSSProperty__moz_user_modify,
                               StyleUserModify::ReadOnly);
      }
    }
  }

  MapLangAttributeInto(aAttributes, aDecls);
}

void nsGenericHTMLElement::MapCommonAttributesInto(
    const nsMappedAttributes* aAttributes, MappedDeclarations& aDecls) {
  MapCommonAttributesIntoExceptHidden(aAttributes, aDecls);

  if (!aDecls.PropertyIsSet(eCSSProperty_display)) {
    if (aAttributes->IndexOfAttr(nsGkAtoms::hidden) >= 0) {
      aDecls.SetKeywordValue(eCSSProperty_display, StyleDisplay::None);
    }
  }
}

/* static */
const nsGenericHTMLElement::MappedAttributeEntry
    nsGenericHTMLElement::sCommonAttributeMap[] = {{nsGkAtoms::contenteditable},
                                                   {nsGkAtoms::lang},
                                                   {nsGkAtoms::hidden},
                                                   {nullptr}};

/* static */
const Element::MappedAttributeEntry
    nsGenericHTMLElement::sImageMarginSizeAttributeMap[] = {{nsGkAtoms::width},
                                                            {nsGkAtoms::height},
                                                            {nsGkAtoms::hspace},
                                                            {nsGkAtoms::vspace},
                                                            {nullptr}};

/* static */
const Element::MappedAttributeEntry
    nsGenericHTMLElement::sImageAlignAttributeMap[] = {{nsGkAtoms::align},
                                                       {nullptr}};

/* static */
const Element::MappedAttributeEntry
    nsGenericHTMLElement::sDivAlignAttributeMap[] = {{nsGkAtoms::align},
                                                     {nullptr}};

/* static */
const Element::MappedAttributeEntry
    nsGenericHTMLElement::sImageBorderAttributeMap[] = {{nsGkAtoms::border},
                                                        {nullptr}};

/* static */
const Element::MappedAttributeEntry
    nsGenericHTMLElement::sBackgroundAttributeMap[] = {
        {nsGkAtoms::background}, {nsGkAtoms::bgcolor}, {nullptr}};

/* static */
const Element::MappedAttributeEntry
    nsGenericHTMLElement::sBackgroundColorAttributeMap[] = {
        {nsGkAtoms::bgcolor}, {nullptr}};

void nsGenericHTMLElement::MapImageAlignAttributeInto(
    const nsMappedAttributes* aAttributes, MappedDeclarations& aDecls) {
  const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::align);
  if (value && value->Type() == nsAttrValue::eEnum) {
    int32_t align = value->GetEnumValue();
    if (!aDecls.PropertyIsSet(eCSSProperty_float)) {
      if (align == uint8_t(StyleTextAlign::Left)) {
        aDecls.SetKeywordValue(eCSSProperty_float, StyleFloat::Left);
      } else if (align == uint8_t(StyleTextAlign::Right)) {
        aDecls.SetKeywordValue(eCSSProperty_float, StyleFloat::Right);
      }
    }
    if (!aDecls.PropertyIsSet(eCSSProperty_vertical_align)) {
      switch (align) {
        case uint8_t(StyleTextAlign::Left):
        case uint8_t(StyleTextAlign::Right):
          break;
        default:
          aDecls.SetKeywordValue(eCSSProperty_vertical_align, align);
          break;
      }
    }
  }
}

void nsGenericHTMLElement::MapDivAlignAttributeInto(
    const nsMappedAttributes* aAttributes, MappedDeclarations& aDecls) {
  if (!aDecls.PropertyIsSet(eCSSProperty_text_align)) {
    // align: enum
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::align);
    if (value && value->Type() == nsAttrValue::eEnum)
      aDecls.SetKeywordValue(eCSSProperty_text_align, value->GetEnumValue());
  }
}

void nsGenericHTMLElement::MapVAlignAttributeInto(
    const nsMappedAttributes* aAttributes, MappedDeclarations& aDecls) {
  if (!aDecls.PropertyIsSet(eCSSProperty_vertical_align)) {
    // align: enum
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::valign);
    if (value && value->Type() == nsAttrValue::eEnum)
      aDecls.SetKeywordValue(eCSSProperty_vertical_align,
                             value->GetEnumValue());
  }
}

static void MapDimensionAttributeInto(MappedDeclarations& aDecls,
                                      nsCSSPropertyID aProp,
                                      const nsAttrValue& aValue) {
  MOZ_ASSERT(!aDecls.PropertyIsSet(aProp),
             "Why mapping the same property twice?");
  if (aValue.Type() == nsAttrValue::eInteger) {
    return aDecls.SetPixelValue(aProp, aValue.GetIntegerValue());
  }
  if (aValue.Type() == nsAttrValue::ePercent) {
    return aDecls.SetPercentValue(aProp, aValue.GetPercentValue());
  }
  if (aValue.Type() == nsAttrValue::eDoubleValue) {
    return aDecls.SetPixelValue(aProp, aValue.GetDoubleValue());
  }
}

void nsGenericHTMLElement::MapImageMarginAttributeInto(
    const nsMappedAttributes* aAttributes, MappedDeclarations& aDecls) {
  const nsAttrValue* value;

  // hspace: value
  value = aAttributes->GetAttr(nsGkAtoms::hspace);
  if (value) {
    MapDimensionAttributeInto(aDecls, eCSSProperty_margin_left, *value);
    MapDimensionAttributeInto(aDecls, eCSSProperty_margin_right, *value);
  }

  // vspace: value
  value = aAttributes->GetAttr(nsGkAtoms::vspace);
  if (value) {
    MapDimensionAttributeInto(aDecls, eCSSProperty_margin_top, *value);
    MapDimensionAttributeInto(aDecls, eCSSProperty_margin_bottom, *value);
  }
}

void nsGenericHTMLElement::MapWidthAttributeInto(
    const nsMappedAttributes* aAttributes, MappedDeclarations& aDecls) {
  if (const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::width)) {
    MapDimensionAttributeInto(aDecls, eCSSProperty_width, *value);
  }
}

void nsGenericHTMLElement::MapHeightAttributeInto(
    const nsMappedAttributes* aAttributes, MappedDeclarations& aDecls) {
  if (const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::height)) {
    MapDimensionAttributeInto(aDecls, eCSSProperty_height, *value);
  }
}

static void DoMapAspectRatio(const nsAttrValue& aWidth,
                             const nsAttrValue& aHeight,
                             MappedDeclarations& aDecls) {
  Maybe<double> w;
  if (aWidth.Type() == nsAttrValue::eInteger) {
    w.emplace(aWidth.GetIntegerValue());
  } else if (aWidth.Type() == nsAttrValue::eDoubleValue) {
    w.emplace(aWidth.GetDoubleValue());
  }

  Maybe<double> h;
  if (aHeight.Type() == nsAttrValue::eInteger) {
    h.emplace(aHeight.GetIntegerValue());
  } else if (aHeight.Type() == nsAttrValue::eDoubleValue) {
    h.emplace(aHeight.GetDoubleValue());
  }

  if (w && h) {
    aDecls.SetAspectRatio(*w, *h);
  }
}

void nsGenericHTMLElement::MapImageSizeAttributesInto(
    const nsMappedAttributes* aAttributes, MappedDeclarations& aDecls,
    MapAspectRatio aMapAspectRatio) {
  auto* width = aAttributes->GetAttr(nsGkAtoms::width);
  auto* height = aAttributes->GetAttr(nsGkAtoms::height);
  if (width) {
    MapDimensionAttributeInto(aDecls, eCSSProperty_width, *width);
  }
  if (height) {
    MapDimensionAttributeInto(aDecls, eCSSProperty_height, *height);
  }
  if (aMapAspectRatio == MapAspectRatio::Yes && width && height) {
    DoMapAspectRatio(*width, *height, aDecls);
  }
}

void nsGenericHTMLElement::MapPictureSourceSizeAttributesInto(
    const nsMappedAttributes* aAttributes, MappedDeclarations& aDecls) {
  const auto* width = aAttributes->GetAttr(nsGkAtoms::width);
  const auto* height = aAttributes->GetAttr(nsGkAtoms::height);
  if (!width && !height) {
    return;
  }

  // We should set the missing property values with auto value to make sure it
  // overrides the declaraion created by the presentation attributes of
  // HTMLImageElement. This can make sure we compute the ratio-dependent axis
  // size properly by the natural aspect-ratio of the image.
  //
  // Note: The spec doesn't specify this, so we follow the implementation in
  // other browsers.
  // Spec issue: https://github.com/whatwg/html/issues/8178.
  if (width) {
    MapDimensionAttributeInto(aDecls, eCSSProperty_width, *width);
  } else {
    aDecls.SetAutoValue(eCSSProperty_width);
  }

  if (height) {
    MapDimensionAttributeInto(aDecls, eCSSProperty_height, *height);
  } else {
    aDecls.SetAutoValue(eCSSProperty_height);
  }

  if (width && height) {
    DoMapAspectRatio(*width, *height, aDecls);
  } else {
    aDecls.SetAutoValue(eCSSProperty_aspect_ratio);
  }
}

void nsGenericHTMLElement::MapAspectRatioInto(
    const nsMappedAttributes* aAttributes, MappedDeclarations& aDecls) {
  auto* width = aAttributes->GetAttr(nsGkAtoms::width);
  auto* height = aAttributes->GetAttr(nsGkAtoms::height);
  if (width && height) {
    DoMapAspectRatio(*width, *height, aDecls);
  }
}

void nsGenericHTMLElement::MapImageBorderAttributeInto(
    const nsMappedAttributes* aAttributes, MappedDeclarations& aDecls) {
  // border: pixels
  const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::border);
  if (!value) return;

  nscoord val = 0;
  if (value->Type() == nsAttrValue::eInteger) val = value->GetIntegerValue();

  aDecls.SetPixelValueIfUnset(eCSSProperty_border_top_width, (float)val);
  aDecls.SetPixelValueIfUnset(eCSSProperty_border_right_width, (float)val);
  aDecls.SetPixelValueIfUnset(eCSSProperty_border_bottom_width, (float)val);
  aDecls.SetPixelValueIfUnset(eCSSProperty_border_left_width, (float)val);

  aDecls.SetKeywordValueIfUnset(eCSSProperty_border_top_style,
                                StyleBorderStyle::Solid);
  aDecls.SetKeywordValueIfUnset(eCSSProperty_border_right_style,
                                StyleBorderStyle::Solid);
  aDecls.SetKeywordValueIfUnset(eCSSProperty_border_bottom_style,
                                StyleBorderStyle::Solid);
  aDecls.SetKeywordValueIfUnset(eCSSProperty_border_left_style,
                                StyleBorderStyle::Solid);

  aDecls.SetCurrentColorIfUnset(eCSSProperty_border_top_color);
  aDecls.SetCurrentColorIfUnset(eCSSProperty_border_right_color);
  aDecls.SetCurrentColorIfUnset(eCSSProperty_border_bottom_color);
  aDecls.SetCurrentColorIfUnset(eCSSProperty_border_left_color);
}

void nsGenericHTMLElement::MapBackgroundInto(
    const nsMappedAttributes* aAttributes, MappedDeclarations& aDecls) {
  if (!aDecls.PropertyIsSet(eCSSProperty_background_image)) {
    // background
    nsAttrValue* value =
        const_cast<nsAttrValue*>(aAttributes->GetAttr(nsGkAtoms::background));
    if (value) {
      aDecls.SetBackgroundImage(*value);
    }
  }
}

void nsGenericHTMLElement::MapBGColorInto(const nsMappedAttributes* aAttributes,
                                          MappedDeclarations& aDecls) {
  if (!aDecls.PropertyIsSet(eCSSProperty_background_color)) {
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::bgcolor);
    nscolor color;
    if (value && value->GetColorValue(color)) {
      aDecls.SetColorValue(eCSSProperty_background_color, color);
    }
  }
}

void nsGenericHTMLElement::MapBackgroundAttributesInto(
    const nsMappedAttributes* aAttributes, MappedDeclarations& aDecls) {
  MapBackgroundInto(aAttributes, aDecls);
  MapBGColorInto(aAttributes, aDecls);
}

//----------------------------------------------------------------------

int32_t nsGenericHTMLElement::GetIntAttr(nsAtom* aAttr,
                                         int32_t aDefault) const {
  const nsAttrValue* attrVal = mAttrs.GetAttr(aAttr);
  if (attrVal && attrVal->Type() == nsAttrValue::eInteger) {
    return attrVal->GetIntegerValue();
  }
  return aDefault;
}

nsresult nsGenericHTMLElement::SetIntAttr(nsAtom* aAttr, int32_t aValue) {
  nsAutoString value;
  value.AppendInt(aValue);

  return SetAttr(kNameSpaceID_None, aAttr, value, true);
}

uint32_t nsGenericHTMLElement::GetUnsignedIntAttr(nsAtom* aAttr,
                                                  uint32_t aDefault) const {
  const nsAttrValue* attrVal = mAttrs.GetAttr(aAttr);
  if (!attrVal || attrVal->Type() != nsAttrValue::eInteger) {
    return aDefault;
  }

  return attrVal->GetIntegerValue();
}

uint32_t nsGenericHTMLElement::GetDimensionAttrAsUnsignedInt(
    nsAtom* aAttr, uint32_t aDefault) const {
  const nsAttrValue* attrVal = mAttrs.GetAttr(aAttr);
  if (!attrVal) {
    return aDefault;
  }

  if (attrVal->Type() == nsAttrValue::eInteger) {
    return attrVal->GetIntegerValue();
  }

  if (attrVal->Type() == nsAttrValue::ePercent) {
    // This is a nasty hack.  When we parsed the value, we stored it as an
    // ePercent, not eInteger, because there was a '%' after it in the string.
    // But the spec says to basically re-parse the string as an integer.
    // Luckily, we can just return the value we have stored.  But
    // GetPercentValue() divides it by 100, so we need to multiply it back.
    return uint32_t(attrVal->GetPercentValue() * 100.0f);
  }

  if (attrVal->Type() == nsAttrValue::eDoubleValue) {
    return uint32_t(attrVal->GetDoubleValue());
  }

  // Unfortunately, the set of values that are valid dimensions is not a
  // superset of values that are valid unsigned ints.  In particular "+100" is
  // not a valid dimension, but should parse as the unsigned int "100".  So if
  // we got here and we don't have a valid dimension value, just try re-parsing
  // the string we have as an integer.
  nsAutoString val;
  attrVal->ToString(val);
  nsContentUtils::ParseHTMLIntegerResultFlags result;
  int32_t parsedInt = nsContentUtils::ParseHTMLInteger(val, &result);
  if ((result & nsContentUtils::eParseHTMLInteger_Error) || parsedInt < 0) {
    return aDefault;
  }

  return parsedInt;
}

void nsGenericHTMLElement::GetURIAttr(nsAtom* aAttr, nsAtom* aBaseAttr,
                                      nsAString& aResult) const {
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

bool nsGenericHTMLElement::GetURIAttr(nsAtom* aAttr, nsAtom* aBaseAttr,
                                      nsIURI** aURI) const {
  *aURI = nullptr;

  const nsAttrValue* attr = mAttrs.GetAttr(aAttr);
  if (!attr) {
    return false;
  }

  nsCOMPtr<nsIURI> baseURI = GetBaseURI();

  if (aBaseAttr) {
    nsAutoString baseAttrValue;
    if (GetAttr(kNameSpaceID_None, aBaseAttr, baseAttrValue)) {
      nsCOMPtr<nsIURI> baseAttrURI;
      nsresult rv = nsContentUtils::NewURIWithDocumentCharset(
          getter_AddRefs(baseAttrURI), baseAttrValue, OwnerDoc(), baseURI);
      if (NS_FAILED(rv)) {
        return true;
      }
      baseURI.swap(baseAttrURI);
    }
  }

  // Don't care about return value.  If it fails, we still want to
  // return true, and *aURI will be null.
  nsContentUtils::NewURIWithDocumentCharset(aURI, attr->GetStringValue(),
                                            OwnerDoc(), baseURI);
  return true;
}

bool nsGenericHTMLElement::IsLabelable() const {
  return IsAnyOfHTMLElements(nsGkAtoms::progress, nsGkAtoms::meter);
}

/* static */
bool nsGenericHTMLElement::MatchLabelsElement(Element* aElement,
                                              int32_t aNamespaceID,
                                              nsAtom* aAtom, void* aData) {
  HTMLLabelElement* element = HTMLLabelElement::FromNode(aElement);
  return element && element->GetControl() == aData;
}

already_AddRefed<nsINodeList> nsGenericHTMLElement::Labels() {
  MOZ_ASSERT(IsLabelable(),
             "Labels() only allow labelable elements to use it.");
  nsExtendedDOMSlots* slots = ExtendedDOMSlots();

  if (!slots->mLabelsList) {
    slots->mLabelsList =
        new nsLabelsNodeList(SubtreeRoot(), MatchLabelsElement, nullptr, this);
  }

  RefPtr<nsLabelsNodeList> labels = slots->mLabelsList;
  return labels.forget();
}

// static
bool nsGenericHTMLElement::LegacyTouchAPIEnabled(JSContext* aCx,
                                                 JSObject* aGlobal) {
  return TouchEvent::LegacyAPIEnabled(aCx, aGlobal);
}

bool nsGenericHTMLElement::IsFormControlDefaultFocusable(
    bool aWithMouse) const {
  if (!aWithMouse) {
    return true;
  }
  switch (StaticPrefs::accessibility_mouse_focuses_formcontrol()) {
    case 0:
      return false;
    case 1:
      return true;
    default:
      return !IsInChromeDocument();
  }
}

//----------------------------------------------------------------------

nsGenericHTMLFormElement::nsGenericHTMLFormElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : nsGenericHTMLElement(std::move(aNodeInfo)) {
  // We should add the ElementState::ENABLED bit here as needed, but
  // that depends on our type, which is not initialized yet.  So we
  // have to do this in subclasses.
}

void nsGenericHTMLFormElement::ClearForm(bool aRemoveFromForm,
                                         bool aUnbindOrDelete) {
  MOZ_ASSERT(IsFormAssociatedElement());

  HTMLFormElement* form = GetFormInternal();
  NS_ASSERTION((form != nullptr) == HasFlag(ADDED_TO_FORM),
               "Form control should have had flag set correctly");

  if (!form) {
    return;
  }

  if (aRemoveFromForm) {
    nsAutoString nameVal, idVal;
    GetAttr(kNameSpaceID_None, nsGkAtoms::name, nameVal);
    GetAttr(kNameSpaceID_None, nsGkAtoms::id, idVal);

    form->RemoveElement(this, true);

    if (!nameVal.IsEmpty()) {
      form->RemoveElementFromTable(this, nameVal);
    }

    if (!idVal.IsEmpty()) {
      form->RemoveElementFromTable(this, idVal);
    }
  }

  UnsetFlags(ADDED_TO_FORM);
  SetFormInternal(nullptr, false);

  AfterClearForm(aUnbindOrDelete);
}

nsresult nsGenericHTMLFormElement::BindToTree(BindContext& aContext,
                                              nsINode& aParent) {
  nsresult rv = nsGenericHTMLElement::BindToTree(aContext, aParent);
  NS_ENSURE_SUCCESS(rv, rv);

  if (IsFormAssociatedElement()) {
    // If @form is set, the element *has* to be in a composed document,
    // otherwise it wouldn't be possible to find an element with the
    // corresponding id. If @form isn't set, the element *has* to have a parent,
    // otherwise it wouldn't be possible to find a form ancestor. We should not
    // call UpdateFormOwner if none of these conditions are fulfilled.
    if (HasAttr(kNameSpaceID_None, nsGkAtoms::form) ? IsInComposedDoc()
                                                    : aParent.IsContent()) {
      UpdateFormOwner(true, nullptr);
    }
  }

  // Set parent fieldset which should be used for the disabled state.
  UpdateFieldSet(false);

  return NS_OK;
}

void nsGenericHTMLFormElement::UnbindFromTree(bool aNullParent) {
  if (IsFormAssociatedElement()) {
    if (HTMLFormElement* form = GetFormInternal()) {
      // Might need to unset form
      if (aNullParent) {
        // No more parent means no more form
        ClearForm(true, true);
      } else {
        // Recheck whether we should still have an form.
        if (HasAttr(kNameSpaceID_None, nsGkAtoms::form) ||
            !FindAncestorForm(form)) {
          ClearForm(true, true);
        } else {
          UnsetFlags(MAYBE_ORPHAN_FORM_ELEMENT);
        }
      }

      if (!GetFormInternal()) {
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
  }

  nsGenericHTMLElement::UnbindFromTree(aNullParent);

  // The element might not have a fieldset anymore.
  UpdateFieldSet(false);
}

nsresult nsGenericHTMLFormElement::BeforeSetAttr(
    int32_t aNameSpaceID, nsAtom* aName, const nsAttrValueOrString* aValue,
    bool aNotify) {
  if (aNameSpaceID == kNameSpaceID_None && IsFormAssociatedElement()) {
    nsAutoString tmp;
    HTMLFormElement* form = GetFormInternal();

    // remove the control from the hashtable as needed

    if (form && (aName == nsGkAtoms::name || aName == nsGkAtoms::id)) {
      GetAttr(kNameSpaceID_None, aName, tmp);

      if (!tmp.IsEmpty()) {
        form->RemoveElementFromTable(this, tmp);
      }
    }

    if (form && aName == nsGkAtoms::type) {
      GetAttr(kNameSpaceID_None, nsGkAtoms::name, tmp);

      if (!tmp.IsEmpty()) {
        form->RemoveElementFromTable(this, tmp);
      }

      GetAttr(kNameSpaceID_None, nsGkAtoms::id, tmp);

      if (!tmp.IsEmpty()) {
        form->RemoveElementFromTable(this, tmp);
      }

      form->RemoveElement(this, false);
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

  return nsGenericHTMLElement::BeforeSetAttr(aNameSpaceID, aName, aValue,
                                             aNotify);
}

nsresult nsGenericHTMLFormElement::AfterSetAttr(
    int32_t aNameSpaceID, nsAtom* aName, const nsAttrValue* aValue,
    const nsAttrValue* aOldValue, nsIPrincipal* aMaybeScriptedPrincipal,
    bool aNotify) {
  if (aNameSpaceID == kNameSpaceID_None && IsFormAssociatedElement()) {
    HTMLFormElement* form = GetFormInternal();

    // add the control to the hashtable as needed
    if (form && (aName == nsGkAtoms::name || aName == nsGkAtoms::id) &&
        aValue && !aValue->IsEmptyString()) {
      MOZ_ASSERT(aValue->Type() == nsAttrValue::eAtom,
                 "Expected atom value for name/id");
      form->AddElementToTable(this,
                              nsDependentAtomString(aValue->GetAtomValue()));
    }

    if (form && aName == nsGkAtoms::type) {
      nsAutoString tmp;

      GetAttr(kNameSpaceID_None, nsGkAtoms::name, tmp);

      if (!tmp.IsEmpty()) {
        form->AddElementToTable(this, tmp);
      }

      GetAttr(kNameSpaceID_None, nsGkAtoms::id, tmp);

      if (!tmp.IsEmpty()) {
        form->AddElementToTable(this, tmp);
      }

      form->AddElement(this, false, aNotify);
    }

    if (aName == nsGkAtoms::form) {
      // We need a new form id observer.
      DocumentOrShadowRoot* docOrShadow =
          GetUncomposedDocOrConnectedShadowRoot();
      if (docOrShadow) {
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

  return nsGenericHTMLElement::AfterSetAttr(
      aNameSpaceID, aName, aValue, aOldValue, aMaybeScriptedPrincipal, aNotify);
}

void nsGenericHTMLFormElement::ForgetFieldSet(nsIContent* aFieldset) {
  if (IsFormAssociatedElement() && GetFieldSetInternal() == aFieldset) {
    SetFieldSetInternal(nullptr);
  }
}

Element* nsGenericHTMLFormElement::AddFormIdObserver() {
  MOZ_ASSERT(IsFormAssociatedElement());

  nsAutoString formId;
  DocumentOrShadowRoot* docOrShadow = GetUncomposedDocOrConnectedShadowRoot();
  GetAttr(kNameSpaceID_None, nsGkAtoms::form, formId);
  NS_ASSERTION(!formId.IsEmpty(),
               "@form value should not be the empty string!");
  RefPtr<nsAtom> atom = NS_Atomize(formId);

  return docOrShadow->AddIDTargetObserver(atom, FormIdUpdated, this, false);
}

void nsGenericHTMLFormElement::RemoveFormIdObserver() {
  MOZ_ASSERT(IsFormAssociatedElement());

  DocumentOrShadowRoot* docOrShadow = GetUncomposedDocOrConnectedShadowRoot();
  if (!docOrShadow) {
    return;
  }

  nsAutoString formId;
  GetAttr(kNameSpaceID_None, nsGkAtoms::form, formId);
  NS_ASSERTION(!formId.IsEmpty(),
               "@form value should not be the empty string!");
  RefPtr<nsAtom> atom = NS_Atomize(formId);

  docOrShadow->RemoveIDTargetObserver(atom, FormIdUpdated, this, false);
}

/* static */
bool nsGenericHTMLFormElement::FormIdUpdated(Element* aOldElement,
                                             Element* aNewElement,
                                             void* aData) {
  nsGenericHTMLFormElement* element =
      static_cast<nsGenericHTMLFormElement*>(aData);

  NS_ASSERTION(element->IsHTMLElement(), "aData should be an HTML element");

  element->UpdateFormOwner(false, aNewElement);

  return true;
}

bool nsGenericHTMLFormElement::IsElementDisabledForEvents(WidgetEvent* aEvent,
                                                          nsIFrame* aFrame) {
  MOZ_ASSERT(aEvent);

  // Allow dispatch of CustomEvent and untrusted Events.
  if (!aEvent->IsTrusted()) {
    return false;
  }

  switch (aEvent->mMessage) {
    case eAnimationStart:
    case eAnimationEnd:
    case eAnimationIteration:
    case eAnimationCancel:
    case eFormChange:
    case eMouseMove:
    case eMouseOver:
    case eMouseOut:
    case eMouseEnter:
    case eMouseLeave:
    case ePointerMove:
    case ePointerOver:
    case ePointerOut:
    case ePointerEnter:
    case ePointerLeave:
    case eTransitionCancel:
    case eTransitionEnd:
    case eTransitionRun:
    case eTransitionStart:
    case eWheel:
    case eLegacyMouseLineOrPageScroll:
    case eLegacyMousePixelScroll:
      return false;
    default:
      break;
  }

  if (aEvent->mSpecifiedEventType == nsGkAtoms::oninput) {
    return false;
  }

  // FIXME(emilio): This poking at the style of the frame is slightly bogus
  // unless we flush before every event, which we don't really want to do.
  if (aFrame && aFrame->StyleUI()->UserInput() == StyleUserInput::None) {
    return true;
  }

  return IsDisabled();
}

void nsGenericHTMLFormElement::UpdateFormOwner(bool aBindToTree,
                                               Element* aFormIdElement) {
  MOZ_ASSERT(IsFormAssociatedElement());
  MOZ_ASSERT(!aBindToTree || !aFormIdElement,
             "aFormIdElement shouldn't be set if aBindToTree is true!");

  bool needStateUpdate = false;
  if (!aBindToTree) {
    HTMLFormElement* form = GetFormInternal();
    needStateUpdate = form && form->IsDefaultSubmitElement(this);
    ClearForm(true, false);
  }

  // We have to get form again since the above ClearForm() call might update the
  // form value.
  HTMLFormElement* oldForm = GetFormInternal();
  if (!oldForm) {
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

        NS_ASSERTION(!IsInComposedDoc() ||
                         element == GetUncomposedDocOrConnectedShadowRoot()
                                        ->GetElementById(formId),
                     "element should be equals to the current element "
                     "associated with the id in @form!");

        if (element && element->IsHTMLElement(nsGkAtoms::form) &&
            nsContentUtils::IsInSameAnonymousTree(this, element)) {
          SetFormInternal(static_cast<HTMLFormElement*>(element), aBindToTree);
        }
      }
    } else {
      // We now have a parent, so we may have picked up an ancestor form. Search
      // for it.  Note that if form is already set we don't want to do this,
      // because that means someone (probably the content sink) has already set
      // it to the right value.  Also note that even if being bound here didn't
      // change our parent, we still need to search, since our parent chain
      // probably changed _somewhere_.
      SetFormInternal(FindAncestorForm(), aBindToTree);
    }
  }

  HTMLFormElement* form = GetFormInternal();
  if (form && !HasFlag(ADDED_TO_FORM)) {
    // Now we need to add ourselves to the form
    nsAutoString nameVal, idVal;
    GetAttr(kNameSpaceID_None, nsGkAtoms::name, nameVal);
    GetAttr(kNameSpaceID_None, nsGkAtoms::id, idVal);

    SetFlags(ADDED_TO_FORM);

    // Notify only if we just found this form.
    form->AddElement(this, true, oldForm == nullptr);

    if (!nameVal.IsEmpty()) {
      form->AddElementToTable(this, nameVal);
    }

    if (!idVal.IsEmpty()) {
      form->AddElementToTable(this, idVal);
    }
  }

  if (form != oldForm || needStateUpdate) {
    UpdateState(true);
  }
}

void nsGenericHTMLFormElement::UpdateFieldSet(bool aNotify) {
  if (IsInNativeAnonymousSubtree() || !IsFormAssociatedElement()) {
    MOZ_ASSERT_IF(IsFormAssociatedElement(), !GetFieldSetInternal());
    return;
  }

  nsIContent* parent = nullptr;
  nsIContent* prev = nullptr;
  HTMLFieldSetElement* fieldset = GetFieldSetInternal();

  for (parent = GetParent(); parent;
       prev = parent, parent = parent->GetParent()) {
    HTMLFieldSetElement* parentFieldset = HTMLFieldSetElement::FromNode(parent);
    if (parentFieldset && (!prev || parentFieldset->GetFirstLegend() != prev)) {
      if (fieldset == parentFieldset) {
        // We already have the right fieldset;
        return;
      }

      if (fieldset) {
        fieldset->RemoveElement(this);
      }
      SetFieldSetInternal(parentFieldset);
      parentFieldset->AddElement(this);

      // The disabled state may have changed
      FieldSetDisabledChanged(aNotify);
      return;
    }
  }

  // No fieldset found.
  if (fieldset) {
    fieldset->RemoveElement(this);
    SetFieldSetInternal(nullptr);
    // The disabled state may have changed
    FieldSetDisabledChanged(aNotify);
  }
}

void nsGenericHTMLFormElement::UpdateDisabledState(bool aNotify) {
  if (!CanBeDisabled()) {
    return;
  }

  HTMLFieldSetElement* fieldset = GetFieldSetInternal();
  const bool isDisabled =
      HasAttr(nsGkAtoms::disabled) || (fieldset && fieldset->IsDisabled());

  const ElementState disabledStates =
      isDisabled ? ElementState::DISABLED : ElementState::ENABLED;

  ElementState oldDisabledStates = State() & ElementState::DISABLED_STATES;
  ElementState changedStates = disabledStates ^ oldDisabledStates;

  if (!changedStates.IsEmpty()) {
    ToggleStates(changedStates, aNotify);
    if (DoesReadOnlyApply()) {
      // :disabled influences :read-only / :read-write.
      UpdateState(aNotify);
    }
  }
}

void nsGenericHTMLFormElement::FieldSetDisabledChanged(bool aNotify) {
  UpdateDisabledState(aNotify);
}

//----------------------------------------------------------------------

void nsGenericHTMLElement::Click(CallerType aCallerType) {
  if (IsDisabled() || HandlingClick()) {
    return;
  }

  // Strong in case the event kills it
  nsCOMPtr<Document> doc = GetComposedDoc();

  RefPtr<nsPresContext> context;
  if (doc) {
    context = doc->GetPresContext();
  }

  SetHandlingClick();

  // Mark this event trusted if Click() is called from system code.
  WidgetMouseEvent event(aCallerType == CallerType::System, eMouseClick,
                         nullptr, WidgetMouseEvent::eReal);
  event.mFlags.mIsPositionless = true;
  event.mInputSource = MouseEvent_Binding::MOZ_SOURCE_UNKNOWN;

  EventDispatcher::Dispatch(static_cast<nsIContent*>(this), context, &event);

  ClearHandlingClick();
}

bool nsGenericHTMLElement::IsHTMLFocusable(bool aWithMouse, bool* aIsFocusable,
                                           int32_t* aTabIndex) {
  if (ShadowRoot* root = GetShadowRoot()) {
    if (root->DelegatesFocus()) {
      *aIsFocusable = false;
      return true;
    }
  }

  Document* doc = GetComposedDoc();
  if (!doc || IsInDesignMode()) {
    // In designMode documents we only allow focusing the document.
    if (aTabIndex) {
      *aTabIndex = -1;
    }

    *aIsFocusable = false;

    return true;
  }

  int32_t tabIndex = TabIndex();
  bool disabled = false;
  bool disallowOverridingFocusability = true;
  Maybe<int32_t> attrVal = GetTabIndexAttrValue();

  if (IsEditableRoot()) {
    // Editable roots should always be focusable.
    disallowOverridingFocusability = true;

    // Ignore the disabled attribute in editable contentEditable/designMode
    // roots.
    if (attrVal.isNothing()) {
      // The default value for tabindex should be 0 for editable
      // contentEditable roots.
      tabIndex = 0;
    }
  } else {
    disallowOverridingFocusability = false;

    // Just check for disabled attribute on form controls
    disabled = IsDisabled();
    if (disabled) {
      tabIndex = -1;
    }
  }

  if (aTabIndex) {
    *aTabIndex = tabIndex;
  }

  // If a tabindex is specified at all, or the default tabindex is 0, we're
  // focusable
  *aIsFocusable = (tabIndex >= 0 || (!disabled && attrVal.isSome()));

  return disallowOverridingFocusability;
}

Result<bool, nsresult> nsGenericHTMLElement::PerformAccesskey(
    bool aKeyCausesActivation, bool aIsTrustedEvent) {
  RefPtr<nsPresContext> presContext = GetPresContext(eForComposedDoc);
  if (!presContext) {
    return Err(NS_ERROR_UNEXPECTED);
  }

  // It's hard to say what HTML4 wants us to do in all cases.
  bool focused = true;
  if (RefPtr<nsFocusManager> fm = nsFocusManager::GetFocusManager()) {
    fm->SetFocus(this, nsIFocusManager::FLAG_BYKEY);

    // Return true if the element became the current focus within its window.
    nsPIDOMWindowOuter* window = OwnerDoc()->GetWindow();
    focused = window && window->GetFocusedElement() == this;
  }

  if (aKeyCausesActivation) {
    // Click on it if the users prefs indicate to do so.
    AutoHandlingUserInputStatePusher userInputStatePusher(aIsTrustedEvent);
    AutoPopupStatePusher popupStatePusher(
        aIsTrustedEvent ? PopupBlocker::openAllowed : PopupBlocker::openAbused);
    DispatchSimulatedClick(this, aIsTrustedEvent, presContext);
    return focused;
  }

  // If the accesskey won't cause the activation and the focus isn't changed,
  // either. Return error so EventStateManager would try to find next element
  // to handle the accesskey.
  return focused ? Result<bool, nsresult>{focused} : Err(NS_ERROR_ABORT);
}

void nsGenericHTMLElement::HandleKeyboardActivation(
    EventChainPostVisitor& aVisitor) {
  MOZ_ASSERT(aVisitor.mEvent->HasKeyEventMessage());
  MOZ_ASSERT(aVisitor.mEvent->IsTrusted());

  const auto message = aVisitor.mEvent->mMessage;
  const WidgetKeyboardEvent* keyEvent = aVisitor.mEvent->AsKeyboardEvent();
  if (nsEventStatus_eIgnore != aVisitor.mEventStatus) {
    if (message == eKeyUp && keyEvent->mKeyCode == NS_VK_SPACE) {
      // Unset the flag even if the event is default-prevented or something.
      UnsetFlags(HTML_ELEMENT_ACTIVE_FOR_KEYBOARD);
    }
    return;
  }

  bool shouldActivate = false;
  switch (message) {
    case eKeyDown:
      if (keyEvent->mKeyCode == NS_VK_SPACE) {
        SetFlags(HTML_ELEMENT_ACTIVE_FOR_KEYBOARD);
      }
      return;
    case eKeyPress:
      shouldActivate = keyEvent->mKeyCode == NS_VK_RETURN;
      if (keyEvent->mKeyCode == NS_VK_SPACE) {
        // Consume 'space' key to prevent scrolling the page down.
        aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
      }
      break;
    case eKeyUp:
      shouldActivate = keyEvent->mKeyCode == NS_VK_SPACE &&
                       HasFlag(HTML_ELEMENT_ACTIVE_FOR_KEYBOARD);
      if (shouldActivate) {
        UnsetFlags(HTML_ELEMENT_ACTIVE_FOR_KEYBOARD);
      }
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("why didn't we bail out earlier?");
      break;
  }

  if (!shouldActivate) {
    return;
  }

  RefPtr<nsPresContext> presContext = aVisitor.mPresContext;
  DispatchSimulatedClick(this, aVisitor.mEvent->IsTrusted(), presContext);
  aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
}

nsresult nsGenericHTMLElement::DispatchSimulatedClick(
    nsGenericHTMLElement* aElement, bool aIsTrusted,
    nsPresContext* aPresContext) {
  WidgetMouseEvent event(aIsTrusted, eMouseClick, nullptr,
                         WidgetMouseEvent::eReal);
  event.mInputSource = MouseEvent_Binding::MOZ_SOURCE_KEYBOARD;
  event.mFlags.mIsPositionless = true;
  // TODO: Bug 1506441
  return EventDispatcher::Dispatch(MOZ_KnownLive(ToSupports(aElement)),
                                   aPresContext, &event);
}

already_AddRefed<EditorBase> nsGenericHTMLElement::GetAssociatedEditor() {
  // If contenteditable is ever implemented, it might need to do something
  // different here?

  RefPtr<TextEditor> textEditor = GetTextEditorInternal();
  return textEditor.forget();
}

// static
void nsGenericHTMLElement::SyncEditorsOnSubtree(nsIContent* content) {
  /* Sync this node */
  nsGenericHTMLElement* element = FromNode(content);
  if (element) {
    if (RefPtr<EditorBase> editorBase = element->GetAssociatedEditor()) {
      editorBase->SyncRealTimeSpell();
    }
  }

  /* Sync all children */
  for (nsIContent* child = content->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    SyncEditorsOnSubtree(child);
  }
}

bool nsGenericHTMLElement::IsEditableRoot() const {
  if (!IsInComposedDoc()) {
    return false;
  }

  if (IsInDesignMode()) {
    return false;
  }

  if (GetContentEditableValue() != eTrue) {
    return false;
  }

  nsIContent* parent = GetParent();

  return !parent || !parent->HasFlag(NODE_IS_EDITABLE);
}

static void MakeContentDescendantsEditable(nsIContent* aContent) {
  // If aContent is not an element, we just need to update its
  // internal editable state and don't need to notify anyone about
  // that.  For elements, we need to send a ElementStateChanged
  // notification.
  if (!aContent->IsElement()) {
    aContent->UpdateEditableState(false);
    return;
  }

  Element* element = aContent->AsElement();

  element->UpdateEditableState(true);

  for (nsIContent* child = aContent->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    if (!child->IsElement() ||
        !child->AsElement()->HasAttr(kNameSpaceID_None,
                                     nsGkAtoms::contenteditable)) {
      MakeContentDescendantsEditable(child);
    }
  }
}

void nsGenericHTMLElement::ChangeEditableState(int32_t aChange) {
  Document* document = GetComposedDoc();
  if (!document) {
    return;
  }

  Document::EditingState previousEditingState = Document::EditingState::eOff;
  if (aChange != 0) {
    document->ChangeContentEditableCount(this, aChange);
    previousEditingState = document->GetEditingState();
  }

  // MakeContentDescendantsEditable is going to call ElementStateChanged for
  // this element and all descendants if editable state has changed.
  // We might as well wrap it all in one script blocker.
  nsAutoScriptBlocker scriptBlocker;
  MakeContentDescendantsEditable(this);

  // If the document already had contenteditable and JS adds new
  // contenteditable, that might cause changing editing host to current editing
  // host's ancestor.  In such case, HTMLEditor needs to know that
  // synchronously to update selection limitter.
  // Additionally, elements in shadow DOM is not editable in the normal cases,
  // but if its content has `contenteditable`, only in it can be ediable.
  // So we don't need to notify HTMLEditor of this change only when we're not
  // in shadow DOM and the composed document is in design mode.
  if (IsInDesignMode() && !IsInShadowTree() && aChange > 0 &&
      previousEditingState == Document::EditingState::eContentEditable) {
    if (HTMLEditor* htmlEditor =
            nsContentUtils::GetHTMLEditor(document->GetPresContext())) {
      htmlEditor->NotifyEditingHostMaybeChanged();
    }
  }
}

//----------------------------------------------------------------------

nsGenericHTMLFormControlElement::nsGenericHTMLFormControlElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo, FormControlType aType)
    : nsGenericHTMLFormElement(std::move(aNodeInfo)),
      nsIFormControl(aType),
      mForm(nullptr),
      mFieldSet(nullptr) {}

nsGenericHTMLFormControlElement::~nsGenericHTMLFormControlElement() {
  if (mFieldSet) {
    mFieldSet->RemoveElement(this);
  }

  // Check that this element doesn't know anything about its form at this point.
  NS_ASSERTION(!mForm, "mForm should be null at this point!");
}

NS_IMPL_ISUPPORTS_INHERITED(nsGenericHTMLFormControlElement,
                            nsGenericHTMLFormElement, nsIFormControl)

nsINode* nsGenericHTMLFormControlElement::GetScopeChainParent() const {
  return mForm ? mForm : nsGenericHTMLElement::GetScopeChainParent();
}

bool nsGenericHTMLFormControlElement::IsNodeOfType(uint32_t aFlags) const {
  return !(aFlags & ~eHTML_FORM_CONTROL);
}

void nsGenericHTMLFormControlElement::SaveSubtreeState() {
  SaveState();

  nsGenericHTMLFormElement::SaveSubtreeState();
}

nsIContent::IMEState nsGenericHTMLFormControlElement::GetDesiredIMEState() {
  TextEditor* textEditor = GetTextEditorInternal();
  if (!textEditor) {
    return nsGenericHTMLFormElement::GetDesiredIMEState();
  }
  IMEState state;
  nsresult rv = textEditor->GetPreferredIMEState(&state);
  if (NS_FAILED(rv)) {
    return nsGenericHTMLFormElement::GetDesiredIMEState();
  }
  return state;
}

nsresult nsGenericHTMLFormControlElement::BindToTree(BindContext& aContext,
                                                     nsINode& aParent) {
  nsresult rv = nsGenericHTMLFormElement::BindToTree(aContext, aParent);
  NS_ENSURE_SUCCESS(rv, rv);

  if (IsAutofocusable() && HasAttr(nsGkAtoms::autofocus) &&
      aContext.AllowsAutoFocus()) {
    aContext.OwnerDoc().SetAutoFocusElement(this);
  }

  return NS_OK;
}

void nsGenericHTMLFormControlElement::UnbindFromTree(bool aNullParent) {
  // Save state before doing anything
  SaveState();

  nsGenericHTMLFormElement::UnbindFromTree(aNullParent);
}

void nsGenericHTMLFormControlElement::GetAutocapitalize(
    nsAString& aValue) const {
  if (nsContentUtils::HasNonEmptyAttr(this, kNameSpaceID_None,
                                      nsGkAtoms::autocapitalize)) {
    nsGenericHTMLFormElement::GetAutocapitalize(aValue);
    return;
  }

  if (mForm && IsAutocapitalizeInheriting()) {
    mForm->GetAutocapitalize(aValue);
  }
}

bool nsGenericHTMLFormControlElement::IsHTMLFocusable(bool aWithMouse,
                                                      bool* aIsFocusable,
                                                      int32_t* aTabIndex) {
  if (nsGenericHTMLFormElement::IsHTMLFocusable(aWithMouse, aIsFocusable,
                                                aTabIndex)) {
    return true;
  }

  *aIsFocusable = *aIsFocusable && IsFormControlDefaultFocusable(aWithMouse);
  return false;
}

void nsGenericHTMLFormControlElement::GetEventTargetParent(
    EventChainPreVisitor& aVisitor) {
  if (aVisitor.mEvent->IsTrusted() && (aVisitor.mEvent->mMessage == eFocus ||
                                       aVisitor.mEvent->mMessage == eBlur)) {
    // We have to handle focus/blur event to change focus states in
    // PreHandleEvent to prevent it breaks event target chain creation.
    aVisitor.mWantsPreHandleEvent = true;
  }
  nsGenericHTMLFormElement::GetEventTargetParent(aVisitor);
}

nsresult nsGenericHTMLFormControlElement::PreHandleEvent(
    EventChainVisitor& aVisitor) {
  if (aVisitor.mEvent->IsTrusted()) {
    switch (aVisitor.mEvent->mMessage) {
      case eFocus: {
        // Check to see if focus has bubbled up from a form control's
        // child textfield or button.  If that's the case, don't focus
        // this parent file control -- leave focus on the child.
        nsIFormControlFrame* formControlFrame = GetFormControlFrame(true);
        if (formControlFrame &&
            aVisitor.mEvent->mOriginalTarget == static_cast<nsINode*>(this)) {
          formControlFrame->SetFocus(true, true);
        }
        break;
      }
      case eBlur: {
        nsIFormControlFrame* formControlFrame = GetFormControlFrame(true);
        if (formControlFrame) {
          formControlFrame->SetFocus(false, false);
        }
        break;
      }
      default:
        break;
    }
  }
  return nsGenericHTMLFormElement::PreHandleEvent(aVisitor);
}

HTMLFieldSetElement* nsGenericHTMLFormControlElement::GetFieldSet() {
  return GetFieldSetInternal();
}

void nsGenericHTMLFormControlElement::SetForm(HTMLFormElement* aForm) {
  MOZ_ASSERT(aForm, "Don't pass null here");
  NS_ASSERTION(!mForm,
               "We don't support switching from one non-null form to another.");

  SetFormInternal(aForm, false);
}

void nsGenericHTMLFormControlElement::ClearForm(bool aRemoveFromForm,
                                                bool aUnbindOrDelete) {
  nsGenericHTMLFormElement::ClearForm(aRemoveFromForm, aUnbindOrDelete);
}

ElementState nsGenericHTMLFormControlElement::IntrinsicState() const {
  // If you add attribute-dependent states here, you need to add them them to
  // AfterSetAttr too.  And add them to AfterSetAttr for all subclasses that
  // implement IntrinsicState() and are affected by that attribute.
  ElementState state = nsGenericHTMLFormElement::IntrinsicState();

  if (mForm && mForm->IsDefaultSubmitElement(this)) {
    NS_ASSERTION(IsSubmitControl(),
                 "Default submit element that isn't a submit control.");
    // We are the default submit element (:default)
    state |= ElementState::DEFAULT;
  }

  // Make the text controls read-write
  if (!state.HasState(ElementState::READWRITE) && DoesReadOnlyApply()) {
    if (!GetBoolAttr(nsGkAtoms::readonly) && !IsDisabled()) {
      state |= ElementState::READWRITE;
      state &= ~ElementState::READONLY;
    }
  }

  return state;
}

bool nsGenericHTMLFormControlElement::IsLabelable() const {
  auto type = ControlType();
  return (IsInputElement(type) && type != FormControlType::InputHidden) ||
         IsButtonElement(type) || type == FormControlType::Output ||
         type == FormControlType::Select || type == FormControlType::Textarea;
}

bool nsGenericHTMLFormControlElement::CanBeDisabled() const {
  auto type = ControlType();
  // It's easier to test the types that _cannot_ be disabled
  return type != FormControlType::Object && type != FormControlType::Output;
}

bool nsGenericHTMLFormControlElement::DoesReadOnlyApply() const {
  auto type = ControlType();
  if (!IsInputElement(type) && type != FormControlType::Textarea) {
    return false;
  }

  switch (type) {
    case FormControlType::InputHidden:
    case FormControlType::InputButton:
    case FormControlType::InputImage:
    case FormControlType::InputReset:
    case FormControlType::InputSubmit:
    case FormControlType::InputRadio:
    case FormControlType::InputFile:
    case FormControlType::InputCheckbox:
    case FormControlType::InputRange:
    case FormControlType::InputColor:
      return false;
#ifdef DEBUG
    case FormControlType::Textarea:
    case FormControlType::InputText:
    case FormControlType::InputPassword:
    case FormControlType::InputSearch:
    case FormControlType::InputTel:
    case FormControlType::InputEmail:
    case FormControlType::InputUrl:
    case FormControlType::InputNumber:
    case FormControlType::InputDate:
    case FormControlType::InputTime:
    case FormControlType::InputMonth:
    case FormControlType::InputWeek:
    case FormControlType::InputDatetimeLocal:
      return true;
    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected input type in DoesReadOnlyApply()");
      return true;
#else   // DEBUG
    default:
      return true;
#endif  // DEBUG
  }
}

void nsGenericHTMLFormControlElement::SetFormInternal(HTMLFormElement* aForm,
                                                      bool aBindToTree) {
  if (aForm) {
    BeforeSetForm(aBindToTree);
  }

  // keep a *weak* ref to the form here
  mForm = aForm;
}

HTMLFormElement* nsGenericHTMLFormControlElement::GetFormInternal() const {
  return mForm;
}

HTMLFieldSetElement* nsGenericHTMLFormControlElement::GetFieldSetInternal()
    const {
  return mFieldSet;
}

void nsGenericHTMLFormControlElement::SetFieldSetInternal(
    HTMLFieldSetElement* aFieldset) {
  mFieldSet = aFieldset;
}

void nsGenericHTMLFormControlElement::UpdateRequiredState(bool aIsRequired,
                                                          bool aNotify) {
#ifdef DEBUG
  auto type = ControlType();
#endif
  MOZ_ASSERT(IsInputElement(type) || type == FormControlType::Select ||
                 type == FormControlType::Textarea,
             "This should be called only on types that @required applies");

#ifdef DEBUG
  if (HTMLInputElement* input = HTMLInputElement::FromNode(this)) {
    MOZ_ASSERT(
        input->DoesRequiredApply(),
        "This should be called only on input types that @required applies");
  }
#endif

  ElementState requiredStates;
  if (aIsRequired) {
    requiredStates |= ElementState::REQUIRED;
  } else {
    requiredStates |= ElementState::OPTIONAL_;
  }

  ElementState oldRequiredStates = State() & ElementState::REQUIRED_STATES;
  ElementState changedStates = requiredStates ^ oldRequiredStates;

  if (!changedStates.IsEmpty()) {
    ToggleStates(changedStates, aNotify);
  }
}

bool nsGenericHTMLFormControlElement::IsAutocapitalizeInheriting() const {
  auto type = ControlType();
  return IsInputElement(type) || IsButtonElement(type) ||
         type == FormControlType::Fieldset || type == FormControlType::Output ||
         type == FormControlType::Select || type == FormControlType::Textarea;
}

bool nsGenericHTMLFormControlElement::IsAutofocusable() const {
  auto type = ControlType();
  return IsInputElement(type) || IsButtonElement(type) ||
         type == FormControlType::Textarea || type == FormControlType::Select;
}

//----------------------------------------------------------------------

nsGenericHTMLFormControlElementWithState::
    nsGenericHTMLFormControlElementWithState(
        already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
        FromParser aFromParser, FormControlType aType)
    : nsGenericHTMLFormControlElement(std::move(aNodeInfo), aType),
      mControlNumber(!!(aFromParser & FROM_PARSER_NETWORK)
                         ? OwnerDoc()->GetNextControlNumber()
                         : -1) {
  mStateKey.SetIsVoid(true);
}

void nsGenericHTMLFormControlElementWithState::GenerateStateKey() {
  // Keep the key if already computed
  if (!mStateKey.IsVoid()) {
    return;
  }

  Document* doc = GetUncomposedDoc();
  if (!doc) {
    mStateKey.Truncate();
    return;
  }

  // Generate the state key
  nsContentUtils::GenerateStateKey(this, doc, mStateKey);

  // If the state key is blank, this is anonymous content or for whatever
  // reason we are not supposed to save/restore state: keep it as such.
  if (!mStateKey.IsEmpty()) {
    // Add something unique to content so layout doesn't muck us up.
    mStateKey += "-C";
  }
}

PresState* nsGenericHTMLFormControlElementWithState::GetPrimaryPresState() {
  if (mStateKey.IsEmpty()) {
    return nullptr;
  }

  nsCOMPtr<nsILayoutHistoryState> history = GetLayoutHistory(false);

  if (!history) {
    return nullptr;
  }

  // Get the pres state for this key, if it doesn't exist, create one.
  PresState* result = history->GetState(mStateKey);
  if (!result) {
    UniquePtr<PresState> newState = NewPresState();
    result = newState.get();
    history->AddState(mStateKey, std::move(newState));
  }

  return result;
}

already_AddRefed<nsILayoutHistoryState>
nsGenericHTMLFormControlElementWithState::GetLayoutHistory(bool aRead) {
  nsCOMPtr<Document> doc = GetUncomposedDoc();
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

bool nsGenericHTMLFormControlElementWithState::RestoreFormControlState() {
  MOZ_ASSERT(!mStateKey.IsVoid(),
             "GenerateStateKey must already have been called");

  if (mStateKey.IsEmpty()) {
    return false;
  }

  nsCOMPtr<nsILayoutHistoryState> history = GetLayoutHistory(true);
  if (!history) {
    return false;
  }

  // Get the pres state for this key
  PresState* state = history->GetState(mStateKey);
  if (state) {
    bool result = RestoreState(state);
    history->RemoveState(mStateKey);
    return result;
  }

  return false;
}

void nsGenericHTMLFormControlElementWithState::NodeInfoChanged(
    Document* aOldDoc) {
  nsGenericHTMLFormControlElement::NodeInfoChanged(aOldDoc);

  // We need to regenerate the state key now we're in a new document.  Clearing
  // mControlNumber means we stop considering this control to be parser
  // inserted, and we'll generate a state key based on its position in the
  // document rather than the order it was inserted into the document.
  mControlNumber = -1;
  mStateKey.SetIsVoid(true);
}

void nsGenericHTMLFormControlElementWithState::GetFormAction(nsString& aValue) {
  auto type = ControlType();
  if (!IsInputElement(type) && !IsButtonElement(type)) {
    return;
  }

  if (!GetAttr(kNameSpaceID_None, nsGkAtoms::formaction, aValue) ||
      aValue.IsEmpty()) {
    Document* document = OwnerDoc();
    nsIURI* docURI = document->GetDocumentURI();
    if (docURI) {
      nsAutoCString spec;
      nsresult rv = docURI->GetSpec(spec);
      if (NS_FAILED(rv)) {
        return;
      }

      CopyUTF8toUTF16(spec, aValue);
    }
  } else {
    GetURIAttr(nsGkAtoms::formaction, nullptr, aValue);
  }
}

bool nsGenericHTMLElement::IsEventAttributeNameInternal(nsAtom* aName) {
  return nsContentUtils::IsEventAttributeName(aName, EventNameType_HTML);
}

/**
 * Construct a URI from a string, as an element.src attribute
 * would be set to. Helper for the media elements.
 */
nsresult nsGenericHTMLElement::NewURIFromString(const nsAString& aURISpec,
                                                nsIURI** aURI) {
  NS_ENSURE_ARG_POINTER(aURI);

  *aURI = nullptr;

  nsCOMPtr<Document> doc = OwnerDoc();

  nsresult rv = nsContentUtils::NewURIWithDocumentCharset(aURI, aURISpec, doc,
                                                          GetBaseURI());
  NS_ENSURE_SUCCESS(rv, rv);

  bool equal;
  if (aURISpec.IsEmpty() && doc->GetDocumentURI() &&
      NS_SUCCEEDED(doc->GetDocumentURI()->Equals(*aURI, &equal)) && equal) {
    // Assume an element can't point to a fragment of its embedding
    // document. Fail here instead of returning the recursive URI
    // and waiting for the subsequent load to fail.
    NS_RELEASE(*aURI);
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  return NS_OK;
}

void nsGenericHTMLElement::GetInnerText(mozilla::dom::DOMString& aValue,
                                        mozilla::ErrorResult& aError) {
  // innerText depends on layout. For example, white space processing is
  // something that happens during reflow and which must be reflected by
  // innerText.  So for:
  //
  //   <div style="white-space:normal"> A     B C </div>
  //
  // innerText should give "A B C".
  //
  // The approach taken here to avoid the expense of reflow is to flush style
  // and then see whether it's necessary to flush layout afterwards. Flushing
  // layout can be skipped if we can detect that the element or its descendants
  // are not dirty.

  // Obtain the composed doc to handle elements in Shadow DOM.
  Document* doc = GetComposedDoc();
  if (doc) {
    doc->FlushPendingNotifications(FlushType::Style);
  }

  // Elements with `display: content` will not have a frame. To handle Shadow
  // DOM, walk the flattened tree looking for parent frame.
  nsIFrame* frame = GetPrimaryFrame();
  if (IsDisplayContents()) {
    for (Element* parent = GetFlattenedTreeParentElement(); parent;
         parent = parent->GetFlattenedTreeParentElement()) {
      frame = parent->GetPrimaryFrame();
      if (frame) {
        break;
      }
    }
  }

  // Check for dirty reflow roots in the subtree from targetFrame; this requires
  // a reflow flush.
  bool dirty = frame && frame->PresShell()->FrameIsAncestorOfDirtyRoot(frame);

  // The way we do that is by checking whether the element has either of the two
  // dirty bits (NS_FRAME_IS_DIRTY or NS_FRAME_HAS_DIRTY_DESCENDANTS) or if any
  // ancestor has NS_FRAME_IS_DIRTY.  We need to check for NS_FRAME_IS_DIRTY on
  // ancestors since that is something that implies NS_FRAME_IS_DIRTY on all
  // descendants.
  dirty |= frame && frame->HasAnyStateBits(NS_FRAME_HAS_DIRTY_CHILDREN);
  while (!dirty && frame) {
    dirty |= frame->HasAnyStateBits(NS_FRAME_IS_DIRTY);
    frame = frame->GetInFlowParent();
  }

  // Flush layout if we determined a reflow is required.
  if (dirty && doc) {
    doc->FlushPendingNotifications(FlushType::Layout);
  }

  if (!IsRendered()) {
    GetTextContentInternal(aValue, aError);
  } else {
    nsRange::GetInnerTextNoFlush(aValue, aError, this);
  }
}

static already_AddRefed<nsINode> TextToNode(const nsAString& aString,
                                            nsNodeInfoManager* aNim) {
  nsString str;
  const char16_t* s = aString.BeginReading();
  const char16_t* end = aString.EndReading();
  RefPtr<DocumentFragment> fragment;
  while (true) {
    if (s != end && *s == '\r' && s + 1 != end && s[1] == '\n') {
      // a \r\n pair should only generate one <br>, so just skip the \r
      ++s;
    }
    if (s == end || *s == '\r' || *s == '\n') {
      if (!str.IsEmpty()) {
        RefPtr<nsTextNode> textContent = new (aNim) nsTextNode(aNim);
        textContent->SetText(str, true);
        if (!fragment) {
          if (s == end) {
            return textContent.forget();
          }
          fragment = new (aNim) DocumentFragment(aNim);
        }
        fragment->AppendChildTo(textContent, true, IgnoreErrors());
      }
      if (s == end) {
        break;
      }
      str.Truncate();
      RefPtr<NodeInfo> ni = aNim->GetNodeInfo(
          nsGkAtoms::br, nullptr, kNameSpaceID_XHTML, nsINode::ELEMENT_NODE);
      auto* nim = ni->NodeInfoManager();
      RefPtr<HTMLBRElement> br = new (nim) HTMLBRElement(ni.forget());
      if (!fragment) {
        if (s + 1 == end) {
          return br.forget();
        }
        fragment = new (aNim) DocumentFragment(aNim);
      }
      fragment->AppendChildTo(br, true, IgnoreErrors());
    } else {
      str.Append(*s);
    }
    ++s;
  }
  return fragment.forget();
}

void nsGenericHTMLElement::SetInnerText(const nsAString& aValue) {
  RefPtr<nsINode> node = TextToNode(aValue, NodeInfo()->NodeInfoManager());
  ReplaceChildren(node, IgnoreErrors());
}

// https://html.spec.whatwg.org/#merge-with-the-next-text-node
static void MergeWithNextTextNode(Text& aText, ErrorResult& aRv) {
  RefPtr<Text> nextSibling = Text::FromNodeOrNull(aText.GetNextSibling());
  if (!nextSibling) {
    return;
  }
  nsAutoString data;
  nextSibling->GetData(data);
  aText.AppendData(data, aRv);
  nextSibling->Remove();
}

// https://html.spec.whatwg.org/#dom-outertext
void nsGenericHTMLElement::SetOuterText(const nsAString& aValue,
                                        ErrorResult& aRv) {
  nsCOMPtr<nsINode> parent = GetParentNode();
  if (!parent) {
    return aRv.ThrowNoModificationAllowedError("Element has no parent");
  }

  RefPtr<nsINode> next = GetNextSibling();
  RefPtr<nsINode> previous = GetPreviousSibling();

  // Batch possible DOMSubtreeModified events.
  mozAutoSubtreeModified subtree(OwnerDoc(), nullptr);

  nsNodeInfoManager* nim = NodeInfo()->NodeInfoManager();
  RefPtr<nsINode> node = TextToNode(aValue, nim);
  if (!node) {
    // This doesn't match the spec, see
    // https://github.com/whatwg/html/issues/7508
    node = new (nim) nsTextNode(nim);
  }
  parent->ReplaceChild(*node, *this, aRv);
  if (aRv.Failed()) {
    return;
  }

  if (next) {
    if (RefPtr<Text> text = Text::FromNodeOrNull(next->GetPreviousSibling())) {
      MergeWithNextTextNode(*text, aRv);
      if (aRv.Failed()) {
        return;
      }
    }
  }
  if (auto* text = Text::FromNodeOrNull(previous)) {
    MergeWithNextTextNode(*text, aRv);
  }
}

already_AddRefed<ElementInternals> nsGenericHTMLElement::AttachInternals(
    ErrorResult& aRv) {
  // ElementInternals is only available on autonomous custom element, so throws
  // an error by default. The spec steps are implemented in HTMLElement because
  // ElementInternals needs to hold a pointer to HTMLElement in order to forward
  // form operation to it.
  aRv.ThrowNotSupportedError(nsPrintfCString(
      "Cannot attach ElementInternals to a customized built-in or non-custom "
      "element "
      "'%s'",
      NS_ConvertUTF16toUTF8(NodeInfo()->NameAtom()->GetUTF16String()).get()));
  return nullptr;
}

ElementInternals* nsGenericHTMLElement::GetInternals() const {
  if (CustomElementData* data = GetCustomElementData()) {
    return data->GetElementInternals();
  }
  return nullptr;
}

bool nsGenericHTMLElement::IsFormAssociatedCustomElements() const {
  if (CustomElementData* data = GetCustomElementData()) {
    return data->IsFormAssociated();
  }
  return false;
}

void nsGenericHTMLElement::GetAutocapitalize(nsAString& aValue) const {
  GetEnumAttr(nsGkAtoms::autocapitalize, nullptr, kDefaultAutocapitalize->tag,
              aValue);
}
