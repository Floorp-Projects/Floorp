/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * url, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_url_h__
#define mozilla_dom_workers_url_h__

#include "mozilla/dom/URLBinding.h"

#include "EventTarget.h"

BEGIN_WORKERS_NAMESPACE

class URL : public EventTarget
{
public: // Methods for WebIDL
  static void
  CreateObjectURL(const WorkerGlobalObject& aGlobal,
                  JSObject* aArg, const objectURLOptionsWorkers& aOptions,
                  nsString& aResult, ErrorResult& aRv);

  static void
  CreateObjectURL(const WorkerGlobalObject& aGlobal,
                  JSObject& aArg, const objectURLOptionsWorkers& aOptions,
                  nsString& aResult, ErrorResult& aRv);

  static void
  RevokeObjectURL(const WorkerGlobalObject& aGlobal, const nsAString& aUrl);
};

END_WORKERS_NAMESPACE

#endif /* mozilla_dom_workers_url_h__ */
