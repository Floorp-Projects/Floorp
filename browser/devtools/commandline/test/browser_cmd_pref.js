/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the pref commands work

let prefBranch = Cc["@mozilla.org/preferences-service;1"]
                    .getService(Ci.nsIPrefService).getBranch(null)
                    .QueryInterface(Ci.nsIPrefBranch2);

let supportsString = Cc["@mozilla.org/supports-string;1"]
                      .createInstance(Ci.nsISupportsString)

let require = (Cu.import("resource://gre/modules/devtools/Require.jsm", {})).require;

let settings = require("gcli/settings");

const TEST_URI = "data:text/html;charset=utf-8,gcli-pref";

let tiltEnabledOrig;
let tabSizeOrig;
let remoteHostOrig;

let tests = {
  setup: function(options) {
    tiltEnabledOrig = prefBranch.getBoolPref("devtools.tilt.enabled");
    tabSizeOrig = prefBranch.getIntPref("devtools.editor.tabsize");
    remoteHostOrig = prefBranch.getComplexValue("devtools.debugger.remote-host",
                                                    Ci.nsISupportsString).data;

    info("originally: devtools.tilt.enabled = " + tiltEnabledOrig);
    info("originally: devtools.editor.tabsize = " + tabSizeOrig);
    info("originally: devtools.debugger.remote-host = " + remoteHostOrig);
  },

  shutdown: function(options) {
    prefBranch.setBoolPref("devtools.tilt.enabled", tiltEnabledOrig);
    prefBranch.setIntPref("devtools.editor.tabsize", tabSizeOrig);
    supportsString.data = remoteHostOrig;
    prefBranch.setComplexValue("devtools.debugger.remote-host",
                               Ci.nsISupportsString, supportsString);
  },

  testPrefStatus: function(options) {
    return helpers.audit(options, [
      {
        setup: 'pref',
        check: {
          input:  'pref',
          hints:      '',
          markup: 'IIII',
          status: 'ERROR'
        },
      },
      {
        setup: 'pref s',
        check: {
          input:  'pref s',
          hints:        'et',
          markup: 'IIIIVI',
          status: 'ERROR'
        },
      },
      {
        setup: 'pref sh',
        check: {
          input:  'pref sh',
          hints:         'ow',
          markup: 'IIIIVII',
          status: 'ERROR'
        },
      },
      {
        setup: 'pref show ',
        check: {
          input:  'pref show ',
          markup: 'VVVVVVVVVV',
          status: 'ERROR'
        },
      },
      {
        setup: 'pref show usetexttospeech',
        check: {
          input:  'pref show usetexttospeech',
          hints:                           ' -> accessibility.usetexttospeech',
          markup: 'VVVVVVVVVVIIIIIIIIIIIIIII',
          status: 'ERROR'
        },
      },
      {
        setup: 'pref show devtools.til',
        check: {
          input:  'pref show devtools.til',
          hints:                        't.enabled',
          markup: 'VVVVVVVVVVIIIIIIIIIIII',
          status: 'ERROR',
          tooltipState: 'true:importantFieldFlag',
          args: {
            setting: { value: undefined, status: 'INCOMPLETE' },
          }
        },
      },
      {
        setup: 'pref reset devtools.tilt.enabled',
        check: {
          input:  'pref reset devtools.tilt.enabled',
          hints:                                  '',
          markup: 'VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV',
          status: 'VALID'
        },
      },
      {
        setup: 'pref show devtools.tilt.enabled 4',
        check: {
          input:  'pref show devtools.tilt.enabled 4',
          hints:                                   '',
          markup: 'VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVE',
          status: 'ERROR'
        },
      },
      {
        setup: 'pref set devtools.tilt.enabled 4',
        check: {
          input:  'pref set devtools.tilt.enabled 4',
          hints:                                  '',
          markup: 'VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVE',
          status: 'ERROR',
          args: {
            setting: { arg: ' devtools.tilt.enabled' },
            value: { status: 'ERROR', message: 'Can\'t use \'4\'.' },
          }
        },
      },
      {
        setup: 'pref set devtools.editor.tabsize 4',
        check: {
          input:  'pref set devtools.editor.tabsize 4',
          hints:                                    '',
          markup: 'VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV',
          status: 'VALID',
          args: {
            setting: { arg: ' devtools.editor.tabsize' },
            value: { value: 4 },
          }
        },
      },
      {
        setup: 'pref list',
        check: {
          input:  'pref list',
          hints:           ' -> pref set',
          markup: 'IIIIVIIII',
          status: 'ERROR'
        },
      },
    ]);
  },

  testPrefSetEnable: function(options) {
    return helpers.audit(options, [
      {
        setup: 'pref set devtools.editor.tabsize 9',
        check: {
          args: {
            setting: { value: settings.getSetting("devtools.editor.tabsize") },
            value: { value: 9 }
          },
        },
        exec: {
          completed: true,
          output: [ /void your warranty/, /I promise/ ],
        },
        post: function() {
          is(prefBranch.getIntPref("devtools.editor.tabsize"),
                                   tabSizeOrig,
                                   "devtools.editor.tabsize is unchanged");
        }
      },
      {
        setup: 'pref set devtools.gcli.allowSet true',
        check: {
          args: {
            setting: { value: settings.getSetting("devtools.gcli.allowSet") },
            value: { value: true }
          },
        },
        exec: {
          completed: true,
          output: '',
        },
        post: function() {
          is(prefBranch.getBoolPref("devtools.gcli.allowSet"), true,
                                    "devtools.gcli.allowSet is true");
        }
      },
      {
        setup: 'pref set devtools.editor.tabsize 10',
        check: {
          args: {
            setting: { value: settings.getSetting("devtools.editor.tabsize") },
            value: { value: 10 }
          },
        },
        exec: {
          completed: true,
          output: '',
        },
        post: function() {
          is(prefBranch.getIntPref("devtools.editor.tabsize"), 10,
                                   "devtools.editor.tabsize is 10");
        }
      },
    ]);
  },

  testPrefBoolExec: function(options) {
    return helpers.audit(options, [
      {
        setup: 'pref show devtools.tilt.enabled',
        check: {
          args: {
            setting: { value: settings.getSetting("devtools.tilt.enabled") }
          },
        },
        exec: {
          completed: true,
          output: new RegExp("^devtools\.tilt\.enabled: " + tiltEnabledOrig + "$"),
        },
      },
      {
        setup: 'pref set devtools.tilt.enabled true',
        check: {
          args: {
            setting: { value: settings.getSetting("devtools.tilt.enabled") },
            value: { value: true }
          },
        },
        exec: {
          completed: true,
          output: '',
        },
        post: function() {
          is(prefBranch.getBoolPref("devtools.tilt.enabled"), true,
                                    "devtools.tilt.enabled is true");
        }
      },
      {
        setup: 'pref show devtools.tilt.enabled',
        check: {
          args: {
            setting: { value: settings.getSetting("devtools.tilt.enabled") }
          },
        },
        exec: {
          completed: true,
          output: new RegExp("^devtools\.tilt\.enabled: true$"),
        },
      },
      {
        setup: 'pref set devtools.tilt.enabled false',
        check: {
          args: {
            setting: { value: settings.getSetting("devtools.tilt.enabled") },
            value: { value: false }
          },
        },
        exec: {
          completed: true,
          output: '',
        },
      },
      {
        setup: 'pref show devtools.tilt.enabled',
        check: {
          args: {
            setting: { value: settings.getSetting("devtools.tilt.enabled") }
          },
        },
        exec: {
          completed: true,
          output: new RegExp("^devtools\.tilt\.enabled: false$"),
        },
        post: function() {
          is(prefBranch.getBoolPref("devtools.tilt.enabled"), false,
                                    "devtools.tilt.enabled is false");
        }
      },
    ]);
  },

  testPrefNumberExec: function(options) {
    return helpers.audit(options, [
      {
        setup: 'pref show devtools.editor.tabsize',
        check: {
          args: {
            setting: { value: settings.getSetting("devtools.editor.tabsize") }
          },
        },
        exec: {
          completed: true,
          output: new RegExp("^devtools\.editor\.tabsize: 10$"),
        },
      },
      {
        setup: 'pref set devtools.editor.tabsize 20',
        check: {
          args: {
            setting: { value: settings.getSetting("devtools.editor.tabsize") },
            value: { value: 20 }
          },
        },
        exec: {
          completed: true,
          output: '',
        },
      },
      {
        setup: 'pref show devtools.editor.tabsize',
        check: {
          args: {
            setting: { value: settings.getSetting("devtools.editor.tabsize") }
          },
        },
        exec: {
          completed: true,
          output: new RegExp("^devtools\.editor\.tabsize: 20$"),
        },
        post: function() {
          is(prefBranch.getIntPref("devtools.editor.tabsize"), 20,
                                   "devtools.editor.tabsize is 20");
        }
      },
      {
        setup: 'pref set devtools.editor.tabsize 1',
        check: {
          args: {
            setting: { value: settings.getSetting("devtools.editor.tabsize") },
            value: { value: 1 }
          },
        },
        exec: {
          completed: true,
          output: '',
        },
      },
      {
        setup: 'pref show devtools.editor.tabsize',
        check: {
          args: {
            setting: { value: settings.getSetting("devtools.editor.tabsize") }
          },
        },
        exec: {
          completed: true,
          output: new RegExp("^devtools\.editor\.tabsize: 1$"),
        },
        post: function() {
          is(prefBranch.getIntPref("devtools.editor.tabsize"), 1,
                                   "devtools.editor.tabsize is 1");
        }
      },
    ]);
  },

  testPrefStringExec: function(options) {
    return helpers.audit(options, [
      {
        setup: 'pref show devtools.debugger.remote-host',
        check: {
          args: {
            setting: { value: settings.getSetting("devtools.debugger.remote-host") }
          },
        },
        exec: {
          completed: true,
          output: new RegExp("^devtools\.debugger\.remote-host: " + remoteHostOrig + "$"),
        },
      },
      {
        setup: 'pref set devtools.debugger.remote-host e.com',
        check: {
          args: {
            setting: { value: settings.getSetting("devtools.debugger.remote-host") },
            value: { value: "e.com" }
          },
        },
        exec: {
          completed: true,
          output: '',
        },
      },
      {
        setup: 'pref show devtools.debugger.remote-host',
        check: {
          args: {
            setting: { value: settings.getSetting("devtools.debugger.remote-host") }
          },
        },
        exec: {
          completed: true,
          output: new RegExp("^devtools\.debugger\.remote-host: e.com$"),
        },
        post: function() {
          var ecom = prefBranch.getComplexValue("devtools.debugger.remote-host",
                                                Ci.nsISupportsString).data;
          is(ecom, "e.com", "devtools.debugger.remote-host is e.com");
        }
      },
      {
        setup: 'pref set devtools.debugger.remote-host moz.foo',
        check: {
          args: {
            setting: { value: settings.getSetting("devtools.debugger.remote-host") },
            value: { value: "moz.foo" }
          },
        },
        exec: {
          completed: true,
          output: '',
        },
      },
      {
        setup: 'pref show devtools.debugger.remote-host',
        check: {
          args: {
            setting: { value: settings.getSetting("devtools.debugger.remote-host") }
          },
        },
        exec: {
          completed: true,
          output: new RegExp("^devtools\.debugger\.remote-host: moz.foo$"),
        },
        post: function() {
          var mozfoo = prefBranch.getComplexValue("devtools.debugger.remote-host",
                                                  Ci.nsISupportsString).data;
          is(mozfoo, "moz.foo", "devtools.debugger.remote-host is moz.foo");
        }
      },
    ]);
  },

  testPrefSetDisable: function(options) {
    return helpers.audit(options, [
      {
        setup: 'pref set devtools.editor.tabsize 32',
        check: {
          args: {
            setting: { value: settings.getSetting("devtools.editor.tabsize") },
            value: { value: 32 }
          },
        },
        exec: {
          completed: true,
          output: '',
        },
        post: function() {
          is(prefBranch.getIntPref("devtools.editor.tabsize"), 32,
                                   "devtools.editor.tabsize is 32");
        }
      },
      {
        setup: 'pref reset devtools.gcli.allowSet',
        check: {
          args: {
            setting: { value: settings.getSetting("devtools.gcli.allowSet") }
          },
        },
        exec: {
          completed: true,
          output: '',
        },
        post: function() {
          is(prefBranch.getBoolPref("devtools.gcli.allowSet"), false,
                                    "devtools.gcli.allowSet is false");
        }
      },
      {
        setup: 'pref set devtools.editor.tabsize 33',
        check: {
          args: {
            setting: { value: settings.getSetting("devtools.editor.tabsize") },
            value: { value: 33 }
          },
        },
        exec: {
          completed: true,
          output: [ /void your warranty/, /I promise/ ],
        },
        post: function() {
          is(prefBranch.getIntPref("devtools.editor.tabsize"), 32,
                                   "devtools.editor.tabsize is still 32");
        }
      },
    ]);
  },
};

function test() {
  helpers.addTabWithToolbar(TEST_URI, function(options) {
    return helpers.runTests(options, tests);
  }).then(finish);
}
