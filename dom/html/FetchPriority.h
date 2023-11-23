/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FetchPriority_h
#define mozilla_dom_FetchPriority_h

#include <cstdint>

namespace mozilla {
class LazyLogModule;

namespace dom {

enum class RequestPriority : uint8_t;

// <https://html.spec.whatwg.org/multipage/urls-and-fetching.html#fetch-priority-attributes>.
enum class FetchPriority : uint8_t { High, Low, Auto };

FetchPriority ToFetchPriority(RequestPriority aRequestPriority);

// @param aSupportsPriority see <nsISupportsPriority.idl>.
void LogPriorityMapping(LazyLogModule& aLazyLogModule,
                        FetchPriority aFetchPriority,
                        int32_t aSupportsPriority);

extern const char* kFetchPriorityAttributeValueHigh;
extern const char* kFetchPriorityAttributeValueLow;
extern const char* kFetchPriorityAttributeValueAuto;

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_FetchPriority_h
