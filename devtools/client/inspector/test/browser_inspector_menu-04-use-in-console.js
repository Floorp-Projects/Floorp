/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Tests "Use in Console" menu item

const TEST_URL = URL_ROOT + "doc_inspector_menu.html";

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.toolbox.splitconsoleEnabled");
});

add_task(function* () {
  let { inspector, toolbox } = yield openInspectorForURL(TEST_URL);

  yield testUseInConsole();

  function* testUseInConsole() {
    info("Testing 'Use in Console' menu item.");
    let useInConsoleNode = inspector.panelDoc.getElementById(
      "node-menu-useinconsole");

    yield selectNode("#console-var", inspector);
    dispatchCommandEvent(useInConsoleNode);
    yield inspector.once("console-var-ready");

    let hud = toolbox.getPanel("webconsole").hud;
    let jsterm = hud.jsterm;

    let jstermInput = jsterm.hud.document.querySelector(".jsterm-input-node");
    is(jstermInput.value, "temp0", "first console variable is named temp0");

    let result = yield jsterm.execute();
    isnot(result.textContent.indexOf('<p id="console-var">'), -1,
          "variable temp0 references correct node");

    yield selectNode("#console-var-multi", inspector);
    dispatchCommandEvent(useInConsoleNode);
    yield inspector.once("console-var-ready");

    is(jstermInput.value, "temp1", "second console variable is named temp1");

    result = yield jsterm.execute();
    isnot(result.textContent.indexOf('<p id="console-var-multi">'), -1,
          "variable temp1 references correct node");

    jsterm.clearHistory();
  }
});
