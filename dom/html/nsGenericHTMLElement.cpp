/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/DeclarationBlock.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/EventStates.h"
#include "mozilla/MappedDeclarations.h"
#include "mozilla/Likely.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/TextEditor.h"

#include "nscore.h"
#include "nsGenericHTMLElement.h"
#include "nsAttrValueInlines.h"
#include "nsCOMPtr.h"
#include "nsAtom.h"
#include "nsQueryObject.h"
#include "nsIContentInlines.h"
#include "nsIContentViewer.h"
#include "nsIDocument.h"
#include "nsIDocumentEncoder.h"
#include "nsIDOMWindow.h"
#include "nsMappedAttributes.h"
#include "nsHTMLStyleSheet.h"
#include "nsIHTMLDocument.h"
#include "nsPIDOMWindow.h"
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
#include "nsIPrincipal.h"
#include "nsContainerFrame.h"
#include "nsStyleUtil.h"

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
#include "nsIForm.h"
#include "nsIFormControl.h"
#include "mozilla/dom/HTMLFormElement.h"
#include "nsFocusManager.h"
#include "nsAttrValueOrString.h"

#include "mozilla/InternalMutationEvent.h"
#include "nsDOMStringMap.h"

#include "nsLayoutUtils.h"
#include "mozAutoDocUpdate.h"
#include "nsHtml5Module.h"
#include "nsITextControlElement.h"
#include "mozilla/dom/ElementInlines.h"
#include "HTMLFieldSetElement.h"
#include "nsTextNode.h"
#include "HTMLBRElement.h"
#include "HTMLMenuElement.h"
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
#include "ReferrerPolicy.h"
#include "mozilla/dom/HTMLLabelElement.h"
#include "mozilla/dom/HTMLInputElement.h"

using namespace mozilla;
using namespace mozilla::dom;

