/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Bug 1093349: Test that the pretty-printing and blackboxing buttons
 * are hidden if the server doesn't support them
 */

const TAB_URL = EXAMPLE_URL + "doc_auto-pretty-print-01.html";

var { RootActor } = require("devtools/server/actors/root");

function test() {
  RootActor.prototype.traits.noBlackBoxing = true;
  RootActor.prototype.traits.noPrettyPrinting = true;

  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    let document = aPanel.panelWin.document;
    let ppButton = document.querySelector('#pretty-print');
    let bbButton = document.querySelector('#black-box');
    let sep = document.querySelector('#sources-toolbar .devtools-separator');

    is(ppButton.style.display, 'none', 'The pretty-print button is hidden');
    is(bbButton.style.display, 'none', 'The blackboxing button is hidden');
    is(sep.style.display, 'none', 'The separator is hidden');
    closeDebuggerAndFinish(aPanel)
  });
}
