const PAGECONTENT_COLORS =
  "<html><head><style>" +
  "  .blue { color: #fff; background-color: #00f; }" +
  "  .green { color: #800080; background-color: green; }" +
  "  .defaultColor { color: -moz-ComboboxText; }" +
  "  .defaultBackground { background-color: -moz-Combobox; }" +
  "</style>" +
  "<body><select id='one'>" +
  '  <option value="One" style="color: #fff; background-color: #f00;">{"color": "rgb(255, 255, 255)", "backgroundColor": "rgb(255, 0, 0)"}</option>' +
  '  <option value="Two" class="blue">{"color": "rgb(255, 255, 255)", "backgroundColor": "rgb(0, 0, 255)"}</option>' +
  '  <option value="Three" class="green">{"color": "rgb(128, 0, 128)", "backgroundColor": "rgb(0, 128, 0)"}</option>' +
  '  <option value="Four" class="defaultColor defaultBackground">{"color": "-moz-ComboboxText", "backgroundColor": "rgba(0, 0, 0, 0)", "unstyled": "true"}</option>' +
  '  <option value="Five" class="defaultColor">{"color": "-moz-ComboboxText", "backgroundColor": "rgba(0, 0, 0, 0)", "unstyled": "true"}</option>' +
  '  <option value="Six" class="defaultBackground">{"color": "-moz-ComboboxText", "backgroundColor": "rgba(0, 0, 0, 0)", "unstyled": "true"}</option>' +
  '  <option value="Seven" selected="true">{"unstyled": "true"}</option>' +
  "</select></body></html>";

const PAGECONTENT_COLORS_ON_SELECT =
  "<html><head><style>" +
  "  #one { background-color: #7E3A3A; color: #fff }" +
  "</style>" +
  "<body><select id='one'>" +
  '  <option value="One">{"color": "rgb(255, 255, 255)", "backgroundColor": "rgba(0, 0, 0, 0)"}</option>' +
  '  <option value="Two">{"color": "rgb(255, 255, 255)", "backgroundColor": "rgba(0, 0, 0, 0)"}</option>' +
  '  <option value="Three">{"color": "rgb(255, 255, 255)", "backgroundColor": "rgba(0, 0, 0, 0)"}</option>' +
  '  <option value="Four" selected="true">{"end": "true"}</option>' +
  "</select></body></html>";

const TRANSPARENT_SELECT =
  "<html><head><style>" +
  "  #one { background-color: transparent; }" +
  "</style>" +
  "<body><select id='one'>" +
  '  <option value="One">{"unstyled": "true"}</option>' +
  '  <option value="Two" selected="true">{"end": "true"}</option>' +
  "</select></body></html>";

const OPTION_COLOR_EQUAL_TO_UABACKGROUND_COLOR_SELECT =
  "<html><head><style>" +
  "  #one { background-color: black; color: white; }" +
  "</style>" +
  "<body><select id='one'>" +
  '  <option value="One" style="background-color: white; color: black;">{"color": "rgb(0, 0, 0)", "backgroundColor": "rgb(255, 255, 255)"}</option>' +
  '  <option value="Two" selected="true">{"end": "true"}</option>' +
  "</select></body></html>";

const GENERIC_OPTION_STYLED_AS_IMPORTANT =
  "<html><head><style>" +
  "  option { background-color: black !important; color: white !important; }" +
  "</style>" +
  "<body><select id='one'>" +
  '  <option value="One">{"color": "rgb(255, 255, 255)", "backgroundColor": "rgb(0, 0, 0)"}</option>' +
  '  <option value="Two" selected="true">{"end": "true"}</option>' +
  "</select></body></html>";

const TRANSLUCENT_SELECT_BECOMES_OPAQUE =
  "<html><head>" +
  "<body><select id='one' style='background-color: rgba(255,255,255,.55);'>" +
  '  <option value="One">{"color": "rgb(0, 0, 0)", "backgroundColor": "rgba(0, 0, 0, 0)"}</option>' +
  '  <option value="Two" selected="true">{"end": "true"}</option>' +
  "</select></body></html>";

const TRANSLUCENT_SELECT_APPLIES_ON_BASE_COLOR =
  "<html><head>" +
  "<body><select id='one' style='background-color: rgba(255,0,0,.55);'>" +
  '  <option value="One">{"color": "rgb(0, 0, 0)", "backgroundColor": "rgba(0, 0, 0, 0)"}</option>' +
  '  <option value="Two" selected="true">{"end": "true"}</option>' +
  "</select></body></html>";

