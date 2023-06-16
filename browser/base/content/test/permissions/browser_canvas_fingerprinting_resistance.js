/**
 * When "privacy.resistFingerprinting" is set to true, user permission is
 * required for canvas data extraction.
 * This tests whether the site permission prompt for canvas data extraction
 * works properly.
 * When "privacy.resistFingerprinting.randomDataOnCanvasExtract" is true,
 * canvas data extraction results in random data, and when it is false, canvas
 * data extraction results in all-white data.
 */
"use strict";

const kUrl = "https://example.com/";
const kPrincipal = Services.scriptSecurityManager.createContentPrincipal(
  Services.io.newURI(kUrl),
  {}
);
const kPermission = "canvas";

function initTab() {
  let contentWindow = content.wrappedJSObject;

  let drawCanvas = (fillStyle, id) => {
    let contentDocument = contentWindow.document;
    let width = 64,
      height = 64;
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
      button.addEventListener("click", function () {
        canvas.toDataURL();
      });
      button.setAttribute("id", "clickme");
      button.innerHTML = "Click Me!";
      contentDocument.body.appendChild(button);
    }

    return canvas;
  };

  let placeholder = drawCanvas("white");
  contentWindow.kPlaceholderData = placeholder.toDataURL();
  let canvas = drawCanvas("cyan", "canvas-id-canvas");
  contentWindow.kPlacedData = canvas.toDataURL();
  is(
    canvas.toDataURL(),
    contentWindow.kPlacedData,
    "privacy.resistFingerprinting = false, canvas data == placed data"
  );
  isnot(
    canvas.toDataURL(),
    contentWindow.kPlaceholderData,
    "privacy.resistFingerprinting = false, canvas data != placeholder data"
  );
}

function enableResistFingerprinting(
  randomDataOnCanvasExtract,
  autoDeclineNoInput
) {
  return SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.resistFingerprinting", true],
      [
        "privacy.resistFingerprinting.randomDataOnCanvasExtract",
        randomDataOnCanvasExtract,
      ],
      [
        "privacy.resistFingerprinting.autoDeclineNoUserInputCanvasPrompts",
        autoDeclineNoInput,
      ],
    ],
  });
}

function promisePopupShown() {
  return BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown");
}

function promisePopupHidden() {
  return BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popuphidden");
}

