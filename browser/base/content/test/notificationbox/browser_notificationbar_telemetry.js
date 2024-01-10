const TELEMETRY_BASE = "notificationbar.";

ChromeUtils.defineESModuleGetters(this, {
  TelemetryTestUtils: "resource://testing-common/TelemetryTestUtils.sys.mjs",
});

add_task(async function showNotification() {
  Services.telemetry.clearScalars();

  let tab1 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/"
  );

  ok(!gBrowser.readNotificationBox(), "no notificationbox created yet");

  let box1 = gBrowser.getNotificationBox();

  ok(gBrowser.readNotificationBox(), "notificationbox was created");

  let tab2 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.org/"
  );

  let tab3 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "data:text/html,<body>Hello</body>"
  );
  let box3 = gBrowser.getNotificationBox();

  verifyTelemetry("initial", 0, 0, 0, 0, 0, 0);

  let notif3 = await box3.appendNotification("infobar-testtwo-value", {
    label: "Message for tab 3",
    priority: box3.PRIORITY_INFO_HIGH,
    telemetry: TELEMETRY_BASE + "testtwo",
  });
  await notif3.updateComplete;

  verifyTelemetry("first notification", 0, 0, 0, 0, 0, 1);

  let notif1 = await box1.appendNotification(
    "infobar-testone-value",
    {
      label: "Message for tab 1",
      priority: box1.PRIORITY_INFO_HIGH,
      telemetry: TELEMETRY_BASE + "testone",
    },
    [
      {
        label: "Button1",
        telemetry: "button1-pressed",
      },
      {
        label: "Button2",
        telemetry: "button2-pressed",
      },
      {
        label: "Button3",
      },
    ]
  );
  await notif1.updateComplete;
  verifyTelemetry("second notification", 0, 0, 0, 0, 0, 1);

  await BrowserTestUtils.switchTab(gBrowser, tab1);
  verifyTelemetry("switch to first tab", 1, 0, 0, 0, 0, 1);

  await BrowserTestUtils.switchTab(gBrowser, tab2);
  verifyTelemetry("switch to second tab", 1, 0, 0, 0, 0, 1);

  await BrowserTestUtils.switchTab(gBrowser, tab3);
  verifyTelemetry("switch to third tab", 1, 0, 0, 0, 0, 1);

  await BrowserTestUtils.switchTab(gBrowser, tab1);
  verifyTelemetry("switch to first tab again", 1, 0, 0, 0, 0, 1);

  notif1.buttonContainer.lastElementChild.click();
  verifyTelemetry("press third button", 1, 1, 0, 0, 0, 1);

  notif1.buttonContainer.lastElementChild.previousElementSibling.click();
  verifyTelemetry("press second button", 1, 1, 0, 1, 0, 1);

  notif1.buttonContainer.lastElementChild.previousElementSibling.previousElementSibling.click();
  verifyTelemetry("press first button", 1, 1, 1, 1, 0, 1);

  notif1.dismiss();
  verifyTelemetry("dismiss notification for box 1", 1, 1, 1, 1, 1, 1);

  notif3.dismiss();
  verifyTelemetry("dismiss notification for box 3", 1, 1, 1, 1, 1, 1, 1);

  let notif4 = await box1.appendNotification(
    "infobar-testtwo-value",
    {
      label: "Additional message for tab 1",
      priority: box1.PRIORITY_INFO_HIGH,
      telemetry: TELEMETRY_BASE + "testone",
      telemetryFilter: ["shown"],
    },
    [
      {
        label: "Button1",
      },
    ]
  );
  await notif4.updateComplete;
  verifyTelemetry("show first filtered notification", 2, 1, 1, 1, 1, 1, 1);

  notif4.buttonContainer.lastElementChild.click();
  notif4.dismiss();
  verifyTelemetry("dismiss first filtered notification", 2, 1, 1, 1, 1, 1, 1);

  let notif5 = await box1.appendNotification(
    "infobar-testtwo-value",
    {
      label: "Dimissed additional message for tab 1",
      priority: box1.PRIORITY_INFO_HIGH,
      telemetry: TELEMETRY_BASE + "testone",
      telemetryFilter: ["dismissed"],
    },
    [
      {
        label: "Button1",
      },
    ]
  );
  await notif5.updateComplete;
  verifyTelemetry("show second filtered notification", 2, 1, 1, 1, 1, 1, 1);

  notif5.buttonContainer.lastElementChild.click();
  notif5.dismiss();
  verifyTelemetry("dismiss second filtered notification", 2, 1, 1, 1, 2, 1, 1);

  let notif6 = await box1.appendNotification(
    "infobar-testtwo-value",
    {
      label: "Dimissed additional message for tab 1",
      priority: box1.PRIORITY_INFO_HIGH,
      telemetry: TELEMETRY_BASE + "testone",
      telemetryFilter: ["button1-pressed", "dismissed"],
    },
    [
      {
        label: "Button1",
        telemetry: "button1-pressed",
      },
    ]
  );
  await notif6.updateComplete;
  verifyTelemetry("show third filtered notification", 2, 1, 1, 1, 2, 1, 1);

  notif6.buttonContainer.lastElementChild.click();
  verifyTelemetry(
    "click button in third filtered notification",
    2,
    1,
    2,
    1,
    2,
    1,
    1
  );
  notif6.dismiss();
  verifyTelemetry("dismiss third filtered notification", 2, 1, 2, 1, 3, 1, 1);

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab3);
});

function verify(scalars, scalar, key, expected, exists) {
  scalar = TELEMETRY_BASE + scalar;

  if (expected > 0) {
    TelemetryTestUtils.assertKeyedScalar(scalars, scalar, key, expected);
    return;
  }

  Assert.equal(
    scalar in scalars,
    exists,
    `expected ${scalar} to be ${exists ? "present" : "unset"}`
  );

  if (exists) {
    Assert.ok(
      !(key in scalars[scalar]),
      "expected key " + key + " to be unset"
    );
  }
}

function verifyTelemetry(
  desc,
  box1shown,
  box1action,
  box1button1,
  box1button2,
  box1dismissed,
  box3shown,
  box3dismissed = 0
) {
  let scalars = TelemetryTestUtils.getProcessScalars("parent", true, false);

  info(desc);
  let n1exists =
    box1shown || box1action || box1button1 || box1button2 || box1dismissed;

  verify(scalars, "testone", "shown", box1shown, n1exists);
  verify(scalars, "testone", "action", box1action, n1exists);
  verify(scalars, "testone", "button1-pressed", box1button1, n1exists);
  verify(scalars, "testone", "button2-pressed", box1button2, n1exists);
  verify(scalars, "testone", "dismissed", box1dismissed, n1exists);
  verify(scalars, "testtwo", "shown", box3shown, box3shown || box3dismissed);
  verify(
    scalars,
    "testtwo",
    "dismissed",
    box3dismissed,
    box3shown || box3dismissed
  );
}
