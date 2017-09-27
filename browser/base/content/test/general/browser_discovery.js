/* eslint-disable mozilla/no-arbitrary-setTimeout */

add_task(async function() {
  let rootDir = getRootDirectory(gTestPath);
  let url = rootDir + "discovery.html";
  info("Test icons discovery");
  await BrowserTestUtils.withNewTab(url, iconDiscovery);
  info("Test search discovery");
  await BrowserTestUtils.withNewTab(url, searchDiscovery);
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
  // These rich icon tests are temporarily disabled due to intermittent failures.
  /*
  { richIcon: true, rel: "apple-touch-icon", text: "apple-touch-icon discovered" },
  { richIcon: true, rel: "apple-touch-icon-precomposed", text: "apple-touch-icon-precomposed discovered" },
  { richIcon: true, rel: "fluid-icon", text: "fluid-icon discovered" },
  */
  { richIcon: true, rel: "unknown-icon", pass: false, text: "unknown icon not discovered" }
];

async function iconDiscovery() {
  for (let testCase of iconDiscoveryTests) {
    if (testCase.pass == undefined)
      testCase.pass = true;
    testCase.rootDir = getRootDirectory(gTestPath);

    // Clear the current icon.
    gBrowser.setIcon(gBrowser.selectedTab, null);

    let promiseLinkAdded =
      BrowserTestUtils.waitForEvent(gBrowser.selectedBrowser, "DOMLinkAdded",
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
      link.href = test.href || test.rootDir + "moz.png";
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
        }, "wait for icon load to finish", 100, 5);
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

let searchDiscoveryTests = [
  { text: "rel search discovered" },
  { rel: "SEARCH", text: "rel is case insensitive" },
  { rel: "-search-", pass: false, text: "rel -search- not discovered" },
  { rel: "foo bar baz search quux", text: "rel may contain additional rels separated by spaces" },
  { href: "https://not.mozilla.com", text: "HTTPS ok" },
  { href: "ftp://not.mozilla.com", text: "FTP ok" },
  { href: "data:text/foo,foo", pass: false, text: "data URI not permitted" },
  { href: "javascript:alert(0)", pass: false, text: "JS URI not permitted" },
  { type: "APPLICATION/OPENSEARCHDESCRIPTION+XML", text: "type is case insensitve" },
  { type: " application/opensearchdescription+xml ", text: "type may contain extra whitespace" },
  { type: "application/opensearchdescription+xml; charset=utf-8", text: "type may have optional parameters (RFC2046)" },
  { type: "aapplication/opensearchdescription+xml", pass: false, text: "type should not be loosely matched" },
  { rel: "search search search", count: 1, text: "only one engine should be added" }
];

async function searchDiscovery() {
  let browser = gBrowser.selectedBrowser;

  for (let testCase of searchDiscoveryTests) {
    if (testCase.pass == undefined)
      testCase.pass = true;
    testCase.title = testCase.title || searchDiscoveryTests.indexOf(testCase);

    let promiseLinkAdded =
      BrowserTestUtils.waitForEvent(gBrowser.selectedBrowser, "DOMLinkAdded",
                                    false, null, true);

    await ContentTask.spawn(gBrowser.selectedBrowser, testCase, test => {
      let doc = content.document;
      let head = doc.getElementById("linkparent");
      let link = doc.createElement("link");
      link.rel = test.rel || "search";
      link.href = test.href || "http://so.not.here.mozilla.com/search.xml";
      link.type = test.type || "application/opensearchdescription+xml";
      link.title = test.title;
      head.appendChild(link);
    });

    await promiseLinkAdded;
    await new Promise(resolve => executeSoon(resolve));

    if (browser.engines) {
      info(`Found ${browser.engines.length} engines`);
      info(`First engine title: ${browser.engines[0].title}`);
      let hasEngine = testCase.count ?
        (browser.engines[0].title == testCase.title && browser.engines.length == testCase.count) :
        (browser.engines[0].title == testCase.title);
      ok(hasEngine, testCase.text);
      browser.engines = null;
    } else {
      ok(!testCase.pass, testCase.text);
    }
  }

  info("Test multiple engines with the same title");
  let count = 0;
  let promiseLinkAdded =
    BrowserTestUtils.waitForEvent(gBrowser.selectedBrowser, "DOMLinkAdded",
                                  false, () => ++count == 2, true);
  await ContentTask.spawn(gBrowser.selectedBrowser, null, () => {
    let doc = content.document;
    let head = doc.getElementById("linkparent");
    let link = doc.createElement("link");
    link.rel = "search";
    link.href = "http://first.mozilla.com/search.xml";
    link.type = "application/opensearchdescription+xml";
    link.title = "Test Engine";
    let link2 = link.cloneNode(false);
    link2.href = "http://second.mozilla.com/search.xml";
    head.appendChild(link);
    head.appendChild(link2);
  });

  await promiseLinkAdded;
  await new Promise(resolve => executeSoon(resolve));

  ok(browser.engines, "has engines");
  is(browser.engines.length, 1, "only one engine");
  is(browser.engines[0].uri, "http://first.mozilla.com/search.xml", "first engine wins");
  browser.engines = null;
}
