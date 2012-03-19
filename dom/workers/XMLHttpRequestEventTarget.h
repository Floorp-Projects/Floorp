/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_xmlhttprequesteventtarget_h__
#define mozilla_dom_workers_xmlhttprequesteventtarget_h__

#include "mozilla/dom/workers/bindings/EventTarget.h"

BEGIN_WORKERS_NAMESPACE

class XMLHttpRequestEventTarget : public EventTarget
{
protected:
  XMLHttpRequestEventTarget(JSContext* aCx)
  : EventTarget(aCx)
  { }

  virtual ~XMLHttpRequestEventTarget()
  { }

public:
  virtual void
  _Trace(JSTracer* aTrc) MOZ_OVERRIDE;

  virtual void
  _Finalize(JSFreeOp* aFop) MOZ_OVERRIDE;

#define IMPL_GETTER_AND_SETTER(_type)                                          \
  JSObject*                                                                    \
  GetOn##_type(nsresult& aRv)                                                  \
  {                                                                            \
    return GetEventListener(NS_LITERAL_STRING(#_type), aRv);                   \
  }                                                                            \
                                                                               \
  void                                                                         \
  SetOn##_type(JSObject* aListener, nsresult& aRv)                             \
  {                                                                            \
    SetEventListener(NS_LITERAL_STRING(#_type), aListener, aRv);               \
  }

  IMPL_GETTER_AND_SETTER(loadstart)
  IMPL_GETTER_AND_SETTER(progress)
  IMPL_GETTER_AND_SETTER(abort)
  IMPL_GETTER_AND_SETTER(error)
  IMPL_GETTER_AND_SETTER(load)
  IMPL_GETTER_AND_SETTER(timeout)
  IMPL_GETTER_AND_SETTER(loadend)

#undef IMPL_GETTER_AND_SETTER
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_xmlhttprequesteventtarget_h__
