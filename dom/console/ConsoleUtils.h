/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ConsoleUtils_h
#define mozilla_dom_ConsoleUtils_h

#include "mozilla/JSObjectHolder.h"
#include "nsISupportsImpl.h"
#include "nsString.h"

namespace mozilla {
namespace dom {

class ConsoleUtils final
{
public:
  NS_INLINE_DECL_REFCOUNTING(ConsoleUtils)

  enum Level {
    eLog,
    eWarning,
    eError,
  };

  // Main-thread only, reports a console message from a ServiceWorker.
  static void
  ReportForServiceWorkerScope(const nsAString& aScope,
                              const nsAString& aMessage,
                              const nsAString& aFilename,
                              uint32_t aLineNumber,
                              uint32_t aColumnNumber,
                              Level aLevel);

private:
  ConsoleUtils();
  ~ConsoleUtils();

  static ConsoleUtils*
  GetOrCreate();

  JSObject*
  GetOrCreateSandbox(JSContext* aCx);

  void
  ReportForServiceWorkerScopeInternal(const nsAString& aScope,
                                      const nsAString& aMessage,
                                      const nsAString& aFilename,
                                      uint32_t aLineNumber,
                                      uint32_t aColumnNumber,
                                      Level aLevel);

  RefPtr<JSObjectHolder> mSandbox;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_ConsoleUtils_h */
