/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DOMPrefs_h
#define mozilla_dom_DOMPrefs_h

namespace mozilla {
namespace dom {

class DOMPrefs final
{
public:
  // Returns true if the browser.dom.window.dump.enabled pref is set.
  static bool DumpEnabled();

  // Returns true if the canvas.imagebitmap_extensions.enabled pref is set.
  static bool ImageBitmapExtensionsEnabled();
  static bool ImageBitmapExtensionsEnabled(JSContext* aCx, JSObject* aObj);

  // Returns true if the dom.caches.enabled pref is set.
  static bool DOMCachesEnabled();
  static bool DOMCachesEnabled(JSContext* aCx, JSObject* aObj);

  // Returns true if the dom.caches.testing.enabled pref is set.
  static bool DOMCachesTestingEnabled();
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_DOMPrefs_h
