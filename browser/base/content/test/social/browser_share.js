
let SocialService = Cu.import("resource://gre/modules/SocialService.jsm", {}).SocialService;

let baseURL = "https://example.com/browser/browser/base/content/test/social/";

let manifest = { // normal provider
  name: "provider 1",
  origin: "https://example.com",
  workerURL: "https://example.com/browser/browser/base/content/test/social/social_worker.js",
  iconURL: "https://example.com/browser/browser/base/content/test/general/moz.png",
  shareURL: "https://example.com/browser/browser/base/content/test/social/share.html"
};
let activationPage = "https://example.com/browser/browser/base/content/test/social/share_activate.html";

function waitForProviderEnabled(cb) {
  Services.obs.addObserver(function providerSet(subject, topic, data) {
    Services.obs.removeObserver(providerSet, "social:provider-enabled");
    info("social:provider-enabled observer was notified");
    cb();
  }, "social:provider-enabled", false);
}

function sendActivationEvent(callback) {
  // hack Social.lastEventReceived so we don't hit the "too many events" check.
  Social.lastEventReceived = 0;
  let doc = SocialShare.iframe.contentDocument;
  // if our test has a frame, use it
  let button = doc.getElementById("activation");
  ok(!!button, "got the activation button");
  EventUtils.synthesizeMouseAtCenter(button, {}, doc.defaultView);
  if (callback)
    executeSoon(callback);
}

function waitForEvent(iframe, eventName, callback) {
  iframe.addEventListener(eventName, function load() {
    info("page load is "+iframe.contentDocument.location.href);
    if (iframe.contentDocument.location.href != "data:text/plain;charset=utf8,") {
      iframe.removeEventListener(eventName, load, true);
      executeSoon(callback);
    }
  }, true);
}

function test() {
  waitForExplicitFinish();
  Services.prefs.setCharPref("social.shareDirectory", activationPage);
  registerCleanupFunction(function () {
    Services.prefs.clearUserPref("social.directories");
    Services.prefs.clearUserPref("social.shareDirectory");
    Services.prefs.clearUserPref("social.share.activationPanelEnabled");
  });
  runSocialTests(tests);
}

let corpus = [
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

function loadURLInTab(url, callback) {
  info("Loading tab with "+url);
  let tab = gBrowser.selectedTab = gBrowser.addTab(url);
  waitForEvent(tab.linkedBrowser, "load", () => {
    is(tab.linkedBrowser.currentURI.spec, url, "tab loaded")
    callback(tab)
  });
}

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
    is(gBrowser.contentDocument.location.href, "about:blank");
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
    let testData = corpus[0];
    loadURLInTab(testData.url, function(tab) {
      SocialService.addProvider(manifest, function(provider) {
        is(SocialUI.enabled, true, "SocialUI is enabled");
        checkSocialUI();
        // share should not be enabled since we only have about:blank page
        let shareButton = SocialShare.shareButton;
        // verify the attribute for proper css
        ok(!shareButton.hasAttribute("disabled"), "share button is enabled");
        // button should be visible
        is(shareButton.hidden, false, "share button is visible");
        gBrowser.removeTab(tab);
        next();
      });
    });
  },
  testSharePage: function(next) {
    let provider = Social._getProviderFromOrigin(manifest.origin);
    let port = provider.getWorkerPort();
    ok(port, "provider has a port");
    let testTab;
    let testIndex = 0;
    let testData = corpus[testIndex++];

    function runOneTest() {
      loadURLInTab(testData.url, function(tab) {
        testTab = tab;
        SocialShare.sharePage(manifest.origin);
      });
    }

    port.onmessage = function (e) {
      let topic = e.data.topic;
      switch (topic) {
        case "got-share-data-message":
          gBrowser.removeTab(testTab);
          hasoptions(testData.options, e.data.result);
          testData = corpus[testIndex++];
          if (testData) {
            executeSoon(runOneTest);
          } else {
            SocialService.disableProvider(manifest.origin, next);
          }
          break;
      }
    }
    port.postMessage({topic: "test-init"});
    executeSoon(runOneTest);
  },
  testShareMicrodata: function(next) {
    SocialService.addProvider(manifest, function(provider) {
      let port = provider.getWorkerPort();
      let target, testTab;

      let expecting = JSON.stringify({
        "url": "https://example.com/browser/browser/base/content/test/social/microdata.html",
        "title": "My Blog",
        "previews": [],
        "microdata": {
          "items": [{
              "types": ["http://schema.org/BlogPosting"],
              "properties": {
                "headline": ["Progress report"],
                "datePublished": ["2013-08-29"],
                "url": ["https://example.com/browser/browser/base/content/test/social/microdata.html?comments=0"],
                "comment": [{
                    "types": ["http://schema.org/UserComments"],
                    "properties": {
                      "url": ["https://example.com/browser/browser/base/content/test/social/microdata.html#c1"],
                      "creator": [{
                          "types": ["http://schema.org/Person"],
                          "properties": {
                            "name": ["Greg"]
                          }
                        }
                      ],
                      "commentTime": ["2013-08-29"]
                    }
                  }, {
                    "types": ["http://schema.org/UserComments"],
                    "properties": {
                      "url": ["https://example.com/browser/browser/base/content/test/social/microdata.html#c2"],
                      "creator": [{
                          "types": ["http://schema.org/Person"],
                          "properties": {
                            "name": ["Charlotte"]
                          }
                        }
                      ],
                      "commentTime": ["2013-08-29"]
                    }
                  }
                ]
              }
            }
          ]
        }
      });

      port.onmessage = function (e) {
        let topic = e.data.topic;
        switch (topic) {
          case "got-share-data-message":
            is(JSON.stringify(e.data.result), expecting, "microdata data ok");
            gBrowser.removeTab(testTab);
            SocialService.disableProvider(manifest.origin, next);
            break;
        }
      }
      port.postMessage({topic: "test-init"});
  
      let url = "https://example.com/browser/browser/base/content/test/social/microdata.html"
      addTab(url, function(tab) {
        testTab = tab;
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

    waitForEvent(iframe, "load", () => {
      waitForCondition(() => {
          // sometimes the iframe is ready before the panel is open, we need to
          // wait for both conditions
          return SocialShare.panel.state == "open";
        }, () => {
        is(iframe.contentDocument.location.href, activationPage, "activation page loaded");
        waitForProviderEnabled(() => {
          let provider = Social._getProviderFromOrigin(manifest.origin);
          let port = provider.getWorkerPort();
          ok(!!port, "got port");
          port.onmessage = function (e) {
            let topic = e.data.topic;
            info("got topic "+topic+"\n");
            switch (topic) {
              case "got-share-data-message":
                ok(true, "share completed");
                gBrowser.removeTab(testTab);
                SocialService.uninstallProvider(manifest.origin, next);
                break;
            }
          }
          port.postMessage({topic: "test-init"});
        });
        sendActivationEvent();
      }, "share panel did not open and load share page");
    });
    loadURLInTab(activationPage, function(tab) {
      testTab = tab;
      SocialShare.sharePage();
    });
  }
}
