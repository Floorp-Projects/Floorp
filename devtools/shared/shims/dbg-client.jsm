/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This file only exists to support add-ons which import this module at a
 * specific path.
 */

const Cu = Components.utils;

const { Services } = Cu.import("resource://gre/modules/Services.jsm", {});

const WARNING_PREF = "devtools.migration.warnings";
if (Services.prefs.getBoolPref(WARNING_PREF)) {
  const { Deprecated } = Cu.import("resource://gre/modules/Deprecated.jsm", {});
  Deprecated.warning("dbg-client.jsm is deprecated.  Please use " +
                     "require(\"devtools/shared/client/main\") to load this " +
                     "module.", "https://bugzil.la/912121");
}

const { require } =
  Cu.import("resource://gre/modules/devtools/shared/Loader.jsm", {});

this.EXPORTED_SYMBOLS = ["DebuggerTransport",
                         "DebuggerClient",
                         "RootClient",
                         "LongStringClient",
                         "EnvironmentClient",
                         "ObjectClient"];

var client = require("devtools/shared/client/main");

this.DebuggerClient = client.DebuggerClient;
this.RootClient = client.RootClient;
this.LongStringClient = client.LongStringClient;
this.EnvironmentClient = client.EnvironmentClient;
this.ObjectClient = client.ObjectClient;

this.DebuggerTransport =
  require("devtools/shared/transport/transport").DebuggerTransport;
