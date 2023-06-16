/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_URL_PATH =
  "/browser/browser/components/originattributes/test/browser/";

// The flags of test modes.
const TEST_MODE_FIRSTPARTY = 0;
const TEST_MODE_NO_ISOLATION = 1;
const TEST_MODE_CONTAINERS = 2;

// The name of each mode.
const TEST_MODE_NAMES = ["first party isolation", "no isolation", "containers"];

// The frame types.
const TEST_TYPE_FRAME = 1;
const TEST_TYPE_IFRAME = 2;

// The default frame setting.
const DEFAULT_FRAME_SETTING = [TEST_TYPE_IFRAME];

let gFirstPartyBasicPage = TEST_URL_PATH + "file_firstPartyBasic.html";

/**
 * Add a tab for the given url with the specific user context id.
 *
 * @param aURL
 *    The url of the page.
 * @param aUserContextId
 *    The user context id for this tab.
 *
 * @return tab     - The tab object of this tab.
 *         browser - The browser object of this tab.
 */
async function openTabInUserContext(aURL, aUserContextId) {
  info(`Start to open tab in specific userContextID: ${aUserContextId}.`);
  let originAttributes = {
    userContextId: aUserContextId,
  };
  info("Create triggeringPrincipal.");
  let triggeringPrincipal =
    Services.scriptSecurityManager.createContentPrincipal(
      makeURI(aURL),
      originAttributes
    );
  // Open the tab in the correct userContextId.
  info("Open the tab and wait for it to be loaded.");
  let tab = BrowserTestUtils.addTab(gBrowser, aURL, {
    userContextId: aUserContextId,
    triggeringPrincipal,
  });

  // Select tab and make sure its browser is focused.
  gBrowser.selectedTab = tab;
  tab.ownerGlobal.focus();

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);
  info("Finished tab opening.");

  return { tab, browser };
}

/**
 * Add a tab for a page with the given first party domain. This page will have
 * an iframe which is loaded with the given url by default or you could specify
 * a frame setting to create nested frames. And this function will also modify
 * the 'content' in the ContentTask to the target frame's window object.
 *
 * @param aURL
 *    The url of the iframe.
 * @param aFirstPartyDomain
 *    The first party domain.
 * @param aFrameSetting
 *    This setting controls how frames are organized within the page. The
 *    setting is an array of frame types, the first item indicates the
 *    frame type (iframe or frame) of the first layer of the frame structure,
 *    and the second item indicates the second layer, and so on. The aURL will
 *    be loaded at the deepest layer. This is optional.
 *
 * @return tab     - The tab object of this tab.
 *         browser - The browser object of this tab.
 */
async function openTabInFirstParty(
  aURL,
  aFirstPartyDomain,
  aFrameSetting = DEFAULT_FRAME_SETTING
) {
  info(`Start to open tab under first party domain "${aFirstPartyDomain}".`);
  // If the first party domain ends with '/', we remove it.
  if (aFirstPartyDomain.endsWith("/")) {
    aFirstPartyDomain = aFirstPartyDomain.slice(0, -1);
  }

  let basicPageURL = aFirstPartyDomain + gFirstPartyBasicPage;

  // Open the tab for the basic first party page.
  info("Open the tab and then wait for it to be loaded.");
  let tab = BrowserTestUtils.addTab(gBrowser, basicPageURL);

  // Select tab and make sure its browser is focused.
  gBrowser.selectedTab = tab;
  tab.ownerGlobal.focus();

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  // Clone the frame setting here since we will modify it later.
  let frameSetting = aFrameSetting.slice(0);
  let frameType;
  let targetBrowsingContext;

  // Create the frame structure.
  info("Create the frame structure.");
  while ((frameType = frameSetting.shift())) {
    if (!targetBrowsingContext) {
      targetBrowsingContext = browser;
    }

    let frameURL = !frameSetting.length ? aURL : basicPageURL;

    if (frameType == TEST_TYPE_FRAME) {
      info("Add the frameset.");
      targetBrowsingContext = await SpecialPowers.spawn(
        targetBrowsingContext,
        [frameURL],
        async function (aFrameURL) {
          // Add a frameset which carries the frame element.
          let frameSet = content.document.createElement("frameset");
          frameSet.cols = "50%,50%";

          let frame = content.document.createElement("frame");
          let dummyFrame = content.document.createElement("frame");

          frameSet.appendChild(frame);
          frameSet.appendChild(dummyFrame);

          content.document.body.appendChild(frameSet);

          // Wait for the frame to be loaded.
          await new Promise(done => {
            frame.addEventListener(
              "load",
              function () {
                done();
              },
              { capture: true, once: true }
            );

            frame.setAttribute("src", aFrameURL);
          });

          return frame.browsingContext;
        }
      );
    } else if (frameType == TEST_TYPE_IFRAME) {
      info("Add the iframe.");
      targetBrowsingContext = await SpecialPowers.spawn(
        targetBrowsingContext,
        [frameURL],
        async function (aFrameURL) {
          // Add an iframe.
          let frame = content.document.createElement("iframe");
          content.document.body.appendChild(frame);

          // Wait for the frame to be loaded.
          await new Promise(done => {
            frame.addEventListener(
              "load",
              function () {
                done();
              },
              { capture: true, once: true }
            );

            frame.setAttribute("src", aFrameURL);
          });

          return frame.browsingContext;
        }
      );
    } else {
      ok(false, "Invalid frame type.");
      break;
    }
    info("Successfully added a frame");
  }
  info("Finished the frame structure");

  return { tab, browser: targetBrowsingContext };
}

