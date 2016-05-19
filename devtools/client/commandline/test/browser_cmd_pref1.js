/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the pref commands work

var prefBranch = Cc["@mozilla.org/preferences-service;1"]
                    .getService(Ci.nsIPrefService).getBranch(null)
                    .QueryInterface(Ci.nsIPrefBranch2);

const TEST_URI = "data:text/html;charset=utf-8,gcli-pref1";

function test() {
  return Task.spawn(spawnTest).then(finish, helpers.handleError);
}

function* spawnTest() {
  let options = yield helpers.openTab(TEST_URI);
  yield helpers.openToolbar(options);

  let netmonEnabledOrig = prefBranch.getBoolPref("devtools.netmonitor.enabled");
  info("originally: devtools.netmonitor.enabled = " + netmonEnabledOrig);

  yield helpers.audit(options, [
    {
      setup: "pref",
      check: {
        input:  "pref",
        hints:      " reset",
        markup: "IIII",
        status: "ERROR"
      },
    },
    {
      setup: "pref s",
      check: {
        input:  "pref s",
        hints:        "et",
        markup: "IIIIVI",
        status: "ERROR"
      },
    },
    {
      setup: "pref sh",
      check: {
        input:  "pref sh",
        hints:         "ow",
        markup: "IIIIVII",
        status: "ERROR"
      },
    },
    {
      setup: "pref show ",
      check: {
        input:  "pref show ",
        markup: "VVVVVVVVVV",
        status: "ERROR"
      },
    },
    {
      setup: "pref show usetexttospeech",
      check: {
        input:  "pref show usetexttospeech",
        hints:                           " -> accessibility.usetexttospeech",
        markup: "VVVVVVVVVVIIIIIIIIIIIIIII",
        status: "ERROR"
      },
    },
    {
      setup: "pref show devtools.netmoni",
      check: {
        input:  "pref show devtools.netmoni",
        hints:                        "tor.enabled",
        markup: "VVVVVVVVVVIIIIIIIIIIIIIIII",
        status: "ERROR",
        tooltipState: "true:importantFieldFlag",
        args: {
          setting: { value: undefined, status: "INCOMPLETE" },
        }
      },
    },
    {
      setup: "pref reset devtools.netmonitor.enabled",
      check: {
        input:  "pref reset devtools.netmonitor.enabled",
        hints:                                  "",
        markup: "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV",
        status: "VALID"
      },
    },
    {
      setup: "pref show devtools.netmonitor.enabled 4",
      check: {
        input:  "pref show devtools.netmonitor.enabled 4",
        hints:                                   "",
        markup: "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVE",
        status: "ERROR"
      },
    },
    {
      setup: "pref set devtools.netmonitor.enabled 4",
      check: {
        input:  "pref set devtools.netmonitor.enabled 4",
        hints:                                  "",
        markup: "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVE",
        status: "ERROR",
        args: {
          setting: { arg: " devtools.netmonitor.enabled" },
          value: { status: "ERROR", message: "Can\u2019t use \u20184\u2019." },
        }
      },
    },
    {
      setup: "pref set devtools.editor.tabsize 4",
      check: {
        input:  "pref set devtools.editor.tabsize 4",
        hints:                                    "",
        markup: "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV",
        status: "VALID",
        args: {
          setting: { arg: " devtools.editor.tabsize" },
          value: { value: 4 },
        }
      },
    },
    {
      setup: "pref list",
      check: {
        input:  "pref list",
        hints:           " -> pref set",
        markup: "IIIIVIIII",
        status: "ERROR"
      },
    },
    {
      setup: "pref show devtools.netmonitor.enabled",
      check: {
        args: {
          setting: {
            value: options.requisition.system.settings.get("devtools.netmonitor.enabled")
          }
        },
      },
      exec: {
        output: "devtools.netmonitor.enabled: " + netmonEnabledOrig,
      },
      post: function () {
        prefBranch.setBoolPref("devtools.netmonitor.enabled", netmonEnabledOrig);
      }
    },
  ]);

  yield helpers.closeToolbar(options);
  yield helpers.closeTab(options);
}
