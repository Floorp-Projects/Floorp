/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");

var kUserValues;

function run_test() {
  do_get_profile();
  Cu.import("resource://gre/modules/KillSwitchMain.jsm");

  check_enabledValues();
  run_next_test();
}

function check_enabledValues() {
  let expected = {
    settings: {
      "debugger.remote-mode": "disabled",
      "developer.menu.enabled": false,
      "devtools.unrestricted": false,
      "lockscreen.enabled": true,
      "lockscreen.locked": true,
      "lockscreen.lock-immediately": true,
      "tethering.usb.enabled": false,
      "tethering.wifi.enabled": false,
      "ums.enabled": false
    },
    prefs: {
      "b2g.killswitch.test": true
    },
    properties: {
      "persist.sys.usb.config": "none"
    },
    services: {
      "adbd": "stop"
    }
  };

  for (let key of Object.keys(KillSwitchMain._enabledValues.settings)) {
    strictEqual(expected.settings[key],
                KillSwitchMain._enabledValues.settings[key],
                "setting " + key);
  }

  for (let key of Object.keys(KillSwitchMain._enabledValues.prefs)) {
    strictEqual(expected.prefs[key],
                KillSwitchMain._enabledValues.prefs[key],
                "pref " + key);
  }

  for (let key of Object.keys(KillSwitchMain._enabledValues.properties)) {
    strictEqual(expected.properties[key],
                KillSwitchMain._enabledValues.properties[key],
                "proprety " + key);
  }

  for (let key of Object.keys(KillSwitchMain._enabledValues.services)) {
    strictEqual(expected.services[key],
                KillSwitchMain._enabledValues.services[key],
                "service " + key);
  }
}

add_test(function test_prepareTestValues() {
  if (("adbd" in KillSwitchMain._enabledValues.services)) {
    strictEqual(KillSwitchMain._enabledValues.services["adbd"], "stop");

    // We replace those for the test because on Gonk we will loose the control
    KillSwitchMain._enabledValues.settings  = {
      "lockscreen.locked": true,
      "lockscreen.lock-immediately": true
    };
    KillSwitchMain._enabledValues.services   = {};
    KillSwitchMain._enabledValues.properties = {
      "dalvik.vm.heapmaxfree": "8m",
      "dalvik.vm.isa.arm.features": "div",
      "dalvik.vm.lockprof.threshold": "500",
      "net.bt.name": "Android",
      "dalvik.vm.stack-trace-file": "/data/anr/traces.txt"
    }
  }

  kUserValues = OS.Path.join(OS.Constants.Path.profileDir, "killswitch.json");
  run_next_test();
});

function reset_status() {
  KillSwitchMain._ksState = undefined;
  KillSwitchMain._libcutils.property_set("persist.moz.killswitch", "undefined");
}