this.IsolationTestTools = {
  /**
   * Adds isolation tests for first party isolation, no isolation
   * and containers respectively.
   *
   * @param aTask
   *    The testing task which will be run in different settings.
   */
  _add_task(aTask) {
    let testSettings = [
      {
        mode: TEST_MODE_FIRSTPARTY,
        skip: false,
        prefs: [["privacy.firstparty.isolate", true]],
      },
      {
        mode: TEST_MODE_NO_ISOLATION,
        skip: false,
        prefs: [["privacy.firstparty.isolate", false]],
      },
      {
        mode: TEST_MODE_CONTAINERS,
        skip: false,
        prefs: [["privacy.userContext.enabled", true]],
      },
    ];

    // Add test tasks.
    for (let testSetting of testSettings) {
      IsolationTestTools._addTaskForMode(
        testSetting.mode,
        testSetting.prefs,
        testSetting.skip,
        aTask
      );
    }
  },

  _addTaskForMode(aMode, aPref, aSkip, aTask) {
    if (aSkip) {
      return;
    }

    add_task(async function () {
      info(`Starting the test for ${TEST_MODE_NAMES[aMode]}.`);

      // Before run this task, reset the preferences first.
      await SpecialPowers.flushPrefEnv();

      // Make sure preferences are set properly.
      await SpecialPowers.pushPrefEnv({ set: aPref });

      await SpecialPowers.pushPrefEnv({ set: [["dom.ipc.processCount", 1]] });
      await SpecialPowers.pushPrefEnv({
        set: [
          [
            "network.auth.non-web-content-triggered-resources-http-auth-allow",
            true,
          ],
        ],
      });

      await aTask(aMode);
    });
  },

  /**
   * Add a tab with the given tab setting, this will open different types of
   * tabs according to the given test mode. A tab setting means a isolation
   * target in different test mode; a tab setting indicates a first party
   * domain when testing the first party isolation, it is a user context
   * id when testing containers.
   *
   * @param aMode
   *    The test mode which decides what type of tabs will be opened.
   * @param aURL
   *    The url which is going to open.
   * @param aTabSettingObj
   *    The tab setting object includes 'firstPartyDomain' for the first party
   *    domain and 'userContextId' for Containers.
   * @param aFrameSetting
   *    This setting controls how frames are organized within the page. The
   *    setting is an array of frame types, the first item indicates the
   *    frame type (iframe or frame) of the first layer of the frame structure,
   *    and the second item indicates the second layer, and so on. The aURL
   *    will be loaded at the deepest layer. This is optional.
   *
   * @return tab     - The tab object of this tab.
   *         browser - The browser object of this tab.
   */
  _addTab(aMode, aURL, aTabSettingObj, aFrameSetting) {
    if (aMode === TEST_MODE_CONTAINERS) {
      return openTabInUserContext(aURL, aTabSettingObj.userContextId);
    }

    return openTabInFirstParty(
      aURL,
      aTabSettingObj.firstPartyDomain,
      aFrameSetting
    );
  },

  /**
   * Run isolation tests. The framework will run tests with standard combinations
   * of prefs and tab settings, and checks whether the isolation is working.
   *
   * @param aURL
   *    The URL of the page that will be tested or an object contains 'url',
   *    the tested page, 'firstFrameSetting' for the frame setting of the first
   *    tab, and 'secondFrameSetting' for the second tab.
   * @param aGetResultFuncs
   *    An array of functions or a single function which are responsible for
   *    returning the isolation result back to the framework for further checking.
   *    Each of these functions will be provided the browser object of the tab,
   *    that allows modifying or fetchings results from the page content.
   * @param aCompareResultFunc
   *    An optional function which allows modifying the way how does framework
   *    check results. This function will be provided a boolean to indicate
   *    the isolation is no or off and two results. This function should return
   *    a boolean to tell that whether isolation is working. If this function
   *    is not given, the framework will take case checking by itself.
   * @param aBeforeFunc
   *    An optional function which is called before any tabs are created so
   *    that the test case can set up/reset local state.
   * @param aGetResultImmediately
   *    An optional boolean to ensure we get results before the next tab is opened.
   */
  runTests(
    aURL,
    aGetResultFuncs,
    aCompareResultFunc,
    aBeforeFunc,
    aGetResultImmediately,
    aUseHttps
  ) {
    let pageURL;
    let firstFrameSetting;
    let secondFrameSetting;

    // Request a longer timeout since the test will run a test for three times
    // with different settings. Thus, one test here represents three tests.
    // For this reason, we triple the timeout.
    requestLongerTimeout(3);

    if (typeof aURL === "string") {
      pageURL = aURL;
    } else if (typeof aURL === "object") {
      pageURL = aURL.url;
      firstFrameSetting = aURL.firstFrameSetting;
      secondFrameSetting = aURL.secondFrameSetting;
    }

    if (!Array.isArray(aGetResultFuncs)) {
      aGetResultFuncs = [aGetResultFuncs];
    }

    let tabSettings = aUseHttps
      ? [
          { firstPartyDomain: "https://example.com", userContextId: 1 },
          { firstPartyDomain: "https://example.org", userContextId: 2 },
        ]
      : [
          { firstPartyDomain: "http://example.com", userContextId: 1 },
          { firstPartyDomain: "http://example.org", userContextId: 2 },
        ];

    this._add_task(async function (aMode) {
      let tabSettingA = 0;

      for (let tabSettingB of [0, 1]) {
        // Give the test a chance to set up before each case is run.
        if (aBeforeFunc) {
          try {
            await aBeforeFunc(aMode);
          } catch (e) {
            ok(false, `Caught error while doing testing setup: ${e}.`);
          }
        }
        // Create Tabs.
        info(`Create tab A for ${TEST_MODE_NAMES[aMode]} test.`);
        let tabInfoA = await IsolationTestTools._addTab(
          aMode,
          pageURL,
          tabSettings[tabSettingA],
          firstFrameSetting
        );
        info(`Finished Create tab A for ${TEST_MODE_NAMES[aMode]} test.`);
        let resultsA = [];
        if (aGetResultImmediately) {
          try {
            info(
              `Immediately get result from tab A for ${TEST_MODE_NAMES[aMode]} test`
            );
            for (let getResultFunc of aGetResultFuncs) {
              resultsA.push(await getResultFunc(tabInfoA.browser));
            }
          } catch (e) {
            ok(false, `Caught error while getting result from Tab A: ${e}.`);
          }
        }
        info(`Create tab B for ${TEST_MODE_NAMES[aMode]}.`);
        let tabInfoB = await IsolationTestTools._addTab(
          aMode,
          pageURL,
          tabSettings[tabSettingB],
          secondFrameSetting
        );
        info(`Finished Create tab B for ${TEST_MODE_NAMES[aMode]} test.`);
        let i = 0;
        for (let getResultFunc of aGetResultFuncs) {
          // Fetch results from tabs.
          info(`Fetching result from tab A for ${TEST_MODE_NAMES[aMode]}.`);
          let resultA;
          try {
            resultA = aGetResultImmediately
              ? resultsA[i++]
              : await getResultFunc(tabInfoA.browser);
          } catch (e) {
            ok(false, `Caught error while getting result from Tab A: ${e}.`);
          }
          info(`Fetching result from tab B for ${TEST_MODE_NAMES[aMode]}.`);
          let resultB;
          try {
            resultB = await getResultFunc(tabInfoB.browser);
          } catch (e) {
            ok(false, `Caught error while getting result from Tab B: ${e}.`);
          }
          // Compare results.
          let result = false;
          let shouldIsolate =
            aMode !== TEST_MODE_NO_ISOLATION && tabSettingA !== tabSettingB;
          if (aCompareResultFunc) {
            result = await aCompareResultFunc(shouldIsolate, resultA, resultB);
          } else {
            result = shouldIsolate ? resultA !== resultB : resultA === resultB;
          }

          let msg =
            `Result of Testing ${TEST_MODE_NAMES[aMode]} for ` +
            `isolation ${shouldIsolate ? "on" : "off"} with TabSettingA ` +
            `${tabSettingA} and tabSettingB ${tabSettingB}` +
            `, resultA = ${resultA}, resultB = ${resultB}`;

          ok(result, msg);
        }

        // Close Tabs.
        BrowserTestUtils.removeTab(tabInfoA.tab);
        BrowserTestUtils.removeTab(tabInfoB.tab);

        // A workaround for avoiding a timing issue in Fission. This workaround
        // makes sure that the shutdown process between parent and content
        // is finished before the next round of testing.
        if (SpecialPowers.useRemoteSubframes) {
          await new Promise(resolve => {
            let observer = (subject, topic, data) => {
              if (topic === "ipc:content-shutdown") {
                Services.obs.removeObserver(observer, "ipc:content-shutdown");
                resolve();
              }
            };
            Services.obs.addObserver(observer, "ipc:content-shutdown");
          });
        }
      }
    });
  },
};
