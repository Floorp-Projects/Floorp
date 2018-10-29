/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Worker_h
#define mozilla_dom_Worker_h

#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/RefPtr.h"
#include "mozilla/WeakPtr.h"

#ifdef XP_WIN
#undef PostMessage
#endif

namespace mozilla {
namespace dom {

struct PostMessageOptions;
struct WorkerOptions;
class WorkerPrivate;

class Worker : public DOMEventTargetHelper
             , public SupportsWeakPtr<Worker>
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(Worker,
                                                         DOMEventTargetHelper)
  MOZ_DECLARE_WEAKREFERENCE_TYPENAME(Worker)

  static already_AddRefed<Worker>
  Constructor(const GlobalObject& aGlobal, const nsAString& aScriptURL,
              const WorkerOptions& aOptions, ErrorResult& aRv);

  JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void
  PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
              const Sequence<JSObject*>& aTransferable,
              ErrorResult& aRv);

  void
  PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
              const PostMessageOptions& aOptions,
              ErrorResult& aRv);

  void
  Terminate();

  IMPL_EVENT_HANDLER(error)
  IMPL_EVENT_HANDLER(message)
  IMPL_EVENT_HANDLER(messageerror)

protected:
  Worker(nsIGlobalObject* aGlobalObject,
         already_AddRefed<WorkerPrivate> aWorkerPrivate);
  ~Worker();

  RefPtr<WorkerPrivate> mWorkerPrivate;
};

} // dom namespace
} // mozilla namespace

#endif /* mozilla_dom_Worker_h */
