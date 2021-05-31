const gSelects = {
  PAGECONTENT_COLORS:
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
    '  <option value="Four" class="defaultColor defaultBackground">{"color": "-moz-ComboboxText", "backgroundColor": "-moz-Combobox"}</option>' +
    '  <option value="Five" class="defaultColor">{"color": "-moz-ComboboxText", "backgroundColor": "rgba(0, 0, 0, 0)", "unstyled": "true"}</option>' +
    '  <option value="Six" class="defaultBackground">{"color": "-moz-ComboboxText", "backgroundColor": "-moz-Combobox"}</option>' +
    '  <option value="Seven" selected="true">{"unstyled": "true"}</option>' +
    "</select></body></html>",

  PAGECONTENT_COLORS_ON_SELECT:
    "<html><head><style>" +
    "  #one { background-color: #7E3A3A; color: #fff }" +
    "</style>" +
    "<body><select id='one'>" +
    '  <option value="One">{"color": "rgb(255, 255, 255)", "backgroundColor": "rgba(0, 0, 0, 0)"}</option>' +
    '  <option value="Two">{"color": "rgb(255, 255, 255)", "backgroundColor": "rgba(0, 0, 0, 0)"}</option>' +
    '  <option value="Three">{"color": "rgb(255, 255, 255)", "backgroundColor": "rgba(0, 0, 0, 0)"}</option>' +
    '  <option value="Four" selected="true">{"end": "true"}</option>' +
    "</select></body></html>",

  TRANSPARENT_SELECT:
    "<html><head><style>" +
    "  #one { background-color: transparent; }" +
    "</style>" +
    "<body><select id='one'>" +
    '  <option value="One">{"unstyled": "true"}</option>' +
    '  <option value="Two" selected="true">{"end": "true"}</option>' +
    "</select></body></html>",

  OPTION_COLOR_EQUAL_TO_UABACKGROUND_COLOR_SELECT:
    "<html><head><style>" +
    "  #one { background-color: black; color: white; }" +
    "</style>" +
    "<body><select id='one'>" +
    '  <option value="One" style="background-color: white; color: black;">{"color": "rgb(0, 0, 0)", "backgroundColor": "rgb(255, 255, 255)"}</option>' +
    '  <option value="Two" selected="true">{"end": "true"}</option>' +
    "</select></body></html>",

  GENERIC_OPTION_STYLED_AS_IMPORTANT:
    "<html><head><style>" +
    "  option { background-color: black !important; color: white !important; }" +
    "</style>" +
    "<body><select id='one'>" +
    '  <option value="One">{"color": "rgb(255, 255, 255)", "backgroundColor": "rgb(0, 0, 0)"}</option>' +
    '  <option value="Two" selected="true">{"end": "true"}</option>' +
    "</select></body></html>",

  TRANSLUCENT_SELECT_BECOMES_OPAQUE:
    "<html><head>" +
    "<body><select id='one' style='background-color: rgba(255,255,255,.55);'>" +
    '  <option value="One">{"color": "rgb(0, 0, 0)", "backgroundColor": "rgba(0, 0, 0, 0)"}</option>' +
    '  <option value="Two" selected="true">{"end": "true"}</option>' +
    "</select></body></html>",

  TRANSLUCENT_SELECT_APPLIES_ON_BASE_COLOR:
    "<html><head>" +
    "<body><select id='one' style='background-color: rgba(255,0,0,.55);'>" +
    '  <option value="One">{"color": "rgb(0, 0, 0)", "backgroundColor": "rgba(0, 0, 0, 0)"}</option>' +
    '  <option value="Two" selected="true">{"end": "true"}</option>' +
    "</select></body></html>",

  DISABLED_OPTGROUP_AND_OPTIONS:
    "<html><head>" +
    "<body><select id='one'>" +
    "  <optgroup label='{\"unstyled\": true}'>" +
    '    <option disabled="">{"color": "GrayText", "backgroundColor": "-moz-Combobox"}</option>' +
    '    <option>{"unstyled": true}</option>' +
    '    <option disabled="">{"color": "GrayText", "backgroundColor": "-moz-Combobox"}</option>' +
    '    <option>{"unstyled": true}</option>' +
    '    <option>{"unstyled": true}</option>' +
    '    <option>{"unstyled": true}</option>' +
    '    <option>{"unstyled": true}</option>' +
    "  </optgroup>" +
    '  <optgroup label=\'{"color": "GrayText", "backgroundColor": "-moz-Combobox"}\' disabled=\'\'>' +
    '    <option>{"color": "GrayText", "backgroundColor": "-moz-Combobox"}</option>' +
    '    <option>{"color": "GrayText", "backgroundColor": "-moz-Combobox"}</option>' +
    '    <option>{"color": "GrayText", "backgroundColor": "-moz-Combobox"}</option>' +
    '    <option>{"color": "GrayText", "backgroundColor": "-moz-Combobox"}</option>' +
    '    <option>{"color": "GrayText", "backgroundColor": "-moz-Combobox"}</option>' +
    '    <option>{"color": "GrayText", "backgroundColor": "-moz-Combobox"}</option>' +
    '    <option>{"color": "GrayText", "backgroundColor": "-moz-Combobox"}</option>' +
    "  </optgroup>" +
    '  <option value="Two" selected="true">{"end": "true"}</option>' +
    "</select></body></html>",

  SELECT_CHANGES_COLOR_ON_FOCUS:
    "<html><head><style>" +
    "  select:focus { background-color: orange; color: black; }" +
    "</style></head>" +
    "<body><select id='one'>" +
    '  <option>{"color": "rgb(0, 0, 0)", "backgroundColor": "rgba(0, 0, 0, 0)"}</option>' +
    '  <option selected="true">{"end": "true"}</option>' +
    "</select></body></html>",

  SELECT_BGCOLOR_ON_SELECT_COLOR_ON_OPTIONS:
    "<html><head><style>" +
    "  select { background-color: black; }" +
    "  option { color: white; }" +
    "</style></head>" +
    "<body><select id='one'>" +
    '  <option>{"color": "rgb(255, 255, 255)", "backgroundColor": "rgb(0, 0, 0)"}</option>' +
    '  <option selected="true">{"end": "true"}</option>' +
    "</select></body></html>",

  SELECT_STYLE_OF_OPTION_IS_BASED_ON_FOCUS_OF_SELECT:
    "<html><head><style>" +
    "  select:focus { background-color: #3a96dd; }" +
    "  select:focus option { background-color: #fff; }" +
    "</style></head>" +
    "<body><select id='one'>" +
    '  <option>{"color": "-moz-ComboboxText", "backgroundColor": "rgb(255, 255, 255)"}</option>' +
    '  <option selected="true">{"end": "true"}</option>' +
    "</select></body></html>",

  SELECT_STYLE_OF_OPTION_CHANGES_AFTER_FOCUS_EVENT:
    "<html><body><select id='one'>" +
    '  <option>{"color": "rgb(255, 0, 0)", "backgroundColor": "rgba(0, 0, 0, 0)"}</option>' +
    '  <option selected="true">{"end": "true"}</option>' +
    "</select></body><scr" +
    "ipt>" +
    "  var select = document.getElementById('one');" +
    "  select.addEventListener('focus', () => select.style.color = 'red');" +
    "</script></html>",

  SELECT_COLOR_OF_OPTION_CHANGES_AFTER_TRANSITIONEND:
    "<html><head><style>" +
    "  select { transition: all .1s; }" +
    "  select:focus { background-color: orange; }" +
    "</style></head><body><select id='one'>" +
    '  <option>{"color": "-moz-ComboboxText", "backgroundColor": "rgba(0, 0, 0, 0)"}</option>' +
    '  <option selected="true">{"end": "true"}</option>' +
    "</select></body></html>",

  SELECT_TEXTSHADOW_OF_OPTION_CHANGES_AFTER_TRANSITIONEND:
    "<html><head><style>" +
    "  select { transition: all .1s; }" +
    "  select:focus { text-shadow: 0 0 0 #303030; }" +
    "  option { color: red; /* It gets the default otherwise, which is fine but we don't have a good way to test for */ }" +
    "</style></head><body><select id='one'>" +
    '  <option>{"color": "rgb(255, 0, 0)", "backgroundColor": "-moz-Combobox", "textShadow": "rgb(48, 48, 48) 0px 0px 0px"}</option>' +
    '  <option selected="true">{"end": "true"}</option>' +
    "</select></body></html>",

  SELECT_TRANSPARENT_COLOR_WITH_TEXT_SHADOW:
    "<html><head><style>" +
    "  select { color: transparent; text-shadow: 0 0 0 #303030; }" +
    "</style></head><body><select id='one'>" +
    '  <option>{"color": "rgba(0, 0, 0, 0)", "backgroundColor": "rgba(0, 0, 0, 0)", "textShadow": "rgb(48, 48, 48) 0px 0px 0px"}</option>' +
    '  <option selected="true">{"end": "true"}</option>' +
    "</select></body></html>",

  SELECT_LONG_WITH_TRANSITION:
    "<html><head><style>" +
    "  select { transition: all .2s linear; }" +
    "  select:focus { color: purple; }" +
    "</style></head><body><select id='one'>" +
    (function() {
      let rv = "";
      for (let i = 0; i < 75; i++) {
        rv +=
          '  <option>{"color": "rgb(128, 0, 128)", "backgroundColor": "rgba(0, 0, 0, 0)"}</option>';
      }
      rv +=
        '  <option selected="true">{"end": "true"}</option>' +
        "</select></body></html>";
      return rv;
    })(),

  SELECT_INHERITED_COLORS_ON_OPTIONS_DONT_GET_UNIQUE_RULES_IF_RULE_SET_ON_SELECT: `
   <html><head><style>
     select { color: blue; text-shadow: 1px 1px 2px blue; }
     .redColor { color: red; }
     .textShadow { text-shadow: 1px 1px 2px black; }
   </style></head><body><select id='one'>
     <option>{"color": "rgb(0, 0, 255)", "backgroundColor": "rgba(0, 0, 0, 0)"}</option>
     <option class="redColor">{"color": "rgb(255, 0, 0)", "backgroundColor": "-moz-Combobox"}</option>
     <option class="textShadow">{"color": "rgb(0, 0, 255)", "textShadow": "rgb(0, 0, 0) 1px 1px 2px", "backgroundColor": "rgba(0, 0, 0, 0)"}</option>
     <option selected="true">{"end": "true"}</option>
   </select></body></html>
`,

  SELECT_FONT_INHERITS_TO_OPTION: `
   <html><head><style>
     select { font-family: monospace }
   </style></head><body><select id='one'>
     <option>One</option>
     <option style="font-family: sans-serif">Two</option>
   </select></body></html>
`,

  SELECT_SCROLLBAR_PROPS: `
   <html><head><style>
     select { scrollbar-width: thin; scrollbar-color: red blue }
   </style></head><body><select id='one'>
     <option>One</option>
     <option style="font-family: sans-serif">Two</option>
   </select></body></html>
`,
  DEFAULT_DARKMODE: `
   <html><body><select id='one'>
     <option>{"unstyled": "true"}</option>
     <option>{"unstyled": "true"}</option>
     <option selected="true">{"end": "true"}</option>
   </select></body></html>
`,

  SPLIT_FG_BG_OPTION_DARKMODE: `
   <html><head><style>
     select { background-color: #fff; }
     option { color: #2b2b2b; }
   </style></head><body><select id='one'>
     <option>{"color": "rgb(43, 43, 43)", "backgroundColor": "-moz-Combobox"}</option>
     <option>{"color": "rgb(43, 43, 43)", "backgroundColor": "-moz-Combobox"}</option>
     <option selected="true">{"end": "true"}</option>
   </select></body></html>
`,

  IDENTICAL_BG_DIFF_FG_OPTION_DARKMODE: `
   <html><head><style>
     select { background-color: #fff; }
     option { color: #2b2b2b; background-color: #fff; }
   </style></head><body><select id='one'>
     <option>{"color": "rgb(43, 43, 43)", "backgroundColor": "-moz-Combobox"}</option>
     <option>{"color": "rgb(43, 43, 43)", "backgroundColor": "-moz-Combobox"}</option>
     <option selected="true">{"end": "true"}</option>
   </select></body></html>
`,
};

