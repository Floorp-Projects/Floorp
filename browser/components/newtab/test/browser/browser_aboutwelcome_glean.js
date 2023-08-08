/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests for the Glean version of onboarding telemetry.
 */

const { AboutWelcomeTelemetry } = ChromeUtils.import(
  "resource://activity-stream/aboutwelcome/lib/AboutWelcomeTelemetry.jsm"
);

const TEST_DEFAULT_CONTENT = [
  {
    id: "AW_STEP1",

    content: {
      position: "split",
      title: "Step 1",
      page: "page 1",
      source: "test",
      primary_button: {
        label: "Next",
        action: {
          navigate: true,
        },
      },
      secondary_button: {
        label: "link",
      },
      secondary_button_top: {
        label: "link top",
        action: {
          type: "SHOW_FIREFOX_ACCOUNTS",
          data: { entrypoint: "test" },
        },
      },
    },
  },
  {
    id: "AW_STEP2",
    content: {
      position: "center",
      title: "Step 2",
      page: "page 1",
      source: "test",
      primary_button: {
        label: "Next",
        action: {
          navigate: true,
        },
      },
      secondary_button: {
        label: "link",
      },
      has_noodles: true,
    },
  },
];

const TEST_DEFAULT_JSON = JSON.stringify(TEST_DEFAULT_CONTENT);

async function openAboutWelcome() {
  await setAboutWelcomePref(true);
  await setAboutWelcomeMultiStage(TEST_DEFAULT_JSON);

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:welcome",
    true
  );
  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(tab);
  });
  return tab.linkedBrowser;
}

add_task(async function test_welcome_telemetry() {
  const sandbox = sinon.createSandbox();
  // Be sure to stub out PingCentre so it doesn't hit the network.
  sandbox
    .stub(AboutWelcomeTelemetry.prototype, "pingCentre")
    .value({ sendStructuredIngestionPing: () => {} });

  // Have to turn on AS telemetry for anything to be recorded.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.newtabpage.activity-stream.telemetry", true]],
  });
  registerCleanupFunction(async () => {
    await SpecialPowers.popPrefEnv();
  });

  Services.fog.testResetFOG();
  // Let's check that there is nothing in the impression event.
  // This is useful in mochitests because glean inits fairly late in startup.
  // We want to make sure we are fully initialized during testing so that
  // when we call testGetValue() we get predictable behavior.
  Assert.equal(undefined, Glean.messagingSystem.messageId.testGetValue());

  // Setup testBeforeNextSubmit. We do this first, progress onboarding, submit
  // and then check submission. We put the asserts inside testBeforeNextSubmit
  // because metric lifetimes are 'ping' and are cleared after submission.
  // See: https://firefox-source-docs.mozilla.org/toolkit/components/glean/user/instrumentation_tests.html#xpcshell-tests
  let pingSubmitted = false;
  GleanPings.messagingSystem.testBeforeNextSubmit(() => {
    pingSubmitted = true;

    const message = Glean.messagingSystem.messageId.testGetValue();
    // Because of the asynchronous nature of receiving messages, we cannot
    // guarantee that we will get the same message first. Instead we check
    // that the one we get is a valid example of that type.
    Assert.ok(
      message.startsWith("MR_WELCOME_DEFAULT"),
      "Ping is of an expected type"
    );
    Assert.equal(
      Glean.messagingSystem.unknownKeyCount.testGetValue(),
      undefined
    );
  });

  let browser = await openAboutWelcome();
  // `openAboutWelcome` isn't synchronous wrt the onboarding flow impressing.
  await TestUtils.waitForCondition(
    () => pingSubmitted,
    "Ping was submitted, callback was called."
  );
  registerCleanupFunction(() => {
    sandbox.restore();
  });

  // Let's reset and assert some values in the next button click.
  pingSubmitted = false;
  GleanPings.messagingSystem.testBeforeNextSubmit(() => {
    pingSubmitted = true;

    // Sometimes the impression for MR_WELCOME_DEFAULT_0_AW_STEP1_SS reaches
    // the parent process before the button click does.
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=1834620
    if (Glean.messagingSystem.event.testGetValue() === "IMPRESSION") {
      Assert.equal(
        Glean.messagingSystem.eventPage.testGetValue(),
        "about:welcome"
      );
      const message = Glean.messagingSystem.messageId.testGetValue();
      Assert.ok(
        message.startsWith("MR_WELCOME_DEFAULT"),
        "Ping is of an expected type"
      );
    } else {
      // This is the common and, to my mind, correct case:
      // the click coming before the next steps' impression.
      Assert.equal(Glean.messagingSystem.event.testGetValue(), "CLICK_BUTTON");
      Assert.equal(
        Glean.messagingSystem.eventSource.testGetValue(),
        "primary_button"
      );
      Assert.equal(
        Glean.messagingSystem.messageId.testGetValue(),
        "MR_WELCOME_DEFAULT_0_AW_STEP1"
      );
    }
    Assert.equal(
      Glean.messagingSystem.unknownKeyCount.testGetValue(),
      undefined
    );
  });
  await onButtonClick(browser, "button.primary");
  Assert.ok(pingSubmitted, "Ping was submitted, callback was called.");
});
