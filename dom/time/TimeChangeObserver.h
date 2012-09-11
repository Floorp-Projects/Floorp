/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _mozilla_time_change_observer_h_
#define _mozilla_time_change_observer_h_

#include "mozilla/Hal.h"
#include "mozilla/Observer.h"
#include "mozilla/HalTypes.h"
#include "nsPIDOMWindow.h"
#include "nsWeakPtr.h"

typedef mozilla::Observer<mozilla::hal::SystemTimeChange> SystemTimeChangeObserver;

class nsSystemTimeChangeObserver : public SystemTimeChangeObserver
{
public:
  static nsSystemTimeChangeObserver* GetInstance();
  virtual ~nsSystemTimeChangeObserver();
  void Notify(const mozilla::hal::SystemTimeChange& aReason);
  static nsresult AddWindowListener(nsIDOMWindow* aWindow);
  static nsresult RemoveWindowListener(nsIDOMWindow* aWindow);
private:
  nsresult AddWindowListenerImpl(nsIDOMWindow* aWindow);
  nsresult RemoveWindowListenerImpl(nsIDOMWindow* aWindow);
  nsSystemTimeChangeObserver() { };
  nsTArray<nsWeakPtr> mWindowListeners;
};

#endif //_mozilla_time_change_observer_h_
