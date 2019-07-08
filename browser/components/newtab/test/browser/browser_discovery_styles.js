"use strict";

function fakePref(layout) {
  return [
    "browser.newtabpage.activity-stream.discoverystream.config",
    JSON.stringify({
      enabled: true,
      layout_endpoint: `data:,${encodeURIComponent(JSON.stringify(layout))}`,
    }),
  ];
}

test_newtab({
  async before({ pushPrefs }) {
    await pushPrefs(
      fakePref({
        layout: [
          {
            width: 12,
            components: [
              {
                type: "TopSites",
              },
              {
                type: "HorizontalRule",
                styles: {
                  hr: "border-width: 3.14159mm",
                },
              },
            ],
          },
        ],
      })
    );
  },
  test: async function test_hr_override() {
    const hr = await ContentTaskUtils.waitForCondition(() =>
      content.document.querySelector("hr")
    );
    ok(
      content.getComputedStyle(hr).borderTopWidth.match(/11.?\d*px/),
      "applied and normalized hr component width override"
    );
  },
});

test_newtab({
  async before({ pushPrefs }) {
    await pushPrefs(
      fakePref({
        layout: [
          {
            width: 12,
            components: [
              {
                type: "TopSites",
              },
              {
                type: "HorizontalRule",
                styles: {
                  "*": "color: #f00",
                  "": "font-size: 1.2345cm",
                  hr: "font-weight: 12345",
                },
              },
            ],
          },
        ],
      })
    );
  },
  test: async function test_multiple_overrides() {
    const hr = await ContentTaskUtils.waitForCondition(() =>
      content.document.querySelector("hr")
    );
    const styles = content.getComputedStyle(hr);
    is(styles.color, "rgb(255, 0, 0)", "applied and normalized color");
    is(styles.fontSize, "46.65px", "applied and normalized font size");
    is(styles.fontWeight, "400", "applied and normalized font weight");
  },
});

test_newtab({
  async before({ pushPrefs }) {
    await pushPrefs(
      fakePref({
        layout: [
          {
            width: 12,
            components: [
              {
                type: "HorizontalRule",
                styles: {
                  // NB: Use display: none to avoid network requests to unfiltered urls
                  hr: `display: none;
                   background-image: url(https://example.com/background);
                   content: url(chrome://browser/content);
                   cursor: url(  resource://activity-stream/cursor  ), auto;
                   list-style-image: url('https://img-getpocket.cdn.mozilla.net/list');`,
                },
              },
            ],
          },
        ],
      })
    );
  },
  test: async function test_url_filtering() {
    const hr = await ContentTaskUtils.waitForCondition(() =>
      content.document.querySelector("hr")
    );
    const styles = content.getComputedStyle(hr);
    is(
      styles.backgroundImage,
      "none",
      "filtered out invalid background image url"
    );
    is(
      styles.content,
      `url("chrome://browser/content/browser.xul")`,
      "applied, normalized and allowed content url"
    );
    is(
      styles.cursor,
      `url("resource://activity-stream/cursor"), auto`,
      "applied, normalized and allowed cursor url"
    );
    is(
      styles.listStyleImage,
      `url("https://img-getpocket.cdn.mozilla.net/list")`,
      "applied, normalized and allowed list style image url"
    );
  },
});

test_newtab({
  async before({ pushPrefs }) {
    await pushPrefs(
      fakePref({
        layout: [
          {
            width: 12,
            components: [
              {
                type: "HorizontalRule",
                styles: {
                  "@media (min-width: 0)":
                    "content: url(chrome://browser/content)",
                  "@media (min-width: 0) *":
                    "content: url(chrome://browser/content)",
                  "@media (min-width: 0) { * }":
                    "content: url(chrome://browser/content)",
                },
              },
            ],
          },
        ],
      })
    );
  },
  test: async function test_atrule_filtering() {
    const hr = await ContentTaskUtils.waitForCondition(() =>
      content.document.querySelector("hr")
    );
    is(
      content.getComputedStyle(hr).content,
      "none",
      "filtered out attempted @media query"
    );
  },
});
