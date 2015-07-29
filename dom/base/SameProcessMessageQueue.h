/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SameProcessMessageQueue_h
#define mozilla_dom_SameProcessMessageQueue_h

#include "nsIRunnable.h"
#include "mozilla/nsRefPtr.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {

class CancelableRunnable;

class SameProcessMessageQueue
{
public:
  SameProcessMessageQueue();
  virtual ~SameProcessMessageQueue();

  class Runnable : public nsIRunnable
  {
  public:
    explicit Runnable();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIRUNNABLE

    virtual nsresult HandleMessage() = 0;

  protected:
    virtual ~Runnable() {}

  private:
    bool mDispatched;
  };

  void Push(Runnable* aRunnable);
  void Flush();

  static SameProcessMessageQueue* Get();

private:
  friend class CancelableRunnable;

  nsTArray<nsRefPtr<Runnable>> mQueue;
  static SameProcessMessageQueue* sSingleton;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SameProcessMessageQueue_h
