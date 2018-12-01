/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_TypedObject_inl_h
#define builtin_TypedObject_inl_h

#include "builtin/TypedObject.h"

#include "gc/ObjectKind-inl.h"

/* static */
js::gc::AllocKind js::InlineTypedObject::allocKindForTypeDescriptor(
    TypeDescr* descr) {
  size_t nbytes = descr->size();
  MOZ_ASSERT(nbytes <= MaximumSize);

  return gc::GetGCObjectKindForBytes(nbytes + sizeof(TypedObject));
}

#endif  // builtin_TypedObject_inl_h
