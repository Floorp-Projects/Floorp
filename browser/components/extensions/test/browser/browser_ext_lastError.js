"use strict";

async function sendMessage(options) {
  function background(options) {
    browser.runtime.sendMessage(result => {
      browser.test.assertEq(undefined, result, "Argument value");
      if (options.checkLastError) {
        browser.test.assertEq(
          "runtime.sendMessage's message argument is missing",
          browser.runtime.lastError?.message,
          "lastError value"
        );
      }
      browser.test.sendMessage("done");
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    background: `(${background})(${JSON.stringify(options)})`,
  });

  await extension.startup();

  await extension.awaitMessage("done");

  await extension.unload();
}

add_task(async function testLastError() {
  // Not necessary in browser-chrome tests, but monitorConsole gripes
  // if we don't call it.
  SimpleTest.waitForExplicitFinish();

  // Check that we have no unexpected console messages when lastError is
  // checked.
  let waitForConsole = new Promise(resolve => {
    SimpleTest.monitorConsole(resolve, [
      { message: /message argument is missing/, forbid: true },
    ]);
  });

  await sendMessage({ checkLastError: true });

  SimpleTest.endMonitorConsole();
  await waitForConsole;

  // Check that we do have a console message when lastError is not checked.
  waitForConsole = new Promise(resolve => {
    SimpleTest.monitorConsole(resolve, [
      {
        message: /Unchecked lastError value: Error: runtime.sendMessage's message argument is missing/,
      },
    ]);
  });

  await sendMessage({});

  SimpleTest.endMonitorConsole();
  await waitForConsole;
});
