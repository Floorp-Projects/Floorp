/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[NoInterfaceObject, JSImplementation="@mozilla.org/aboutcapabilities;1"]
interface AboutCapabilities
{
  /**
   * When using setBoolPref, one also has to whitelist the
   * pref to be updated within AsyncPrefs.jsm
   */
  Promise<void> setBoolPref(DOMString aPref, boolean aValue);
  boolean getBoolPref(DOMString aPref, boolean? aDefaultValue);

  /**
   * When using setCharPref, one also has to whitelist the
   * pref to be updated within AsyncPrefs.jsm
   */
  Promise<void> setCharPref(DOMString aPref, DOMString aValue);
  DOMString getCharPref(DOMString aPref, DOMString? aDefaultValue);

  DOMString getStringFromBundle(DOMString aStrBundle, DOMString aStr);

  DOMString formatURLPref(DOMString aFormatURL);

  void sendAsyncMessage(DOMString aMessage, object? aParams);

  /* about:privatebrowsing */
  boolean isWindowPrivate();
};
