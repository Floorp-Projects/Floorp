/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const ROOT = "http://mochi.test:8888/browser/browser/base/content/test/favicons/";

async function waitIcon(url) {
  let icon = await waitForFaviconMessage(true, url);
  is(icon.iconURL, url, "Should have seen the right icon.");
}

function createLinks(linkInfos) {
  return ContentTask.spawn(gBrowser.selectedBrowser, linkInfos, links => {
    let doc = content.document;
    let head = doc.head;
    for (let l of links) {
      let link = doc.createElement("link");
      link.rel = "icon";
      link.href = l.href;
      if (l.type)
        link.type = l.type;
      if (l.size)
        link.setAttribute("sizes", `${l.size}x${l.size}`);
      head.appendChild(link);
    }
  });
}

add_task(async function setup() {
  const URL = ROOT + "discovery.html";
  let iconPromise = waitIcon("http://mochi.test:8888/favicon.ico");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, URL);
  await iconPromise;
  registerCleanupFunction(async function() {
    BrowserTestUtils.removeTab(tab);
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
});

add_task(async function prefer_sized() {
  let promise = waitIcon(ROOT + "moz.png");
  await createLinks([
    { href: ROOT + "icon.ico",
      type: "image/x-icon"
    },
    { href: ROOT + "moz.png",
      type: "image/png",
      size: 16 * Math.ceil(window.devicePixelRatio)
    },
    { href: ROOT + "icon2.ico",
      type: "image/x-icon"
    },
  ]);
  await promise;
});

add_task(async function prefer_last_ico() {
  let promise = waitIcon(ROOT + "file_generic_favicon.ico");
  await createLinks([
    { href: ROOT + "icon.ico",
      type: "image/x-icon"
    },
    { href: ROOT + "icon.png",
      type: "image/png",
    },
    { href: ROOT + "file_generic_favicon.ico",
      type: "image/x-icon"
    },
  ]);
  await promise;
});

add_task(async function fuzzy_ico() {
  let promise = waitIcon(ROOT + "file_generic_favicon.ico");
  await createLinks([
    { href: ROOT + "icon.ico",
      type: "image/x-icon"
    },
    { href: ROOT + "icon.png",
      type: "image/png",
    },
    { href: ROOT + "file_generic_favicon.ico",
      type: "image/vnd.microsoft.icon"
    },
  ]);
  await promise;
});

add_task(async function guess_svg() {
  let promise = waitIcon(ROOT + "icon.svg");
  await createLinks([
    { href: ROOT + "icon.svg" },
    { href: ROOT + "icon.png",
      type: "image/png",
      size: 16 * Math.ceil(window.devicePixelRatio)
    },
    { href: ROOT + "icon.ico",
      type: "image/x-icon"
    },
  ]);
  await promise;
});

add_task(async function guess_ico() {
  let promise = waitIcon(ROOT + "file_generic_favicon.ico");
  await createLinks([
    { href: ROOT + "file_generic_favicon.ico" },
    { href: ROOT + "icon.png",
      type: "image/png",
    },
  ]);
  await promise;
});

add_task(async function guess_invalid() {
  let promise = waitIcon(ROOT + "icon.svg");
  // Create strange links to make sure they don't break us
  await createLinks([
    { href: ROOT + "icon.svg" },
    { href: ROOT + "icon" },
    { href: ROOT + "icon?.svg" },
    { href: ROOT + "icon#.svg" },
    { href: "data:text/plain,icon" },
    { href: "file:///icon" },
    { href: "about:icon" },
  ]);
  await promise;
});

add_task(async function guess_bestSized() {
  let preferredWidth = 16 * Math.ceil(window.devicePixelRatio);
  let promise = waitIcon(ROOT + "moz.png");
  await createLinks([
    { href: ROOT + "icon.png",
      type: "image/png",
      size: preferredWidth - 1
    },
    { href: ROOT + "icon2.png",
      type: "image/png",
    },
    { href: ROOT + "moz.png",
      type: "image/png",
      size: preferredWidth + 1
    },
    { href: ROOT + "icon4.png",
      type: "image/png",
      size: preferredWidth + 2
    },
  ]);
  await promise;
});
