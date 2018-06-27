/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLSpanElement.h"
#include "mozilla/dom/HTMLSpanElementBinding.h"

#include "mozilla/MappedDeclarations.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsAtom.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Span)

namespace mozilla {
namespace dom {

HTMLSpanElement::~HTMLSpanElement()
{
}

NS_IMPL_ELEMENT_CLONE(HTMLSpanElement)

JSObject*
HTMLSpanElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLSpanElement_Binding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
