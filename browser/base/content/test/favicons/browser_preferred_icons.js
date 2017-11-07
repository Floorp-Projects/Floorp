/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const ROOT = "http://mochi.test:8888/browser/browser/base/content/test/favicons/";

function waitIcon(url) {
  // Because there is debounce logic in ContentLinkHandler.jsm to reduce the
  // favicon loads, we have to wait some time before checking that icon was
  // stored properly.
  return BrowserTestUtils.waitForCondition(
    () => {
      let tabIcon = gBrowser.getIcon();
      info("Found icon " + tabIcon);
      return tabIcon == url;
    },
    "wait for icon load to finish", 200, 25);
}

function createLinks(linkInfos) {
  return ContentTask.spawn(gBrowser.selectedBrowser, linkInfos, links => {
    let doc = content.document;
    let head = doc.getElementById("linkparent");
    for (let l of links) {
      let link = doc.createElement("link");
      link.rel = "icon";
      link.href = l.href;
      link.type = l.type;
      if (l.size)
        link.setAttribute("sizes", `${l.size}x${l.size}`);
      head.appendChild(link);
    }
  });
}

add_task(async function setup() {
  const URL = ROOT + "discovery.html";
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, URL);
  registerCleanupFunction(async function() {
    await BrowserTestUtils.removeTab(tab);
  });
});

add_task(async function prefer_svg() {
  let promise = waitIcon(ROOT + "icon.svg");
  await createLinks([
    { href: ROOT + "icon.ico",
      type: "image/x-icon"
    },
    { href: ROOT + "icon.svg",
      type: "image/svg+xml"
    },
    { href: ROOT + "icon.png",
      type: "image/png",
      size: 16 * Math.ceil(window.devicePixelRatio)
    },
  ]);
  await promise;
  // Must have at least one test.
  Assert.ok(true, "The expected icon has been set");
});

add_task(async function prefer_sized() {
  let promise = waitIcon(ROOT + "icon.png");
  await createLinks([
    { href: ROOT + "icon.ico",
      type: "image/x-icon"
    },
    { href: ROOT + "icon.png",
      type: "image/png",
      size: 16 * Math.ceil(window.devicePixelRatio)
    },
    { href: ROOT + "icon2.ico",
      type: "image/x-icon"
    },
  ]);
  await promise;
  // Must have at least one test.
  Assert.ok(true, "The expected icon has been set");
});

add_task(async function prefer_ico() {
  let promise = waitIcon(ROOT + "icon2.ico");
  await createLinks([
    { href: ROOT + "icon.ico",
      type: "image/x-icon"
    },
    { href: ROOT + "icon.png",
      type: "image/png",
    },
    { href: ROOT + "icon2.ico",
    type: "image/x-icon"
  },
  ]);
  await promise;
  // Must have at least one test.
  Assert.ok(true, "The expected icon has been set");
});
