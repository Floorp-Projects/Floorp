/**
 * When "privacy.resistFingerprinting" is set to true, user permission is
 * required for canvas data extraction.
 * This tests whether the site permission prompt for canvas data extraction
 * works properly.
 */
"use strict";

const kUrl = "https://example.com/";
const kPrincipal = Services.scriptSecurityManager.createCodebasePrincipal(Services.io.newURI(kUrl), {});
const kPermission = "canvas";

function initTab() {
  let contentWindow = content.wrappedJSObject;

  let drawCanvas = (fillStyle, id) => {
    let contentDocument = contentWindow.document;
    let width = 64, height = 64;
    let canvas = contentDocument.createElement("canvas");
    if (id) {
      canvas.setAttribute("id", id);
    }
    canvas.setAttribute("width", width);
    canvas.setAttribute("height", height);
    contentDocument.body.appendChild(canvas);

    let context = canvas.getContext("2d");
    context.fillStyle = fillStyle;
    context.fillRect(0, 0, width, height);

    if (id) {
      let button = contentDocument.createElement("button");
      button.addEventListener("click", function() { canvas.toDataURL(); });
      button.setAttribute("id", "clickme");
      button.innerHTML = "Click Me!";
      contentDocument.body.appendChild(button);
    }

    return canvas;
  };

  let placeholder = drawCanvas("white");
  contentWindow.kPlaceholderData = placeholder.toDataURL();
  let canvas = drawCanvas("cyan", "canvas-id-canvas");
  isnot(canvas.toDataURL(), contentWindow.kPlaceholderData,
      "privacy.resistFingerprinting = false, canvas data != placeholder data");
}

function enableResistFingerprinting(autoDeclineNoInput) {
  return SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.resistFingerprinting", true],
      ["privacy.resistFingerprinting.autoDeclineNoUserInputCanvasPrompts", autoDeclineNoInput],
    ],
  });
}

function promisePopupShown() {
  return BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown");
}

function promisePopupHidden() {
  return BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popuphidden");
}

function extractCanvasData(grantPermission) {
  let contentWindow = content.wrappedJSObject;
  let canvas = contentWindow.document.getElementById("canvas-id-canvas");
  let canvasData = canvas.toDataURL();
  if (grantPermission) {
    isnot(canvasData, contentWindow.kPlaceholderData,
        "privacy.resistFingerprinting = true, permission granted, canvas data != placeholderdata");
  } else if (grantPermission === false) {
    is(canvasData, contentWindow.kPlaceholderData,
        "privacy.resistFingerprinting = true, permission denied, canvas data == placeholderdata");
  } else {
    is(canvasData, contentWindow.kPlaceholderData,
        "privacy.resistFingerprinting = true, requesting permission, canvas data == placeholderdata");
  }
}

function triggerCommand(button) {
  let notifications = PopupNotifications.panel.children;
  let notification = notifications[0];
  EventUtils.synthesizeMouseAtCenter(notification[button], {});
}

function triggerMainCommand() {
  triggerCommand("button");
}

function triggerSecondaryCommand() {
  triggerCommand("secondaryButton");
}

function testPermission() {
  return Services.perms.testPermissionFromPrincipal(kPrincipal, kPermission);
}

async function withNewTabNoInput(grantPermission, browser) {
  await ContentTask.spawn(browser, null, initTab);
  await enableResistFingerprinting(false);
  let popupShown = promisePopupShown();
  await ContentTask.spawn(browser, null, extractCanvasData);
  await popupShown;
  let popupHidden = promisePopupHidden();
  if (grantPermission) {
    triggerMainCommand();
    await popupHidden;
    is(testPermission(), Services.perms.ALLOW_ACTION, "permission granted");
  } else {
    triggerSecondaryCommand();
    await popupHidden;
    is(testPermission(), Services.perms.DENY_ACTION, "permission denied");
  }
  await ContentTask.spawn(browser, grantPermission, extractCanvasData);
  await SpecialPowers.popPrefEnv();
}

async function doTestNoInput(grantPermission) {
  Services.perms.removeFromPrincipal(kPrincipal, kPermission);
  await BrowserTestUtils.withNewTab(kUrl, withNewTabNoInput.bind(null, grantPermission));
}

// With auto-declining disabled (not the default)
// Tests clicking "Don't Allow" button of the permission prompt.
add_task(doTestNoInput.bind(null, false));

// Tests clicking "Allow" button of the permission prompt.
add_task(doTestNoInput.bind(null, true));

// I don't know how to write a test for "Make sure a prompt is not shown"...
// But ideally we would have one of those...

function extractCanvasDataUserInput(grantPermission) {
  let contentWindow = content.wrappedJSObject;
  let canvas = contentWindow.document.getElementById("canvas-id-canvas");
  let canvasData = canvas.toDataURL();
  if (grantPermission) {
    isnot(canvasData, contentWindow.kPlaceholderData,
        "privacy.resistFingerprinting = true, permission granted, canvas data != placeholderdata");
  } else if (grantPermission === false) {
    is(canvasData, contentWindow.kPlaceholderData,
        "privacy.resistFingerprinting = true, permission denied, canvas data == placeholderdata");
  } else {
    is(canvasData, contentWindow.kPlaceholderData,
        "privacy.resistFingerprinting = true, requesting permission, canvas data == placeholderdata");
  }
}

async function withNewTabInput(grantPermission, browser) {
  await ContentTask.spawn(browser, null, initTab);
  await enableResistFingerprinting(true);
  let popupShown = promisePopupShown();
  await ContentTask.spawn(browser, null, function(host) {
    E10SUtils.wrapHandlingUserInput(content, true, function() {
      var button = content.document.getElementById("clickme");
      button.click();
    });
  });
  await popupShown;
  let popupHidden = promisePopupHidden();
  if (grantPermission) {
    triggerMainCommand();
    await popupHidden;
    is(testPermission(), Services.perms.ALLOW_ACTION, "permission granted");
  } else {
    triggerSecondaryCommand();
    await popupHidden;
    is(testPermission(), Services.perms.DENY_ACTION, "permission denied");
  }
  await ContentTask.spawn(browser, grantPermission, extractCanvasDataUserInput);
  await SpecialPowers.popPrefEnv();
}

async function doTestInput(grantPermission, autoDeclineNoInput) {
  Services.perms.removeFromPrincipal(kPrincipal, kPermission);
  await BrowserTestUtils.withNewTab(kUrl, withNewTabInput.bind(null, grantPermission));
}

// With auto-declining enabled (the default)
// Tests clicking "Don't Allow" button of the permission prompt.
add_task(doTestInput.bind(null, false));

// Tests clicking "Allow" button of the permission prompt.
add_task(doTestInput.bind(null, true));