const DISABLED_OPTGROUP_AND_OPTIONS =
  "<html><head>" +
  "<body><select id='one'>" +
  "  <optgroup label='{\"color\": \"rgb(0, 0, 0)\", \"backgroundColor\": \"buttonface\"}'>" +
  '    <option disabled="">{"color": "GrayText", "backgroundColor": "rgba(0, 0, 0, 0)"}</option>' +
  '    <option>{"color": "rgb(0, 0, 0)", "backgroundColor": "rgba(0, 0, 0, 0)"}</option>' +
  '    <option disabled="">{"color": "GrayText", "backgroundColor": "rgba(0, 0, 0, 0)"}</option>' +
  '    <option>{"color": "rgb(0, 0, 0)", "backgroundColor": "rgba(0, 0, 0, 0)"}</option>' +
  '    <option>{"color": "rgb(0, 0, 0)", "backgroundColor": "rgba(0, 0, 0, 0)"}</option>' +
  '    <option>{"color": "rgb(0, 0, 0)", "backgroundColor": "rgba(0, 0, 0, 0)"}</option>' +
  '    <option>{"color": "rgb(0, 0, 0)", "backgroundColor": "rgba(0, 0, 0, 0)"}</option>' +
  "  </optgroup>" +
  "  <optgroup label='{\"color\": \"GrayText\", \"backgroundColor\": \"buttonface\"}' disabled=''>" +
  '    <option>{"color": "GrayText", "backgroundColor": "rgba(0, 0, 0, 0)"}</option>' +
  '    <option>{"color": "GrayText", "backgroundColor": "rgba(0, 0, 0, 0)"}</option>' +
  '    <option>{"color": "GrayText", "backgroundColor": "rgba(0, 0, 0, 0)"}</option>' +
  '    <option>{"color": "GrayText", "backgroundColor": "rgba(0, 0, 0, 0)"}</option>' +
  '    <option>{"color": "GrayText", "backgroundColor": "rgba(0, 0, 0, 0)"}</option>' +
  '    <option>{"color": "GrayText", "backgroundColor": "rgba(0, 0, 0, 0)"}</option>' +
  '    <option>{"color": "GrayText", "backgroundColor": "rgba(0, 0, 0, 0)"}</option>' +
  "  </optgroup>" +
  '  <option value="Two" selected="true">{"end": "true"}</option>' +
  "</select></body></html>";

const SELECT_CHANGES_COLOR_ON_FOCUS =
  "<html><head><style>" +
  "  select:focus { background-color: orange; color: black; }" +
  "</style></head>" +
  "<body><select id='one'>" +
  '  <option>{"color": "rgb(0, 0, 0)", "backgroundColor": "rgba(0, 0, 0, 0)"}</option>' +
  '  <option selected="true">{"end": "true"}</option>' +
  "</select></body></html>";

const SELECT_BGCOLOR_ON_SELECT_COLOR_ON_OPTIONS =
  "<html><head><style>" +
  "  select { background-color: black; }" +
  "  option { color: white; }" +
  "</style></head>" +
  "<body><select id='one'>" +
  '  <option>{"color": "rgb(255, 255, 255)", "backgroundColor": "rgba(0, 0, 0, 0)"}</option>' +
  '  <option selected="true">{"end": "true"}</option>' +
  "</select></body></html>";

const SELECT_STYLE_OF_OPTION_IS_BASED_ON_FOCUS_OF_SELECT =
  "<html><head><style>" +
  "  select:focus { background-color: #3a96dd; }" +
  "  select:focus option { background-color: #fff; }" +
  "</style></head>" +
  "<body><select id='one'>" +
  '  <option>{"color": "rgb(0, 0, 0)", "backgroundColor": "rgb(255, 255, 255)"}</option>' +
  '  <option selected="true">{"end": "true"}</option>' +
  "</select></body></html>";

const SELECT_STYLE_OF_OPTION_CHANGES_AFTER_FOCUS_EVENT =
  "<html><body><select id='one'>" +
  '  <option>{"color": "rgb(255, 0, 0)", "backgroundColor": "rgba(0, 0, 0, 0)"}</option>' +
  '  <option selected="true">{"end": "true"}</option>' +
  "</select></body><scr" +
  "ipt>" +
  "  var select = document.getElementById('one');" +
  "  select.addEventListener('focus', () => select.style.color = 'red');" +
  "</script></html>";

