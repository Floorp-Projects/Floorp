/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FontFaceSet.h"

#include "mozilla/dom/FontFaceSetBinding.h"
#include "mozilla/dom/Promise.h"

using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_INHERITED(FontFaceSet,
                                   DOMEventTargetHelper,
                                   mReady)

NS_IMPL_ADDREF_INHERITED(FontFaceSet, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(FontFaceSet, DOMEventTargetHelper)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(FontFaceSet)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

FontFaceSet::FontFaceSet(nsPIDOMWindow* aWindow)
  : DOMEventTargetHelper(aWindow)
{
  MOZ_COUNT_CTOR(FontFaceSet);
}

FontFaceSet::~FontFaceSet()
{
  MOZ_COUNT_DTOR(FontFaceSet);
}

JSObject*
FontFaceSet::WrapObject(JSContext* aContext)
{
  return FontFaceSetBinding::Wrap(aContext, this);
}

already_AddRefed<Promise>
FontFaceSet::Load(const nsAString& aFont,
                  const nsAString& aText,
                  ErrorResult& aRv)
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return nullptr;
}

bool
FontFaceSet::Check(const nsAString& aFont,
                   const nsAString& aText,
                   ErrorResult& aRv)
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return false;
}

Promise*
FontFaceSet::GetReady(ErrorResult& aRv)
{
  if (!mReady) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return mReady;
}

FontFaceSetLoadStatus
FontFaceSet::Status()
{
  return FontFaceSetLoadStatus::Loaded;
}

FontFaceSet*
FontFaceSet::Add(FontFace& aFontFace, ErrorResult& aRv)
{
  return this;
}

void
FontFaceSet::Clear()
{
}

bool
FontFaceSet::Delete(FontFace& aFontFace, ErrorResult& aRv)
{
  return false;
}

bool
FontFaceSet::Has(FontFace& aFontFace)
{
  return false;
}

FontFace*
FontFaceSet::IndexedGetter(uint32_t aIndex, bool& aFound)
{
  aFound = false;
  return nullptr;
}

uint32_t
FontFaceSet::Length()
{
  return 0;
}