nsresult
nsGenericHTMLElement::CopyInnerTo(Element* aDst, bool aPreallocateChildren)
{
  MOZ_ASSERT(!aDst->GetUncomposedDoc(),
             "Should not CopyInnerTo an Element in a document");
  nsresult rv;

  bool reparse = (aDst->OwnerDoc() != OwnerDoc());

  rv = static_cast<nsGenericHTMLElement*>(aDst)->mAttrsAndChildren.
       EnsureCapacityToClone(mAttrsAndChildren, aPreallocateChildren);
  NS_ENSURE_SUCCESS(rv, rv);

  int32_t i, count = GetAttrCount();
  for (i = 0; i < count; ++i) {
    const nsAttrName *name = mAttrsAndChildren.AttrNameAt(i);
    const nsAttrValue *value = mAttrsAndChildren.AttrAt(i);

    if (name->Equals(nsGkAtoms::style, kNameSpaceID_None) &&
        value->Type() == nsAttrValue::eCSSDeclaration) {
      // We still clone CSS attributes, even in the cross-document case.
      // https://github.com/w3c/webappsec-csp/issues/212

      // We can't just set this as a string, because that will fail
      // to reparse the string into style data until the node is
      // inserted into the document.  Clone the Rule instead.
      nsAttrValue valueCopy(*value);
      rv = aDst->SetParsedAttr(name->NamespaceID(), name->LocalName(),
                               name->GetPrefix(), valueCopy, false);
      NS_ENSURE_SUCCESS(rv, rv);

      DeclarationBlock* cssDeclaration = value->GetCSSDeclarationValue();
      cssDeclaration->SetImmutable();
    } else if (reparse) {
      nsAutoString valStr;
      value->ToString(valStr);

      rv = aDst->SetAttr(name->NamespaceID(), name->LocalName(),
                         name->GetPrefix(), valStr, false);
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      nsAttrValue valueCopy(*value);
      rv = aDst->SetParsedAttr(name->NamespaceID(), name->LocalName(),
                               name->GetPrefix(), valueCopy, false);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

static const nsAttrValue::EnumTable kDirTable[] = {
  { "ltr", eDir_LTR },
  { "rtl", eDir_RTL },
  { "auto", eDir_Auto },
  { nullptr, 0 }
};

void
nsGenericHTMLElement::AddToNameTable(nsAtom* aName)
{
  MOZ_ASSERT(HasName(), "Node doesn't have name?");
  nsIDocument* doc = GetUncomposedDoc();
  if (doc && !IsInAnonymousSubtree()) {
    doc->AddToNameTable(this, aName);
  }
}

void
nsGenericHTMLElement::RemoveFromNameTable()
{
  if (HasName() && CanHaveName(NodeInfo()->NameAtom())) {
    if (nsIDocument* doc = GetUncomposedDoc()) {
      doc->RemoveFromNameTable(this,
                               GetParsedAttr(nsGkAtoms::name)->GetAtomValue());
    }
  }
}

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

static bool
IS_TABLE_CELL(LayoutFrameType frameType)
{
  return LayoutFrameType::TableCell == frameType ||
         LayoutFrameType::BCTableCell == frameType;
}

static bool
IsOffsetParent(nsIFrame* aFrame)
{
  LayoutFrameType frameType = aFrame->Type();

  if (IS_TABLE_CELL(frameType) || frameType == LayoutFrameType::Table) {
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

  nsIFrame* frame = GetPrimaryFrame(FlushType::Layout);
  if (!frame) {
    return nullptr;
  }

  nsIFrame* styleFrame = nsLayoutUtils::GetStyleFrame(frame);

  nsIFrame* parent = frame->GetParent();
  nsPoint origin(0, 0);

  nsIContent* offsetParent = nullptr;
  Element* docElement = GetComposedDoc()->GetRootElement();
  nsIContent* content = frame->GetContent();

  if (content && (content->IsHTMLElement(nsGkAtoms::body) ||
                  content == docElement)) {
    parent = frame;
  }
  else {
    const bool isPositioned = styleFrame->IsAbsPosContainingBlock();
    const bool isAbsolutelyPositioned = styleFrame->IsAbsolutelyPositioned();
    origin += frame->GetPositionIgnoringScrolling();

    for ( ; parent ; parent = parent->GetParent()) {
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
      offsetParent = GetComposedDoc()->GetBodyElement();
    }
  }

  // Subtract the parent border unless it uses border-box sizing.
  if (parent &&
      parent->StylePosition()->mBoxSizing != StyleBoxSizing::Border) {
    const nsStyleBorder* border = parent->StyleBorder();
    origin.x -= border->GetComputedBorderWidth(eSideLeft);
    origin.y -= border->GetComputedBorderWidth(eSideTop);
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

bool
nsGenericHTMLElement::Spellcheck()
{
  // Has the state has been explicitly set?
  nsIContent* node;
  for (node = this; node; node = node->GetParent()) {
    if (node->IsHTMLElement()) {
      static Element::AttrValuesArray strings[] =
        {&nsGkAtoms::_true, &nsGkAtoms::_false, nullptr};
      switch (node->AsElement()->FindAttrValueIn(kNameSpaceID_None,
                                                 nsGkAtoms::spellcheck, strings,
                                                 eCaseMatters)) {
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
  int32_t controlType = formControl->ControlType();
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
  bool isEditable = IsInComposedDoc() && HasFlag(NODE_IS_EDITABLE) &&
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
    RegAccessKey();
    if (HasName() && CanHaveName(NodeInfo()->NameAtom())) {
      aDocument->
        AddToNameTable(this, GetParsedAttr(nsGkAtoms::name)->GetAtomValue());
    }
  }

  if (HasFlag(NODE_IS_EDITABLE) && GetContentEditableValue() == eTrue &&
      IsInComposedDoc()) {
    nsCOMPtr<nsIHTMLDocument> htmlDocument =
      do_QueryInterface(GetComposedDoc());
    if (htmlDocument) {
      htmlDocument->ChangeContentEditableCount(this, +1);
    }
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

void
nsGenericHTMLElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  if (IsInUncomposedDoc()) {
    UnregAccessKey();
  }

  RemoveFromNameTable();

  if (GetContentEditableValue() == eTrue) {
    nsCOMPtr<nsIHTMLDocument> htmlDocument = do_QueryInterface(GetComposedDoc());
    if (htmlDocument) {
      htmlDocument->ChangeContentEditableCount(this, -1);
    }
  }

  nsStyledElement::UnbindFromTree(aDeep, aNullParent);

  // Invalidate .labels list. It will be repopulated when used the next time.
  nsExtendedDOMSlots* slots = GetExistingExtendedDOMSlots();
  if (slots && slots->mLabelsList) {
    slots->mLabelsList->MaybeResetRoot(SubtreeRoot());
  }
}

HTMLFormElement*
nsGenericHTMLElement::FindAncestorForm(HTMLFormElement* aCurrentForm)
{
  NS_ASSERTION(!HasAttr(kNameSpaceID_None, nsGkAtoms::form) ||
               IsHTMLElement(nsGkAtoms::img),
               "FindAncestorForm should not be called if @form is set!");

  // Make sure we don't end up finding a form that's anonymous from
  // our point of view. See also nsGenericHTMLFormElement::UpdateFieldSet.
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
          NS_ASSERTION(child->GetParent()->ComputeIndexOf(child) != -1,
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
  MOZ_ASSERT(nsCOMPtr<Link>(do_QueryObject(this)),
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

void 
nsGenericHTMLElement::GetEventTargetParentForAnchors(EventChainPreVisitor& aVisitor)
{
  nsGenericHTMLElementBase::GetEventTargetParent(aVisitor);

  if (!CheckHandleEventForAnchorsPreconditions(aVisitor)) {
    return;
  }

  GetEventTargetParentForLinks(aVisitor);
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
  MOZ_ASSERT(aURI, "Must provide aURI out param");

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
nsGenericHTMLElement::BeforeSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                    const nsAttrValueOrString* aValue,
                                    bool aNotify)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::accesskey) {
      // Have to unregister before clearing flag. See UnregAccessKey
      UnregAccessKey();
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
        manager->RemoveEventHandler(aName, EmptyString());
      }
    }
  }

  return nsGenericHTMLElementBase::BeforeSetAttr(aNamespaceID, aName, aValue,
                                                 aNotify);
}

nsresult
nsGenericHTMLElement::AfterSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                   const nsAttrValue* aValue,
                                   const nsAttrValue* aOldValue,
                                   nsIPrincipal* aMaybeScriptedPrincipal,
                                   bool aNotify)
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
      // A boolean tracking whether we need to recompute our directionality.
      // This needs to happen after we update our internal "dir" attribute
      // state but before we call SetDirectionalityOnDescendants.
      bool recomputeDirectionality = false;
      // We don't want to have to keep getting the "dir" attribute in
      // IntrinsicState, so we manually recompute our dir-related event states
      // here and send the relevant update notifications.
      EventStates dirStates;
      if (aValue && aValue->Type() == nsAttrValue::eEnum) {
        SetHasValidDir();
        dirStates |= NS_EVENT_STATE_HAS_DIR_ATTR;
        Directionality dirValue = (Directionality)aValue->GetEnumValue();
        if (dirValue == eDir_Auto) {
          dirStates |= NS_EVENT_STATE_DIR_ATTR_LIKE_AUTO;
        } else {
          dir = dirValue;
          SetDirectionality(dir, aNotify);
          if (dirValue == eDir_LTR) {
            dirStates |= NS_EVENT_STATE_DIR_ATTR_LTR;
          } else {
            MOZ_ASSERT(dirValue == eDir_RTL);
            dirStates |= NS_EVENT_STATE_DIR_ATTR_RTL;
          }
        }
      } else {
        if (aValue) {
          // We have a value, just not a valid one.
          dirStates |= NS_EVENT_STATE_HAS_DIR_ATTR;
        }
        ClearHasValidDir();
        if (NodeInfo()->Equals(nsGkAtoms::bdi)) {
          dirStates |= NS_EVENT_STATE_DIR_ATTR_LIKE_AUTO;
        } else {
          recomputeDirectionality = true;
        }
      }
      // Now figure out what's changed about our dir states.
      EventStates oldDirStates = State() & DIR_ATTR_STATES;
      EventStates changedStates = dirStates ^ oldDirStates;
      ToggleStates(changedStates, aNotify);
      if (recomputeDirectionality) {
        dir = RecomputeDirectionality(this, aNotify);
      }
      SetDirectionalityOnDescendants(this, dir, aNotify);
    } else if (aName == nsGkAtoms::contenteditable) {
      int32_t editableCountDelta = 0;
      if (aOldValue &&
          (aOldValue->Equals(NS_LITERAL_STRING("true"), eIgnoreCase) ||
          aOldValue->Equals(EmptyString(), eIgnoreCase))) {
        editableCountDelta = -1;
      }
      if (aValue && (aValue->Equals(NS_LITERAL_STRING("true"), eIgnoreCase) ||
                     aValue->Equals(EmptyString(), eIgnoreCase))) {
        ++editableCountDelta;
      }
      ChangeEditableState(editableCountDelta);
    } else if (aName == nsGkAtoms::accesskey) {
      if (aValue && !aValue->Equals(EmptyString(), eIgnoreCase)) {
        SetFlags(NODE_HAS_ACCESSKEY);
        RegAccessKey();
      }
    } else if (aName == nsGkAtoms::name) {
      if (aValue && !aValue->Equals(EmptyString(), eIgnoreCase)) {
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
    }
  }

  return nsGenericHTMLElementBase::AfterSetAttr(aNamespaceID, aName,
                                                aValue, aOldValue,
                                                aMaybeScriptedPrincipal,
                                                aNotify);
}

EventListenerManager*
nsGenericHTMLElement::GetEventListenerManagerForAttr(nsAtom* aAttrName,
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
    nsPIDOMWindowInner *win;

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
    if (nsPIDOMWindowInner* win = OwnerDoc()->GetInnerWindow()) {             \
      nsGlobalWindowInner* globalWin = nsGlobalWindowInner::Cast(win);        \
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
    nsPIDOMWindowInner* win = OwnerDoc()->GetInnerWindow();                   \
    if (!win) {                                                               \
      return;                                                                 \
    }                                                                         \
                                                                              \
    nsGlobalWindowInner* globalWin = nsGlobalWindowInner::Cast(win);          \
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
    if (nsPIDOMWindowInner* win = OwnerDoc()->GetInnerWindow()) {             \
      nsGlobalWindowInner* globalWin = nsGlobalWindowInner::Cast(win);        \
      OnErrorEventHandlerNonNull* errorHandler = globalWin->GetOn##name_();   \
      if (errorHandler) {                                                     \
        RefPtr<EventHandlerNonNull> handler =                                 \
          new EventHandlerNonNull(errorHandler);                              \
        return handler.forget();                                              \
      }                                                                       \
    }                                                                         \
    return nullptr;                                                           \
  }                                                                           \
                                                                              \
  RefPtr<EventHandlerNonNull> handler = nsINode::GetOn##name_();              \
  return handler.forget();                                                    \
}                                                                             \
void                                                                          \
nsGenericHTMLElement::SetOn##name_(EventHandlerNonNull* handler)              \
{                                                                             \
  if (IsAnyOfHTMLElements(nsGkAtoms::body, nsGkAtoms::frameset)) {            \
    nsPIDOMWindowInner* win = OwnerDoc()->GetInnerWindow();                   \
    if (!win) {                                                               \
      return;                                                                 \
    }                                                                         \
                                                                              \
    nsGlobalWindowInner* globalWin = nsGlobalWindowInner::Cast(win);          \
    RefPtr<OnErrorEventHandlerNonNull> errorHandler;                          \
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

void
nsGenericHTMLElement::GetBaseTarget(nsAString& aBaseTarget) const
{
  OwnerDoc()->GetBaseTarget(aBaseTarget);
}

//----------------------------------------------------------------------

bool
nsGenericHTMLElement::ParseAttribute(int32_t aNamespaceID,
                                     nsAtom* aAttribute,
                                     const nsAString& aValue,
                                     nsIPrincipal* aMaybeScriptedPrincipal,
                                     nsAttrValue& aResult)
{
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
  }

  return nsGenericHTMLElementBase::ParseAttribute(aNamespaceID, aAttribute,
                                                  aValue, aMaybeScriptedPrincipal,
                                                  aResult);
}

bool
nsGenericHTMLElement::ParseBackgroundAttribute(int32_t aNamespaceID,
                                               nsAtom* aAttribute,
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
    aResult.SetTo(uri, &aValue);
    return true;
  }

  return false;
}

bool
nsGenericHTMLElement::IsAttributeMapped(const nsAtom* aAttribute) const
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
    GetComposedDoc()->FlushPendingNotifications(FlushType::Frames);
  }
  nsIFrame* frame = GetPrimaryFrame();
  if (frame) {
    nsIFormControlFrame* form_frame = do_QueryFrame(frame);
    if (form_frame) {
      return form_frame;
    }

    // If we have generated content, the primary frame will be a
    // wrapper frame..  out real frame will be in its child list.
    for (frame = frame->PrincipalChildList().FirstChild();
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
    return doc->GetPresContext();
  }

  return nullptr;
}

static const nsAttrValue::EnumTable kDivAlignTable[] = {
  { "left", NS_STYLE_TEXT_ALIGN_MOZ_LEFT },
  { "right", NS_STYLE_TEXT_ALIGN_MOZ_RIGHT },
  { "center", NS_STYLE_TEXT_ALIGN_MOZ_CENTER },
  { "middle", NS_STYLE_TEXT_ALIGN_MOZ_CENTER },
  { "justify", NS_STYLE_TEXT_ALIGN_JUSTIFY },
  { nullptr, 0 }
};

static const nsAttrValue::EnumTable kFrameborderTable[] = {
  { "yes", NS_STYLE_FRAME_YES },
  { "no", NS_STYLE_FRAME_NO },
  { "1", NS_STYLE_FRAME_1 },
  { "0", NS_STYLE_FRAME_0 },
  { nullptr, 0 }
};

static const nsAttrValue::EnumTable kScrollingTable[] = {
  { "yes", NS_STYLE_FRAME_YES },
  { "no", NS_STYLE_FRAME_NO },
  { "on", NS_STYLE_FRAME_ON },
  { "off", NS_STYLE_FRAME_OFF },
  { "scroll", NS_STYLE_FRAME_SCROLL },
  { "noscroll", NS_STYLE_FRAME_NOSCROLL },
  { "auto", NS_STYLE_FRAME_AUTO },
  { nullptr, 0 }
};

static const nsAttrValue::EnumTable kTableVAlignTable[] = {
  { "top",     NS_STYLE_VERTICAL_ALIGN_TOP },
  { "middle",  NS_STYLE_VERTICAL_ALIGN_MIDDLE },
  { "bottom",  NS_STYLE_VERTICAL_ALIGN_BOTTOM },
  { "baseline",NS_STYLE_VERTICAL_ALIGN_BASELINE },
  { nullptr,   0 }
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
    { nullptr,     0 }
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
  { nullptr,  0 }
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
  { nullptr,  0 }
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
nsGenericHTMLElement::ParseImageAttribute(nsAtom* aAttribute,
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
    { net::kRPS_No_Referrer, static_cast<int16_t>(net::RP_No_Referrer) },
    { net::kRPS_Origin, static_cast<int16_t>(net::RP_Origin) },
    { net::kRPS_Origin_When_Cross_Origin, static_cast<int16_t>(net::RP_Origin_When_Crossorigin) },
    { net::kRPS_No_Referrer_When_Downgrade, static_cast<int16_t>(net::RP_No_Referrer_When_Downgrade) },
    { net::kRPS_Unsafe_URL, static_cast<int16_t>(net::RP_Unsafe_URL) },
    { net::kRPS_Strict_Origin, static_cast<int16_t>(net::RP_Strict_Origin) },
    { net::kRPS_Same_Origin, static_cast<int16_t>(net::RP_Same_Origin) },
    { net::kRPS_Strict_Origin_When_Cross_Origin, static_cast<int16_t>(net::RP_Strict_Origin_When_Cross_Origin) },
    { nullptr, 0 }
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

static inline void
MapLangAttributeInto(const nsMappedAttributes* aAttributes, MappedDeclarations& aDecls)
{
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
void
nsGenericHTMLElement::MapCommonAttributesIntoExceptHidden(const nsMappedAttributes* aAttributes,
                                                          MappedDeclarations& aDecls)
{
  if (!aDecls.PropertyIsSet(eCSSProperty__moz_user_modify)) {
    const nsAttrValue* value =
      aAttributes->GetAttr(nsGkAtoms::contenteditable);
    if (value) {
      if (value->Equals(nsGkAtoms::_empty, eCaseMatters) ||
          value->Equals(nsGkAtoms::_true, eIgnoreCase)) {
        aDecls.SetKeywordValue(eCSSProperty__moz_user_modify,
                               StyleUserModify::ReadWrite);
      }
      else if (value->Equals(nsGkAtoms::_false, eIgnoreCase)) {
          aDecls.SetKeywordValue(eCSSProperty__moz_user_modify,
                                 StyleUserModify::ReadOnly);
      }
    }
  }

  MapLangAttributeInto(aAttributes, aDecls);
}

void
nsGenericHTMLElement::MapCommonAttributesInto(const nsMappedAttributes* aAttributes,
                                              MappedDeclarations& aDecls)
{
  MapCommonAttributesIntoExceptHidden(aAttributes, aDecls);

  if (!aDecls.PropertyIsSet(eCSSProperty_display)) {
    if (aAttributes->IndexOfAttr(nsGkAtoms::hidden) >= 0) {
      aDecls.SetKeywordValue(eCSSProperty_display, StyleDisplay::None);
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
                                                 MappedDeclarations& aDecls)
{
  const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::align);
  if (value && value->Type() == nsAttrValue::eEnum) {
    int32_t align = value->GetEnumValue();
    if (!aDecls.PropertyIsSet(eCSSProperty_float)) {
      if (align == NS_STYLE_TEXT_ALIGN_LEFT) {
        aDecls.SetKeywordValue(eCSSProperty_float, StyleFloat::Left);
      } else if (align == NS_STYLE_TEXT_ALIGN_RIGHT) {
        aDecls.SetKeywordValue(eCSSProperty_float, StyleFloat::Right);
      }
    }
    if (!aDecls.PropertyIsSet(eCSSProperty_vertical_align)) {
      switch (align) {
      case NS_STYLE_TEXT_ALIGN_LEFT:
      case NS_STYLE_TEXT_ALIGN_RIGHT:
        break;
      default:
        aDecls.SetKeywordValue(eCSSProperty_vertical_align, align);
        break;
      }
    }
  }
}

void
nsGenericHTMLElement::MapDivAlignAttributeInto(const nsMappedAttributes* aAttributes,
                                               MappedDeclarations& aDecls)
{
  if (!aDecls.PropertyIsSet(eCSSProperty_text_align)) {
    // align: enum
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::align);
    if (value && value->Type() == nsAttrValue::eEnum)
      aDecls.SetKeywordValue(eCSSProperty_text_align, value->GetEnumValue());
  }
}

void
nsGenericHTMLElement::MapVAlignAttributeInto(const nsMappedAttributes* aAttributes,
                                             MappedDeclarations& aDecls)
{
  if (!aDecls.PropertyIsSet(eCSSProperty_vertical_align)) {
    // align: enum
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::valign);
    if (value && value->Type() == nsAttrValue::eEnum)
      aDecls.SetKeywordValue(eCSSProperty_vertical_align, value->GetEnumValue());
  }
}

void
nsGenericHTMLElement::MapImageMarginAttributeInto(const nsMappedAttributes* aAttributes,
                                                  MappedDeclarations& aDecls)
{
  const nsAttrValue* value;

  // hspace: value
  value = aAttributes->GetAttr(nsGkAtoms::hspace);
  if (value) {
    if (value->Type() == nsAttrValue::eInteger) {
      aDecls.SetPixelValueIfUnset(eCSSProperty_margin_left,
                                  (float)value->GetIntegerValue());
      aDecls.SetPixelValueIfUnset(eCSSProperty_margin_right,
                                  (float)value->GetIntegerValue());
    } else if (value->Type() == nsAttrValue::ePercent) {
      aDecls.SetPercentValueIfUnset(eCSSProperty_margin_left,
                                    value->GetPercentValue());
      aDecls.SetPercentValueIfUnset(eCSSProperty_margin_right,
                                    value->GetPercentValue());
    }
  }

  // vspace: value
  value = aAttributes->GetAttr(nsGkAtoms::vspace);
  if (value) {
    if (value->Type() == nsAttrValue::eInteger) {
      aDecls.SetPixelValueIfUnset(eCSSProperty_margin_top,
                                  (float)value->GetIntegerValue());
      aDecls.SetPixelValueIfUnset(eCSSProperty_margin_bottom,
                                  (float)value->GetIntegerValue());
    } else if (value->Type() == nsAttrValue::ePercent) {
      aDecls.SetPercentValueIfUnset(eCSSProperty_margin_top,
                                    value->GetPercentValue());
      aDecls.SetPercentValueIfUnset(eCSSProperty_margin_bottom,
                                    value->GetPercentValue());
    }
  }
}

void
nsGenericHTMLElement::MapWidthAttributeInto(const nsMappedAttributes* aAttributes,
                                            MappedDeclarations& aDecls)
{
  // width: value
  if (!aDecls.PropertyIsSet(eCSSProperty_width)) {
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::width);
    if (value && value->Type() == nsAttrValue::eInteger) {
      aDecls.SetPixelValue(eCSSProperty_width,
                           (float)value->GetIntegerValue());
    } else if (value && value->Type() == nsAttrValue::ePercent) {
      aDecls.SetPercentValue(eCSSProperty_width,
                             value->GetPercentValue());
    }
  }
}

void
nsGenericHTMLElement::MapHeightAttributeInto(const nsMappedAttributes* aAttributes,
                                             MappedDeclarations& aDecls)
{
  // height: value
  if (!aDecls.PropertyIsSet(eCSSProperty_height)) {
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::height);
    if (value && value->Type() == nsAttrValue::eInteger) {
      aDecls.SetPixelValue(eCSSProperty_height,
                           (float)value->GetIntegerValue());
    } else if (value && value->Type() == nsAttrValue::ePercent) {
      aDecls.SetPercentValue(eCSSProperty_height,
                             value->GetPercentValue());
    }
  }
}

void
nsGenericHTMLElement::MapImageSizeAttributesInto(const nsMappedAttributes* aAttributes,
                                                 MappedDeclarations& aDecls)
{
  nsGenericHTMLElement::MapWidthAttributeInto(aAttributes, aDecls);
  nsGenericHTMLElement::MapHeightAttributeInto(aAttributes, aDecls);
}

void
nsGenericHTMLElement::MapImageBorderAttributeInto(const nsMappedAttributes* aAttributes,
                                                  MappedDeclarations& aDecls)
{
  // border: pixels
  const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::border);
  if (!value)
    return;

  nscoord val = 0;
  if (value->Type() == nsAttrValue::eInteger)
    val = value->GetIntegerValue();

  aDecls.SetPixelValueIfUnset(eCSSProperty_border_top_width, (float)val);
  aDecls.SetPixelValueIfUnset(eCSSProperty_border_right_width, (float)val);
  aDecls.SetPixelValueIfUnset(eCSSProperty_border_bottom_width, (float)val);
  aDecls.SetPixelValueIfUnset(eCSSProperty_border_left_width, (float)val);

  aDecls.SetKeywordValueIfUnset(eCSSProperty_border_top_style,
                                NS_STYLE_BORDER_STYLE_SOLID);
  aDecls.SetKeywordValueIfUnset(eCSSProperty_border_right_style,
                                NS_STYLE_BORDER_STYLE_SOLID);
  aDecls.SetKeywordValueIfUnset(eCSSProperty_border_bottom_style,
                                NS_STYLE_BORDER_STYLE_SOLID);
  aDecls.SetKeywordValueIfUnset(eCSSProperty_border_left_style,
                                NS_STYLE_BORDER_STYLE_SOLID);

  aDecls.SetCurrentColorIfUnset(eCSSProperty_border_top_color);
  aDecls.SetCurrentColorIfUnset(eCSSProperty_border_right_color);
  aDecls.SetCurrentColorIfUnset(eCSSProperty_border_bottom_color);
  aDecls.SetCurrentColorIfUnset(eCSSProperty_border_left_color);
}

void
nsGenericHTMLElement::MapBackgroundInto(const nsMappedAttributes* aAttributes,
                                        MappedDeclarations& aDecls)
{

  if (!aDecls.PropertyIsSet(eCSSProperty_background_image)) {
    // background
    nsAttrValue* value =
      const_cast<nsAttrValue*>(aAttributes->GetAttr(nsGkAtoms::background));
    if (value) {
      aDecls.SetBackgroundImage(*value);
    }
  }
}

void
nsGenericHTMLElement::MapBGColorInto(const nsMappedAttributes* aAttributes,
                                     MappedDeclarations& aDecls)
{
  if (!aDecls.PropertyIsSet(eCSSProperty_background_color)) {
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::bgcolor);
    nscolor color;
    if (value && value->GetColorValue(color)) {
      aDecls.SetColorValue(eCSSProperty_background_color, color);
    }
  }
}

void
nsGenericHTMLElement::MapBackgroundAttributesInto(const nsMappedAttributes* aAttributes,
                                                  MappedDeclarations& aDecls)
{
  MapBackgroundInto(aAttributes, aDecls);
  MapBGColorInto(aAttributes, aDecls);
}

//----------------------------------------------------------------------

int32_t
nsGenericHTMLElement::GetIntAttr(nsAtom* aAttr, int32_t aDefault) const
{
  const nsAttrValue* attrVal = mAttrsAndChildren.GetAttr(aAttr);
  if (attrVal && attrVal->Type() == nsAttrValue::eInteger) {
    return attrVal->GetIntegerValue();
  }
  return aDefault;
}

nsresult
nsGenericHTMLElement::SetIntAttr(nsAtom* aAttr, int32_t aValue)
{
  nsAutoString value;
  value.AppendInt(aValue);

  return SetAttr(kNameSpaceID_None, aAttr, value, true);
}

uint32_t
nsGenericHTMLElement::GetUnsignedIntAttr(nsAtom* aAttr,
                                         uint32_t aDefault) const
{
  const nsAttrValue* attrVal = mAttrsAndChildren.GetAttr(aAttr);
  if (!attrVal || attrVal->Type() != nsAttrValue::eInteger) {
    return aDefault;
  }

  return attrVal->GetIntegerValue();
}

void
nsGenericHTMLElement::GetURIAttr(nsAtom* aAttr, nsAtom* aBaseAttr,
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
nsGenericHTMLElement::GetURIAttr(nsAtom* aAttr, nsAtom* aBaseAttr, nsIURI** aURI) const
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

HTMLMenuElement*
nsGenericHTMLElement::GetContextMenu() const
{
  nsAutoString value;
  GetHTMLAttr(nsGkAtoms::contextmenu, value);
  if (!value.IsEmpty()) {
    //XXXsmaug How should this work in Shadow DOM?
    nsIDocument* doc = GetUncomposedDoc();
    if (doc) {
      return HTMLMenuElement::FromNodeOrNull(doc->GetElementById(value));
    }
  }
  return nullptr;
}

bool
nsGenericHTMLElement::IsLabelable() const
{
  return IsAnyOfHTMLElements(nsGkAtoms::progress, nsGkAtoms::meter);
}

/* static */ bool
nsGenericHTMLElement::MatchLabelsElement(Element* aElement, int32_t aNamespaceID,
                                         nsAtom* aAtom, void* aData)
{
  HTMLLabelElement* element = HTMLLabelElement::FromNode(aElement);
  return element && element->GetControl() == aData;
}

already_AddRefed<nsINodeList>
nsGenericHTMLElement::Labels()
{
  MOZ_ASSERT(IsLabelable(),
             "Labels() only allow labelable elements to use it.");
  nsExtendedDOMSlots* slots = ExtendedDOMSlots();

  if (!slots->mLabelsList) {
    slots->mLabelsList = new nsLabelsNodeList(SubtreeRoot(), MatchLabelsElement,
                                              nullptr, this);
  }

  RefPtr<nsLabelsNodeList> labels = slots->mLabelsList;
  return labels.forget();
}

bool
nsGenericHTMLElement::IsInteractiveHTMLContent(bool aIgnoreTabindex) const
{
  return IsAnyOfHTMLElements(nsGkAtoms::details, nsGkAtoms::embed,
                             nsGkAtoms::keygen) ||
         (!aIgnoreTabindex && HasAttr(kNameSpaceID_None, nsGkAtoms::tabindex));
}

// static
bool
nsGenericHTMLElement::TouchEventsEnabled(JSContext* aCx, JSObject* aGlobal)
{
  return TouchEvent::PrefEnabled(aCx, aGlobal);
}

//----------------------------------------------------------------------

nsGenericHTMLFormElement::nsGenericHTMLFormElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo,
                                                   uint8_t aType)
  : nsGenericHTMLElement(aNodeInfo)
  , nsIFormControl(aType)
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
  return !(aFlags & ~eHTML_FORM_CONTROL);
}

void
nsGenericHTMLFormElement::SaveSubtreeState()
{
  SaveState();

  nsGenericHTMLElement::SaveSubtreeState();
}

void
nsGenericHTMLFormElement::SetForm(HTMLFormElement* aForm)
{
  MOZ_ASSERT(aForm, "Don't pass null here");
  NS_ASSERTION(!mForm,
               "We don't support switching from one non-null form to another.");

  SetForm(aForm, false);
}

void nsGenericHTMLFormElement::SetForm(HTMLFormElement* aForm, bool aBindToTree)
{
  if (aForm) {
    BeforeSetForm(aBindToTree);
  }

  // keep a *weak* ref to the form here
  mForm = aForm;
}

void
nsGenericHTMLFormElement::ClearForm(bool aRemoveFromForm, bool aUnbindOrDelete)
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
      mForm->RemoveElementFromTable(this, nameVal);
    }

    if (!idVal.IsEmpty()) {
      mForm->RemoveElementFromTable(this, idVal);
    }
  }

  UnsetFlags(ADDED_TO_FORM);
  mForm = nullptr;

  AfterClearForm(aUnbindOrDelete);
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

