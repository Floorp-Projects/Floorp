/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

callback BrowserElementNextPaintEventCallback = void ();

enum BrowserFindCaseSensitivity { "case-sensitive", "case-insensitive" };
enum BrowserFindDirection { "forward", "backward" };

dictionary BrowserElementDownloadOptions {
  DOMString? filename;
  DOMString? referrer;
};

dictionary BrowserElementExecuteScriptOptions {
  DOMString? url;
  DOMString? origin;
};

[NoInterfaceObject]
interface BrowserElement {
};

BrowserElement implements BrowserElementCommon;
BrowserElement implements BrowserElementPrivileged;

[NoInterfaceObject]
interface BrowserElementCommon {
  [Throws,
   Pref="dom.mozBrowserFramesEnabled",
   ChromeOnly]
  void setVisible(boolean visible);

  [Throws,
   Pref="dom.mozBrowserFramesEnabled",
   ChromeOnly]
  DOMRequest getVisible();

  [Throws,
   Pref="dom.mozBrowserFramesEnabled",
   ChromeOnly]
  void setActive(boolean active);

  [Throws,
   Pref="dom.mozBrowserFramesEnabled",
   ChromeOnly]
  boolean getActive();

  [Throws,
   Pref="dom.mozBrowserFramesEnabled",
   ChromeOnly]
  void addNextPaintListener(BrowserElementNextPaintEventCallback listener);

  [Throws,
   Pref="dom.mozBrowserFramesEnabled",
   ChromeOnly]
  void removeNextPaintListener(BrowserElementNextPaintEventCallback listener);
};

[NoInterfaceObject]
interface BrowserElementPrivileged {
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
   Func="TouchEvent::PrefEnabled",
   ChromeOnly]
  void sendTouchEvent(DOMString type,
                      sequence<unsigned long> identifiers,
                      sequence<long> x,
                      sequence<long> y,
                      sequence<unsigned long> rx,
                      sequence<unsigned long> ry,
                      sequence<float> rotationAngles,
                      sequence<float> forces,
                      unsigned long count,
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
  DOMRequest download(DOMString url,
                      optional BrowserElementDownloadOptions options);

  [Throws,
   Pref="dom.mozBrowserFramesEnabled",
   ChromeOnly]
  DOMRequest purgeHistory();

  [Throws,
   Pref="dom.mozBrowserFramesEnabled",
   ChromeOnly]
  DOMRequest getScreenshot([EnforceRange] unsigned long width,
                           [EnforceRange] unsigned long height,
                           optional DOMString mimeType="");

  [Throws,
   Pref="dom.mozBrowserFramesEnabled",
   ChromeOnly]
  void zoom(float zoom);

  [Throws,
   Pref="dom.mozBrowserFramesEnabled",
   ChromeOnly]
  DOMRequest getCanGoBack();

  [Throws,
   Pref="dom.mozBrowserFramesEnabled",
   ChromeOnly]
  DOMRequest getCanGoForward();

  [Throws,
   Pref="dom.mozBrowserFramesEnabled",
   ChromeOnly]
  DOMRequest getContentDimensions();

  [Throws,
   Pref="dom.mozBrowserFramesEnabled",
   ChromeOnly]
  void findAll(DOMString searchString, BrowserFindCaseSensitivity caseSensitivity);

  [Throws,
   Pref="dom.mozBrowserFramesEnabled",
   ChromeOnly]
  void findNext(BrowserFindDirection direction);

  [Throws,
   Pref="dom.mozBrowserFramesEnabled",
   ChromeOnly]
  void clearMatch();

  [Throws,
   Pref="dom.mozBrowserFramesEnabled",
   ChromeOnly]
  DOMRequest executeScript(DOMString script,
                           optional BrowserElementExecuteScriptOptions options);

  [Throws,
   Pref="dom.mozBrowserFramesEnabled",
   ChromeOnly]
  DOMRequest getWebManifest();

};

// Bits needed for BrowserElementAudioChannel.
partial interface BrowserElementPrivileged {
  [Pure, Cached, Throws,
   Pref="dom.mozBrowserFramesEnabled",
   ChromeOnly]
  readonly attribute sequence<BrowserElementAudioChannel> allowedAudioChannels;

  /**
   * Mutes all audio in this browser.
   */
  [Throws,
   Pref="dom.mozBrowserFramesEnabled",
   ChromeOnly]
  void mute();

  /**
   * Unmutes all audio in this browser.
   */
  [Throws,
   Pref="dom.mozBrowserFramesEnabled",
   ChromeOnly]
  void unmute();

  /**
   * Obtains whether or not the browser is muted.
   */
  [Throws,
   Pref="dom.mozBrowserFramesEnabled",
   ChromeOnly]
  DOMRequest getMuted();

  /**
   * Sets the volume for the browser.
   */
  [Throws,
   Pref="dom.mozBrowserFramesEnabled",
   ChromeOnly]
  void setVolume(float volume);

  /**
   * Gets the volume for the browser.
   */
  [Throws,
   Pref="dom.mozBrowserFramesEnabled",
   ChromeOnly]
  DOMRequest getVolume();
};