const SELECT_COLOR_OF_OPTION_CHANGES_AFTER_TRANSITIONEND =
  "<html><head><style>" +
  "  select { transition: all .1s; }" +
  "  select:focus { background-color: orange; }" +
  "</style></head><body><select id='one'>" +
  '  <option>{"color": "rgb(0, 0, 0)", "backgroundColor": "rgba(0, 0, 0, 0)"}</option>' +
  '  <option selected="true">{"end": "true"}</option>' +
  "</select></body></html>";

const SELECT_TEXTSHADOW_OF_OPTION_CHANGES_AFTER_TRANSITIONEND =
  "<html><head><style>" +
  "  select { transition: all .1s; }" +
  "  select:focus { text-shadow: 0 0 0 #303030; }" +
  "</style></head><body><select id='one'>" +
  '  <option>{"color": "rgb(0, 0, 0)", "backgroundColor": "rgba(0, 0, 0, 0)", "textShadow": "rgb(48, 48, 48) 0px 0px 0px"}</option>' +
  '  <option selected="true">{"end": "true"}</option>' +
  "</select></body></html>";

const SELECT_TRANSPARENT_COLOR_WITH_TEXT_SHADOW =
  "<html><head><style>" +
  "  select { color: transparent; text-shadow: 0 0 0 #303030; }" +
  "</style></head><body><select id='one'>" +
  '  <option>{"color": "rgba(0, 0, 0, 0)", "backgroundColor": "rgba(0, 0, 0, 0)", "textShadow": "rgb(48, 48, 48) 0px 0px 0px"}</option>' +
  '  <option selected="true">{"end": "true"}</option>' +
  "</select></body></html>";

let SELECT_LONG_WITH_TRANSITION =
  "<html><head><style>" +
  "  select { transition: all .2s linear; }" +
  "  select:focus { color: purple; }" +
  "</style></head><body><select id='one'>";
for (let i = 0; i < 75; i++) {
  SELECT_LONG_WITH_TRANSITION +=
    '  <option>{"color": "rgb(128, 0, 128)", "backgroundColor": "rgba(0, 0, 0, 0)"}</option>';
}
SELECT_LONG_WITH_TRANSITION +=
    '  <option selected="true">{"end": "true"}</option>' +
  "</select></body></html>";

function getSystemColor(color) {
  // Need to convert system color to RGB color.
  let textarea = document.createElementNS("http://www.w3.org/1999/xhtml", "textarea");
  textarea.style.color = color;
  return getComputedStyle(textarea).color;
}

function testOptionColors(index, item, menulist) {
  // The label contains a JSON string of the expected colors for
  // `color` and `background-color`.
  let expected = JSON.parse(item.label);

  for (let color of Object.keys(expected)) {
    if (color.toLowerCase().includes("color") &&
        !expected[color].startsWith("rgb")) {
      expected[color] = getSystemColor(expected[color]);
    }
  }

  // Press Down to move the selected item to the next item in the
  // list and check the colors of this item when it's not selected.
  EventUtils.synthesizeKey("KEY_ArrowDown", { code: "ArrowDown" });

  if (expected.end) {
    return;
  }

  if (expected.unstyled) {
    ok(!item.hasAttribute("customoptionstyling"),
      `Item ${index} should not have any custom option styling`);
  } else {
    is(getComputedStyle(item).color, expected.color,
       "Item " + (index) + " has correct foreground color");
    is(getComputedStyle(item).backgroundColor, expected.backgroundColor,
       "Item " + (index) + " has correct background color");
    if (expected.textShadow) {
      is(getComputedStyle(item).textShadow, expected.textShadow,
         "Item " + (index) + " has correct text-shadow color");
    }
  }
}

