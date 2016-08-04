
var SocialService = Cu.import("resource:///modules/SocialService.jsm", {}).SocialService;

var baseURL = "https://example.com/browser/browser/base/content/test/social/";

var manifest = { // normal provider
  name: "provider 1",
  origin: "https://example.com",
  iconURL: "https://example.com/browser/browser/base/content/test/general/moz.png",
  shareURL: "https://example.com/browser/browser/base/content/test/social/share.html"
};
var activationPage = "https://example.com/browser/browser/base/content/test/social/share_activate.html";

function sendActivationEvent(subframe) {
  // hack Social.lastEventReceived so we don't hit the "too many events" check.
  Social.lastEventReceived = 0;
  let doc = subframe.contentDocument;
  // if our test has a frame, use it
  let button = doc.getElementById("activation");
  ok(!!button, "got the activation button");
  EventUtils.synthesizeMouseAtCenter(button, {}, doc.defaultView);
}

function test() {
  waitForExplicitFinish();
  Services.prefs.setCharPref("social.shareDirectory", activationPage);

  let frameScript = "data:,(" + function frame_script() {
    addEventListener("OpenGraphData", function (aEvent) {
      sendAsyncMessage("sharedata", aEvent.detail);
    }, true, true);
    /* bug 1042991, ensure history is available by calling history.back on close */
    addMessageListener("closeself", function(e) {
      content.history.back();
      content.close();
    }, true);
    /* if text is entered into field, onbeforeunload will cause a modal dialog
       unless dialogs have been disabled for the iframe. */
    content.onbeforeunload = function(e) {
      return 'FAIL.';
    };
  }.toString() + ")();";
  let mm = getGroupMessageManager("social");
  mm.loadFrameScript(frameScript, true);

  // Animation on the panel can cause intermittent failures such as bug 1115131.
  SocialShare.panel.setAttribute("animate", "false");
  registerCleanupFunction(function () {
    SocialShare.panel.removeAttribute("animate");
    mm.removeDelayedFrameScript(frameScript);
    Services.prefs.clearUserPref("social.directories");
    Services.prefs.clearUserPref("social.shareDirectory");
    Services.prefs.clearUserPref("social.share.activationPanelEnabled");
  });
  runSocialTests(tests, undefined, function(next) {
    let shareButton = SocialShare.shareButton;
    if (shareButton) {
      CustomizableUI.removeWidgetFromArea("social-share-button", CustomizableUI.AREA_NAVBAR)
      shareButton.remove();
    }
    next();
  });
}

var corpus = [
  {
    url: baseURL+"opengraph/opengraph.html",
    options: {
      // og:title
      title: ">This is my title<",
      // og:description
      description: "A test corpus file for open graph tags we care about",
      //medium: this.getPageMedium(),
      //source: this.getSourceURL(),
      // og:url
      url: "https://www.mozilla.org/",
      //shortUrl: this.getShortURL(),
      // og:image
      previews:["https://www.mozilla.org/favicon.png"],
      // og:site_name
      siteName: ">My simple test page<"
    }
  },
  {
    // tests that og:url doesn't override the page url if it is bad
    url: baseURL+"opengraph/og_invalid_url.html",
    options: {
      description: "A test corpus file for open graph tags passing a bad url",
      url: baseURL+"opengraph/og_invalid_url.html",
      previews: [],
      siteName: "Evil chrome delivering website"
    }
  },
  {
    url: baseURL+"opengraph/shorturl_link.html",
    options: {
      previews: ["http://example.com/1234/56789.jpg"],
      url: "http://www.example.com/photos/56789/",
      shortUrl: "http://imshort/p/abcde"
    }
  },
  {
    url: baseURL+"opengraph/shorturl_linkrel.html",
    options: {
      previews: ["http://example.com/1234/56789.jpg"],
      url: "http://www.example.com/photos/56789/",
      shortUrl: "http://imshort/p/abcde"
    }
  },
  {
    url: baseURL+"opengraph/shortlink_linkrel.html",
    options: {
      previews: ["http://example.com/1234/56789.jpg"],
      url: "http://www.example.com/photos/56789/",
      shortUrl: "http://imshort/p/abcde"
    }
  }
];

function hasoptions(testOptions, options) {
  let msg;
  for (let option in testOptions) {
    let data = testOptions[option];
    info("data: "+JSON.stringify(data));
    let message_data = options[option];
    info("message_data: "+JSON.stringify(message_data));
    if (Array.isArray(data)) {
      // the message may have more array elements than we are testing for, this
      // is ok since some of those are hard to test. So we just test that
      // anything in our test data IS in the message.
      ok(Array.every(data, function(item) { return message_data.indexOf(item) >= 0 }), "option "+option);
    } else {
      is(message_data, data, "option "+option);
    }
  }
}