function rgbaToString(parsedColor) {
  let { r, g, b, a } = parsedColor;
  if (a == 1) {
    return `rgb(${r}, ${g}, ${b})`;
  }
  return `rgba(${r}, ${g}, ${b}, ${a})`;
}

function testOptionColors(test, index, item, menulist) {
  // The label contains a JSON string of the expected colors for
  // `color` and `background-color`.
  let expected = JSON.parse(item.label);

  // Press Down to move the selected item to the next item in the
  // list and check the colors of this item when it's not selected.
  EventUtils.synthesizeKey("KEY_ArrowDown");

  if (expected.end) {
    return;
  }

  if (expected.unstyled) {
    ok(
      !item.hasAttribute("customoptionstyling"),
      `${test}: Item ${index} should not have any custom option styling: ${item.outerHTML}`
    );
  } else {
    is(
      getComputedStyle(item).color,
      expected.color,
      `${test}: Item ${index} has correct foreground color`
    );
    is(
      getComputedStyle(item).backgroundColor,
      expected.backgroundColor,
      `${test}: Item ${index} has correct background color`
    );
    if (expected.textShadow) {
      is(
        getComputedStyle(item).textShadow,
        expected.textShadow,
        `${test}: Item ${index} has correct text-shadow color`
      );
    }
  }
}

