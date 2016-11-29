/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const TEST_URL_PATH = "/browser/browser/components/originattributes/test/browser/";

// The flags of test modes.
const TEST_MODE_FIRSTPARTY   = 0;
const TEST_MODE_NO_ISOLATION = 1;
const TEST_MODE_CONTAINERS   = 2;

// The name of each mode.
const TEST_MODE_NAMES = [ "first party isolation",
                          "no isolation",
                          "containers" ];

// The frame types.
const TEST_TYPE_FRAME  = 0;
const TEST_TYPE_IFRAME = 1;

// The default frame setting.
const DEFAULT_FRAME_SETTING = [ TEST_TYPE_IFRAME ];

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
function* openTabInUserContext(aURL, aUserContextId) {
  // Open the tab in the correct userContextId.
  let tab = gBrowser.addTab(aURL, {userContextId: aUserContextId});

  // Select tab and make sure its browser is focused.
  gBrowser.selectedTab = tab;
  tab.ownerGlobal.focus();

  let browser = gBrowser.getBrowserForTab(tab);
  yield BrowserTestUtils.browserLoaded(browser);
  return {tab, browser};
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
function* openTabInFirstParty(aURL, aFirstPartyDomain,
                              aFrameSetting = DEFAULT_FRAME_SETTING) {

  // If the first party domain ends with '/', we remove it.
  if (aFirstPartyDomain.endsWith('/')) {
    aFirstPartyDomain = aFirstPartyDomain.slice(0, -1);
  }

  let basicPageURL = aFirstPartyDomain + gFirstPartyBasicPage;

  // Open the tab for the basic first party page.
  let tab = gBrowser.addTab(basicPageURL);

  // Select tab and make sure its browser is focused.
  gBrowser.selectedTab = tab;
  tab.ownerGlobal.focus();

  let browser = gBrowser.getBrowserForTab(tab);
  yield BrowserTestUtils.browserLoaded(browser);

  let pageArgs = { url: aURL,
                   frames: aFrameSetting,
                   typeFrame: TEST_TYPE_FRAME,
                   typeIFrame: TEST_TYPE_IFRAME,
                   basicFrameSrc: basicPageURL};

  // Create the frame structure.
  yield ContentTask.spawn(browser, pageArgs, function* (arg) {
    let typeFrame = arg.typeFrame;
    let typeIFrame = arg.typeIFrame;

    // Redefine the 'content' for allowing us to change its target, and making
    // ContentTask.spawn can directly work on the frame element.
    this.frameWindow = content;

    Object.defineProperty(this, "content", {
      get: () => this.frameWindow
    });

    let frameElement;
    let numOfLayers = 0;

    for (let type of arg.frames) {
      let document = content.document;
      numOfLayers++;

      if (type === typeFrame) {
        // Add a frameset which carries the frame element.
        let frameSet = document.createElement('frameset');
        frameSet.cols = "50%,50%";

        let frame = document.createElement('frame');
        let dummyFrame = document.createElement('frame');

        frameSet.appendChild(frame);
        frameSet.appendChild(dummyFrame);

        document.body.appendChild(frameSet);

        frameElement = frame;
      } else if (type === typeIFrame) {
        // Add an iframe.
        let iframe = document.createElement('iframe');
        document.body.appendChild(iframe);

        frameElement = iframe;
      } else {
        ok(false, "Invalid frame type.");
        break;
      }

      // Wait for the frame to be loaded.
      yield new Promise(done => {
        frameElement.addEventListener("load", function loadEnd() {
          frameElement.removeEventListener("load", loadEnd, true);
          done();
        }, true);

        // If it is the deepest layer, we load the target URL. Otherwise, we
        // load a basic page.
        if (numOfLayers === arg.frames.length) {
          frameElement.setAttribute("src", arg.url);
        } else {
          frameElement.setAttribute("src", arg.basicFrameSrc);
        }
      });

      // Redirect the 'content' to the frame's window.
      this.frameWindow = frameElement.contentWindow;
    }
  });

  return {tab, browser};
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
    add_task(function* addTaskForIsolationTests() {
      let testSettings = [
        { mode: TEST_MODE_FIRSTPARTY,
          skip: false,
          prefs: [["privacy.firstparty.isolate", true]]
        },
        { mode: TEST_MODE_NO_ISOLATION,
          skip: false,
          prefs: [["privacy.firstparty.isolate", false]]
        },
        { mode: TEST_MODE_CONTAINERS,
          skip: false,
          prefs: [["privacy.userContext.enabled", true]]
        },
      ];

      // Add test tasks.
      for (let testSetting of testSettings) {
        IsolationTestTools._addTaskForMode(testSetting.mode,
                                           testSetting.prefs,
                                           testSetting.skip,
                                           aTask);
      }
    });
  },

  _addTaskForMode(aMode, aPref, aSkip, aTask) {
    if (aSkip) {
      return;
    }

    add_task(function* () {
      info("Starting the test for " + TEST_MODE_NAMES[aMode]);

      // Before run this task, reset the preferences first.
      yield SpecialPowers.flushPrefEnv();

      // Make sure preferences are set properly.
      yield SpecialPowers.pushPrefEnv({"set": aPref});

      yield SpecialPowers.pushPrefEnv({"set": [["dom.ipc.processCount", 1]]});

      yield aTask(aMode);
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

    return openTabInFirstParty(aURL, aTabSettingObj.firstPartyDomain,
                                aFrameSetting);

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
  runTests(aURL, aGetResultFuncs, aCompareResultFunc, aBeforeFunc,
           aGetResultImmediately) {
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

    let tabSettings = [
                        { firstPartyDomain: "http://example.com", userContextId: 1},
                        { firstPartyDomain: "http://example.org", userContextId: 2}
                      ];

    this._add_task(function* (aMode) {
      let tabSettingA = 0;

      for (let tabSettingB of [0, 1]) {
        // Give the test a chance to set up before each case is run.
        if (aBeforeFunc) {
          yield aBeforeFunc(aMode);
        }

        // Create Tabs.
        let tabInfoA = yield IsolationTestTools._addTab(aMode,
                                                        pageURL,
                                                        tabSettings[tabSettingA],
                                                        firstFrameSetting);
        let resultsA = [];
        if (aGetResultImmediately) {
          for (let getResultFunc of aGetResultFuncs) {
            resultsA.push(yield getResultFunc(tabInfoA.browser));
          }
        }
        let tabInfoB = yield IsolationTestTools._addTab(aMode,
                                                        pageURL,
                                                        tabSettings[tabSettingB],
                                                        secondFrameSetting);
        let i = 0;
        for (let getResultFunc of aGetResultFuncs) {
          // Fetch results from tabs.
          let resultA = aGetResultImmediately ? resultsA[i++] :
                        yield getResultFunc(tabInfoA.browser);
          let resultB = yield getResultFunc(tabInfoB.browser);

          // Compare results.
          let result = false;
          let shouldIsolate = (aMode !== TEST_MODE_NO_ISOLATION) &&
                              tabSettingA !== tabSettingB;
          if (aCompareResultFunc) {
            result = yield aCompareResultFunc(shouldIsolate, resultA, resultB);
          } else {
            result = shouldIsolate ? resultA !== resultB :
                                     resultA === resultB;
          }

          let msg = `Testing ${TEST_MODE_NAMES[aMode]} for ` +
            `isolation ${shouldIsolate ? "on" : "off"} with TabSettingA ` +
            `${tabSettingA} and tabSettingB ${tabSettingB}` +
            `, resultA = ${resultA}, resultB = ${resultB}`;

          ok(result, msg);
        }

        // Close Tabs.
        yield BrowserTestUtils.removeTab(tabInfoA.tab);
        yield BrowserTestUtils.removeTab(tabInfoB.tab);
      }
    });
  }
};
