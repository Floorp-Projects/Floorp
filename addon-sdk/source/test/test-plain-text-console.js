/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const prefs = require("sdk/preferences/service");
const { id, name } = require("sdk/self");
const { Cc, Cu, Ci } = require("chrome");
const { loadSubScript } = Cc['@mozilla.org/moz/jssubscript-loader;1'].
                     getService(Ci.mozIJSSubScriptLoader);

const ADDON_LOG_LEVEL_PREF = "extensions." + id + ".sdk.console.logLevel";
const SDK_LOG_LEVEL_PREF = "extensions.sdk.console.logLevel";

const HAS_ORIGINAL_ADDON_LOG_LEVEL = prefs.has(ADDON_LOG_LEVEL_PREF);
const ORIGINAL_ADDON_LOG_LEVEL = prefs.get(ADDON_LOG_LEVEL_PREF);
const HAS_ORIGINAL_SDK_LOG_LEVEL = prefs.has(SDK_LOG_LEVEL_PREF);
const ORIGINAL_SDK_LOG_LEVEL = prefs.get(SDK_LOG_LEVEL_PREF);

exports.testPlainTextConsole = function(assert) {
  let prints = [];
  function print(message) {
    prints.push(message);
  }
  function lastPrint() {
    let last = prints.slice(-1)[0];
    prints = [];
    return last;
  }

  prefs.set(SDK_LOG_LEVEL_PREF, "all");
  prefs.reset(ADDON_LOG_LEVEL_PREF);

  let Console = require("sdk/console/plain-text").PlainTextConsole;
  let con = new Console(print);

  assert.ok("PlainTextConsole instantiates");

  con.log('testing', 1, [2, 3, 4]);
  assert.equal(lastPrint(), "console.log: " + name + ": testing 1 Array [2,3,4]\n",
                   "PlainTextConsole.log() must work.");

  con.info('testing', 1, [2, 3, 4]);
  assert.equal(lastPrint(), "console.info: " + name + ": testing 1 Array [2,3,4]\n",
                   "PlainTextConsole.info() must work.");

  con.warn('testing', 1, [2, 3, 4]);
  assert.equal(lastPrint(), "console.warn: " + name + ": testing 1 Array [2,3,4]\n",
                   "PlainTextConsole.warn() must work.");

  con.error('testing', 1, [2, 3, 4]);
  assert.equal(prints[0], "console.error: " + name + ": \n",
                   "PlainTextConsole.error() must work.");
  assert.equal(prints[1], "  testing\n")
  assert.equal(prints[2], "  1\n")
  assert.equal(prints[3], "Array\n    - 0 = 2\n    - 1 = 3\n    - 2 = 4\n    - length = 3\n");
  prints = [];

  con.debug('testing', 1, [2, 3, 4]);
  assert.equal(prints[0], "console.debug: " + name + ": \n",
                   "PlainTextConsole.debug() must work.");
  assert.equal(prints[1], "  testing\n")
  assert.equal(prints[2], "  1\n")
  assert.equal(prints[3], "Array\n    - 0 = 2\n    - 1 = 3\n    - 2 = 4\n    - length = 3\n");
  prints = [];

  con.log('testing', undefined);
  assert.equal(lastPrint(), "console.log: " + name + ": testing undefined\n",
                   "PlainTextConsole.log() must stringify undefined.");

  con.log('testing', null);
  assert.equal(lastPrint(), "console.log: " + name + ": testing null\n",
                   "PlainTextConsole.log() must stringify null.");

  // TODO: Fix console.jsm to detect custom toString.
  con.log("testing", { toString: function() "obj.toString()" });
  assert.equal(lastPrint(), "console.log: " + name + ": testing {}\n",
                   "PlainTextConsole.log() doesn't printify custom toString.");

  con.log("testing", { toString: function() { throw "fail!"; } });
  assert.equal(lastPrint(), "console.log: " + name + ": testing {}\n",
                   "PlainTextConsole.log() must stringify custom bad toString.");


  con.exception(new Error("blah"));


  assert.equal(prints[0], "console.error: " + name + ": \n");
  let tbLines = prints[1].split("\n");
  assert.equal(tbLines[0], "  Message: Error: blah");
  assert.equal(tbLines[1], "  Stack:");
  assert.ok(prints[1].indexOf(module.uri + ":84") !== -1);
  prints = []

  try {
    loadSubScript("invalid-url", {});
    assert.fail("successed in calling loadSubScript with invalid-url");
  }
  catch(e) {
    con.exception(e);
  }
  assert.equal(prints[0], "console.error: " + name + ": \n");
  assert.equal(prints[1], "  Error creating URI (invalid URL scheme?)\n");
  prints = [];

  con.trace();
  let tbLines = prints[0].split("\n");
  assert.equal(tbLines[0], "console.trace: " + name + ": ");
  assert.ok(tbLines[1].indexOf("_ain-text-console.js 105") == 0);
  prints = [];

  // Whether or not console methods should print at the various log levels,
  // structured as a hash of levels, each of which contains a hash of methods,
  // each of whose value is whether or not it should print, i.e.:
  // { [level]: { [method]: [prints?], ... }, ... }.
  let levels = {
    all:   { debug: true,  log: true,  info: true,  warn: true,  error: true  },
    debug: { debug: true,  log: true,  info: true,  warn: true,  error: true  },
    info:  { debug: false, log: true,  info: true,  warn: true,  error: true  },
    warn:  { debug: false, log: false, info: false, warn: true,  error: true  },
    error: { debug: false, log: false, info: false, warn: false, error: true  },
    off:   { debug: false, log: false, info: false, warn: false, error: false },
  };

  // The messages we use to test the various methods, as a hash of methods.
  let messages = {
    debug: "console.debug: " + name + ": \n  \n",
    log: "console.log: " + name + ": \n",
    info: "console.info: " + name + ": \n",
    warn: "console.warn: " + name + ": \n",
    error: "console.error: " + name + ": \n  \n",
  };

  for (let level in levels) {
    let methods = levels[level];
    for (let method in methods) {
      // We have to reset the log level pref each time we run the test
      // because the test runner relies on the console to print test output,
      // and test results would not get printed to the console for some
      // values of the pref.
      prefs.set(SDK_LOG_LEVEL_PREF, level);
      con[method]("");
      prefs.set(SDK_LOG_LEVEL_PREF, "all");
      assert.equal(prints.join(""),
                       (methods[method] ? messages[method] : ""),
                       "at log level '" + level + "', " + method + "() " +
                       (methods[method] ? "prints" : "doesn't print"));
      prints = [];
    }
  }

  prefs.set(SDK_LOG_LEVEL_PREF, "off");
  prefs.set(ADDON_LOG_LEVEL_PREF, "all");
  con.debug("");
  assert.equal(prints.join(""), messages["debug"],
                   "addon log level 'all' overrides SDK log level 'off'");
  prints = [];

  prefs.set(SDK_LOG_LEVEL_PREF, "all");
  prefs.set(ADDON_LOG_LEVEL_PREF, "off");
  con.error("");
  prefs.reset(ADDON_LOG_LEVEL_PREF);
  assert.equal(lastPrint(), null,
                   "addon log level 'off' overrides SDK log level 'all'");

  restorePrefs();
};

