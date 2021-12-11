/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


[Exposed=Worker]
interface WorkerNavigator {
};

WorkerNavigator includes NavigatorID;
WorkerNavigator includes NavigatorLanguage;
WorkerNavigator includes NavigatorOnLine;
WorkerNavigator includes NavigatorConcurrentHardware;
WorkerNavigator includes NavigatorStorage;

// http://wicg.github.io/netinfo/#extensions-to-the-navigator-interface
[Exposed=Worker]
partial interface WorkerNavigator {
    [Pref="dom.netinfo.enabled", Throws]
    readonly attribute NetworkInformation connection;
};

// https://wicg.github.io/media-capabilities/#idl-index
[Exposed=Worker]
partial interface WorkerNavigator {
  [SameObject, Func="mozilla::dom::MediaCapabilities::Enabled"]
  readonly attribute MediaCapabilities mediaCapabilities;
};

// https://wicg.github.io/web-locks/#navigator-mixins
WorkerNavigator includes NavigatorLocks;
