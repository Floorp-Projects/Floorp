/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

const { Cc, Ci, Cu } = require("chrome");

const { AddonManager } = Cu.import("resource://gre/modules/AddonManager.jsm", {});
const { Promise: promise } = Cu.import("resource://gre/modules/Promise.jsm", {});
const { Services } = Cu.import("resource://gre/modules/Services.jsm", {});
const { Simulator } = Cu.import("resource://gre/modules/devtools/shared/apps/Simulator.jsm");
const { SimulatorProcess } = require("./simulator-process");
const Runtime = require("sdk/system/runtime");
const URL = require("sdk/url");

const ROOT_URI = require("addon").uri;
const PROFILE_URL = ROOT_URI + "profile/";
const BIN_URL = ROOT_URI + "b2g/";

var process;

function launch(options) {
  // Close already opened simulation.
  if (process) {
    return close().then(launch.bind(null, options));
  }

  // Compute B2G runtime path.
  let path;
  try {
    let pref = "extensions." + require("addon").id + ".customRuntime";
    path = Services.prefs.getComplexValue(pref, Ci.nsIFile);
  } catch(e) {}

  if (!path) {
    let executables = {
      WINNT: "b2g-bin.exe",
      Darwin: "B2G.app/Contents/MacOS/b2g-bin",
      Linux: "b2g-bin",
    };
    path = URL.toFilename(BIN_URL);
    path += Runtime.OS == "WINNT" ? "\\" : "/";
    path += executables[Runtime.OS];
  }
  options.runtimePath = path;
  console.log("simulator path:", options.runtimePath);

  // Compute Gaia profile path.
  if (!options.profilePath) {
    let gaiaProfile;
    try {
      let pref = "extensions." + require("addon").id + ".gaiaProfile";
      gaiaProfile = Services.prefs.getComplexValue(pref, Ci.nsIFile).path;
    } catch(e) {}

    options.profilePath = gaiaProfile || URL.toFilename(PROFILE_URL);
  }

  process = new SimulatorProcess(options);
  process.run();

  return promise.resolve();
}

function close() {
  if (!process) {
    return promise.resolve();
  }
  let p = process;
  process = null;
  return p.kill();
}

var name;

AddonManager.getAddonByID(require("addon").id, function (addon) {
  name = addon.name.replace(" Simulator", "");

  Simulator.register(name, {
    // We keep the deprecated `appinfo` object so that recent simulator addons
    // remain forward-compatible with older Firefox.
    appinfo: { label: name },
    launch: launch,
    close: close
  });
});

exports.shutdown = function () {
  Simulator.unregister(name);
  close();
}