exports.testPlainTextConsoleBoundMethods = function(assert) {
  let prints = [];
  function print(message) {
    prints.push(message);
  }
  function lastPrint() {
    let last = prints.slice(-1)[0];
    prints = [];
    return last;
  }

  prefs.set(SDK_LOG_LEVEL_PREF, "all");
  prefs.reset(ADDON_LOG_LEVEL_PREF);

  let Console = require("sdk/console/plain-text").PlainTextConsole;
  let { log, info, warn, error, debug, exception, trace } = new Console(print);

  assert.ok("PlainTextConsole instantiates");

  log('testing', 1, [2, 3, 4]);
  assert.equal(lastPrint(), "console.log: " + name + ": testing 1 Array [2,3,4]\n",
                   "PlainTextConsole.log() must work.");

  info('testing', 1, [2, 3, 4]);
  assert.equal(lastPrint(), "console.info: " + name + ": testing 1 Array [2,3,4]\n",
                   "PlainTextConsole.info() must work.");

  warn('testing', 1, [2, 3, 4]);
  assert.equal(lastPrint(), "console.warn: " + name + ": testing 1 Array [2,3,4]\n",
                   "PlainTextConsole.warn() must work.");

  error('testing', 1, [2, 3, 4]);
  assert.equal(prints[0], "console.error: " + name + ": \n",
                   "PlainTextConsole.error() must work.");
  assert.equal(prints[1], "  testing\n")
  assert.equal(prints[2], "  1\n")
  assert.equal(prints[3], "Array\n    - 0 = 2\n    - 1 = 3\n    - 2 = 4\n    - length = 3\n");
  prints = [];

  debug('testing', 1, [2, 3, 4]);
  assert.equal(prints[0], "console.debug: " + name + ": \n",
                   "PlainTextConsole.debug() must work.");
  assert.equal(prints[1], "  testing\n")
  assert.equal(prints[2], "  1\n")
  assert.equal(prints[3], "Array\n    - 0 = 2\n    - 1 = 3\n    - 2 = 4\n    - length = 3\n");
  prints = [];

  exception(new Error("blah"));

  assert.equal(prints[0], "console.error: " + name + ": \n");
  let tbLines = prints[1].split("\n");
  assert.equal(tbLines[0], "  Message: Error: blah");
  assert.equal(tbLines[1], "  Stack:");
  assert.ok(prints[1].indexOf(module.uri + ":215") !== -1);
  prints = []

  trace();
  let tbLines = prints[0].split("\n");
  assert.equal(tbLines[0], "console.trace: " + name + ": ");
  assert.ok(tbLines[1].indexOf("_ain-text-console.js 224") === 0);
  prints = [];

  restorePrefs();
};

