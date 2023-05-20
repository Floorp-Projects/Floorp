/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function () {
  await addTab("about:blank");

  info(`Open browser console`);
  const hud = await BrowserConsoleManager.openBrowserConsoleOrFocus();

  info(`Clear existing messages`);
  const onMessagesCleared = hud.ui.once("messages-cleared");
  await clearOutput(hud);
  await onMessagesCleared;

  info(`Create a DOM window object`);
  await hud.commands.scriptCommand.execute(`
    globalThis.myBrowser = Services.appShell.createWindowlessBrowser();
    globalThis.myWindow = myBrowser.document.defaultView;
  `);

  info(`Check objects inheriting from a DOM window`);
  async function check(input, expected, name) {
    const msg = await executeAndWaitForResultMessage(hud, input, "");
    is(msg.node.querySelector(".message-body").textContent, expected, name);
  }
  await check("Object.create(myWindow)", "Object {  }", "Empty object");
  await check(
    "Object.create(myWindow, { location: { value: 1, enumerable: true } })",
    "Object { location: 1 }",
    "Object with 'location' property"
  );
  await check(
    `Object.create(myWindow, {
      location: {
        get() {
          console.error("pwned!");
          return { href: "Oops" };
        },
        enumerable: true,
      },
    })`,
    "Object { location: Getter }",
    "Object with 'location' unsafe getter"
  );

  info(`Check that no error was logged`);
  // wait a bit so potential errors can be printed
  await wait(1000);
  const error = findErrorMessage(hud, "", ":not(.network)");
  if (error) {
    ok(false, `Got error ${JSON.stringify(error.textContent)}`);
  } else {
    ok(true, "No error was logged");
  }

  info(`Cleanup`);
  await hud.commands.scriptCommand.execute(`
    myBrowser.close();
    delete globalThis.myBrowser;
    delete globalThis.myWindow;
  `);
});
