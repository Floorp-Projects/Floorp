/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function test() {
  var tab1 = addTab(TAB1_URL, function() {
    gBrowser.selectedTab = tab1;

    ok(!DebuggerUI.getDebugger(),
      "Shouldn't have a debugger pane for this tab yet.");

    let pane = DebuggerUI.toggleDebugger();
    let someWidth1 = parseInt(Math.random() * 200) + 100;
    let someWidth2 = parseInt(Math.random() * 200) + 100;

    ok(pane, "toggleDebugger() should return a pane.");

    is(DebuggerUI.getDebugger(), pane,
      "getDebugger() should return the same pane as toggleDebugger().");

    let frame = pane._frame;
    let content = pane.contentWindow;
    let stackframes;
    let variables;

    frame.addEventListener("Debugger:Loaded", function dbgLoaded() {
      frame.removeEventListener("Debugger:Loaded", dbgLoaded, true);

      ok(content.Prefs.stackframesWidth,
        "The debugger preferences should have a saved stackframesWidth value.");
      ok(content.Prefs.variablesWidth,
        "The debugger preferences should have a saved variablesWidth value.");

      stackframes = content.document.getElementById("stackframes+breakpoints");
      variables = content.document.getElementById("variables");

      is(content.Prefs.stackframesWidth, stackframes.getAttribute("width"),
        "The stackframes pane width should be the same as the preferred value.");
      is(content.Prefs.variablesWidth, variables.getAttribute("width"),
        "The variables pane width should be the same as the preferred value.");

      stackframes.setAttribute("width", someWidth1);
      variables.setAttribute("width", someWidth2);

      removeTab(tab1);

    }, true);

    frame.addEventListener("Debugger:Unloaded", function dbgUnloaded() {
      frame.removeEventListener("Debugger:Unloaded", dbgUnloaded, true);

      is(content.Prefs.stackframesWidth, stackframes.getAttribute("width"),
        "The stackframes pane width should have been saved by now.");
      is(content.Prefs.variablesWidth, variables.getAttribute("width"),
        "The variables pane width should have been saved by now.");

      finish();

    }, true);
  });
}
