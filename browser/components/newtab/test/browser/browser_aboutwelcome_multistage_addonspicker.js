"use strict";

const { AboutWelcomeParent } = ChromeUtils.importESModule(
  "resource:///actors/AboutWelcomeParent.sys.mjs"
);

const { AboutWelcomeTelemetry } = ChromeUtils.import(
  "resource://activity-stream/aboutwelcome/lib/AboutWelcomeTelemetry.jsm"
);
const { AWScreenUtils } = ChromeUtils.import(
  "resource://activity-stream/lib/AWScreenUtils.jsm"
);
const { InternalTestingProfileMigrator } = ChromeUtils.importESModule(
  "resource:///modules/InternalTestingProfileMigrator.sys.mjs"
);

async function clickVisibleButton(browser, selector) {
  // eslint-disable-next-line no-shadow
  await ContentTask.spawn(browser, { selector }, async ({ selector }) => {
    function getVisibleElement() {
      for (const el of content.document.querySelectorAll(selector)) {
        if (el.offsetParent !== null) {
          return el;
        }
      }
      return null;
    }
    await ContentTaskUtils.waitForCondition(
      getVisibleElement,
      selector,
      200, // interval
      100 // maxTries
    );
    getVisibleElement().click();
  });
}

add_setup(async function () {
  SpecialPowers.pushPrefEnv({
    set: [
      ["ui.prefersReducedMotion", 1],
      ["browser.aboutwelcome.transitions", false],
    ],
  });
});

function initSandbox({ pin = true, isDefault = false } = {}) {
  const sandbox = sinon.createSandbox();
  sandbox.stub(AboutWelcomeParent, "doesAppNeedPin").returns(pin);
  sandbox.stub(AboutWelcomeParent, "isDefaultBrowser").returns(isDefault);

  return sandbox;
}

add_task(async function test_aboutwelcome_addonspicker() {
  const TEST_ADDON_CONTENT = [
    {
      id: "AW_ADDONS_PICKER",
      content: {
        position: "center",
        tiles: {
          type: "addons-picker",
          data: [
            {
              id: "addon-one-id",
              name: "uBlock Origin",
              install_label: "Add to Firefox",
              icon: "",
              type: "extension",
              description: "An efficient wide-spectrum content blocker.",
              source_id: "ADD_EXTENSION_BUTTON",
              action: {
                type: "INSTALL_ADDON_FROM_URL",
                data: {
                  url: "https://test.xpi",
                  telemetrySource: "aboutwelcome-addon",
                },
              },
            },
            {
              id: "addon-two-id",
              name: "Tree-Style Tabs",
              install_label: "Add to Firefox",
              icon: "",
              type: "extension",
              description: "Show tabs like a tree.",
              source_id: "ADD_EXTENSION_BUTTON",
              action: {
                type: "INSTALL_ADDON_FROM_URL",
                data: {
                  url: "https://test.xpi",
                  telemetrySource: "aboutwelcome-addon",
                },
              },
            },
          ],
        },
        progress_bar: true,
        logo: {},
        title: {
          raw: "Customize your Firefox",
        },
        subtitle: {
          raw: "Extensions and themes are like apps for your browser, and they let you protect passwords, download videos, find deals, block annoying ads, change how your browser looks, and much more.",
        },
        additional_button: {
          label: {
            raw: "Explore more add-ons",
          },
          style: "link",
          action: {
            type: "OPEN_URL",
            data: {
              args: "https://test.xpi",
              where: "tab",
            },
          },
        },
        secondary_button: {
          label: {
            string_id: "mr2-onboarding-start-browsing-button-label",
          },
          action: {
            navigate: true,
          },
        },
      },
    },
  ];

  await setAboutWelcomeMultiStage(JSON.stringify(TEST_ADDON_CONTENT)); // NB: calls SpecialPowers.pushPrefEnv
  let { cleanup, browser } = await openMRAboutWelcome();
  let aboutWelcomeActor = await getAboutWelcomeParent(browser);
  const messageSandbox = sinon.createSandbox();
  registerCleanupFunction(() => {
    messageSandbox.restore();
  });
  // Stub AboutWelcomeParent's Content Message Handler
  const messageStub = messageSandbox
    .stub(aboutWelcomeActor, "onContentMessage")
    .withArgs("AWPage:SPECIAL_ACTION");

  // execution
  await test_screen_content(
    browser,
    "renders the addons-picker screen and tiles",
    //Expected selectors
    [
      "main.AW_ADDONS_PICKER",
      "div.addons-picker-container",
      "button[value='secondary_button']",
      "button[value='additional_button']",
    ],

    //Unexpected selectors:
    [
      `main.screen[pos="split"]`,
      "main.AW_SET_DEFAULT",
      "button[value='primary_button']",
    ]
  );

  await clickVisibleButton(browser, ".addon-container button[value='0']"); //click the first install button

  const installExtensionCall = messageStub.getCall(0);
  info(
    `Call #${installExtensionCall}: ${
      installExtensionCall.args[0]
    } ${JSON.stringify(installExtensionCall.args[1])}`
  );
  Assert.equal(
    installExtensionCall.args[0],
    "AWPage:SPECIAL_ACTION",
    "send special action to install add on"
  );
  Assert.equal(
    installExtensionCall.args[1].type,
    "INSTALL_ADDON_FROM_URL",
    "Special action type is INSTALL_ADDON_FROM_URL"
  );

  // cleanup
  await SpecialPowers.popPrefEnv(); // for setAboutWelcomeMultiStage
  await cleanup();
  messageSandbox.restore();
});