function computeLabels(tab) {
  return SpecialPowers.spawn(tab.linkedBrowser, [], function() {
    function _rgbaToString(parsedColor) {
      let { r, g, b, a } = parsedColor;
      if (a == 1) {
        return `rgb(${r}, ${g}, ${b})`;
      }
      return `rgba(${r}, ${g}, ${b}, ${a})`;
    }
    function computeColors(expected) {
      let any = false;
      for (let color of Object.keys(expected)) {
        if (
          color.toLowerCase().includes("color") &&
          !expected[color].startsWith("rgb")
        ) {
          any = true;
          expected[color] = _rgbaToString(
            InspectorUtils.colorToRGBA(expected[color], content.document)
          );
        }
      }
      return any;
    }
    for (let option of content.document.querySelectorAll("option,optgroup")) {
      if (!option.label) {
        continue;
      }
      let expected;
      try {
        expected = JSON.parse(option.label);
      } catch (ex) {
        continue;
      }
      if (computeColors(expected)) {
        option.label = JSON.stringify(expected);
      }
    }
  });
}

async function openSelectPopup(select) {
  const pageUrl = "data:text/html," + escape(select);
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, pageUrl);

  await computeLabels(tab);

  let menulist = document.getElementById("ContentSelectDropdown");
  let selectPopup = menulist.menupopup;

  let popupShownPromise = BrowserTestUtils.waitForEvent(
    selectPopup,
    "popupshown"
  );
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#one",
    { type: "mousedown" },
    gBrowser.selectedBrowser
  );
  await popupShownPromise;
  return { tab, menulist, selectPopup };
}

