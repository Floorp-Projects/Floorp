/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Load a page that generates cookie warning/info messages. See bug 1622306.

"use strict";
requestLongerTimeout(2);

const TEST_FILE =
  "browser/devtools/client/webconsole/test/browser/test-warning-groups.html";

pushPref("devtools.webconsole.groupWarningMessages", true);

async function cleanUp() {
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
      resolve()
    );
  });
}

add_task(cleanUp);

add_task(async function testSameSiteCookieMessage() {
  const tests = [
    {
      pref: true,
      message1:
        "Cookie “a” has “SameSite” policy set to “Lax” because it is missing a “SameSite” attribute, and “SameSite=Lax” is the default value for this attribute.",
      typeMessage1: ".info",
      groupLabel:
        "Some cookies are misusing the “SameSite“ attribute, so it won’t work as expected",
      message2:
        "Cookie “b” has “SameSite” policy set to “Lax” because it is missing a “SameSite” attribute, and “SameSite=Lax” is the default value for this attribute.",
    },
    {
      pref: false,
      groupLabel:
        "Some cookies are misusing the recommended “SameSite“ attribute",
      message1:
        "Cookie “a” will be soon rejected because it has the “SameSite” attribute set to “None” or an invalid value, without the “secure” attribute.",
      typeMessage1: ".warn",
      message2:
        "Cookie “b” will be soon rejected because it has the “SameSite” attribute set to “None” or an invalid value, without the “secure” attribute.",
    },
  ];

  for (const test of tests) {
    info("LaxByDefault: " + test.pref);
    await pushPref("network.cookie.sameSite.laxByDefault", test.pref);

    const { hud, tab, win } = await openNewWindowAndConsole(
      "http://example.org/" + TEST_FILE
    );

    info("Test cookie messages");
    const onLaxMissingWarningMessage = waitForMessage(
      hud,
      test.message1,
      test.typeMessage1
    );

    SpecialPowers.spawn(tab.linkedBrowser, [], () => {
      content.wrappedJSObject.createCookie("a=1");
    });

    await onLaxMissingWarningMessage;

    ok(true, "The first message was displayed");

    info("Emit a new cookie message to check that it causes a grouping");

    const onCookieSameSiteWarningGroupMessage = waitForMessage(
      hud,
      test.groupLabel,
      ".warn"
    );

    SpecialPowers.spawn(tab.linkedBrowser, [], () => {
      content.wrappedJSObject.createCookie("b=1");
    });

    const { node } = await onCookieSameSiteWarningGroupMessage;
    is(
      node.querySelector(".warning-group-badge").textContent,
      "2",
      "The badge has the expected text"
    );

    checkConsoleOutputForWarningGroup(hud, [`▶︎⚠ ${test.groupLabel} 2`]);

    info("Open the group");
    node.querySelector(".arrow").click();
    await waitFor(() => findMessage(hud, "SameSite"));

    checkConsoleOutputForWarningGroup(hud, [
      `▼︎⚠ ${test.groupLabel} 2`,
      `| ${test.message1}`,
      `| ${test.message2}`,
    ]);

    await win.close();
  }
});

add_task(cleanUp);

add_task(async function testInvalidSameSiteMessage() {
  await pushPref("network.cookie.sameSite.laxByDefault", true);

  const groupLabel =
    "Some cookies are misusing the “SameSite“ attribute, so it won’t work as expected";
  const message1 =
    "Invalid “SameSite“ value for cookie “a”. The supported values are: “Lax“, “Strict“, “None“.";
  const message2 =
    "Cookie “a” has “SameSite” policy set to “Lax” because it is missing a “SameSite” attribute, and “SameSite=Lax” is the default value for this attribute.";

  const { hud, tab, win } = await openNewWindowAndConsole(
    "http://example.org/" + TEST_FILE
  );

  info("Test cookie messages");

  SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    content.wrappedJSObject.createCookie("a=1; sameSite=batman");
  });

  const { node } = await waitForMessage(hud, groupLabel, ".warn");
  is(
    node.querySelector(".warning-group-badge").textContent,
    "2",
    "The badge has the expected text"
  );

  checkConsoleOutputForWarningGroup(hud, [`▶︎⚠ ${groupLabel} 2`]);

  info("Open the group");
  node.querySelector(".arrow").click();
  await waitFor(() => findMessage(hud, "SameSite"));

  checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${groupLabel} 2`,
    `| ${message1}`,
    `| ${message2}`,
  ]);

  // Source map are being resolved in background and we might have
  // pending request related to this service if we close the window
  // immeditely. So just wait for these request to finish before proceeding.
  await hud.toolbox.sourceMapURLService.waitForSourcesLoading();

  await win.close();
});

add_task(cleanUp);
