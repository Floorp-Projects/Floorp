/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var SocialService = Cu.import("resource://gre/modules/SocialService.jsm", {}).SocialService;

var manifest = { // used for testing install
  name: "provider test1",
  origin: "https://test1.example.com",
  workerURL: "https://test1.example.com/browser/browser/base/content/test/social/social_worker.js",
  markURL: "https://test1.example.com/browser/browser/base/content/test/social/social_mark.html?url=%{url}",
  markedIcon: "https://test1.example.com/browser/browser/base/content/test/social/unchecked.jpg",
  unmarkedIcon: "https://test1.example.com/browser/browser/base/content/test/social/checked.jpg",

  iconURL: "https://test1.example.com/browser/browser/base/content/test/general/moz.png",
  version: "1.0"
};

function test() {
  waitForExplicitFinish();

  runSocialTestWithProvider(manifest, function (finishcb) {
    runSocialTests(tests, undefined, undefined, function () {
      finishcb();
    });
  });
}

var tests = {
  testMarkMicroformats: function(next) {
    // emulates context menu action using target element, calling SocialMarks.markLink
    let provider = Social._getProviderFromOrigin(manifest.origin);
    let port = provider.getWorkerPort();
    let target, testTab;

    // browser_share tests microformats on the full page, this is testing a
    // specific target element.
    let expecting = JSON.stringify({
      "url": "https://example.com/browser/browser/base/content/test/social/microformats.html",
      "microformats": {
        "items": [{
            "type": ["h-review"],
            "properties": {
              "rating": ["4.5"]
            }
          }
        ],
        "rels": {},
        "rel-urls": {}
      }
    });

    port.onmessage = function (e) {
      let topic = e.data.topic;
      switch (topic) {
        case "got-share-data-message":
          is(JSON.stringify(e.data.result), expecting, "microformats data ok");
          gBrowser.removeTab(testTab);
          port.close();
          next();
          break;
      }
    }
    port.postMessage({topic: "test-init"});

    let url = "https://example.com/browser/browser/base/content/test/social/microformats.html"
    addTab(url, function(tab) {
      testTab = tab;
      let doc = tab.linkedBrowser.contentDocument;
      target = doc.getElementById("test-review");
      SocialMarks.markLink(manifest.origin, url, target);
    });
  }
}