async function testSelectColors(selectID, itemCount, options) {
  let select = gSelects[selectID];
  let { tab, menulist, selectPopup } = await openSelectPopup(select);
  if (options.unstyled) {
    ok(
      !selectPopup.hasAttribute("customoptionstyling"),
      `Shouldn't have custom option styling for ${selectID}`
    );
  }
  let arrowSB = selectPopup.shadowRoot.querySelector(
    ".menupopup-arrowscrollbox"
  );
  if (options.waitForComputedStyle) {
    let property = options.waitForComputedStyle.property;
    let value = options.waitForComputedStyle.value;
    await TestUtils.waitForCondition(() => {
      let node = ["background-image", "background-color"].includes(property)
        ? arrowSB
        : selectPopup;
      info(
        `<${node.localName}> has ${property}: ${
          getComputedStyle(node)[property]
        }`
      );
      return getComputedStyle(node)[property] == value;
    }, `${selectID} - Waiting for <select> to have ${property}: ${value}`);
  }

  is(selectPopup.parentNode.itemCount, itemCount, "Correct number of items");
  let child = selectPopup.firstElementChild;
  let idx = 1;

  if (typeof options.skipSelectColorTest != "object") {
    let skip = !!options.skipSelectColorTest;
    options.skipSelectColorTest = {
      color: skip,
      background: skip,
    };
  }
  if (!options.skipSelectColorTest.color) {
    is(
      getComputedStyle(selectPopup).color,
      options.selectColor,
      selectID + " popup has expected foreground color"
    );
  }

  if (options.selectTextShadow) {
    is(
      getComputedStyle(selectPopup).textShadow,
      options.selectTextShadow,
      selectID + " popup has expected text-shadow color"
    );
  }

  if (!options.skipSelectColorTest.background) {
    // Combine the select popup's backgroundColor and the
    // backgroundImage color to get the color that is seen by
    // the user.
    let base = getComputedStyle(arrowSB).backgroundColor;
    if (base == "rgba(0, 0, 0, 0)") {
      base = getComputedStyle(selectPopup).backgroundColor;
    }
    info("Parsing background color: " + base);
    let [, /* unused */ bR, bG, bB] = base.match(/rgb\((\d+), (\d+), (\d+)\)/);
    bR = parseInt(bR, 10);
    bG = parseInt(bG, 10);
    bB = parseInt(bB, 10);
    let topCoat = getComputedStyle(arrowSB).backgroundImage;
    if (topCoat == "none") {
      is(
        `rgb(${bR}, ${bG}, ${bB})`,
        options.selectBgColor,
        selectID + " popup has expected background color (top coat)"
      );
    } else {
      let [, , /* unused */ /* unused */ tR, tG, tB, tA] = topCoat.match(
        /(rgba?\((\d+), (\d+), (\d+)(?:, (0\.\d+))?\)), \1/
      );
      tR = parseInt(tR, 10);
      tG = parseInt(tG, 10);
      tB = parseInt(tB, 10);
      tA = parseFloat(tA) || 1;
      let actualR = Math.round(tR * tA + bR * (1 - tA));
      let actualG = Math.round(tG * tA + bG * (1 - tA));
      let actualB = Math.round(tB * tA + bB * (1 - tA));
      is(
        `rgb(${actualR}, ${actualG}, ${actualB})`,
        options.selectBgColor,
        selectID + " popup has expected background color (no top coat)"
      );
    }
  }

  ok(!child.selected, "The first child should not be selected");
  while (child) {
    testOptionColors(selectID, idx, child, menulist);
    idx++;
    child = child.nextElementSibling;
  }

  if (!options.leaveOpen) {
    await hideSelectPopup(selectPopup, "escape");
    BrowserTestUtils.removeTab(tab);
  }
}

