/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// XXX Remove this when the file is migrated to the new frontend.
/* eslint-disable no-undef */

// Check that inspecting a closure in the variables view sidebar works when
// execution is paused.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/mochitest/test-closures.html";

var gWebConsole, gJSTerm, gVariablesView;

// Force the old debugger UI since it's directly used (see Bug 1301705)
Services.prefs.setBoolPref("devtools.debugger.new-debugger-frontend", false);
registerCleanupFunction(function() {
  Services.prefs.clearUserPref("devtools.debugger.new-debugger-frontend");
});

function test() {
  registerCleanupFunction(() => {
    gWebConsole = gJSTerm = gVariablesView = null;
  });

  function fetchScopes(hud, toolbox, panelWin, deferred) {
    panelWin.once(panelWin.EVENTS.FETCHED_SCOPES, () => {
      ok(true, "Scopes were fetched");
      toolbox.selectTool("webconsole").then(() => consoleOpened(hud));
      deferred.resolve();
    });
  }

  loadTab(TEST_URI).then(() => {
    openConsole().then((hud) => {
      openDebugger().then(({ toolbox, panelWin }) => {
        const deferred = defer();
        fetchScopes(hud, toolbox, panelWin, deferred);

        // eslint-disable-next-line
        ContentTask.spawn(gBrowser.selectedBrowser, {}, () => {
          const button = content.document.querySelector("button");
          ok(button, "button element found");
          button.click();
        });

        return deferred.promise;
      });
    });
  });
}

function consoleOpened(hud) {
  gWebConsole = hud;
  gJSTerm = hud.jsterm;
  gJSTerm.execute("window.george.getName");

  waitForMessages({
    webconsole: gWebConsole,
    messages: [{
      text: "getName()",
      category: CATEGORY_OUTPUT,
      objects: true,
    }],
  }).then(onExecuteGetName);
}

function onExecuteGetName(results) {
  const clickable = results[0].clickableElements[0];
  ok(clickable, "clickable object found");

  gJSTerm.once("variablesview-fetched", onGetNameFetch);
  const contextMenu =
      gWebConsole.iframeWindow.document.getElementById("output-contextmenu");
  waitForContextMenu(contextMenu, clickable, () => {
    const openInVarView = contextMenu.querySelector("#menu_openInVarView");
    ok(openInVarView.disabled === false,
       "the \"Open In Variables View\" context menu item should be clickable");
    // EventUtils.synthesizeMouseAtCenter seems to fail here in Mac OSX
    openInVarView.click();
  });
}

function onGetNameFetch(view) {
  gVariablesView = view._variablesView;
  ok(gVariablesView, "variables view object");

  findVariableViewProperties(view, [
    { name: /_pfactory/, value: "" },
  ], { webconsole: gWebConsole }).then(onExpandClosure);
}

function onExpandClosure(results) {
  const prop = results[0].matchedProp;
  ok(prop, "matched the name property in the variables view");

  gVariablesView.window.focus();
  gJSTerm.once("sidebar-closed", finishTest);
  EventUtils.synthesizeKey("VK_ESCAPE", {}, gVariablesView.window);
}
