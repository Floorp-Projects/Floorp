/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function test() {
  info("Test that the split console state is persisted");

  let toolbox;
  let TEST_URI = "data:text/html;charset=utf-8,<p>Web Console test for splitting</p>";

  Task.spawn(runner).then(finish);

  function* runner() {
    info("Opening a tab while there is no user setting on split console pref");
    let {tab} = yield loadTab(TEST_URI);
    let target = TargetFactory.forTab(tab);
    toolbox = yield gDevTools.showToolbox(target, "inspector");

    ok(!toolbox.splitConsole, "Split console is hidden by default.");
    yield toggleSplitConsoleWithEscape();
    ok(toolbox.splitConsole, "Split console is now visible.");
    ok(getPrefValue(), "Pref is true");

    yield toolbox.destroy();

    info("Opening a tab while there is a true user setting on split console pref");
    let {tab} = yield loadTab(TEST_URI);
    let target = TargetFactory.forTab(tab);
    toolbox = yield gDevTools.showToolbox(target, "inspector");

    ok(toolbox.splitConsole, "Split console is visible by default.");
    yield toggleSplitConsoleWithEscape();
    ok(!toolbox.splitConsole, "Split console is now hidden.");
    ok(!getPrefValue(), "Pref is false");

    yield toolbox.destroy();

    info("Opening a tab while there is a false user setting on split console pref");
    let {tab} = yield loadTab(TEST_URI);
    let target = TargetFactory.forTab(tab);
    toolbox = yield gDevTools.showToolbox(target, "inspector");

    ok(!toolbox.splitConsole, "Split console is hidden by default.");
    ok(!getPrefValue(), "Pref is false");

    yield toolbox.destroy();
  }

  function getPrefValue() {
    return Services.prefs.getBoolPref("devtools.toolbox.splitconsoleEnabled");
  }

  function toggleSplitConsoleWithEscape() {
    let onceSplitConsole = toolbox.once("split-console");
    let contentWindow = toolbox.frame.contentWindow;
    contentWindow.focus();
    EventUtils.sendKey("ESCAPE", contentWindow);
    return onceSplitConsole;
  }

  function finish() {
    toolbox = TEST_URI = null;
    Services.prefs.clearUserPref("devtools.toolbox.splitconsoleEnabled");
    finishTest();
  }
}