// System colors may be different in content pages and chrome pages.
let kDefaultSelectStyles = {};

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.select_popup_in_parent.enabled", true],
      ["dom.forms.select.customstyling", true],
    ],
  });
  kDefaultSelectStyles = await BrowserTestUtils.withNewTab(
    `data:text/html,<select>`,
    function(browser) {
      return SpecialPowers.spawn(browser, [], function() {
        let cs = content.getComputedStyle(
          content.document.querySelector("select")
        );
        return {
          backgroundColor: cs.backgroundColor,
        };
      });
    }
  );
});

// This test checks when a <select> element has styles applied to <option>s within it.
add_task(async function test_colors_applied_to_popup_items() {
  await testSelectColors("PAGECONTENT_COLORS", 7, {
    skipSelectColorTest: true,
  });
});

// This test checks when a <select> element has styles applied to itself.
add_task(async function test_colors_applied_to_popup() {
  let options = {
    selectColor: "rgb(255, 255, 255)",
    selectBgColor: "rgb(126, 58, 58)",
  };
  await testSelectColors("PAGECONTENT_COLORS_ON_SELECT", 4, options);
});

// This test checks when a <select> element has a transparent background applied to itself.
add_task(async function test_transparent_applied_to_popup() {
  let options = {
    unstyled: true,
    skipSelectColorTest: true,
  };
  await testSelectColors("TRANSPARENT_SELECT", 2, options);
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
    selectBgColor: "rgb(0, 0, 0)",
  };
  await testSelectColors(
    "OPTION_COLOR_EQUAL_TO_UABACKGROUND_COLOR_SELECT",
    2,
    options
  );
});

