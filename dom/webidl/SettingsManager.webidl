/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[JSImplementation="@mozilla.org/settingsLock;1",
 Pref="dom.mozSettings.enabled"]
interface SettingsLock : EventTarget {
  // Whether this lock is invalid
  readonly attribute boolean closed;

  // Contains a JSON object with name/value pairs to be set.
  DOMRequest set(object settings);

  // Result contains the value of the setting.
  DOMRequest get(DOMString name);

  DOMRequest clear();
  attribute EventHandler onsettingstransactionsuccess;
  attribute EventHandler onsettingstransactionfailure;
};

dictionary SettingChange {
  DOMString settingName;
  DOMString settingValue;
};

callback SettingChangeCallback = void (SettingChange setting);

[JSImplementation="@mozilla.org/settingsManager;1",
 NavigatorProperty="mozSettings",
 Pref="dom.mozSettings.enabled"]
interface SettingsManager : EventTarget {
  SettingsLock createLock();

  void addObserver(DOMString name, SettingChangeCallback callback);
  void removeObserver(DOMString name, SettingChangeCallback callback);

  attribute EventHandler onsettingchange;
};
