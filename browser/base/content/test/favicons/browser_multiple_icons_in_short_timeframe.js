/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function() {
  const ROOT = "http://mochi.test:8888/browser/browser/base/content/test/favicons/";
  const URL = ROOT + "discovery.html";

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, URL);

  // Because there is debounce logic in ContentLinkHandler.jsm to reduce the
  // favicon loads, we have to wait some time before checking that icon was
  // stored properly.
  let promiseIcon = BrowserTestUtils.waitForCondition(
    () => {
      let tabIcon = gBrowser.getIcon();
      info("Found icon " + tabIcon);
      return tabIcon == ROOT + "two.png";
    },
    "wait for icon load to finish", 200, 25);

  await ContentTask.spawn(gBrowser.selectedBrowser, ROOT, root => {
    let doc = content.document;
    let head = doc.getElementById("linkparent");
    let link = doc.createElement("link");
    link.rel = "icon";
    link.href = root + "one.png";
    link.type = "image/png";
    head.appendChild(link);
    let link2 = link.cloneNode(false);
    link2.href = root + "two.png";
    head.appendChild(link2);
  });

  await promiseIcon;
  // The test must have at least one pass.
  Assert.equal(gBrowser.getIcon(), ROOT + "two.png",
               "The expected icon has been set");

  BrowserTestUtils.removeTab(tab);
});
