/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-disable mozilla/no-arbitrary-setTimeout */

const ROOTURI = "http://mochi.test:8888/browser/browser/base/content/test/favicons/";
const ICON = "moz.png";
const DATAURL = "data:image/x-icon;base64,AAABAAEAEBAAAAAAAABoBQAAFgAAACgAAAAQAAAAIAAAAAEACAAAAAAAAAEAAAAAAAAAAAAAAAEAAAABAAAAAAAAAACAAACAAAAAgIAAgAAAAIAAgACAgAAAwMDAAMDcwADwyqYABAQEAAgICAAMDAwAERERABYWFgAcHBwAIiIiACkpKQBVVVUATU1NAEJCQgA5OTkAgHz/AFBQ/wCTANYA/+zMAMbW7wDW5+cAkKmtAAAAMwAAAGYAAACZAAAAzAAAMwAAADMzAAAzZgAAM5kAADPMAAAz/wAAZgAAAGYzAABmZgAAZpkAAGbMAABm/wAAmQAAAJkzAACZZgAAmZkAAJnMAACZ/wAAzAAAAMwzAADMZgAAzJkAAMzMAADM/wAA/2YAAP+ZAAD/zAAzAAAAMwAzADMAZgAzAJkAMwDMADMA/wAzMwAAMzMzADMzZgAzM5kAMzPMADMz/wAzZgAAM2YzADNmZgAzZpkAM2bMADNm/wAzmQAAM5kzADOZZgAzmZkAM5nMADOZ/wAzzAAAM8wzADPMZgAzzJkAM8zMADPM/wAz/zMAM/9mADP/mQAz/8wAM///AGYAAABmADMAZgBmAGYAmQBmAMwAZgD/AGYzAABmMzMAZjNmAGYzmQBmM8wAZjP/AGZmAABmZjMAZmZmAGZmmQBmZswAZpkAAGaZMwBmmWYAZpmZAGaZzABmmf8AZswAAGbMMwBmzJkAZszMAGbM/wBm/wAAZv8zAGb/mQBm/8wAzAD/AP8AzACZmQAAmTOZAJkAmQCZAMwAmQAAAJkzMwCZAGYAmTPMAJkA/wCZZgAAmWYzAJkzZgCZZpkAmWbMAJkz/wCZmTMAmZlmAJmZmQCZmcwAmZn/AJnMAACZzDMAZsxmAJnMmQCZzMwAmcz/AJn/AACZ/zMAmcxmAJn/mQCZ/8wAmf//AMwAAACZADMAzABmAMwAmQDMAMwAmTMAAMwzMwDMM2YAzDOZAMwzzADMM/8AzGYAAMxmMwCZZmYAzGaZAMxmzACZZv8AzJkAAMyZMwDMmWYAzJmZAMyZzADMmf8AzMwAAMzMMwDMzGYAzMyZAMzMzADMzP8AzP8AAMz/MwCZ/2YAzP+ZAMz/zADM//8AzAAzAP8AZgD/AJkAzDMAAP8zMwD/M2YA/zOZAP8zzAD/M/8A/2YAAP9mMwDMZmYA/2aZAP9mzADMZv8A/5kAAP+ZMwD/mWYA/5mZAP+ZzAD/mf8A/8wAAP/MMwD/zGYA/8yZAP/MzAD/zP8A//8zAMz/ZgD//5kA///MAGZm/wBm/2YAZv//AP9mZgD/Zv8A//9mACEApQBfX18Ad3d3AIaGhgCWlpYAy8vLALKysgDX19cA3d3dAOPj4wDq6uoA8fHxAPj4+ADw+/8ApKCgAICAgAAAAP8AAP8AAAD//wD/AAAA/wD/AP//AAD///8ACgoKCgoKCgoKCgoKCgoKCgoKCgoHAQEMbQoKCgoKCgoAAAdDH/kgHRIAAAAAAAAAAADrHfn5ASQQAAAAAAAAAArsBx0B+fkgHesAAAAAAAD/Cgwf+fn5IA4dEus/IvcACgcMAfkg+QEB+SABHushbf8QHR/5HQH5+QEdHetEHx4K7B/5+QH5+fkdDBL5+SBE/wwdJfkf+fn5AR8g+fkfEArsCh/5+QEeJR/5+SAeBwAACgoe+SAlHwFAEhAfAAAAAPcKHh8eASYBHhAMAAAAAAAA9EMdIB8gHh0dBwAAAAAAAAAA7BAdQ+wHAAAAAAAAAAAAAAAAAAAAAAAAAAAAAP//AADwfwAAwH8AAMB/AAAAPwAAAAEAAAAAAAAAAAAAAAAAAAAAAAAAAQAAgAcAAIAPAADADwAA8D8AAP//AAA=";

