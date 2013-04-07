/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test()
{
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
    gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);
    openScratchpad(runTests);
  }, true);

  content.location = "data:text/html,test context switch in Scratchpad";
}


function runTests()
{
  let sp = gScratchpadWindow.Scratchpad;
  let contentMenu = gScratchpadWindow.document.getElementById("sp-menu-content");
  let chromeMenu = gScratchpadWindow.document.getElementById("sp-menu-browser");
  let notificationBox = sp.notificationBox;

  ok(contentMenu, "found #sp-menu-content");
  ok(chromeMenu, "found #sp-menu-browser");
  ok(notificationBox, "found Scratchpad.notificationBox");

  let tests = [{
    method: "run",
    prepare: function() {
      sp.setContentContext();

      is(sp.executionContext, gScratchpadWindow.SCRATCHPAD_CONTEXT_CONTENT,
         "executionContext is content");

      is(contentMenu.getAttribute("checked"), "true",
         "content menuitem is checked");

      isnot(chromeMenu.getAttribute("checked"), "true",
         "chrome menuitem is not checked");

      ok(!notificationBox.currentNotification,
         "there is no notification in content context");

      let dsp = sp.contentSandbox.__SCRATCHPAD__;

      ok(sp.contentSandbox.__SCRATCHPAD__,
          "there is a variable named __SCRATCHPAD__");

      ok(sp.contentSandbox.__SCRATCHPAD__.editor,
          "scratchpad is actually an instance of Scratchpad");

      sp.setText("window.foobarBug636725 = 'aloha';");

      ok(!content.wrappedJSObject.foobarBug636725,
         "no content.foobarBug636725");
    },
    then: function() {
      is(content.wrappedJSObject.foobarBug636725, "aloha",
         "content.foobarBug636725 has been set");
    }
  },
  {
    method: "run",
    prepare: function() {
      sp.setBrowserContext();

      is(sp.executionContext, gScratchpadWindow.SCRATCHPAD_CONTEXT_BROWSER,
         "executionContext is chrome");

      is(chromeMenu.getAttribute("checked"), "true",
         "chrome menuitem is checked");

      isnot(contentMenu.getAttribute("checked"), "true",
         "content menuitem is not checked");

      ok(sp.chromeSandbox.__SCRATCHPAD__,
        "there is a variable named __SCRATCHPAD__");

      ok(sp.chromeSandbox.__SCRATCHPAD__.editor,
          "scratchpad is actually an instance of Scratchpad");

      ok(notificationBox.currentNotification,
         "there is a notification in browser context");

      sp.setText("2'", 31, 32);

      is(sp.getText(), "window.foobarBug636725 = 'aloha2';",
         "setText() worked");
    },
    then: function() {
      is(window.foobarBug636725, "aloha2",
         "window.foobarBug636725 has been set");

      delete window.foobarBug636725;
      ok(!window.foobarBug636725, "no window.foobarBug636725");
    }
  },
  {
    method: "run",
    prepare: function() {
      sp.setText("gBrowser", 7);

      is(sp.getText(), "window.gBrowser",
         "setText() worked with no end for the replace range");
    },
    then: function([, , result]) {
      is(typeof result.addTab, "function",
         "chrome context has access to chrome objects");
    }
  },
  {
    method: "run",
    prepare: function() {
      // Check that the sandbox is cached.
      sp.setText("typeof foobarBug636725cache;");
    },
    then: function([, , result]) {
      is(result, "undefined", "global variable does not exist");
    }
  },
  {
    method: "run",
    prepare: function() {
      sp.setText("var foobarBug636725cache = 'foo';" +
                 "typeof foobarBug636725cache;");
    },
    then: function([, , result]) {
      is(result, "string",
         "global variable exists across two different executions");
    }
  },
  {
    method: "run",
    prepare: function() {
      sp.resetContext();
      sp.setText("typeof foobarBug636725cache;");
    },
    then: function([, , result]) {
      is(result, "undefined",
         "global variable no longer exists after calling resetContext()");
    }
  },
  {
    method: "run",
    prepare: function() {
      sp.setText("var foobarBug636725cache2 = 'foo';" +
                 "typeof foobarBug636725cache2;");
    },
    then: function([, , result]) {
      is(result, "string",
         "global variable exists across two different executions");
    }
  },
  {
    method: "run",
    prepare: function() {
      sp.setContentContext();

      is(sp.executionContext, gScratchpadWindow.SCRATCHPAD_CONTEXT_CONTENT,
         "executionContext is content");

      sp.setText("typeof foobarBug636725cache2;");
    },
    then: function([, , result]) {
      is(result, "undefined",
         "global variable no longer exists after changing the context");
    }
  }];

  runAsyncCallbackTests(sp, tests).then(finish);
}