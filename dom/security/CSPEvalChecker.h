/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CSPEvalChecker_h
#define mozilla_dom_CSPEvalChecker_h

#include "nsString.h"

struct JSContext;
class nsGlobalWindowInner;

namespace mozilla {
namespace dom {

class WorkerPrivate;

class CSPEvalChecker final
{
public:
  static nsresult
  CheckForWindow(JSContext* aCx, nsGlobalWindowInner* aWindow,
                 const nsAString& aExpression, bool* aAllowEval);

  static nsresult
  CheckForWorker(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
                 const nsAString& aExpression, bool* aAllowEval);
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_CSPEvalChecker_h
