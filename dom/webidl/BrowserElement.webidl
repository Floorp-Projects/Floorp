/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

interface mixin BrowserElement {
  [Throws,
   Pref="dom.mozBrowserFramesEnabled",
   ChromeOnly]
  void sendMouseEvent(DOMString type,
                      unsigned long x,
                      unsigned long y,
                      unsigned long button,
                      unsigned long clickCount,
                      unsigned long modifiers);

  [Throws,
   Pref="dom.mozBrowserFramesEnabled",
   ChromeOnly]
  void goBack();

  [Throws,
   Pref="dom.mozBrowserFramesEnabled",
   ChromeOnly]
  void goForward();

  [Throws,
   Pref="dom.mozBrowserFramesEnabled",
   ChromeOnly]
  void reload(optional boolean hardReload = false);

  [Throws,
   Pref="dom.mozBrowserFramesEnabled",
   ChromeOnly]
  void stop();

  [Throws,
   Pref="dom.mozBrowserFramesEnabled",
   ChromeOnly]
  Promise<boolean> getCanGoBack();

  [Throws,
   Pref="dom.mozBrowserFramesEnabled",
   ChromeOnly]
  Promise<boolean> getCanGoForward();
};
