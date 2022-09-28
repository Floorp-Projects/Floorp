/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_RootedRecord_h__
#define mozilla_dom_RootedRecord_h__

#include "mozilla/dom/Record.h"
#include "js/RootingAPI.h"

namespace mozilla::dom {

template <typename K, typename V>
class MOZ_RAII RootedRecord final : public Record<K, V>,
                                    private JS::CustomAutoRooter {
 public:
  template <typename CX>
  explicit RootedRecord(const CX& cx)
      : Record<K, V>(), JS::CustomAutoRooter(cx) {}

  virtual void trace(JSTracer* trc) override { TraceRecord(trc, *this); }
};

}  // namespace mozilla::dom

#endif /* mozilla_dom_RootedRecord_h__ */
