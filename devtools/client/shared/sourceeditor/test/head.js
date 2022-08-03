/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from ../../../shared/test/shared-head.js */
/* exported promiseWaitForFocus, setup, ch, teardown, loadHelperScript,
            limit, ch, read, codemirrorSetStatus */

"use strict";

// shared-head.js handles imports, constants, and utility functions
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-head.js",
  this
);

const Editor = require("devtools/client/shared/sourceeditor/editor");
const {
  getClientCssProperties,
} = require("devtools/client/fronts/css-properties");

function promiseWaitForFocus(el) {
  return new Promise(resolve => waitForFocus(resolve, el));
}

async function setup(additionalOpts = {}) {
  try {
    const opt = "chrome,titlebar,toolbar,centerscreen,resizable,dialog=no";
    const win = Services.ww.openWindow(
      null,
      CHROME_URL_ROOT + "head.xhtml",
      "_blank",
      opt,
      null
    );
    const opts = {
      value: "Hello.",
      lineNumbers: true,
      foldGutter: true,
      gutters: [
        "CodeMirror-linenumbers",
        "breakpoints",
        "CodeMirror-foldgutter",
      ],
      cssProperties: getClientCssProperties(),
      ...additionalOpts,
    };

    await once(win, "load");
    await promiseWaitForFocus(win);

    const box = win.document.querySelector("box");
    const editor = new Editor(opts);
    await editor.appendTo(box);

    return {
      ed: editor,
      win,
      edWin: editor.container.contentWindow.wrappedJSObject,
    };
  } catch (o_O) {
    ok(false, o_O.message);
    return null;
  }
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
 */
function loadHelperScript(filePath) {
  const testDir = gTestPath.substr(0, gTestPath.lastIndexOf("/"));
  Services.scriptloader.loadSubScript(testDir + "/" + filePath, this);
}

/**
 * This method returns the portion of the input string `source` up to the
 * [line, ch] location.
 */
function limit(source, [line, char]) {
  line++;
  const list = source.split("\n");
  if (list.length < line) {
    return source;
  }
  if (line == 1) {
    return list[0].slice(0, char);
  }
  return [...list.slice(0, line - 1), list[line - 1].slice(0, char)].join("\n");
}

function read(url) {
  const scriptableStream = Cc[
    "@mozilla.org/scriptableinputstream;1"
  ].getService(Ci.nsIScriptableInputStream);

  const channel = NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
  });
  const input = channel.open();
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

async function runCodeMirrorTest(uri) {
  const actorURI =
    "chrome://mochitests/content/browser/devtools/client/shared/sourceeditor/test/CodeMirrorTestActors.jsm";

  const { CodeMirrorTestParent } = ChromeUtils.import(actorURI);

  ChromeUtils.registerWindowActor("CodeMirrorTest", {
    parent: {
      moduleURI: actorURI,
    },
    child: {
      moduleURI: actorURI,
      events: {
        DOMWindowCreated: {},
      },
    },
  });

  const donePromise = new Promise(resolve => {
    CodeMirrorTestParent.setCallback((name, data) => {
      switch (name) {
        case "setStatus":
          const { statusMsg, type, customMsg } = data;
          codemirrorSetStatus(statusMsg, type, customMsg);
          break;
        case "done":
          resolve(!data.failed);
          break;
      }
    });
  });

  await addTab(uri);
  const result = await donePromise;
  ok(result, "CodeMirror tests all passed");
  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }

  ChromeUtils.unregisterWindowActor("CodeMirrorTest");
}
