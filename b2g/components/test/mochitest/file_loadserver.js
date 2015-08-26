let Ci = Components.interfaces;
let Cc = Components.classes;
let Cu = Components.utils;

// Stolen from SpecialPowers, since at this point we don't know we're in a test.
let isMainProcess = function() {
  try {
    return Cc["@mozilla.org/xre/app-info;1"]
             .getService(Ci.nsIXULRuntime)
             .processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
  } catch (e) { }
  return true;
};

var fakeLibcUtils = {
  _props_: {},
  property_set: function(name, value) {
    dump("property_set('" + name + "', '" + value+ "' [" + (typeof value) + "]);\n");
    this._props_[name] = value;
  },
  property_get: function(name, defaultValue) {
    dump("property_get('" + name + "', '" + defaultValue+ "');\n");
    if (Object.keys(this._props_).indexOf(name) !== -1) {
      return this._props_[name];
    } else {
      return defaultValue;
    }
  }
};

var kUserValues;

function installUserValues(next) {
  var fakeValues = {
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

  OS.File.writeAtomic(kUserValues,
                      JSON.stringify(fakeValues)).then(() => {
    next();
  });
}

if (isMainProcess()) {
  Cu.import("resource://gre/modules/SettingsRequestManager.jsm");
  Cu.import("resource://gre/modules/osfile.jsm");
  Cu.import("resource://gre/modules/KillSwitchMain.jsm");

  kUserValues = OS.Path.join(OS.Constants.Path.profileDir, "killswitch.json");

  installUserValues(() => {
    KillSwitchMain._libcutils = fakeLibcUtils;
  });
}
