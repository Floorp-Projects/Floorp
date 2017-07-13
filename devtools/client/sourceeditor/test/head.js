/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from ../../framework/test/shared-head.js */
/* exported promiseWaitForFocus, setup, ch, teardown, loadHelperScript,
            limit, ch, read, codemirrorSetStatus */

"use strict";

// shared-head.js handles imports, constants, and utility functions
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/framework/test/shared-head.js",
  this);

const { NetUtil } = require("resource://gre/modules/NetUtil.jsm");
const Editor = require("devtools/client/sourceeditor/editor");
const {getClientCssProperties} = require("devtools/shared/fronts/css-properties");

flags.testing = true;
SimpleTest.registerCleanupFunction(() => {
  flags.testing = false;
});

function promiseWaitForFocus() {
  return new Promise(resolve =>
    waitForFocus(resolve));
}

function setup(cb, additionalOpts = {}) {
  cb = cb || function () {};
  return new Promise(resolve => {
    const opt = "chrome,titlebar,toolbar,centerscreen,resizable,dialog=no";

    let win = Services.ww.openWindow(null, CHROME_URL_ROOT + "head.xul", "_blank", opt,
                                     null);
    let opts = {
      value: "Hello.",
      lineNumbers: true,
      foldGutter: true,
      gutters: ["CodeMirror-linenumbers", "breakpoints", "CodeMirror-foldgutter"],
      cssProperties: getClientCssProperties()
    };

    for (let o in additionalOpts) {
      opts[o] = additionalOpts[o];
    }

    win.addEventListener("load", function () {
      waitForFocus(function () {
        let box = win.document.querySelector("box");
        let editor = new Editor(opts);

        editor.appendTo(box)
          .then(() => {
            resolve({
              ed: editor,
              win: win,
              edWin: editor.container.contentWindow.wrappedJSObject
            });
            cb(editor, win);
          }, err => ok(false, err.message));
      }, win);
    }, {once: true});
  });
}

function ch(exp, act, label) {
  is(exp.line, act.line, label + " (line)");
  is(exp.ch, act.ch, label + " (ch)");
}

function teardown(ed, win) {
  ed.destroy();
  win.close();

  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }
  finish();
}

/**
 * Some tests may need to import one or more of the test helper scripts.
 * A test helper script is simply a js file that contains common test code that
 * is either not common-enough to be in head.js, or that is located in a
 * separate directory.
 * The script will be loaded synchronously and in the test's scope.
 * @param {String} filePath The file path, relative to the current directory.
 *                 Examples:
 *                 - "helper_attributes_test_runner.js"
 *                 - "../../../commandline/test/helpers.js"
 */
function loadHelperScript(filePath) {
  let testDir = gTestPath.substr(0, gTestPath.lastIndexOf("/"));
  Services.scriptloader.loadSubScript(testDir + "/" + filePath, this);
}

/**
 * This method returns the portion of the input string `source` up to the
 * [line, ch] location.
 */
function limit(source, [line, char]) {
  line++;
  let list = source.split("\n");
  if (list.length < line) {
    return source;
  }
  if (line == 1) {
    return list[0].slice(0, char);
  }
  return [...list.slice(0, line - 1), list[line - 1].slice(0, char)].join("\n");
}

function read(url) {
  let scriptableStream = Cc["@mozilla.org/scriptableinputstream;1"]
    .getService(Ci.nsIScriptableInputStream);

  let channel = NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true
  });
  let input = channel.open2();
  scriptableStream.init(input);

  let data = "";
  while (input.available()) {
    data = data.concat(scriptableStream.read(input.available()));
  }
  scriptableStream.close();
  input.close();

  return data;
}

/**
 * This function is called by the CodeMirror test runner to report status
 * messages from the CM tests.
 * @see codemirror.html
 */
function codemirrorSetStatus(statusMsg, type, customMsg) {
  switch (type) {
    case "expected":
    case "ok":
      ok(1, statusMsg);
      break;
    case "error":
    case "fail":
      ok(0, statusMsg);
      break;
    default:
      info(statusMsg);
      break;
  }

  if (customMsg && typeof customMsg == "string" && customMsg != statusMsg) {
    info(customMsg);
  }
}
