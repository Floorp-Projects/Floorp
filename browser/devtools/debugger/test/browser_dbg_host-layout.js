/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This if the debugger's layout is correctly modified when the toolbox's
 * host changes.
 */

let gDefaultHostType = Services.prefs.getCharPref("devtools.toolbox.host");

function test() {
  Task.spawn(function() {
    yield testHosts(["bottom", "side", "window"], ["horizontal", "vertical", "horizontal"]);
    yield testHosts(["side", "bottom", "side"], ["vertical", "horizontal", "vertical"]);
    yield testHosts(["bottom", "side", "bottom"], ["horizontal", "vertical", "horizontal"]);
    yield testHosts(["side", "window", "side"], ["vertical", "horizontal", "vertical"]);
    yield testHosts(["window", "side", "window"], ["horizontal", "vertical", "horizontal"]);
    finish();
  });
}

function testHosts(aHostTypes, aLayoutTypes) {
  let [firstHost, secondHost, thirdHost] = aHostTypes;
  let [firstLayout, secondLayout, thirdLayout] = aLayoutTypes;

  Services.prefs.setCharPref("devtools.toolbox.host", firstHost);

  return Task.spawn(function() {
    let [tab, debuggee, panel] = yield initDebugger("about:blank");
    yield testHost(tab, debuggee, panel, firstHost, firstLayout);
    yield switchAndTestHost(tab, debuggee, panel, secondHost, secondLayout);
    yield switchAndTestHost(tab, debuggee, panel, thirdHost, thirdLayout);
    yield teardown(panel);
  });
}

function switchAndTestHost(aTab, aDebuggee, aPanel, aHostType, aLayoutType) {
  let gToolbox = aPanel._toolbox;
  let gDebugger = aPanel.panelWin;

  return Task.spawn(function() {
    let layoutChanged = once(gDebugger, gDebugger.EVENTS.LAYOUT_CHANGED);
    let hostChanged = gToolbox.switchHost(aHostType);

    yield hostChanged;
    ok(true, "The toolbox's host has changed.");

    yield layoutChanged;
    ok(true, "The debugger's layout has changed.");

    yield testHost(aTab, aDebuggee, aPanel, aHostType, aLayoutType);
  });

  function once(aTarget, aEvent) {
    let deferred = promise.defer();
    aTarget.once(aEvent, deferred.resolve);
    return deferred.promise;
  }
}

function testHost(aTab, aDebuggee, aPanel, aHostType, aLayoutType) {
  let gDebugger = aPanel.panelWin;
  let gView = gDebugger.DebuggerView;

  is(gView._hostType, aHostType,
    "The default host type should've been set on the panel window (1).");
  is(gDebugger.gHostType, aHostType,
    "The default host type should've been set on the panel window (2).");

  is(gView._body.getAttribute("layout"), aLayoutType,
    "The default host type is present as an attribute on the panel's body.");

  if (aLayoutType == "horizontal") {
    is(gView._sourcesPane.parentNode.id, "debugger-widgets",
      "The sources pane's parent is correct for the horizontal layout.");
    is(gView._instrumentsPane.parentNode.id, "debugger-widgets",
      "The instruments pane's parent is correct for the horizontal layout.");
  } else {
    is(gView._sourcesPane.parentNode.id, "vertical-layout-panes-container",
      "The sources pane's parent is correct for the vertical layout.");
    is(gView._instrumentsPane.parentNode.id, "vertical-layout-panes-container",
      "The instruments pane's parent is correct for the vertical layout.");
  }

  let widgets = gDebugger.document.getElementById("debugger-widgets").childNodes;
  let panes = gDebugger.document.getElementById("vertical-layout-panes-container").childNodes;

  if (aLayoutType == "horizontal") {
    is(widgets.length, 7, // 2 panes, 1 editor, 3 splitters and a phantom box.
      "Found the correct number of debugger widgets.");
    is(panes.length, 1, // 1 lonely splitter in the phantom box.
      "Found the correct number of debugger panes.");
  } else {
    is(widgets.length, 5, // 1 editor, 3 splitters and a phantom box.
      "Found the correct number of debugger widgets.");
    is(panes.length, 3, // 2 panes and 1 splitter in the phantom box.
      "Found the correct number of debugger panes.");
  }
}

registerCleanupFunction(function() {
  Services.prefs.setCharPref("devtools.toolbox.host", gDefaultHostType);
  gDefaultHostType = null;
});
