/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-disable mozilla/no-arbitrary-setTimeout */

add_task(async function() {
  let url = "http://mochi.test:8888/browser/browser/base/content/test/favicons/discovery.html";
  info("Test icons discovery");
  // First we need to clear the failed favicons cache, since previous tests
  // likely added this non-existing icon, and useDefaultIcon would skip it.
  PlacesUtils.favicons.removeFailedFavicon(makeURI("http://mochi.test:8888/favicon.ico"));
  await BrowserTestUtils.withNewTab(url, iconDiscovery);
});

let iconDiscoveryTests = [
  { text: "rel icon discovered" },
  { rel: "abcdefg icon qwerty", text: "rel may contain additional rels separated by spaces" },
  { rel: "ICON", text: "rel is case insensitive" },
  { rel: "shortcut-icon", pass: false, text: "rel shortcut-icon not discovered" },
  { href: "moz.png", text: "relative href works" },
  { href: "notthere.png", text: "404'd icon is removed properly" },
  { href: "data:image/x-icon,%00", type: "image/x-icon", text: "data: URIs work" },
  { type: "image/png; charset=utf-8", text: "type may have optional parameters (RFC2046)" },
  { richIcon: true, rel: "apple-touch-icon", text: "apple-touch-icon discovered" },
  { richIcon: true, rel: "apple-touch-icon-precomposed", text: "apple-touch-icon-precomposed discovered" },
  { richIcon: true, rel: "fluid-icon", text: "fluid-icon discovered" },
  { richIcon: true, rel: "unknown-icon", pass: false, text: "unknown icon not discovered" }
];

async function iconDiscovery() {
  // Since the page doesn't have an icon, we should try using the root domain
  // icon.
  await BrowserTestUtils.waitForCondition(() => {
    return gBrowser.getIcon() == "http://mochi.test:8888/favicon.ico";
  }, "wait for default icon load to finish");

  for (let testCase of iconDiscoveryTests) {
    if (testCase.pass == undefined)
      testCase.pass = true;

    // Clear the current icon.
    gBrowser.setIcon(gBrowser.selectedTab, null);

    let promiseLinkAdded =
      BrowserTestUtils.waitForContentEvent(gBrowser.selectedBrowser, "DOMLinkAdded",
                                           false, null, true);
    let promiseMessage = new Promise(resolve => {
      let mm = window.messageManager;
      mm.addMessageListener("Link:SetIcon", function listenForIcon(msg) {
        mm.removeMessageListener("Link:SetIcon", listenForIcon);
        resolve(msg.data);
      });
    });

    await ContentTask.spawn(gBrowser.selectedBrowser, testCase, test => {
      let doc = content.document;
      let head = doc.getElementById("linkparent");
      let link = doc.createElement("link");
      link.rel = test.rel || "icon";
      link.href = test.href || "http://mochi.test:8888/browser/browser/base/content/test/favicons/moz.png";
      link.type = test.type || "image/png";
      head.appendChild(link);
    });

    await promiseLinkAdded;

    if (!testCase.richIcon) {
      // Because there is debounce logic in ContentLinkHandler.jsm to reduce the
      // favicon loads, we have to wait some time before checking that icon was
      // stored properly.
      try {
        await BrowserTestUtils.waitForCondition(() => {
          return gBrowser.getIcon() != null;
        }, "wait for icon load to finish", 100, 20);
        ok(testCase.pass, testCase.text);
      } catch (ex) {
        ok(!testCase.pass, testCase.text);
      }
    } else {
      // Rich icons are not set as tab icons, so just check for the SetIcon message.
      try {
        let data = await Promise.race([promiseMessage,
                                       new Promise((resolve, reject) => setTimeout(reject, 2000))]);
        is(data.canUseForTab, false, "Rich icons cannot be used for tabs");
        ok(testCase.pass, testCase.text);
      } catch (ex) {
        ok(!testCase.pass, testCase.text);
      }
    }

    await ContentTask.spawn(gBrowser.selectedBrowser, null, () => {
      let head = content.document.getElementById("linkparent");
      head.removeChild(head.getElementsByTagName("link")[0]);
    });
  }
}