// This test checks when a <select> element has a background set using !important,
// which was affecting how we calculated the user-agent styling.
add_task(async function test_select_background_using_important() {
  await testSelectColors("GENERIC_OPTION_STYLED_AS_IMPORTANT", 2, {
    skipSelectColorTest: true,
  });
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
    selectBgColor: "rgb(255, 255, 255)",
  };
  await testSelectColors("TRANSLUCENT_SELECT_BECOMES_OPAQUE", 2, options);
});

// This test checks when a popup has a translucent background color,
// and that the color painted to the screen of the translucent background
// matches what the user expects.
add_task(async function test_translucent_select_applies_on_base_color() {
  // The popup is requested to show a translucent background
  // but we apply the requested background color on the system's base color.
  let options = {
    selectColor: "rgb(0, 0, 0)",
    selectBgColor: "rgb(255, 115, 115)",
  };
  await testSelectColors(
    "TRANSLUCENT_SELECT_APPLIES_ON_BASE_COLOR",
    2,
    options
  );
});

add_task(async function test_disabled_optgroup_and_options() {
  await testSelectColors("DISABLED_OPTGROUP_AND_OPTIONS", 17, {
    skipSelectColorTest: true,
  });
});

add_task(async function test_disabled_optgroup_and_options() {
  let options = {
    selectColor: "rgb(0, 0, 0)",
    selectBgColor: "rgb(255, 165, 0)",
  };

  await testSelectColors("SELECT_CHANGES_COLOR_ON_FOCUS", 2, options);
});

add_task(async function test_bgcolor_on_select_color_on_options() {
  let options = {
    selectColor: "rgb(0, 0, 0)",
    selectBgColor: "rgb(0, 0, 0)",
  };

  await testSelectColors(
    "SELECT_BGCOLOR_ON_SELECT_COLOR_ON_OPTIONS",
    2,
    options
  );
});

add_task(
  async function test_style_of_options_is_dependent_on_focus_of_select() {
    let options = {
      selectColor: "rgb(0, 0, 0)",
      selectBgColor: "rgb(58, 150, 221)",
    };

    await testSelectColors(
      "SELECT_STYLE_OF_OPTION_IS_BASED_ON_FOCUS_OF_SELECT",
      2,
      options
    );
  }
);

add_task(
  async function test_style_of_options_is_dependent_on_focus_of_select_after_event() {
    let options = {
      skipSelectColorTest: true,
      waitForComputedStyle: {
        property: "color",
        value: "rgb(255, 0, 0)",
      },
    };
    await testSelectColors(
      "SELECT_STYLE_OF_OPTION_CHANGES_AFTER_FOCUS_EVENT",
      2,
      options
    );
  }
);

