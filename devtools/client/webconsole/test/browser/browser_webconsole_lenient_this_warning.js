/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that calling the LenientThis warning is only called when expected.
const TEST_URI = `data:text/html;charset=utf8,<!DOCTYPE html>${encodeURI(`
    <h1>LenientThis warning</h1>
    <script>
      const el = document.createElement('div');
      globalThis.htmlDivElementProto = Object.getPrototypeOf(el);
      function triggerLenientThisWarning(){
        Object.getOwnPropertyDescriptor(
          Object.getPrototypeOf(globalThis.htmlDivElementProto),
          'onmouseenter'
        ).get.call()
      }
    </script>`)}`;

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);

  const expectedWarningMessageText =
    "Ignoring get or set of property that has [LenientThis] ";

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    const global = content.wrappedJSObject;
    global.console.log(global.htmlDivElementProto);
  });

  info("Wait for a bit so any warning message could be displayed");
  await wait(1000);
  await waitFor(() => findConsoleAPIMessage(hud, "HTMLDivElementPrototype"));

  ok(
    !findWarningMessage(hud, expectedWarningMessageText, ".warn"),
    "Displaying the HTMLDivElementPrototype does not trigger the LenientThis warning"
  );

  info(
    "Call a LenientThis getter with the wrong `this` to trigger a warning message"
  );
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    content.wrappedJSObject.triggerLenientThisWarning();
  });

  await waitFor(() =>
    findWarningMessage(hud, expectedWarningMessageText, ".warn")
  );
  ok(
    true,
    "Calling the LenientThis getter with an unexpected `this` did triggered the warning"
  );

  info(
    "Clear the console and call the LenientThis getter with an unexpected `this` again"
  );
  await clearOutput(hud);
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    content.wrappedJSObject.triggerLenientThisWarning();
  });
  info("Wait for a bit so any warning message could be displayed");
  await wait(1000);
  ok(
    !findWarningMessage(hud, expectedWarningMessageText, ".warn"),
    "Calling the LenientThis getter a second time did not trigger the warning again"
  );
});
