/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MMPrinter_h
#define MMPrinter_h

#include "mozilla/dom/DOMTypes.h"
#include "nsString.h"

namespace mozilla {
namespace dom {

class MMPrinter {
 public:
  static void Print(char const* aLocation, const nsAString& aMsg,
                    ClonedMessageData const& aData) {
    if (MOZ_UNLIKELY(MOZ_LOG_TEST(MMPrinter::sMMLog, LogLevel::Debug))) {
      MMPrinter::PrintImpl(aLocation, aMsg, aData);
    }
  }

 private:
  static LazyLogModule sMMLog;
  static void PrintImpl(char const* aLocation, const nsAString& aMsg,
                        ClonedMessageData const& aData);
};

}  // namespace dom
}  // namespace mozilla

#endif /* MMPrinter_h */