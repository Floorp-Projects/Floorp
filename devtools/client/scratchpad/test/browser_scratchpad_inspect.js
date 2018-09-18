/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(function() {
    openScratchpad(runTests);
  });

  gBrowser.loadURI("data:text/html;charset=utf8,<p>test inspect() in Scratchpad</p>");
}

function runTests() {
  const sp = gScratchpadWindow.Scratchpad;

  sp.setText("({ a: 'foobarBug636725' })");

  sp.inspect().then(function() {
    const sidebar = sp.sidebar;
    ok(sidebar.visible, "sidebar is open");

    let found = false;

    outer: for (const scope of sidebar.variablesView) {
      for (const [, obj] of scope) {
        for (const [, prop] of obj) {
          if (prop.name == "a" && prop.value == "foobarBug636725") {
            found = true;
            break outer;
          }
        }
      }
    }

    ok(found, "found the property");

    const tabbox = sidebar._sidebar._tabbox;
    is(tabbox.width, 300, "Scratchpad sidebar width is correct");
    ok(!tabbox.hasAttribute("hidden"), "Scratchpad sidebar visible");
    sidebar.hide();
    ok(tabbox.hasAttribute("hidden"), "Scratchpad sidebar hidden");
    sp.inspect().then(function() {
      is(tabbox.width, 300, "Scratchpad sidebar width is still correct");
      ok(!tabbox.hasAttribute("hidden"), "Scratchpad sidebar visible again");
      finish();
    });
  });
}