var tests = {
  testShareDisabledOnActivation: function(next) {
    // starting on about:blank page, share should be visible but disabled when
    // adding provider
    is(gBrowser.currentURI.spec, "about:blank");

    // initialize the button into the navbar
    CustomizableUI.addWidgetToArea("social-share-button", CustomizableUI.AREA_NAVBAR);
    // ensure correct state
    SocialUI.onCustomizeEnd(window);

    SocialService.addProvider(manifest, function(provider) {
      is(SocialUI.enabled, true, "SocialUI is enabled");
      checkSocialUI();
      // share should not be enabled since we only have about:blank page
      let shareButton = SocialShare.shareButton;
      // verify the attribute for proper css
      is(shareButton.getAttribute("disabled"), "true", "share button attribute is disabled");
      // button should be visible
      is(shareButton.hidden, false, "share button is visible");
      SocialService.disableProvider(manifest.origin, next);
    });
  },
  testShareEnabledOnActivation: function(next) {
    // starting from *some* page, share should be visible and enabled when
    // activating provider
    // initialize the button into the navbar
    CustomizableUI.addWidgetToArea("social-share-button", CustomizableUI.AREA_NAVBAR);
    // ensure correct state
    SocialUI.onCustomizeEnd(window);

    let testData = corpus[0];
    BrowserTestUtils.openNewForegroundTab(gBrowser, testData.url).then(tab => {
      SocialService.addProvider(manifest, function(provider) {
        is(SocialUI.enabled, true, "SocialUI is enabled");
        checkSocialUI();
        // share should not be enabled since we only have about:blank page
        let shareButton = SocialShare.shareButton;
        // verify the attribute for proper css
        ok(!shareButton.hasAttribute("disabled"), "share button is enabled");
        // button should be visible
        is(shareButton.hidden, false, "share button is visible");
        BrowserTestUtils.removeTab(tab).then(next);
      });
    });
  },
  testSharePage: function(next) {
    let provider = Social._getProviderFromOrigin(manifest.origin);

    let testTab;
    let testIndex = 0;
    let testData = corpus[testIndex++];

    // initialize the button into the navbar
    CustomizableUI.addWidgetToArea("social-share-button", CustomizableUI.AREA_NAVBAR);
    // ensure correct state
    SocialUI.onCustomizeEnd(window);

    let mm = getGroupMessageManager("social");
    mm.addMessageListener("sharedata", function handler(msg) {
      BrowserTestUtils.removeTab(testTab).then(() => {
        hasoptions(testData.options, JSON.parse(msg.data));
        testData = corpus[testIndex++];
        BrowserTestUtils.waitForCondition(() => { return SocialShare.currentShare == null; },"share panel closed").then(() => {
          if (testData) {
            runOneTest();
          } else {
            mm.removeMessageListener("sharedata", handler);
            SocialService.disableProvider(manifest.origin, next);
          }
        });
        SocialShare.iframe.messageManager.sendAsyncMessage("closeself", {});
      });
    });

    function runOneTest() {
      BrowserTestUtils.openNewForegroundTab(gBrowser, testData.url).then(tab => {
        testTab = tab;

        let shareButton = SocialShare.shareButton;
        // verify the attribute for proper css
        ok(!shareButton.hasAttribute("disabled"), "share button is enabled");
        // button should be visible
        is(shareButton.hidden, false, "share button is visible");

        SocialShare.sharePage(manifest.origin);
      });
    }
    executeSoon(runOneTest);
  },
  testShareMicroformats: function(next) {
    // initialize the button into the navbar
    CustomizableUI.addWidgetToArea("social-share-button", CustomizableUI.AREA_NAVBAR);
    // ensure correct state
    SocialUI.onCustomizeEnd(window);

    SocialService.addProvider(manifest, function(provider) {
      let target, testTab;

      let expecting = JSON.stringify({
        "url": "https://example.com/browser/browser/base/content/test/social/microformats.html",
        "title": "Raspberry Pi Page",
        "previews": ["https://example.com/someimage.jpg"],
        "microformats": {
          "items": [{
              "type": ["h-product"],
              "properties": {
                "name": ["Raspberry Pi"],
                "photo": ["https://example.com/someimage.jpg"],
                "description": [{
                    "value": "The Raspberry Pi is a credit-card sized computer that plugs into your TV and a keyboard. It's a capable little PC which can be used for many of the things that your desktop PC does, like spreadsheets, word-processing and games. It also plays high-definition video. We want to see it being used by kids all over the world to learn programming.",
                    "html": "The Raspberry Pi is a credit-card sized computer that plugs into your TV and a keyboard. It's a capable little PC which can be used for many of the things that your desktop PC does, like spreadsheets, word-processing and games. It also plays high-definition video. We want to see it being used by kids all over the world to learn programming."
                  }
                ],
                "url": ["https://example.com/"],
                "price": ["29.95"],
                "review": [{
                    "value": "4.5 out of 5",
                    "type": ["h-review"],
                    "properties": {
                      "rating": ["4.5"]
                    }
                  }
                ],
                "category": ["Computer", "Education"]
              }
            }
          ],
          "rels": {
            "tag": ["https://example.com/wiki/computer", "https://example.com/wiki/education"]
          },
          "rel-urls": {
            "https://example.com/wiki/computer": {
              "text": "Computer",
              "rels": ["tag"]
            },
            "https://example.com/wiki/education": {
              "text": "Education",
              "rels": ["tag"]
            }
          }
        }
      });

      let mm = getGroupMessageManager("social");
      mm.addMessageListener("sharedata", function handler(msg) {
        is(msg.data, expecting, "microformats data ok");
        BrowserTestUtils.waitForCondition(() => { return SocialShare.currentShare == null; },
                                          "share panel closed").then(() => {
          mm.removeMessageListener("sharedata", handler);
          BrowserTestUtils.removeTab(testTab).then(() => {
            SocialService.disableProvider(manifest.origin, next);
          });
        });
        SocialShare.iframe.messageManager.sendAsyncMessage("closeself", {});
      });

      let url = "https://example.com/browser/browser/base/content/test/social/microformats.html"
      BrowserTestUtils.openNewForegroundTab(gBrowser, url).then(tab => {
        testTab = tab;

        let shareButton = SocialShare.shareButton;
        // verify the attribute for proper css
        ok(!shareButton.hasAttribute("disabled"), "share button is enabled");
        // button should be visible
        is(shareButton.hidden, false, "share button is visible");

        let doc = tab.linkedBrowser.contentDocument;
        target = doc.getElementById("simple-hcard");
        SocialShare.sharePage(manifest.origin, null, target);
      });
    });
  },
  testSharePanelActivation: function(next) {
    let testTab;
    // cleared in the cleanup function
    Services.prefs.setCharPref("social.directories", "https://example.com");
    Services.prefs.setBoolPref("social.share.activationPanelEnabled", true);
    // make the iframe so we can wait on the load
    SocialShare._createFrame();
    let iframe = SocialShare.iframe;

    // initialize the button into the navbar
    CustomizableUI.addWidgetToArea("social-share-button", CustomizableUI.AREA_NAVBAR);
    // ensure correct state
    SocialUI.onCustomizeEnd(window);

    ensureFrameLoaded(iframe).then(() => {
      let subframe = iframe.contentDocument.getElementById("activation-frame");
      ensureFrameLoaded(subframe, activationPage).then(() => {
        is(subframe.contentDocument.location.href, activationPage, "activation page loaded");
        promiseObserverNotified("social:provider-enabled").then(() => {
          let mm = getGroupMessageManager("social");
          mm.addMessageListener("sharedata", function handler(msg) {
            ok(true, "share completed");

            BrowserTestUtils.waitForCondition(() => { return SocialShare.currentShare == null; },
                                              "share panel closed").then(() => {
              BrowserTestUtils.removeTab(testTab).then(() => {
                mm.removeMessageListener("sharedata", handler);
                SocialService.uninstallProvider(manifest.origin, next);
              });
            });
            SocialShare.iframe.messageManager.sendAsyncMessage("closeself", {});
          });
        });
        sendActivationEvent(subframe);
      });
    });
    BrowserTestUtils.openNewForegroundTab(gBrowser, activationPage).then(tab => {
      let shareButton = SocialShare.shareButton;
      // verify the attribute for proper css
      ok(!shareButton.hasAttribute("disabled"), "share button is enabled");
      // button should be visible
      is(shareButton.hidden, false, "share button is visible");

      testTab = tab;
      SocialShare.sharePage();
    });
  },
  testSharePanelDialog: function(next) {
    let testTab;
    // initialize the button into the navbar
    CustomizableUI.addWidgetToArea("social-share-button", CustomizableUI.AREA_NAVBAR);
    // ensure correct state
    SocialUI.onCustomizeEnd(window);
    SocialShare._createFrame();

    SocialService.addProvider(manifest, () => {
      BrowserTestUtils.openNewForegroundTab(gBrowser, activationPage).then(tab => {
        ensureFrameLoaded(SocialShare.iframe).then(() => {
          // send keys to the input field.  An unexpected failure will happen
          // if the onbeforeunload handler is fired.
          EventUtils.sendKey("f");
          EventUtils.sendKey("a");
          EventUtils.sendKey("i");
          EventUtils.sendKey("l");
  
          SocialShare.panel.addEventListener("popuphidden", function hidden(evt) {
            SocialShare.panel.removeEventListener("popuphidden", hidden);
            let topwin = Services.wm.getMostRecentWindow(null);
            is(topwin, window, "no dialog is open");

            BrowserTestUtils.removeTab(testTab).then(() => {
              SocialService.disableProvider(manifest.origin, next);
            });
          });
          SocialShare.iframe.messageManager.sendAsyncMessage("closeself", {});
        });

        let shareButton = SocialShare.shareButton;
        // verify the attribute for proper css
        ok(!shareButton.hasAttribute("disabled"), "share button is enabled");
        // button should be visible
        is(shareButton.hidden, false, "share button is visible");

        testTab = tab;
        SocialShare.sharePage();
      });
    });
  }
}
