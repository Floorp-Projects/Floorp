/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsJSInspector.h"
#include "jsfriendapi.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/dom/ScriptSettings.h"
#include "nsServiceManagerUtils.h"
#include "nsMemory.h"
#include "nsArray.h"
#include "nsTArray.h"

#define JSINSPECTOR_CONTRACTID "@mozilla.org/jsinspector;1"

#define JSINSPECTOR_CID                              \
  {                                                  \
    0xec5aa99c, 0x7abb, 0x4142, {                    \
      0xac, 0x5f, 0xaa, 0xb2, 0x41, 0x9e, 0x38, 0xe2 \
    }                                                \
  }

namespace mozilla {
namespace jsinspector {

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsJSInspector)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIJSInspector)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_CLASS(nsJSInspector)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsJSInspector)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsJSInspector)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsJSInspector)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsJSInspector)
  tmp->mRequestors.Clear();
  tmp->mLastRequestor = JS::NullValue();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsJSInspector)
  for (uint32_t i = 0; i < tmp->mRequestors.Length(); ++i) {
    NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mRequestors[i])
  }
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mLastRequestor)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

nsJSInspector::nsJSInspector()
    : mNestedLoopLevel(0), mRequestors(1), mLastRequestor(JS::NullValue()) {}

nsJSInspector::~nsJSInspector() {
  MOZ_ASSERT(mRequestors.Length() == 0);
  MOZ_ASSERT(mLastRequestor.isNull());
  mozilla::DropJSObjects(this);
}

NS_IMETHODIMP
nsJSInspector::EnterNestedEventLoop(JS::Handle<JS::Value> requestor,
                                    uint32_t* out) {
  nsresult rv = NS_OK;

  mLastRequestor = requestor;
  mRequestors.AppendElement(requestor);
  mozilla::HoldJSObjects(this);

  mozilla::dom::AutoNoJSAPI nojsapi;

  uint32_t nestLevel = ++mNestedLoopLevel;
  if (!SpinEventLoopUntil("nsJSInspector::EnterNestedEventLoop"_ns,
                          [&]() { return mNestedLoopLevel < nestLevel; })) {
    rv = NS_ERROR_UNEXPECTED;
  }

  NS_ASSERTION(mNestedLoopLevel <= nestLevel,
               "nested event didn't unwind properly");

  if (mNestedLoopLevel == nestLevel) {
    mLastRequestor = mRequestors.ElementAt(--mNestedLoopLevel);
  }

  *out = mNestedLoopLevel;
  return rv;
}

NS_IMETHODIMP
nsJSInspector::ExitNestedEventLoop(uint32_t* out) {
  if (mNestedLoopLevel > 0) {
    mRequestors.RemoveElementAt(--mNestedLoopLevel);
    if (mNestedLoopLevel > 0)
      mLastRequestor = mRequestors.ElementAt(mNestedLoopLevel - 1);
    else
      mLastRequestor = JS::NullValue();
  } else {
    return NS_ERROR_FAILURE;
  }

  *out = mNestedLoopLevel;

  return NS_OK;
}

NS_IMETHODIMP
nsJSInspector::GetEventLoopNestLevel(uint32_t* out) {
  *out = mNestedLoopLevel;
  return NS_OK;
}

NS_IMETHODIMP
nsJSInspector::GetLastNestRequestor(JS::MutableHandle<JS::Value> out) {
  out.set(mLastRequestor);
  return NS_OK;
}

}  // namespace jsinspector
}  // namespace mozilla
