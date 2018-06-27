/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGTitleElement.h"
#include "mozilla/dom/SVGTitleElementBinding.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(Title)

namespace mozilla {
namespace dom {

JSObject*
SVGTitleElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SVGTitleElement_Binding::Wrap(aCx, this, aGivenProto);
}

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ISUPPORTS_INHERITED(SVGTitleElement, SVGTitleElementBase,
                            nsIMutationObserver)

//----------------------------------------------------------------------
// Implementation

SVGTitleElement::SVGTitleElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : SVGTitleElementBase(aNodeInfo)
{
  AddMutationObserver(this);
}

SVGTitleElement::~SVGTitleElement()
{
}

void
SVGTitleElement::CharacterDataChanged(nsIContent* aContent,
                                      const CharacterDataChangeInfo&)
{
  SendTitleChangeEvent(false);
}

void
SVGTitleElement::ContentAppended(nsIContent* aFirstNewContent)
{
  SendTitleChangeEvent(false);
}

void
SVGTitleElement::ContentInserted(nsIContent* aChild)
{
  SendTitleChangeEvent(false);
}

void
SVGTitleElement::ContentRemoved(nsIContent* aChild,
                                nsIContent* aPreviousSibling)
{
  SendTitleChangeEvent(false);
}

nsresult
SVGTitleElement::BindToTree(nsIDocument *aDocument,
                             nsIContent *aParent,
                             nsIContent *aBindingParent,
                             bool aCompileEventHandlers)
{
  // Let this fall through.
  nsresult rv = SVGTitleElementBase::BindToTree(aDocument, aParent,
                                                aBindingParent,
                                                aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  SendTitleChangeEvent(true);

  return NS_OK;
}

void
SVGTitleElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  SendTitleChangeEvent(false);

  // Let this fall through.
  SVGTitleElementBase::UnbindFromTree(aDeep, aNullParent);
}

void
SVGTitleElement::DoneAddingChildren(bool aHaveNotified)
{
  if (!aHaveNotified) {
    SendTitleChangeEvent(false);
  }
}

void
SVGTitleElement::SendTitleChangeEvent(bool aBound)
{
  nsIDocument* doc = GetUncomposedDoc();
  if (doc) {
    doc->NotifyPossibleTitleChange(aBound);
  }
}

//----------------------------------------------------------------------
// nsINode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGTitleElement)

} // namespace dom
} // namespace mozilla

