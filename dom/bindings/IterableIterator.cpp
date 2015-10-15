/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/IterableIterator.h"

namespace mozilla {
namespace dom {

// Due to IterableIterator being a templated class, we implement the necessary
// CC bits in a superclass that IterableIterator then inherits from. This allows
// us to put the macros outside of the header. The base class has pure virtual
// functions for Traverse/Unlink that the templated subclasses will override.

NS_IMPL_CYCLE_COLLECTION_CLASS(IterableIteratorBase)

NS_IMPL_CYCLE_COLLECTING_ADDREF(IterableIteratorBase)
NS_IMPL_CYCLE_COLLECTING_RELEASE(IterableIteratorBase)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(IterableIteratorBase)
  tmp->TraverseHelper(cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(IterableIteratorBase)
  tmp->UnlinkHelper();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IterableIteratorBase)
NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

}
}
