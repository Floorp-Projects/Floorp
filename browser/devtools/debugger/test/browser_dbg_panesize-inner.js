/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function test() {
  var tab1 = addTab(TAB1_URL, function() {
    gBrowser.selectedTab = tab1;
    let target1 = TargetFactory.forTab(tab1);

    ok(!gDevTools.getToolbox(target1),
      "Shouldn't have a debugger panel for this tab yet.");

    gDevTools.showToolbox(target1, "jsdebugger").then(function(toolbox) {
      let dbg = toolbox.getCurrentPanel();
      ok(dbg, "We should have a debugger panel.");

      let preferredSw = Services.prefs.getIntPref("devtools.debugger.ui.panes-sources-width");
      let preferredIw = Services.prefs.getIntPref("devtools.debugger.ui.panes-instruments-width");
      let someWidth1, someWidth2;

      do {
        someWidth1 = parseInt(Math.random() * 200) + 100;
        someWidth2 = parseInt(Math.random() * 200) + 100;
      } while (someWidth1 == preferredSw ||
               someWidth2 == preferredIw)

      let someWidth1 = parseInt(Math.random() * 200) + 100;
      let someWidth2 = parseInt(Math.random() * 200) + 100;

      info("Preferred sources width: " + preferredSw);
      info("Preferred instruments width: " + preferredIw);
      info("Generated sources width: " + someWidth1);
      info("Generated instruments width: " + someWidth2);

      let content = dbg.panelWin;
      let sources;
      let instruments;

      wait_for_connect_and_resume(function() {
        ok(content.Prefs.sourcesWidth,
          "The debugger preferences should have a saved sourcesWidth value.");
        ok(content.Prefs.instrumentsWidth,
          "The debugger preferences should have a saved instrumentsWidth value.");

        sources = content.document.getElementById("sources-pane");
        instruments = content.document.getElementById("instruments-pane");

        is(content.Prefs.sourcesWidth, sources.getAttribute("width"),
          "The sources pane width should be the same as the preferred value.");
        is(content.Prefs.instrumentsWidth, instruments.getAttribute("width"),
          "The instruments pane width should be the same as the preferred value.");

        sources.setAttribute("width", someWidth1);
        instruments.setAttribute("width", someWidth2);

        removeTab(tab1);
      }, tab1);

      window.addEventListener("Debugger:Shutdown", function dbgShutdown() {
        window.removeEventListener("Debugger:Shutdown", dbgShutdown, true);

        is(content.Prefs.sourcesWidth, sources.getAttribute("width"),
          "The sources pane width should have been saved by now.");
        is(content.Prefs.instrumentsWidth, instruments.getAttribute("width"),
          "The instruments pane width should have been saved by now.");

        // Cleanup after ourselves!
        Services.prefs.setIntPref("devtools.debugger.ui.panes-sources-width", preferredSw);
        Services.prefs.setIntPref("devtools.debugger.ui.panes-instruments-width", preferredIw);

        finish();
      }, true);
    });
  });
}