let iconDiscoveryTests = [
  {
    text: "rel icon discovered",
    icons: [{}]
  }, {
    text: "rel may contain additional rels separated by spaces",
    icons: [{ rel: "abcdefg icon qwerty" }],
  }, {
    text: "rel is case insensitive",
    icons: [{ rel: "ICON" }],
  }, {
    text: "rel shortcut-icon not discovered",
    expectedIcon: ROOTURI + ICON,
    icons: [ // We will prefer the later icon if detected
      { },
      { rel: "shortcut-icon", href: "nothere.png" },
    ],
  }, {
    text: "relative href works",
    icons: [{ href: "moz.png" }],
  }, {
    text: "404'd icon is removed properly",
    pass: false,
    icons: [{ href: "notthere.png" }],
  }, {
    text: "data: URIs work",
    icons: [{ href: DATAURL, type: "image/x-icon" }],
  }, {
    text: "type may have optional parameters (RFC2046)",
    icons: [{ type: "image/png; charset=utf-8" }],
  }, {
    text: "apple-touch-icon discovered",
    richIcon: true,
    icons: [{ rel: "apple-touch-icon" }],
  }, {
    text: "apple-touch-icon-precomposed discovered",
    richIcon: true,
    icons: [{ rel: "apple-touch-icon-precomposed" }],
  }, {
    text: "fluid-icon discovered",
    richIcon: true,
    icons: [{ rel: "fluid-icon" }],
  }, {
    text: "unknown icon not discovered",
    expectedIcon: ROOTURI + ICON,
    richIcon: true,
    icons: [ // We will prefer the larger icon if detected
      { rel: "apple-touch-icon", sizes: "32x32" },
      { rel: "unknown-icon", sizes: "128x128", href: "notthere.png" },
    ],
  },
];

add_task(async function() {
  let url = ROOTURI + "discovery.html";
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  for (let testCase of iconDiscoveryTests) {
    info(`Running test "${testCase.text}"`);

    if (testCase.pass === undefined)
      testCase.pass = true;

    if (testCase.icons.length > 1 && !testCase.expectedIcon) {
      ok(false, "Invalid test data, missing expectedIcon");
      continue;
    }

    let expectedIcon = testCase.expectedIcon || testCase.icons[0].href || ICON;
    expectedIcon = (new URL(expectedIcon, ROOTURI)).href;

    let iconPromise = waitForFaviconMessage(!testCase.richIcon, expectedIcon);

    await ContentTask.spawn(gBrowser.selectedBrowser, [testCase.icons, ROOTURI + ICON], ([icons, defaultIcon]) => {
      let doc = content.document;
      let head = doc.head;

      for (let icon of icons) {
        let link = doc.createElement("link");
        link.rel = icon.rel || "icon";
        link.href = icon.href || defaultIcon;
        link.type = icon.type || "image/png";
        if (icon.sizes) {
          link.sizes = icon.sizes;
        }
        head.appendChild(link);
      }
    });

    try {
      let { iconURL } = await iconPromise;
      ok(testCase.pass, testCase.text);
      is(iconURL, expectedIcon, "Should have seen the expected icon.");
    } catch (e) {
      ok(!testCase.pass, testCase.text);
    }

    await ContentTask.spawn(gBrowser.selectedBrowser, null, () => {
      let links = content.document.querySelectorAll("link");
      for (let link of links) {
        link.remove();
      }
    });
  }

  BrowserTestUtils.removeTab(tab);
});