function extractCanvasData(randomDataOnCanvasExtract, grantPermission) {
  let contentWindow = content.wrappedJSObject;
  let canvas = contentWindow.document.getElementById("canvas-id-canvas");
  let canvasData = canvas.toDataURL();
  if (grantPermission) {
    is(
      canvasData,
      contentWindow.kPlacedData,
      "privacy.resistFingerprinting = true, permission granted, canvas data == placed data"
    );
    if (!randomDataOnCanvasExtract) {
      isnot(
        canvasData,
        contentWindow.kPlaceholderData,
        "privacy.resistFingerprinting = true and randomDataOnCanvasExtract = false, permission granted, canvas data != placeholderdata"
      );
    }
  } else if (grantPermission === false) {
    isnot(
      canvasData,
      contentWindow.kPlacedData,
      "privacy.resistFingerprinting = true, permission denied, canvas data != placed data"
    );
    if (!randomDataOnCanvasExtract) {
      is(
        canvasData,
        contentWindow.kPlaceholderData,
        "privacy.resistFingerprinting = true and randomDataOnCanvasExtract = false, permission denied, canvas data == placeholderdata"
      );
    } else {
      isnot(
        canvasData,
        contentWindow.kPlaceholderData,
        "privacy.resistFingerprinting = true and randomDataOnCanvasExtract = true, permission denied, canvas data != placeholderdata"
      );
    }
  } else {
    isnot(
      canvasData,
      contentWindow.kPlacedData,
      "privacy.resistFingerprinting = true, requesting permission, canvas data != placed data"
    );
    if (!randomDataOnCanvasExtract) {
      is(
        canvasData,
        contentWindow.kPlaceholderData,
        "privacy.resistFingerprinting = true and randomDataOnCanvasExtract = false, requesting permission, canvas data == placeholderdata"
      );
    } else {
      isnot(
        canvasData,
        contentWindow.kPlaceholderData,
        "privacy.resistFingerprinting = true and randomDataOnCanvasExtract = true, requesting permission, canvas data != placeholderdata"
      );
    }
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

async function withNewTabNoInput(
  randomDataOnCanvasExtract,
  grantPermission,
  browser
) {
  await SpecialPowers.spawn(browser, [], initTab);
  await enableResistFingerprinting(randomDataOnCanvasExtract, false);
  let popupShown = promisePopupShown();
  await SpecialPowers.spawn(
    browser,
    [randomDataOnCanvasExtract],
    extractCanvasData
  );
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
  await SpecialPowers.spawn(
    browser,
    [randomDataOnCanvasExtract, grantPermission],
    extractCanvasData
  );
  await SpecialPowers.popPrefEnv();
}

async function doTestNoInput(randomDataOnCanvasExtract, grantPermission) {
  await BrowserTestUtils.withNewTab(
    kUrl,
    withNewTabNoInput.bind(null, randomDataOnCanvasExtract, grantPermission)
  );
  Services.perms.removeFromPrincipal(kPrincipal, kPermission);
}

// With auto-declining disabled (not the default)
// Tests clicking "Don't Allow" button of the permission prompt.
add_task(doTestNoInput.bind(null, true, false));
add_task(doTestNoInput.bind(null, false, false));

// Tests clicking "Allow" button of the permission prompt.
add_task(doTestNoInput.bind(null, true, true));
add_task(doTestNoInput.bind(null, false, true));

async function withNewTabAutoBlockNoInput(randomDataOnCanvasExtract, browser) {
  await SpecialPowers.spawn(browser, [], initTab);
  await enableResistFingerprinting(randomDataOnCanvasExtract, true);

  let noShowHandler = () => {
    ok(false, "The popup notification should not show in this case.");
  };
  PopupNotifications.panel.addEventListener("popupshown", noShowHandler, {
    once: true,
  });

  let promisePopupObserver = TestUtils.topicObserved(
    "PopupNotifications-updateNotShowing"
  );

  // Try to extract canvas data without user inputs.
  await SpecialPowers.spawn(
    browser,
    [randomDataOnCanvasExtract],
    extractCanvasData
  );

  await promisePopupObserver;
  info("There should be no popup shown on the panel.");

  // Check that the icon of canvas permission is shown.
  let canvasNotification = PopupNotifications.getNotification(
    "canvas-permissions-prompt",
    browser
  );

  is(
    canvasNotification.anchorElement.getAttribute("showing"),
    "true",
    "The canvas permission icon is correctly shown."
  );
  PopupNotifications.panel.removeEventListener("popupshown", noShowHandler);

  await SpecialPowers.popPrefEnv();
}

async function doTestAutoBlockNoInput(randomDataOnCanvasExtract) {
  await BrowserTestUtils.withNewTab(
    kUrl,
    withNewTabAutoBlockNoInput.bind(null, randomDataOnCanvasExtract)
  );
}

add_task(doTestAutoBlockNoInput.bind(null, true));
add_task(doTestAutoBlockNoInput.bind(null, false));

function extractCanvasDataUserInput(
  randomDataOnCanvasExtract,
  grantPermission
) {
  let contentWindow = content.wrappedJSObject;
  let canvas = contentWindow.document.getElementById("canvas-id-canvas");
  let canvasData = canvas.toDataURL();
  if (grantPermission) {
    is(
      canvasData,
      contentWindow.kPlacedData,
      "privacy.resistFingerprinting = true, permission granted, canvas data == placed data"
    );
    if (!randomDataOnCanvasExtract) {
      isnot(
        canvasData,
        contentWindow.kPlaceholderData,
        "privacy.resistFingerprinting = true and randomDataOnCanvasExtract = false, permission granted, canvas data != placeholderdata"
      );
    }
  } else if (grantPermission === false) {
    isnot(
      canvasData,
      contentWindow.kPlacedData,
      "privacy.resistFingerprinting = true, permission denied, canvas data != placed data"
    );
    if (!randomDataOnCanvasExtract) {
      is(
        canvasData,
        contentWindow.kPlaceholderData,
        "privacy.resistFingerprinting = true and randomDataOnCanvasExtract = false, permission denied, canvas data == placeholderdata"
      );
    } else {
      isnot(
        canvasData,
        contentWindow.kPlaceholderData,
        "privacy.resistFingerprinting = true and randomDataOnCanvasExtract = true, permission denied, canvas data != placeholderdata"
      );
    }
  } else {
    isnot(
      canvasData,
      contentWindow.kPlacedData,
      "privacy.resistFingerprinting = true, requesting permission, canvas data != placed data"
    );
    if (!randomDataOnCanvasExtract) {
      is(
        canvasData,
        contentWindow.kPlaceholderData,
        "privacy.resistFingerprinting = true and randomDataOnCanvasExtract = false, requesting permission, canvas data == placeholderdata"
      );
    } else {
      isnot(
        canvasData,
        contentWindow.kPlaceholderData,
        "privacy.resistFingerprinting = true and randomDataOnCanvasExtract = true, requesting permission, canvas data != placeholderdata"
      );
    }
  }
}

async function withNewTabInput(
  randomDataOnCanvasExtract,
  grantPermission,
  browser
) {
  await SpecialPowers.spawn(browser, [], initTab);
  await enableResistFingerprinting(randomDataOnCanvasExtract, true);
  let popupShown = promisePopupShown();
  await SpecialPowers.spawn(browser, [], function (host) {
    E10SUtils.wrapHandlingUserInput(content, true, function () {
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
  await SpecialPowers.spawn(
    browser,
    [randomDataOnCanvasExtract, grantPermission],
    extractCanvasDataUserInput
  );
  await SpecialPowers.popPrefEnv();
}

async function doTestInput(
  randomDataOnCanvasExtract,
  grantPermission,
  autoDeclineNoInput
) {
  await BrowserTestUtils.withNewTab(
    kUrl,
    withNewTabInput.bind(null, randomDataOnCanvasExtract, grantPermission)
  );
  Services.perms.removeFromPrincipal(kPrincipal, kPermission);
}

// With auto-declining enabled (the default)
// Tests clicking "Don't Allow" button of the permission prompt.
add_task(doTestInput.bind(null, true, false));
add_task(doTestInput.bind(null, false, false));

// Tests clicking "Allow" button of the permission prompt.
add_task(doTestInput.bind(null, true, true));
add_task(doTestInput.bind(null, false, true));
