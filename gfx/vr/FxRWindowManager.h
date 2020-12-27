/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once
#include <cstdint>

class nsPIDOMWindowOuter;
class nsWindow;

// FxRWindowManager is a singleton that is responsible for tracking all of
// the top-level windows created for Firefox Reality on Desktop. Only a
// single window is initially supported.
class FxRWindowManager final {
 public:
  static FxRWindowManager* GetInstance();

  void AddWindow(nsPIDOMWindowOuter* aWindow);
  bool IsFxRWindow(uint64_t aOuterWindowID);
  bool IsFxRWindow(const nsWindow* aWindow) const;
  uint64_t GetWindowID() const;

 private:
  FxRWindowManager();

  // Only a single window is supported for tracking. Support for multiple
  // windows will require a data structure to collect windows as they are
  // created.
  nsPIDOMWindowOuter* mWindow;
};
