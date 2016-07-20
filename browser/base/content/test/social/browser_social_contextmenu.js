/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var SocialService = Cu.import("resource://gre/modules/SocialService.jsm", {}).SocialService;

var manifest = { // used for testing install
  name: "provider test1",
  origin: "https://test1.example.com",
  markURL: "https://test1.example.com/browser/browser/base/content/test/social/social_mark.html?url=%{url}",
  markedIcon: "https://test1.example.com/browser/browser/base/content/test/social/unchecked.jpg",
  unmarkedIcon: "https://test1.example.com/browser/browser/base/content/test/social/checked.jpg",

  iconURL: "https://test1.example.com/browser/browser/base/content/test/general/moz.png",
  version: "1.0"
};

function test() {
  waitForExplicitFinish();
  let frameScript = "data:,(" + function frame_script() {
    addEventListener("OpenGraphData", function (aEvent) {
      sendAsyncMessage("sharedata", aEvent.detail);
    }, true, true);
  }.toString() + ")();";
  let mm = getGroupMessageManager("social");
  mm.loadFrameScript(frameScript, true);

  runSocialTestWithProvider(manifest, function (finishcb) {
    runSocialTests(tests, undefined, undefined, function () {
      mm.removeDelayedFrameScript(frameScript);
      finishcb();
    });
  });
}

var tests = {
  testMarkMicroformats: function(next) {
    // emulates context menu action using target element, calling SocialMarks.markLink
    let provider = Social._getProviderFromOrigin(manifest.origin);
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

    let mm = getGroupMessageManager("social");
    mm.addMessageListener("sharedata", function handler(msg) {
      is(msg.data, expecting, "microformats data ok");
      mm.removeMessageListener("sharedata", handler);
      BrowserTestUtils.removeTab(testTab).then(next);
    });

    let url = "https://example.com/browser/browser/base/content/test/social/microformats.html"
    BrowserTestUtils.openNewForegroundTab(gBrowser, url).then(tab => {
      testTab = tab;
      let doc = tab.linkedBrowser.contentDocument;
      target = doc.getElementById("test-review");
      SocialMarks.markLink(manifest.origin, url, target);
    });
  }
}
