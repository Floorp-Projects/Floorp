/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the pref commands work

var prefBranch = Cc["@mozilla.org/preferences-service;1"]
                    .getService(Ci.nsIPrefService).getBranch(null)
                    .QueryInterface(Ci.nsIPrefBranch2);

const TEST_URI = "data:text/html;charset=utf-8,gcli-pref2";

function test() {
  return Task.spawn(spawnTest).then(finish, helpers.handleError);
}

function* spawnTest() {
  let options = yield helpers.openTab(TEST_URI);
  yield helpers.openToolbar(options);

  let tabSizeOrig = prefBranch.getIntPref("devtools.editor.tabsize");
  info("originally: devtools.editor.tabsize = " + tabSizeOrig);

  yield helpers.audit(options, [
    {
      setup: "pref show devtools.editor.tabsize",
      check: {
        args: {
          setting: {
            value: options.requisition.system.settings.get("devtools.editor.tabsize")
          }
        },
      },
      exec: {
        output: "devtools.editor.tabsize: " + tabSizeOrig,
      },
    },
    {
      setup: "pref set devtools.editor.tabsize 20",
      check: {
        args: {
          setting: {
            value: options.requisition.system.settings.get("devtools.editor.tabsize")
          },
          value: { value: 20 }
        },
      },
      exec: {
        output: "",
      },
      post: function () {
        is(prefBranch.getIntPref("devtools.editor.tabsize"), 20,
                                 "devtools.editor.tabsize is 20");
      }
    },
    {
      setup: "pref show devtools.editor.tabsize",
      check: {
        args: {
          setting: {
            value: options.requisition.system.settings.get("devtools.editor.tabsize")
          }
        },
      },
      exec: {
        output: "devtools.editor.tabsize: 20",
      }
    },
    {
      setup: "pref set devtools.editor.tabsize 1",
      check: {
        args: {
          setting: {
            value: options.requisition.system.settings.get("devtools.editor.tabsize")
          },
          value: { value: 1 }
        },
      },
      exec: {
        output: "",
      },
    },
    {
      setup: "pref show devtools.editor.tabsize",
      check: {
        args: {
          setting: {
            value: options.requisition.system.settings.get("devtools.editor.tabsize")
          }
        },
      },
      exec: {
        output: "devtools.editor.tabsize: 1",
      },
      post: function () {
        is(prefBranch.getIntPref("devtools.editor.tabsize"), 1,
                                 "devtools.editor.tabsize is 1");
      }
    },
  ]);

  prefBranch.setIntPref("devtools.editor.tabsize", tabSizeOrig);

  yield helpers.closeToolbar(options);
  yield helpers.closeTab(options);
}