async function testSelectColors(select, itemCount, options) {
  const pageUrl = "data:text/html," + escape(select);
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, pageUrl);

  let menulist = document.getElementById("ContentSelectDropdown");
  let selectPopup = menulist.menupopup;

  let popupShownPromise = BrowserTestUtils.waitForEvent(selectPopup, "popupshown");
  await BrowserTestUtils.synthesizeMouseAtCenter("#one", { type: "mousedown" }, gBrowser.selectedBrowser);
  await popupShownPromise;

  if (options.waitForComputedStyle) {
    let property = options.waitForComputedStyle.property;
    let value = options.waitForComputedStyle.value;
    await BrowserTestUtils.waitForCondition(() => {
      info(`<select> has ${property}: ${getComputedStyle(selectPopup)[property]}`);
      return getComputedStyle(selectPopup)[property] == value;
    }, `Waiting for <select> to have ${property}: ${value}`);
  }

  is(selectPopup.parentNode.itemCount, itemCount, "Correct number of items");
  let child = selectPopup.firstChild;
  let idx = 1;

  if (!options.skipSelectColorTest) {
    is(getComputedStyle(selectPopup).color, options.selectColor,
      "popup has expected foreground color");

    if (options.selectTextShadow) {
      is(getComputedStyle(selectPopup).textShadow, options.selectTextShadow,
        "popup has expected text-shadow color");
    }

    // Combine the select popup's backgroundColor and the
    // backgroundImage color to get the color that is seen by
    // the user.
    let base = getComputedStyle(selectPopup).backgroundColor;
    let [/* unused */, bR, bG, bB] =
      base.match(/rgb\((\d+), (\d+), (\d+)\)/);
    bR = parseInt(bR, 10);
    bG = parseInt(bG, 10);
    bB = parseInt(bB, 10);
    let topCoat = getComputedStyle(selectPopup).backgroundImage;
    if (topCoat == "none") {
      is(`rgb(${bR}, ${bG}, ${bB})`, options.selectBgColor,
        "popup has expected background color");
    } else {
      let [/* unused */, /* unused */, tR, tG, tB, tA] =
        topCoat.match(/(rgba?\((\d+), (\d+), (\d+)(?:, (0\.\d+))?\)), \1/);
      tR = parseInt(tR, 10);
      tG = parseInt(tG, 10);
      tB = parseInt(tB, 10);
      tA = parseFloat(tA) || 1;
      let actualR = Math.round(tR * tA + bR * (1 - tA));
      let actualG = Math.round(tG * tA + bG * (1 - tA));
      let actualB = Math.round(tB * tA + bB * (1 - tA));
      is(`rgb(${actualR}, ${actualG}, ${actualB})`, options.selectBgColor,
        "popup has expected background color");
    }
  }

  ok(!child.selected, "The first child should not be selected");
  while (child) {
    testOptionColors(idx, child, menulist);
    idx++;
    child = child.nextSibling;
  }

  if (!options.leaveOpen) {
    await hideSelectPopup(selectPopup, "escape");
    await BrowserTestUtils.removeTab(tab);
  }
}

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    "set": [
      ["dom.select_popup_in_parent.enabled", true],
      ["dom.forms.select.customstyling", true]
    ]
  });
});

// This test checks when a <select> element has styles applied to <option>s within it.
add_task(async function test_colors_applied_to_popup_items() {
  await testSelectColors(PAGECONTENT_COLORS, 7,
                         {skipSelectColorTest: true});
});

// This test checks when a <select> element has styles applied to itself.
add_task(async function test_colors_applied_to_popup() {
  let options = {
    selectColor: "rgb(255, 255, 255)",
    selectBgColor: "rgb(126, 58, 58)"
  };
  await testSelectColors(PAGECONTENT_COLORS_ON_SELECT, 4, options);
});

// This test checks when a <select> element has a transparent background applied to itself.
add_task(async function test_transparent_applied_to_popup() {
  let options = {
    selectColor: getSystemColor("-moz-ComboboxText"),
    selectBgColor: getSystemColor("-moz-Combobox")
  };
  await testSelectColors(TRANSPARENT_SELECT, 2, options);
});

// This test checks when a <select> element has a background set, and the
// options have their own background set which is equal to the default
// user-agent background color, but should be used because the select
// background color has been changed.
add_task(async function test_options_inverted_from_select_background() {
  // The popup has a black background and white text, but the
  // options inside of it have flipped the colors.
  let options = {
    selectColor: "rgb(255, 255, 255)",
    selectBgColor: "rgb(0, 0, 0)"
  };
  await testSelectColors(OPTION_COLOR_EQUAL_TO_UABACKGROUND_COLOR_SELECT,
                         2, options);
});

// This test checks when a <select> element has a background set using !important,
// which was affecting how we calculated the user-agent styling.
add_task(async function test_select_background_using_important() {
  await testSelectColors(GENERIC_OPTION_STYLED_AS_IMPORTANT, 2,
                         {skipSelectColorTest: true});
});

// This test checks when a <select> element has a background set, and the
// options have their own background set which is equal to the default
// user-agent background color, but should be used because the select
// background color has been changed.
add_task(async function test_translucent_select_becomes_opaque() {
  // The popup is requested to show a translucent background
  // but we apply the requested background color on the system's base color.
  let options = {
    selectColor: "rgb(0, 0, 0)",
    selectBgColor: "rgb(255, 255, 255)"
  };
  await testSelectColors(TRANSLUCENT_SELECT_BECOMES_OPAQUE, 2, options);
});

