/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PERMISSION_DIALOG =
  "chrome://mozapps/content/handling/permissionDialog.xhtml";

add_setup(async function () {
  // Set a new handler as default.
  const protoSvc = Cc[
    "@mozilla.org/uriloader/external-protocol-service;1"
  ].getService(Ci.nsIExternalProtocolService);
  let protoInfo = protoSvc.getProtocolHandlerInfo("web+testprotocol");
  protoInfo.preferredAction = protoInfo.useHelperApp;
  let handler = Cc["@mozilla.org/uriloader/web-handler-app;1"].createInstance(
    Ci.nsIWebHandlerApp
  );
  handler.uriTemplate = "https://example.com/foobar?uri=%s";
  handler.name = "Test protocol";
  let handlers = protoInfo.possibleApplicationHandlers;
  handlers.appendElement(handler);

  protoInfo.preferredApplicationHandler = handler;
  protoInfo.alwaysAskBeforeHandling = false;

  const handlerSvc = Cc["@mozilla.org/uriloader/handler-service;1"].getService(
    Ci.nsIHandlerService
  );
  handlerSvc.store(protoInfo);

  registerCleanupFunction(() => {
    handlerSvc.remove(protoInfo);
  });
});

add_task(async function test_promptWhileNotForeground() {
  await BrowserTestUtils.withNewTab("about:blank", async browser => {
    let windowOpened = BrowserTestUtils.waitForNewWindow();
    await SpecialPowers.spawn(browser, [], () => {
      content.eval(`window.open('about:blank', "_blank", "height=600");`);
    });
    let otherWin = await windowOpened;
    info("Opened extra window, now start a prompt.");

    // To ensure we test the delay helper correctly, shorten the delay:
    await SpecialPowers.pushPrefEnv({
      set: [["security.dialog_enable_delay", 50]],
    });

    let promptPromise = BrowserTestUtils.promiseAlertDialogOpen(
      null,
      PERMISSION_DIALOG,
      { isSubDialog: true }
    );
    await SpecialPowers.spawn(browser, [], () => {
      content.document.location.href = "web+testprotocol:hello";
    });
    info("Started opening prompt.");
    let prompt = await promptPromise;
    info("Opened prompt.");
    let dialog = prompt.document.querySelector("dialog");
    let button = dialog.getButton("accept");
    is(button.getAttribute("disabled"), "true", "Button should be disabled");

    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(r => setTimeout(r, 500));
    is(
      button.getAttribute("disabled"),
      "true",
      "Button should still be disabled while the dialog is in the background"
    );

    let buttonGetsEnabled = BrowserTestUtils.waitForMutationCondition(
      button,
      { attributeFilter: ["disabled"] },
      () => button.getAttribute("disabled") != "true"
    );
    await BrowserTestUtils.closeWindow(otherWin);
    info("Waiting for button to be enabled.");
    await buttonGetsEnabled;
    ok(true, "The button was enabled.");
    dialog.cancelDialog();

    await SpecialPowers.popPrefEnv();
  });
});

add_task(async function test_promptWhileForeground() {
  await BrowserTestUtils.withNewTab("about:blank", async browser => {
    let promptPromise = BrowserTestUtils.promiseAlertDialogOpen(
      null,
      PERMISSION_DIALOG,
      { isSubDialog: true }
    );
    await SpecialPowers.spawn(browser, [], () => {
      content.document.location.href = "web+testprotocol:hello";
    });
    info("Started opening prompt.");
    let prompt = await promptPromise;
    info("Opened prompt.");
    let dialog = prompt.document.querySelector("dialog");
    let button = dialog.getButton("accept");
    is(button.getAttribute("disabled"), "true", "Button should be disabled");
    await BrowserTestUtils.waitForMutationCondition(
      button,
      { attributeFilter: ["disabled"] },
      () => button.getAttribute("disabled") != "true"
    );
    ok(true, "The button was enabled.");
    dialog.cancelDialog();
  });
});
