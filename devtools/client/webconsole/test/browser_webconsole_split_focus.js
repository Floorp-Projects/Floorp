/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 "use strict";

function test() {
  info("Test that the split console state is persisted");

  let toolbox;
  let TEST_URI = "data:text/html;charset=utf-8,<p>Web Console test for " +
                 "splitting</p>";

  Task.spawn(runner).then(finish);

  function* runner() {
    info("Opening a tab while there is no user setting on split console pref");
    let {tab} = yield loadTab(TEST_URI);
    let target = TargetFactory.forTab(tab);
    toolbox = yield gDevTools.showToolbox(target, "inspector");

    ok(!toolbox.splitConsole, "Split console is hidden by default");

    info("Focusing the search box before opening the split console");
    let inspector = toolbox.getPanel("inspector");
    inspector.searchBox.focus();

    // Use the binding element since inspector.searchBox is a XUL element.
    let activeElement = getActiveElement(inspector.panelDoc);
    activeElement = activeElement.ownerDocument.getBindingParent(activeElement);
    is(activeElement, inspector.searchBox, "Search box is focused");

    yield toolbox.openSplitConsole();

    ok(toolbox.splitConsole, "Split console is now visible");

    // Use the binding element since jsterm.inputNode is a XUL textarea element.
    activeElement = getActiveElement(toolbox.doc);
    activeElement = activeElement.ownerDocument.getBindingParent(activeElement);
    let inputNode = toolbox.getPanel("webconsole").hud.jsterm.inputNode;
    is(activeElement, inputNode, "Split console input is focused by default");

    yield toolbox.closeSplitConsole();

    info("Making sure that the search box is refocused after closing the " +
         "split console");
    // Use the binding element since inspector.searchBox is a XUL element.
    activeElement = getActiveElement(inspector.panelDoc);
    activeElement = activeElement.ownerDocument.getBindingParent(activeElement);
    is(activeElement, inspector.searchBox, "Search box is focused");

    yield toolbox.destroy();
  }

  function getActiveElement(doc) {
    let activeElement = doc.activeElement;
    while (activeElement && activeElement.contentDocument) {
      activeElement = activeElement.contentDocument.activeElement;
    }
    return activeElement;
  }

  function finish() {
    toolbox = TEST_URI = null;
    Services.prefs.clearUserPref("devtools.toolbox.splitconsoleEnabled");
    Services.prefs.clearUserPref("devtools.toolbox.splitconsoleHeight");
    finishTest();
  }
}