exports.testConsoleInnerID = function(assert) {
  let Console = require("sdk/console/plain-text").PlainTextConsole;
  let { log, info, warn, error, debug, exception, trace } = new Console(function() {}, "test ID");

  let messages = [];
  function onMessage({ subject }) {
    let message = subject.wrappedJSObject;
    messages.push({ msg: message.arguments[0], type: message.level, innerID: message.innerID });
  }

  const system = require("sdk/system/events");
  system.on("console-api-log-event", onMessage);

  log("Test log");
  warn("Test warning");
  error("Test error");

  assert.equal(messages.length, 3, "Should see 3 log events");
  assert.deepEqual(messages[0], { msg: "Test log", type: "log", innerID: "test ID" }, "Should see the right event");
  assert.deepEqual(messages[1], { msg: "Test warning", type: "warn", innerID: "test ID" }, "Should see the right event");
  assert.deepEqual(messages[2], { msg: "Test error", type: "error", innerID: "test ID" }, "Should see the right event");

  system.off("console-api-log-event", onMessage);
};

function restorePrefs() {
  if (HAS_ORIGINAL_ADDON_LOG_LEVEL)
    prefs.set(ADDON_LOG_LEVEL_PREF, ORIGINAL_ADDON_LOG_LEVEL);
  else
    prefs.reset(ADDON_LOG_LEVEL_PREF);

  if (HAS_ORIGINAL_SDK_LOG_LEVEL)
    prefs.set(SDK_LOG_LEVEL_PREF, ORIGINAL_SDK_LOG_LEVEL);
  else
    prefs.reset(SDK_LOG_LEVEL_PREF);
}

require("test").run(exports);
