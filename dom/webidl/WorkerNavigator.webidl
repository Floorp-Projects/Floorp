/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


[Exposed=Worker,
 InstrumentedProps=(permissions)]
interface WorkerNavigator {
};

WorkerNavigator includes NavigatorID;
WorkerNavigator includes NavigatorLanguage;
WorkerNavigator includes NavigatorOnLine;
WorkerNavigator includes NavigatorConcurrentHardware;
WorkerNavigator includes NavigatorStorage;
WorkerNavigator includes GlobalPrivacyControl;

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

// https://w3c.github.io/web-locks/#navigator-mixins
WorkerNavigator includes NavigatorLocks;

// https://gpuweb.github.io/gpuweb/#navigator-gpu
WorkerNavigator includes NavigatorGPU;