// This test checks when a popup has a translucent background color,
// and that the color painted to the screen of the translucent background
// matches what the user expects.
add_task(async function test_translucent_select_applies_on_base_color() {
  // The popup is requested to show a translucent background
  // but we apply the requested background color on the system's base color.
  let options = {
    selectColor: "rgb(0, 0, 0)",
    selectBgColor: "rgb(255, 115, 115)"
  };
  await testSelectColors(TRANSLUCENT_SELECT_APPLIES_ON_BASE_COLOR, 2, options);
});

add_task(async function test_disabled_optgroup_and_options() {
  // The colors used by this test are platform-specific.
  if (AppConstants.platform != "win") {
    return;
  }

  await testSelectColors(DISABLED_OPTGROUP_AND_OPTIONS, 17,
                         {skipSelectColorTest: true});
});

add_task(async function test_disabled_optgroup_and_options() {
  let options = {
    selectColor: "rgb(0, 0, 0)",
    selectBgColor: "rgb(255, 165, 0)"
  };

  await testSelectColors(SELECT_CHANGES_COLOR_ON_FOCUS, 2, options);
});

add_task(async function test_bgcolor_on_select_color_on_options() {
  let options = {
    selectColor: "rgb(0, 0, 0)",
    selectBgColor: "rgb(0, 0, 0)"
  };

  await testSelectColors(SELECT_BGCOLOR_ON_SELECT_COLOR_ON_OPTIONS, 2, options);
});

add_task(async function test_style_of_options_is_dependent_on_focus_of_select() {
  let options = {
    selectColor: "rgb(0, 0, 0)",
    selectBgColor: "rgb(58, 150, 221)"
  };

  await testSelectColors(SELECT_STYLE_OF_OPTION_IS_BASED_ON_FOCUS_OF_SELECT, 2, options);
});

add_task(async function test_style_of_options_is_dependent_on_focus_of_select_after_event() {
  let options = {
    skipSelectColorTest: true,
    waitForComputedStyle: {
      property: "color",
      value: "rgb(255, 0, 0)"
    }
  };
  await testSelectColors(SELECT_STYLE_OF_OPTION_CHANGES_AFTER_FOCUS_EVENT, 2, options);
});

add_task(async function test_color_of_options_is_dependent_on_transitionend() {
  let options = {
    selectColor: "rgb(0, 0, 0)",
    selectBgColor: "rgb(255, 165, 0)",
    waitForComputedStyle: {
      property: "background-image",
      value: "linear-gradient(rgb(255, 165, 0), rgb(255, 165, 0))"
    }
  };

  await testSelectColors(SELECT_COLOR_OF_OPTION_CHANGES_AFTER_TRANSITIONEND, 2, options);
});

add_task(async function test_textshadow_of_options_is_dependent_on_transitionend() {
  let options = {
    skipSelectColorTest: true,
    waitForComputedStyle: {
      property: "text-shadow",
      value: "rgb(48, 48, 48) 0px 0px 0px"
    }
  };

  await testSelectColors(SELECT_TEXTSHADOW_OF_OPTION_CHANGES_AFTER_TRANSITIONEND, 2, options);
});

add_task(async function test_transparent_color_with_text_shadow() {
  let options = {
    selectColor: "rgba(0, 0, 0, 0)",
    selectTextShadow: "rgb(48, 48, 48) 0px 0px 0px",
    selectBgColor: "rgb(255, 255, 255)"
  };

  await testSelectColors(SELECT_TRANSPARENT_COLOR_WITH_TEXT_SHADOW, 2, options);
});

add_task(async function test_select_with_transition_doesnt_lose_scroll_position() {
  let options = {
    selectColor: "rgb(128, 0, 128)",
    selectBgColor: "rgb(255, 255, 255)",
    waitForComputedStyle: {
      property: "color",
      value: "rgb(128, 0, 128)"
    },
    leaveOpen: true
  };

  await testSelectColors(SELECT_LONG_WITH_TRANSITION, 76, options);

  let menulist = document.getElementById("ContentSelectDropdown");
  let selectPopup = menulist.menupopup;
  let scrollBox = selectPopup.scrollBox;
  is(scrollBox.scrollTop, scrollBox.scrollTopMax,
    "The popup should be scrolled to the bottom of the list (where the selected item is)");

  await hideSelectPopup(selectPopup, "escape");
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