add_task(async function test_color_of_options_is_dependent_on_transitionend() {
  let options = {
    selectColor: "rgb(0, 0, 0)",
    selectBgColor: "rgb(255, 165, 0)",
    waitForComputedStyle: {
      property: "background-image",
      value: "linear-gradient(rgb(255, 165, 0), rgb(255, 165, 0))",
    },
  };

  await testSelectColors(
    "SELECT_COLOR_OF_OPTION_CHANGES_AFTER_TRANSITIONEND",
    2,
    options
  );
});

add_task(
  async function test_textshadow_of_options_is_dependent_on_transitionend() {
    let options = {
      skipSelectColorTest: true,
      waitForComputedStyle: {
        property: "text-shadow",
        value: "rgb(48, 48, 48) 0px 0px 0px",
      },
    };

    await testSelectColors(
      "SELECT_TEXTSHADOW_OF_OPTION_CHANGES_AFTER_TRANSITIONEND",
      2,
      options
    );
  }
);

add_task(async function test_transparent_color_with_text_shadow() {
  let options = {
    selectColor: "rgba(0, 0, 0, 0)",
    selectTextShadow: "rgb(48, 48, 48) 0px 0px 0px",
    selectBgColor: kDefaultSelectStyles.backgroundColor,
  };

  await testSelectColors(
    "SELECT_TRANSPARENT_COLOR_WITH_TEXT_SHADOW",
    2,
    options
  );
});

add_task(
  async function test_select_with_transition_doesnt_lose_scroll_position() {
    let options = {
      selectColor: "rgb(128, 0, 128)",
      selectBgColor: kDefaultSelectStyles.backgroundColor,
      waitForComputedStyle: {
        property: "color",
        value: "rgb(128, 0, 128)",
      },
      leaveOpen: true,
    };

    await testSelectColors("SELECT_LONG_WITH_TRANSITION", 76, options);

    let menulist = document.getElementById("ContentSelectDropdown");
    let selectPopup = menulist.menupopup;
    let scrollBox = selectPopup.scrollBox;
    is(
      scrollBox.scrollTop,
      scrollBox.scrollTopMax,
      "The popup should be scrolled to the bottom of the list (where the selected item is)"
    );

    await hideSelectPopup(selectPopup, "escape");
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }
);

add_task(
  async function test_select_inherited_colors_on_options_dont_get_unique_rules_if_rule_set_on_select() {
    let options = {
      selectColor: "rgb(0, 0, 255)",
      selectTextShadow: "rgb(0, 0, 255) 1px 1px 2px",
      selectBgColor: kDefaultSelectStyles.backgroundColor,
      leaveOpen: true,
    };

    await testSelectColors(
      "SELECT_INHERITED_COLORS_ON_OPTIONS_DONT_GET_UNIQUE_RULES_IF_RULE_SET_ON_SELECT",
      4,
      options
    );

    let stylesheetEl = document.getElementById(
      "ContentSelectDropdownStylesheet"
    );
    let sheet = stylesheetEl.sheet;
    /* Check that there are no rulesets for the first option, but that
     one exists for the second option and sets the color of that
     option to "rgb(255, 0, 0)" */

    function hasMatchingRuleForOption(cssRules, index, styles = {}) {
      for (let rule of cssRules) {
        if (rule.selectorText.includes(`:nth-child(${index})`)) {
          if (
            Object.keys(styles).some(key => rule.style[key] !== styles[key])
          ) {
            continue;
          }
          return true;
        }
      }
      return false;
    }

    is(
      hasMatchingRuleForOption(sheet.cssRules, 1),
      false,
      "There should be no rules specific to option1"
    );
    is(
      hasMatchingRuleForOption(sheet.cssRules, 2, {
        color: "rgb(255, 0, 0)",
      }),
      true,
      "There should be a rule specific to option2 and it should have color: red"
    );
    is(
      hasMatchingRuleForOption(sheet.cssRules, 3, {
        "text-shadow": "rgb(0, 0, 0) 1px 1px 2px",
      }),
      true,
      "There should be a rule specific to option3 and it should have text-shadow: rgb(0, 0, 0) 1px 1px 2px"
    );

    let menulist = document.getElementById("ContentSelectDropdown");
    let selectPopup = menulist.menupopup;

    await hideSelectPopup(selectPopup, "escape");
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }
);

