"use strict";

const BACKGROUND = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg==";
const ACCENT_COLOR = "#a14040";
const TEXT_COLOR = "#fac96e";

function hexToRGB(hex) {
  hex = parseInt((hex.indexOf("#") > -1 ? hex.substring(1) : hex), 16);
  return [hex >> 16, (hex & 0x00FF00) >> 8, (hex & 0x0000FF)];
}

add_task(function* setup() {
  yield SpecialPowers.pushPrefEnv({
    set: [["extensions.webextensions.themes.enabled", true]],
  });
});

add_task(function* test_support_LWT_properties() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "theme": {
        "images": {
          "headerURL": BACKGROUND,
        },
        "colors": {
          "accentcolor": ACCENT_COLOR,
          "textcolor": TEXT_COLOR,
        },
      },
    },
  });

  yield extension.startup();

  let docEl = window.document.documentElement;
  let style = window.getComputedStyle(docEl);

  Assert.ok(docEl.hasAttribute("lwtheme"), "LWT attribute should be set");
  Assert.equal(docEl.getAttribute("lwthemetextcolor"), "bright",
    "LWT text color attribute should be set");

  Assert.equal(style.backgroundImage, 'url("' + BACKGROUND.replace(/"/g, '\\"') + '")',
    "Expected background image");
  Assert.equal(style.backgroundColor, "rgb(" + hexToRGB(ACCENT_COLOR).join(", ") + ")",
    "Expected correct background color");
  Assert.equal(style.color, "rgb(" + hexToRGB(TEXT_COLOR).join(", ") + ")",
    "Expected correct text color");

  yield extension.unload();

  Assert.ok(!docEl.hasAttribute("lwtheme"), "LWT attribute should not be set");
});

add_task(function* test_LWT_requires_all_properties_defined_image_only() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "theme": {
        "images": {
          "headerURL": BACKGROUND,
        },
      },
    },
  });

  yield extension.startup();

  let docEl = window.document.documentElement;
  Assert.ok(!docEl.hasAttribute("lwtheme"), "LWT attribute should not be set");
  yield extension.unload();
  Assert.ok(!docEl.hasAttribute("lwtheme"), "LWT attribute should not be set");
});

add_task(function* test_LWT_requires_all_properties_defined_colors_only() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "theme": {
        "colors": {
          "accentcolor": ACCENT_COLOR,
          "textcolor": TEXT_COLOR,
        },
      },
    },
  });

  yield extension.startup();

  let docEl = window.document.documentElement;
  Assert.ok(!docEl.hasAttribute("lwtheme"), "LWT attribute should not be set");
  yield extension.unload();
  Assert.ok(!docEl.hasAttribute("lwtheme"), "LWT attribute should not be set");
});
