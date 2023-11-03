"use strict";

const TEST_URI = "data:text/html,test-page";

const {
  HELP_URL,
} = require("resource://devtools/client/webconsole/constants.js");

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);

  info("Trigger the :help command");
  // Hook the browser method in order to prevent the URL from opening
  // as mochitest forbids opening remote URLs.
  const onUrlOpened = new Promise(resolve => {
    const originalMethod = window.openWebLinkIn;
    window.openWebLinkIn = url => {
      window.openWebLinkIn = originalMethod;
      resolve(url);
    };
  });
  execute(hud, ":help");
  info("Wait for the help URL to be requested to open");
  const url = await onUrlOpened;
  is(url, HELP_URL, "Expected url was opened when executing :help");

  info("Execute only ':'");
  await executeAndWaitForMessageByType(
    hud,
    ":",
    "Missing a command name after ':'",
    ".console-api.error"
  );

  // Any space after a `:`  would lead to the same exception
  info("Execute only ': '");
  await executeAndWaitForMessageByType(
    hud,
    ": ",
    "Missing a command name after ':'",
    ".console-api.error"
  );

  info("Execute only ': foo'");
  await executeAndWaitForMessageByType(
    hud,
    ": foo",
    "Missing a command name after ':'",
    ".console-api.error"
  );

  info("Execute unsupported command ':bar'");
  await executeAndWaitForMessageByType(
    hud,
    ":bar",
    "bar' is not a valid command",
    ".console-api.error"
  );

  info("Execute string with two commands");
  await executeAndWaitForMessageByType(
    hud,
    ":help :help",
    "Executing multiple commands in one evaluation is not supported",
    ".console-api.error"
  );
});