add_task(async function test_select_font_inherits_to_option() {
  let { tab, menulist, selectPopup } = await openSelectPopup(
    gSelects.SELECT_FONT_INHERITS_TO_OPTION
  );

  let popupFont = getComputedStyle(selectPopup).fontFamily;
  let items = menulist.querySelectorAll("menuitem");
  is(items.length, 2, "Should have two options");
  let firstItemFont = getComputedStyle(items[0]).fontFamily;
  let secondItemFont = getComputedStyle(items[1]).fontFamily;

  is(
    popupFont,
    firstItemFont,
    "First menuitem's font should be inherited from the select"
  );
  isnot(
    popupFont,
    secondItemFont,
    "Second menuitem's font should be the author specified one"
  );

  await hideSelectPopup(selectPopup, "escape");
  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_scrollbar_props() {
  let { tab, selectPopup } = await openSelectPopup(
    gSelects.SELECT_SCROLLBAR_PROPS
  );

  let popupStyle = getComputedStyle(selectPopup);
  is(popupStyle.getPropertyValue("--content-select-scrollbar-width"), "thin");
  is(popupStyle.scrollbarColor, "rgb(255, 0, 0) rgb(0, 0, 255)");

  let scrollBoxStyle = getComputedStyle(selectPopup.scrollBox.scrollbox);
  is(scrollBoxStyle.overflow, "auto", "Should be the scrollable box");
  is(scrollBoxStyle.scrollbarWidth, "thin");
  is(scrollBoxStyle.scrollbarColor, "rgb(255, 0, 0) rgb(0, 0, 255)");

  await hideSelectPopup(selectPopup, "escape");
  BrowserTestUtils.removeTab(tab);
});

if (AppConstants.isPlatformAndVersionAtLeast("win", "10")) {
  add_task(async function test_darkmode() {
    // Force dark mode:
    await SpecialPowers.pushPrefEnv({ set: [["ui.systemUsesDarkTheme", 1]] });

    // Determine colours from the main context menu:
    let cs = getComputedStyle(document.documentElement);
    let selectColor = rgbaToString(
      InspectorUtils.colorToRGBA(cs.getPropertyValue("--menu-color"))
    );
    let selectBgColor = rgbaToString(
      InspectorUtils.colorToRGBA(cs.getPropertyValue("--menu-background-color"))
    );

    // Check that by default, we use the dark mode styles:
    let { tab, selectPopup } = await openSelectPopup(gSelects.DEFAULT_DARKMODE);

    await testSelectColors("DEFAULT_DARKMODE", 3, {
      selectColor,
      selectBgColor,
    });

    await hideSelectPopup(selectPopup, "escape");
    BrowserTestUtils.removeTab(tab);

    ({ tab, selectPopup } = await openSelectPopup(
      gSelects.IDENTICAL_BG_DIFF_FG_OPTION_DARKMODE
    ));

    // Custom styling on the options enforces using the select styling, too,
    // even if it matched the UA style. They'll be overridden on individual
    // options where necessary.
    await testSelectColors("IDENTICAL_BG_DIFF_FG_OPTION_DARKMODE", 3, {
      selectColor,
      selectBgColor,
    });

    await hideSelectPopup(selectPopup, "escape");
    BrowserTestUtils.removeTab(tab);

    ({ tab, selectPopup } = await openSelectPopup(
      gSelects.SPLIT_FG_BG_OPTION_DARKMODE
    ));

    // Like the previous case, but here the bg colour is defined on the
    // select, and the fg colour on the option. The behaviour should be the
    // same.
    await testSelectColors("SPLIT_FG_BG_OPTION_DARKMODE", 3, {
      selectColor,
      selectBgColor,
    });

    await hideSelectPopup(selectPopup, "escape");
    BrowserTestUtils.removeTab(tab);
  });
}
