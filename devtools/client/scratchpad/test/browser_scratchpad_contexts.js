/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(function() {
    openScratchpad(runTests);
  });

  gBrowser.loadURI("data:text/html,test context switch in Scratchpad");
}

function runTests() {
  const sp = gScratchpadWindow.Scratchpad;
  const contentMenu = gScratchpadWindow.document.getElementById("sp-menu-content");
  const chromeMenu = gScratchpadWindow.document.getElementById("sp-menu-browser");
  const notificationBox = sp.notificationBox;

  ok(contentMenu, "found #sp-menu-content");
  ok(chromeMenu, "found #sp-menu-browser");
  ok(notificationBox, "found Scratchpad.notificationBox");

  const tests = [{
    method: "run",
    prepare: async function() {
      sp.setContentContext();

      is(sp.executionContext, gScratchpadWindow.SCRATCHPAD_CONTEXT_CONTENT,
         "executionContext is content");

      is(contentMenu.getAttribute("checked"), "true",
         "content menuitem is checked");

      isnot(chromeMenu.getAttribute("checked"), "true",
         "chrome menuitem is not checked");

      ok(!notificationBox.currentNotification,
         "there is no notification in content context");

      sp.editor.setText("window.foobarBug636725 = 'aloha';");

      const pageResult = await inContent(function() {
        return content.wrappedJSObject.foobarBug636725;
      });
      ok(!pageResult, "no content.foobarBug636725");
    },
    then: () => ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
      is(content.wrappedJSObject.foobarBug636725, "aloha",
         "content.foobarBug636725 has been set");
    }),
  }, {
    method: "run",
    prepare: function() {
      sp.setBrowserContext();

      is(sp.executionContext, gScratchpadWindow.SCRATCHPAD_CONTEXT_BROWSER,
         "executionContext is chrome");

      is(chromeMenu.getAttribute("checked"), "true",
         "chrome menuitem is checked");

      isnot(contentMenu.getAttribute("checked"), "true",
         "content menuitem is not checked");

      ok(notificationBox.currentNotification,
         "there is a notification in browser context");

      const [ from, to ] = sp.editor.getPosition(31, 32);
      sp.editor.replaceText("2'", from, to);

      is(sp.getText(), "window.foobarBug636725 = 'aloha2';",
         "setText() worked");
    },
    then: function() {
      is(window.foobarBug636725, "aloha2",
         "window.foobarBug636725 has been set");

      delete window.foobarBug636725;
      ok(!window.foobarBug636725, "no window.foobarBug636725");
    }
  }, {
    method: "run",
    prepare: function() {
      sp.editor.replaceText("gBrowser", sp.editor.getPosition(7));

      is(sp.getText(), "window.gBrowser",
         "setText() worked with no end for the replace range");
    },
    then: function([, , result]) {
      is(result.class, "Object",
         "chrome context has access to chrome objects");
    }
  }, {
    method: "run",
    prepare: function() {
      // Check that the sandbox is cached.
      sp.editor.setText("typeof foobarBug636725cache;");
    },
    then: function([, , result]) {
      is(result, "undefined", "global variable does not exist");
    }
  }, {
    method: "run",
    prepare: function() {
      sp.editor.setText("window.foobarBug636725cache = 'foo';" +
                 "typeof foobarBug636725cache;");
    },
    then: function([, , result]) {
      is(result, "string",
         "global variable exists across two different executions");
    }
  }, {
    method: "run",
    prepare: function() {
      sp.editor.setText("window.foobarBug636725cache2 = 'foo';" +
                 "typeof foobarBug636725cache2;");
    },
    then: function([, , result]) {
      is(result, "string",
         "global variable exists across two different executions");
    }
  }, {
    method: "run",
    prepare: function() {
      sp.setContentContext();

      is(sp.executionContext, gScratchpadWindow.SCRATCHPAD_CONTEXT_CONTENT,
         "executionContext is content");

      sp.editor.setText("typeof foobarBug636725cache2;");
    },
    then: function([, , result]) {
      is(result, "undefined",
         "global variable no longer exists after changing the context");
    }
  }];

  runAsyncCallbackTests(sp, tests).then(() => {
    sp.setBrowserContext();
    sp.editor.setText("delete foobarBug636725cache;" +
               "delete foobarBug636725cache2;");
    sp.run().then(finish);
  });
}