nsIContent::IMEState
nsGenericHTMLFormElement::GetDesiredIMEState()
{
  TextEditor* textEditor = GetTextEditorInternal();
  if (!textEditor) {
    return nsGenericHTMLElement::GetDesiredIMEState();
  }
  IMEState state;
  nsresult rv = textEditor->GetPreferredIMEState(&state);
  if (NS_FAILED(rv)) {
    return nsGenericHTMLElement::GetDesiredIMEState();
  }
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
      nsContentUtils::AutoFocusEnabled() && aDocument) {
    aDocument->SetAutoFocusElement(this);
  }

  // If @form is set, the element *has* to be in a composed document, otherwise
  // it wouldn't be possible to find an element with the corresponding id.
  // If @form isn't set, the element *has* to have a parent, otherwise it
  // wouldn't be possible to find a form ancestor.
  // We should not call UpdateFormOwner if none of these conditions are
  // fulfilled.
  if (HasAttr(kNameSpaceID_None, nsGkAtoms::form) ? IsInComposedDoc()
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
      ClearForm(true, true);
    } else {
      // Recheck whether we should still have an mForm.
      if (HasAttr(kNameSpaceID_None, nsGkAtoms::form) ||
          !FindAncestorForm(mForm)) {
        ClearForm(true, true);
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
nsGenericHTMLFormElement::BeforeSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                        const nsAttrValueOrString* aValue,
                                        bool aNotify)
{
  if (aNameSpaceID == kNameSpaceID_None) {
    nsAutoString tmp;

    // remove the control from the hashtable as needed

    if (mForm && (aName == nsGkAtoms::name || aName == nsGkAtoms::id)) {
      GetAttr(kNameSpaceID_None, aName, tmp);

      if (!tmp.IsEmpty()) {
        mForm->RemoveElementFromTable(this, tmp);
      }
    }

    if (mForm && aName == nsGkAtoms::type) {
      GetAttr(kNameSpaceID_None, nsGkAtoms::name, tmp);

      if (!tmp.IsEmpty()) {
        mForm->RemoveElementFromTable(this, tmp);
      }

      GetAttr(kNameSpaceID_None, nsGkAtoms::id, tmp);

      if (!tmp.IsEmpty()) {
        mForm->RemoveElementFromTable(this, tmp);
      }

      mForm->RemoveElement(this, false);
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
nsGenericHTMLFormElement::AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                       const nsAttrValue* aValue,
                                       const nsAttrValue* aOldValue,
                                       nsIPrincipal* aMaybeScriptedPrincipal,
                                       bool aNotify)
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

  return nsGenericHTMLElement::AfterSetAttr(aNameSpaceID, aName,
                                            aValue, aOldValue,
                                            aMaybeScriptedPrincipal,
                                            aNotify);
}

void
nsGenericHTMLFormElement::GetEventTargetParent(EventChainPreVisitor& aVisitor)
{
  if (aVisitor.mEvent->IsTrusted() && (aVisitor.mEvent->mMessage == eFocus ||
                                       aVisitor.mEvent->mMessage == eBlur)) {
    // We have to handle focus/blur event to change focus states in
    // PreHandleEvent to prevent it breaks event target chain creation.
    aVisitor.mWantsPreHandleEvent = true;
  }
  nsGenericHTMLElement::GetEventTargetParent(aVisitor);
}

nsresult
nsGenericHTMLFormElement::PreHandleEvent(EventChainVisitor& aVisitor)
{
  if (aVisitor.mEvent->IsTrusted()) {
    switch (aVisitor.mEvent->mMessage) {
      case eFocus: {
        // Check to see if focus has bubbled up from a form control's
        // child textfield or button.  If that's the case, don't focus
        // this parent file control -- leave focus on the child.
        nsIFormControlFrame* formControlFrame = GetFormControlFrame(true);
        if (formControlFrame &&
            aVisitor.mEvent->mOriginalTarget == static_cast<nsINode*>(this))
          formControlFrame->SetFocus(true, true);
        break;
      }
      case eBlur: {
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
  int32_t type = ControlType();
  // It's easier to test the types that _cannot_ be disabled
  return
    type != NS_FORM_OBJECT &&
    type != NS_FORM_OUTPUT;
}

bool
nsGenericHTMLFormElement::DoesReadOnlyApply() const
{
  int32_t type = ControlType();
  if (!(type & NS_FORM_INPUT_ELEMENT) && type != NS_FORM_TEXTAREA) {
    return false;
  }

  switch (type)
  {
    case NS_FORM_INPUT_HIDDEN:
    case NS_FORM_INPUT_BUTTON:
    case NS_FORM_INPUT_IMAGE:
    case NS_FORM_INPUT_RESET:
    case NS_FORM_INPUT_SUBMIT:
    case NS_FORM_INPUT_RADIO:
    case NS_FORM_INPUT_FILE:
    case NS_FORM_INPUT_CHECKBOX:
    case NS_FORM_INPUT_RANGE:
    case NS_FORM_INPUT_COLOR:
      return false;
#ifdef DEBUG
    case NS_FORM_TEXTAREA:
    case NS_FORM_INPUT_TEXT:
    case NS_FORM_INPUT_PASSWORD:
    case NS_FORM_INPUT_SEARCH:
    case NS_FORM_INPUT_TEL:
    case NS_FORM_INPUT_EMAIL:
    case NS_FORM_INPUT_URL:
    case NS_FORM_INPUT_NUMBER:
    case NS_FORM_INPUT_DATE:
    case NS_FORM_INPUT_TIME:
    case NS_FORM_INPUT_MONTH:
    case NS_FORM_INPUT_WEEK:
    case NS_FORM_INPUT_DATETIME_LOCAL:
      return true;
    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected input type in DoesReadOnlyApply()");
      return true;
#else // DEBUG
    default:
      return true;
#endif // DEBUG
  }
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

  if (mForm && mForm->IsDefaultSubmitElement(this)) {
      NS_ASSERTION(IsSubmitControl(),
                   "Default submit element that isn't a submit control.");
      // We are the default submit element (:default)
      state |= NS_EVENT_STATE_DEFAULT;
  }

  // Make the text controls read-write
  if (!state.HasState(NS_EVENT_STATE_MOZ_READWRITE) &&
      DoesReadOnlyApply()) {
    if (!GetBoolAttr(nsGkAtoms::readonly)) {
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
  if (nsPIDOMWindowOuter* win = doc->GetWindow()) {
    nsCOMPtr<nsPIDOMWindowOuter> rootWindow = win->GetPrivateRoot();

    nsCOMPtr<nsIFocusManager> fm = do_GetService(FOCUSMANAGER_CONTRACTID);
    if (fm && rootWindow) {
      nsCOMPtr<mozIDOMWindowProxy> activeWindow;
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
  nsAutoString formId;
  DocumentOrShadowRoot* docOrShadow = GetUncomposedDocOrConnectedShadowRoot();
  GetAttr(kNameSpaceID_None, nsGkAtoms::form, formId);
  NS_ASSERTION(!formId.IsEmpty(),
               "@form value should not be the empty string!");
  RefPtr<nsAtom> atom = NS_Atomize(formId);

  return docOrShadow->AddIDTargetObserver(atom, FormIdUpdated, this, false);
}

void
nsGenericHTMLFormElement::RemoveFormIdObserver()
{
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
  switch (aMessage) {
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
    case eWheel:
    case eLegacyMouseLineOrPageScroll:
    case eLegacyMousePixelScroll:
      return false;
    default:
      break;
  }

  // FIXME(emilio): This poking at the style of the frame is slightly bogus
  // unless we flush before every event, which we don't really want to do.
  if (aFrame &&
      aFrame->StyleUserInterface()->mUserInput == StyleUserInput::None) {
    return true;
  }

  return IsDisabled();
}

void
nsGenericHTMLFormElement::UpdateFormOwner(bool aBindToTree,
                                          Element* aFormIdElement)
{
  MOZ_ASSERT(!aBindToTree || !aFormIdElement,
             "aFormIdElement shouldn't be set if aBindToTree is true!");

  bool needStateUpdate = false;
  if (!aBindToTree) {
    needStateUpdate = mForm && mForm->IsDefaultSubmitElement(this);
    ClearForm(true, false);
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

        NS_ASSERTION(!IsInComposedDoc() ||
                     element == GetUncomposedDocOrConnectedShadowRoot()->
                       GetElementById(formId),
                     "element should be equals to the current element "
                     "associated with the id in @form!");

        if (element &&
            element->IsHTMLElement(nsGkAtoms::form) &&
            nsContentUtils::IsInSameAnonymousTree(this, element)) {
          SetForm(static_cast<HTMLFormElement*>(element), aBindToTree);
        }
      }
     } else {
      // We now have a parent, so we may have picked up an ancestor form.  Search
      // for it.  Note that if mForm is already set we don't want to do this,
      // because that means someone (probably the content sink) has already set
      // it to the right value.  Also note that even if being bound here didn't
      // change our parent, we still need to search, since our parent chain
      // probably changed _somewhere_.
      SetForm(FindAncestorForm(), aBindToTree);
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

  // Don't walk out of anonymous subtrees. Note the similar code in
  // nsGenericHTMLElement::FindAncestorForm.
  nsIContent* bindingParent = GetBindingParent();

  for (parent = GetParent(); parent && parent != bindingParent;
       prev = parent, parent = parent->GetParent()) {
    HTMLFieldSetElement* fieldset =
      HTMLFieldSetElement::FromNode(parent);
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
nsGenericHTMLFormElement::UpdateDisabledState(bool aNotify)
{
  if (!CanBeDisabled()) {
    return;
  }

  bool isDisabled = HasAttr(kNameSpaceID_None, nsGkAtoms::disabled);
  if (!isDisabled && mFieldSet) {
    isDisabled = mFieldSet->IsDisabled();
  }

  EventStates disabledStates;
  if (isDisabled) {
    disabledStates |= NS_EVENT_STATE_DISABLED;
  } else {
    disabledStates |= NS_EVENT_STATE_ENABLED;
  }

  EventStates oldDisabledStates = State() & DISABLED_STATES;
  EventStates changedStates = disabledStates ^ oldDisabledStates;

  if (!changedStates.IsEmpty()) {
    ToggleStates(changedStates, aNotify);
  }
}

void
nsGenericHTMLFormElement::UpdateRequiredState(bool aIsRequired, bool aNotify)
{
#ifdef DEBUG
  int32_t type = ControlType();
#endif
  MOZ_ASSERT((type & NS_FORM_INPUT_ELEMENT) ||
              type == NS_FORM_SELECT ||
              type == NS_FORM_TEXTAREA,
             "This should be called only on types that @required applies");

#ifdef DEBUG
  HTMLInputElement* input = HTMLInputElement::FromNode(this);
  if (input) {
    MOZ_ASSERT(input->DoesRequiredApply(),
               "This should be called only on input types that @required applies");
  }
#endif

  EventStates requiredStates;
  if (aIsRequired) {
    requiredStates |= NS_EVENT_STATE_REQUIRED;
  } else {
    requiredStates |= NS_EVENT_STATE_OPTIONAL;
  }

  EventStates oldRequiredStates = State() & REQUIRED_STATES;
  EventStates changedStates = requiredStates ^ oldRequiredStates;

  if (!changedStates.IsEmpty()) {
    ToggleStates(changedStates, aNotify);
  }
}

void
nsGenericHTMLFormElement::FieldSetDisabledChanged(bool aNotify)
{
  UpdateDisabledState(aNotify);
}

bool
nsGenericHTMLFormElement::IsLabelable() const
{
  // TODO: keygen should be in that list, see bug 101019.
  uint32_t type = ControlType();
  return (type & NS_FORM_INPUT_ELEMENT && type != NS_FORM_INPUT_HIDDEN) ||
         type & NS_FORM_BUTTON_ELEMENT ||
         // type == NS_FORM_KEYGEN ||
         type == NS_FORM_OUTPUT ||
         type == NS_FORM_SELECT ||
         type == NS_FORM_TEXTAREA;
}

void
nsGenericHTMLFormElement::GetFormAction(nsString& aValue)
{
  uint32_t type = ControlType();
  if (!(type & NS_FORM_INPUT_ELEMENT) && !(type & NS_FORM_BUTTON_ELEMENT)) {
    return;
  }

  if (!GetAttr(kNameSpaceID_None, nsGkAtoms::formaction, aValue) ||
      aValue.IsEmpty()) {
    nsIDocument* document = OwnerDoc();
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

//----------------------------------------------------------------------

void
nsGenericHTMLElement::Click(CallerType aCallerType)
{
  if (HandlingClick())
    return;

  // Strong in case the event kills it
  nsCOMPtr<nsIDocument> doc = GetComposedDoc();

  RefPtr<nsPresContext> context;
  if (doc) {
    context = doc->GetPresContext();
  }

  SetHandlingClick();

  // Mark this event trusted if Click() is called from system code.
  WidgetMouseEvent event(aCallerType == CallerType::System,
                         eMouseClick, nullptr, WidgetMouseEvent::eReal);
  event.mFlags.mIsPositionless = true;
  event.inputSource = MouseEvent_Binding::MOZ_SOURCE_UNKNOWN;

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
  bool disabled = false;
  bool disallowOverridingFocusability = true;

  if (IsEditableRoot()) {
    // Editable roots should always be focusable.
    disallowOverridingFocusability = true;

    // Ignore the disabled attribute in editable contentEditable/designMode
    // roots.
    if (!HasAttr(kNameSpaceID_None, nsGkAtoms::tabindex)) {
      // The default value for tabindex should be 0 for editable
      // contentEditable roots.
      tabIndex = 0;
    }
  }
  else {
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

  // If a tabindex is specified at all, or the default tabindex is 0, we're focusable
  *aIsFocusable =
    (tabIndex >= 0 || (!disabled && HasAttr(kNameSpaceID_None, nsGkAtoms::tabindex)));

  return disallowOverridingFocusability;
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

bool
nsGenericHTMLElement::PerformAccesskey(bool aKeyCausesActivation,
                                       bool aIsTrustedEvent)
{
  nsPresContext* presContext = GetPresContext(eForUncomposedDoc);
  if (!presContext) {
    return false;
  }

  // It's hard to say what HTML4 wants us to do in all cases.
  bool focused = true;
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm) {
    fm->SetFocus(this, nsIFocusManager::FLAG_BYKEY);

    // Return true if the element became the current focus within its window.
    nsPIDOMWindowOuter* window = OwnerDoc()->GetWindow();
    focused = (window && window->GetFocusedElement());
  }

  if (aKeyCausesActivation) {
    // Click on it if the users prefs indicate to do so.
    nsAutoPopupStatePusher popupStatePusher(aIsTrustedEvent ?
                                            openAllowed : openAbused);
    DispatchSimulatedClick(this, aIsTrustedEvent, presContext);
  }

  return focused;
}

nsresult
nsGenericHTMLElement::DispatchSimulatedClick(nsGenericHTMLElement* aElement,
                                             bool aIsTrusted,
                                             nsPresContext* aPresContext)
{
  WidgetMouseEvent event(aIsTrusted, eMouseClick, nullptr,
                         WidgetMouseEvent::eReal);
  event.inputSource = MouseEvent_Binding::MOZ_SOURCE_KEYBOARD;
  event.mFlags.mIsPositionless = true;
  return EventDispatcher::Dispatch(ToSupports(aElement), aPresContext, &event);
}

already_AddRefed<TextEditor>
nsGenericHTMLElement::GetAssociatedEditor()
{
  // If contenteditable is ever implemented, it might need to do something different here?

  RefPtr<TextEditor> textEditor = GetTextEditorInternal();
  return textEditor.forget();
}

// static
void
nsGenericHTMLElement::SyncEditorsOnSubtree(nsIContent* content)
{
  /* Sync this node */
  nsGenericHTMLElement* element = FromNode(content);
  if (element) {
    RefPtr<TextEditor> textEditor = element->GetAssociatedEditor();
    if (textEditor) {
      textEditor->SyncRealTimeSpell();
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

        nsAtom *attr = name->Atom();
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
    if (!child->IsElement() ||
        !child->AsElement()->HasAttr(kNameSpaceID_None,
                                     nsGkAtoms::contenteditable)) {
      MakeContentDescendantsEditable(child, aDocument);
    }
  }
}

void
nsGenericHTMLElement::ChangeEditableState(int32_t aChange)
{
  nsIDocument* document = GetComposedDoc();
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
    // Don't update across Shadow DOM boundary.
    while (parent && parent->IsElement()) {
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
    already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo, uint8_t aType
  )
  : nsGenericHTMLFormElement(aNodeInfo, aType)
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

PresState*
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
  PresState* result = history->GetState(mStateKey);
  if (!result) {
    UniquePtr<PresState> newState = NewPresState();
    result = newState.get();
    history->AddState(mStateKey, std::move(newState));
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

  // Get the pres state for this key
  PresState* state = history->GetState(mStateKey);
  if (state) {
    bool result = RestoreState(state);
    history->RemoveState(mStateKey);
    return result;
  }

  return false;
}

void
nsGenericHTMLFormElementWithState::NodeInfoChanged(nsIDocument* aOldDoc)
{
  nsGenericHTMLElement::NodeInfoChanged(aOldDoc);
  mStateKey.SetIsVoid(true);
}

nsSize
nsGenericHTMLElement::GetWidthHeightForImage(RefPtr<imgRequestProxy>& aImageRequest)
{
  nsSize size(0,0);

  nsIFrame* frame = GetPrimaryFrame(FlushType::Layout);

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
nsGenericHTMLElement::IsEventAttributeNameInternal(nsAtom *aName)
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

static bool
IsOrHasAncestorWithDisplayNone(Element* aElement, nsIPresShell* aPresShell)
{
  return !aElement->HasServoData() || Servo_Element_IsDisplayNone(aElement);
}

void
nsGenericHTMLElement::GetInnerText(mozilla::dom::DOMString& aValue,
                                   mozilla::ErrorResult& aError)
{
  if (!GetPrimaryFrame(FlushType::Layout)) {
    nsIPresShell* presShell = nsContentUtils::GetPresShellForContent(this);
    // NOTE(emilio): We need to check the presshell is initialized in order to
    // ensure the document is styled.
    if (!presShell || !presShell->DidInitialize() ||
        IsOrHasAncestorWithDisplayNone(this, presShell)) {
      GetTextContentInternal(aValue, aError);
      return;
    }
  }

  nsRange::GetInnerTextNoFlush(aValue, aError, this);
}

void
nsGenericHTMLElement::SetInnerText(const nsAString& aValue)
{
  // Batch possible DOMSubtreeModified events.
  mozAutoSubtreeModified subtree(OwnerDoc(), nullptr);
  FireNodeRemovedForChildren();

  // Might as well stick a batch around this since we're performing several
  // mutations.
  mozAutoDocUpdate updateBatch(GetComposedDoc(), true);
  nsAutoMutationBatch mb;

  mb.Init(this, true, false);

  while (HasChildren()) {
    RemoveChildNode(nsINode::GetFirstChild(), true);
  }

  mb.RemovalDone();

  nsString str;
  const char16_t* s = aValue.BeginReading();
  const char16_t* end = aValue.EndReading();
  while (true) {
    if (s != end && *s == '\r' && s + 1 != end && s[1] == '\n') {
      // a \r\n pair should only generate one <br>, so just skip the \r
      ++s;
    }
    if (s == end || *s == '\r' || *s == '\n') {
      if (!str.IsEmpty()) {
        RefPtr<nsTextNode> textContent =
          new nsTextNode(NodeInfo()->NodeInfoManager());
        textContent->SetText(str, true);
        AppendChildTo(textContent, true);
      }
      if (s == end) {
        break;
      }
      str.Truncate();
      already_AddRefed<mozilla::dom::NodeInfo> ni =
        NodeInfo()->NodeInfoManager()->GetNodeInfo(nsGkAtoms::br,
          nullptr, kNameSpaceID_XHTML, ELEMENT_NODE);
      RefPtr<HTMLBRElement> br = new HTMLBRElement(ni);
      AppendChildTo(br, true);
    } else {
      str.Append(*s);
    }
    ++s;
  }

  mb.NodesAdded();
}
