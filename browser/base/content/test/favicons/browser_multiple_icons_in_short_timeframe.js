/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function() {
  const ROOT = "http://mochi.test:8888/browser/browser/base/content/test/favicons/";
  const URL = ROOT + "discovery.html";

  let iconPromise = waitForFaviconMessage(true, "http://mochi.test:8888/favicon.ico");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, URL);
  let icon = await iconPromise;

  await ContentTask.spawn(gBrowser.selectedBrowser, ROOT, root => {
    let doc = content.document;
    let head = doc.head;
    let link = doc.createElement("link");
    link.rel = "icon";
    link.href = root + "rich_moz_1.png";
    link.type = "image/png";
    head.appendChild(link);
    let link2 = link.cloneNode(false);
    link2.href = root + "rich_moz_2.png";
    head.appendChild(link2);
  });

  icon = await waitForFaviconMessage();
  Assert.equal(icon.iconURL, ROOT + "rich_moz_2.png", "The expected icon has been set");

  BrowserTestUtils.removeTab(tab);
});
