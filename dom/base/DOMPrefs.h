/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DOMPrefs_h
#define mozilla_dom_DOMPrefs_h

namespace mozilla {
namespace dom {

class DOMPrefs final {
 public:
  // This must be called on the main-thread.
  static void Initialize();

  // Returns true if the browser.dom.window.dump.enabled pref is set.
  static bool DumpEnabled();
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_DOMPrefs_h
