/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var tab1;
var tab2;
var sp;

function test() {
  waitForExplicitFinish();

  tab1 = BrowserTestUtils.addTab(gBrowser);
  gBrowser.selectedTab = tab1;
  BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(function() {
    tab2 = BrowserTestUtils.addTab(gBrowser);
    gBrowser.selectedTab = tab2;
    BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(function() {
      openScratchpad(runTests);
    });
    BrowserTestUtils.loadURI(
      gBrowser,
      "data:text/html,test context switch in Scratchpad tab 2"
    );
  });

  BrowserTestUtils.loadURI(
    gBrowser,
    "data:text/html,test context switch in Scratchpad tab 1"
  );
}

async function runTests() {
  sp = gScratchpadWindow.Scratchpad;

  const contentMenu = gScratchpadWindow.document.getElementById(
    "sp-menu-content"
  );
  const browserMenu = gScratchpadWindow.document.getElementById(
    "sp-menu-browser"
  );
  const notificationBox = sp.notificationBox;

  ok(contentMenu, "found #sp-menu-content");
  ok(browserMenu, "found #sp-menu-browser");
  ok(notificationBox, "found Scratchpad.notificationBox");

  sp.setContentContext();

  is(
    sp.executionContext,
    gScratchpadWindow.SCRATCHPAD_CONTEXT_CONTENT,
    "executionContext is content"
  );

  is(
    contentMenu.getAttribute("checked"),
    "true",
    "content menuitem is checked"
  );

  isnot(
    browserMenu.getAttribute("checked"),
    "true",
    "chrome menuitem is not checked"
  );

  is(
    notificationBox.currentNotification.messageText.textContent,
    "Scratchpad will be disabled in a future release. Learn more…",
    "The deprecation warning is displayed in content context"
  );

  sp.setText("window.foosbug653108 = 'aloha';");

  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    ok(!content.wrappedJSObject.foosbug653108, "no content.foosbug653108");
  });

  await sp.run();

  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    is(
      content.wrappedJSObject.foosbug653108,
      "aloha",
      "content.foosbug653108 has been set"
    );
  });

  gBrowser.tabContainer.addEventListener("TabSelect", runTests2, true);
  gBrowser.selectedTab = tab1;
}

async function runTests2() {
  gBrowser.tabContainer.removeEventListener("TabSelect", runTests2, true);

  ok(!window.foosbug653108, "no window.foosbug653108");

  sp.setText("window.foosbug653108");
  const [, , result] = await sp.run();
  isnot(result, "aloha", "window.foosbug653108 is not aloha");

  sp.setText("window.foosbug653108 = 'ahoyhoy';");
  await sp.run();
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    is(
      content.wrappedJSObject.foosbug653108,
      "ahoyhoy",
      "content.foosbug653108 has been set 2"
    );
  });

  BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(runTests3);
  BrowserTestUtils.loadURI(
    gBrowser,
    "data:text/html,test context switch in Scratchpad location 2"
  );
}

function runTests3() {
  // Check that the sandbox is not cached.

  sp.setText("typeof foosbug653108;");
  sp.run().then(function([, , result]) {
    is(result, "undefined", "global variable does not exist");

    tab1 = null;
    tab2 = null;
    sp = null;
    finish();
  });
}