function install_common_tests() {
  add_test(function test_readStateProperty() {
    KillSwitchMain._libcutils.property_set("persist.moz.killswitch", "false");
    KillSwitchMain.readStateProperty();
    strictEqual(KillSwitchMain._ksState, false);

    KillSwitchMain._libcutils.property_set("persist.moz.killswitch", "true");
    KillSwitchMain.readStateProperty();
    strictEqual(KillSwitchMain._ksState, true);

    run_next_test();
  });

  add_test(function test_writeStateProperty() {
    KillSwitchMain._ksState = false;
    KillSwitchMain.writeStateProperty();
    let state = KillSwitchMain._libcutils.property_get("persist.moz.killswitch");
    strictEqual(state, "false");

    KillSwitchMain._ksState = true;
    KillSwitchMain.writeStateProperty();
    state = KillSwitchMain._libcutils.property_get("persist.moz.killswitch");
    strictEqual(state, "true");

    run_next_test();
  });

  add_test(function test_doEnable() {
    reset_status();

    let enable = KillSwitchMain.doEnable();
    ok(enable, "should have a Promise");

    enable.then(() => {
      strictEqual(KillSwitchMain._ksState, true);
      let state = KillSwitchMain._libcutils.property_get("persist.moz.killswitch");
      strictEqual(state, "true");

      run_next_test();
    }).catch(err => {
      ok(false, "should have succeeded");
      run_next_test();
    });
  });

  add_test(function test_enableMessage() {
    reset_status();

    let listener = {
      assertPermission: function() {
        return true;
      },
      sendAsyncMessage: function(name, data) {
        strictEqual(name, "KillSwitch:Enable:OK");
        strictEqual(data.requestID, 1);

        let state = KillSwitchMain._libcutils.property_get("persist.moz.killswitch");
        strictEqual(state, "true");

        run_next_test();
      }
    };

    KillSwitchMain.receiveMessage({
      name: "KillSwitch:Enable",
      target: listener,
      data: {
        requestID: 1
      }
    });
  });

  add_test(function test_saveUserValues_prepare() {
    reset_status();

    OS.File.exists(kUserValues).then(e => {
      if (e) {
        OS.File.remove(kUserValues).then(() => {
          run_next_test();
        }).catch(err => {
          ok(false, "should have succeeded");
          run_next_test();
        });
      } else {
        run_next_test();
      }
    }).catch(err => {
      ok(false, "should have succeeded");
      run_next_test();
    });
  });

  add_test(function test_saveUserValues() {
    reset_status();

    let expectedValues = Object.assign({}, KillSwitchMain._enabledValues);

    // Reset _enabledValues so we check properly that the dumped state
    // is the current device state
    KillSwitchMain._enabledValues = {
      settings: {
        "lockscreen.locked": false,
        "lockscreen.lock-immediately": false
      },
      prefs: {
        "b2g.killswitch.test": false
      },
      properties: {
        "dalvik.vm.heapmaxfree": "32m",
        "dalvik.vm.isa.arm.features": "fdiv",
        "dalvik.vm.lockprof.threshold": "5000",
        "net.bt.name": "BTAndroid",
        "dalvik.vm.stack-trace-file": "/data/anr/stack-traces.txt"
      },
      services: {}
    };

    KillSwitchMain.saveUserValues().then(e => {
      ok(e, "should have succeeded");

      OS.File.read(kUserValues, { encoding: "utf-8" }).then(content => {
        let obj = JSON.parse(content);

        deepEqual(obj.settings, expectedValues.settings);
        notDeepEqual(obj.settings, KillSwitchMain._enabledValues.settings);

        deepEqual(obj.prefs, expectedValues.prefs);
        notDeepEqual(obj.prefs, KillSwitchMain._enabledValues.prefs);

        deepEqual(obj.properties, expectedValues.properties);
        notDeepEqual(obj.properties, KillSwitchMain._enabledValues.properties);

        run_next_test();
      }).catch(err => {
        ok(false, "should have succeeded");
        run_next_test();
      });
    }).catch(err => {
      ok(false, "should have succeeded");
      run_next_test();
    });
  });

  add_test(function test_saveUserValues_cleaup() {
    reset_status();

    OS.File.exists(kUserValues).then(e => {
      if (e) {
        OS.File.remove(kUserValues).then(() => {
          ok(true, "should have had a file");
          run_next_test();
        }).catch(err => {
          ok(false, "should have succeeded");
          run_next_test();
        });
      } else {
        ok(false, "should have had a file");
        run_next_test();
      }
    }).catch(err => {
      ok(false, "should have succeeded");
      run_next_test();
    });
  });

  add_test(function test_restoreUserValues_prepare() {
    reset_status();

    let fakeValues = {
      settings: {
        "lockscreen.locked": false,
        "lockscreen.lock-immediately": false
      },
      prefs: {
        "b2g.killswitch.test": false
      },
      properties: {
        "dalvik.vm.heapmaxfree": "32m",
        "dalvik.vm.isa.arm.features": "fdiv",
        "dalvik.vm.lockprof.threshold": "5000",
        "net.bt.name": "BTAndroid",
        "dalvik.vm.stack-trace-file": "/data/anr/stack-traces.txt"
      }
    };

    OS.File.exists(kUserValues).then(e => {
      if (!e) {
        OS.File.writeAtomic(kUserValues,
                            JSON.stringify(fakeValues)).then(() => {
          ok(true, "success writing file");
          run_next_test();
        }, err => {
          ok(false, "error writing file");
          run_next_test();
        });
      } else {
        ok(false, "file should not have been there");
        run_next_test();
      }
    }).catch(err => {
      ok(false, "should have succeeded");
      run_next_test();
    });
  });

  add_test(function test_restoreUserValues() {
    reset_status();

    KillSwitchMain.restoreUserValues().then(e => {
      ok(e, "should have succeeded");

      strictEqual(e.settings["lockscreen.locked"], false);
      strictEqual(e.settings["lockscreen.lock-immediately"], false);

      strictEqual(e.prefs["b2g.killswitch.test"], false);

      strictEqual(
        e.properties["dalvik.vm.heapmaxfree"],
        "32m");
      strictEqual(
        e.properties["dalvik.vm.isa.arm.features"],
        "fdiv");
      strictEqual(
        e.properties["dalvik.vm.lockprof.threshold"],
        "5000");
      strictEqual(
        e.properties["net.bt.name"],
        "BTAndroid");
      strictEqual(
        e.properties["dalvik.vm.stack-trace-file"],
        "/data/anr/stack-traces.txt");

      strictEqual(
        KillSwitchMain._libcutils.property_get("dalvik.vm.heapmaxfree"),
        "32m");
      strictEqual(
        KillSwitchMain._libcutils.property_get("dalvik.vm.isa.arm.features"),
        "fdiv");
      strictEqual(
        KillSwitchMain._libcutils.property_get("dalvik.vm.lockprof.threshold"),
        "5000");
      strictEqual(
        KillSwitchMain._libcutils.property_get("net.bt.name"),
        "BTAndroid");
      strictEqual(
        KillSwitchMain._libcutils.property_get("dalvik.vm.stack-trace-file"),
        "/data/anr/stack-traces.txt");

      run_next_test();
    }).catch(err => {
      ok(false, "should not have had an error");
      run_next_test();
    });
  });

  add_test(function test_doDisable() {
    reset_status();

    let disable = KillSwitchMain.doDisable()
    ok(disable, "should have a Promise");

    disable.then(() => {
      strictEqual(KillSwitchMain._ksState, false);
      let state = KillSwitchMain._libcutils.property_get("persist.moz.killswitch");
      strictEqual(state, "false");

      run_next_test();
    }).catch(err => {
      ok(false, "should have succeeded");
      run_next_test();
    });
  });

  add_test(function test_disableMessage() {
    reset_status();

    let listener = {
      assertPermission: function() {
        return true;
      },
      sendAsyncMessage: function(name, data) {
        strictEqual(name, "KillSwitch:Disable:OK");
        strictEqual(data.requestID, 2);

        let state = KillSwitchMain._libcutils.property_get("persist.moz.killswitch");
        strictEqual(state, "false");

        run_next_test();
      }
    };

    KillSwitchMain.receiveMessage({
      name: "KillSwitch:Disable",
      target: listener,
      data: {
        requestID: 2
      }
    });
  });

  add_test(function test_doEnable_only_once() {
    reset_status();

    let firstEnable = KillSwitchMain.doEnable();
    ok(firstEnable, "should have a first Promise");

    firstEnable.then(() => {
      let secondEnable = KillSwitchMain.doEnable();
      ok(secondEnable, "should have a second Promise");

      secondEnable.then(() => {
        ok(false, "second enable should have not succeeded");
        run_next_test();
      }).catch(err => {
        strictEqual(err, true, "second enable should reject(true);");
        run_next_test();
      });

    }).catch(err => {
      ok(false, "first enable should have succeeded");
      run_next_test();
    });
  });
}
