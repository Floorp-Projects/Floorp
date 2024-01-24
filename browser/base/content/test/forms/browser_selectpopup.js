/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

// This test tests <select> in a child process. This is different than
// single-process as a <menulist> is used to implement the dropdown list.

// FIXME(bug 1774835): This test should be split.
requestLongerTimeout(2);

const XHTML_DTD =
  '<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.1//EN" "http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd">';

const PAGECONTENT =
  "<html xmlns='http://www.w3.org/1999/xhtml'>" +
  "<body onload='gChangeEvents = 0;gInputEvents = 0; gClickEvents = 0; document.getElementById(\"select\").focus();'>" +
  "<select id='select' oninput='gInputEvents++' onchange='gChangeEvents++' onclick='if (event.target == this) gClickEvents++'>" +
  "  <optgroup label='First Group'>" +
  "    <option value='One'>One</option>" +
  "    <option value='Two'>Two</option>" +
  "  </optgroup>" +
  "  <option value='Three'>Three</option>" +
  "  <optgroup label='Second Group' disabled='true'>" +
  "    <option value='Four'>Four</option>" +
  "    <option value='Five'>Five</option>" +
  "  </optgroup>" +
  "  <option value='Six' disabled='true'>Six</option>" +
  "  <optgroup label='Third Group'>" +
  "    <option value='Seven'>   Seven  </option>" +
  "    <option value='Eight'>&nbsp;&nbsp;Eight&nbsp;&nbsp;</option>" +
  "  </optgroup></select><input />Text" +
  "</body></html>";

const PAGECONTENT_XSLT =
  "<?xml-stylesheet type='text/xml' href='#style1'?>" +
  "<xsl:stylesheet id='style1'" +
  "                version='1.0'" +
  "                xmlns:xsl='http://www.w3.org/1999/XSL/Transform'" +
  "                xmlns:html='http://www.w3.org/1999/xhtml'>" +
  "<xsl:template match='xsl:stylesheet'>" +
  PAGECONTENT +
  "</xsl:template>" +
  "</xsl:stylesheet>";

const PAGECONTENT_SMALL =
  "<html>" +
  "<body><select id='one'>" +
  "  <option value='One'>One</option>" +
  "  <option value='Two'>Two</option>" +
  "</select><select id='two'>" +
  "  <option value='Three'>Three</option>" +
  "  <option value='Four'>Four</option>" +
  "</select><select id='three'>" +
  "  <option value='Five'>Five</option>" +
  "  <option value='Six'>Six</option>" +
  "</select></body></html>";

const PAGECONTENT_GROUPS =
  "<html>" +
  "<body><select id='one'>" +
  "  <optgroup label='Group 1'>" +
  "    <option value='G1 O1'>G1 O1</option>" +
  "    <option value='G1 O2'>G1 O2</option>" +
  "    <option value='G1 O3'>G1 O3</option>" +
  "  </optgroup>" +
  "  <optgroup label='Group 2'>" +
  "    <option value='G2 O1'>G2 O4</option>" +
  "    <option value='G2 O2'>G2 O5</option>" +
  "    <option value='Hidden' style='display: none;'>Hidden</option>" +
  "  </optgroup>" +
  "</select></body></html>";

const PAGECONTENT_SOMEHIDDEN =
  "<html><head><style>.hidden { display: none; }</style></head>" +
  "<body><select id='one'>" +
  "  <option value='One' style='display: none;'>OneHidden</option>" +
  "  <option value='Two' class='hidden'>TwoHidden</option>" +
  "  <option value='Three'>ThreeVisible</option>" +
  "  <option value='Four'style='display: table;'>FourVisible</option>" +
  "  <option value='Five'>FiveVisible</option>" +
  "  <optgroup label='GroupHidden' class='hidden'>" +
  "    <option value='Four'>Six.OneHidden</option>" +
  "    <option value='Five' style='display: block;'>Six.TwoHidden</option>" +
  "  </optgroup>" +
  "  <option value='Six' class='hidden' style='display: block;'>SevenVisible</option>" +
  "</select></body></html>";

const PAGECONTENT_TRANSLATED =
  "<html><body>" +
  "<div id='div'>" +
  "<iframe id='frame' width='320' height='295' style='border: none;'" +
  "        src='data:text/html,<select id=select><option>he he he</option><option>boo boo</option><option>baz baz</option></select>'" +
  "</iframe>" +
  "</div></body></html>";

function getInputEvents() {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    return content.wrappedJSObject.gInputEvents;
  });
}

function getChangeEvents() {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    return content.wrappedJSObject.gChangeEvents;
  });
}

function getClickEvents() {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    return content.wrappedJSObject.gClickEvents;
  });
}

async function doSelectTests(contentType, content) {
  const pageUrl = "data:" + contentType + "," + encodeURIComponent(content);
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, pageUrl);

  let selectPopup = await openSelectPopup();
  let menulist = selectPopup.parentNode;

  let isWindows = navigator.platform.includes("Win");

  is(menulist.selectedIndex, 1, "Initial selection");
  is(
    selectPopup.firstElementChild.localName,
    "menucaption",
    "optgroup is caption"
  );
  is(
    selectPopup.firstElementChild.getAttribute("label"),
    "First Group",
    "optgroup label"
  );
  is(selectPopup.children[1].localName, "menuitem", "option is menuitem");
  is(selectPopup.children[1].getAttribute("label"), "One", "option label");

  EventUtils.synthesizeKey("KEY_ArrowDown");
  is(menulist.activeChild, menulist.getItemAtIndex(2), "Select item 2");
  is(menulist.selectedIndex, isWindows ? 2 : 1, "Select item 2 selectedIndex");

  EventUtils.synthesizeKey("KEY_ArrowDown");
  is(menulist.activeChild, menulist.getItemAtIndex(3), "Select item 3");
  is(menulist.selectedIndex, isWindows ? 3 : 1, "Select item 3 selectedIndex");

  EventUtils.synthesizeKey("KEY_ArrowDown");

  // On Windows, one can navigate on disabled menuitems
  is(
    menulist.activeChild,
    menulist.getItemAtIndex(9),
    "Skip optgroup header and disabled items select item 7"
  );
  is(
    menulist.selectedIndex,
    isWindows ? 9 : 1,
    "Select or skip disabled item selectedIndex"
  );

  for (let i = 0; i < 10; i++) {
    is(
      menulist.getItemAtIndex(i).disabled,
      i >= 4 && i <= 7,
      "item " + i + " disabled"
    );
  }

  EventUtils.synthesizeKey("KEY_ArrowUp");
  is(menulist.activeChild, menulist.getItemAtIndex(3), "Select item 3 again");
  is(menulist.selectedIndex, isWindows ? 3 : 1, "Select item 3 selectedIndex");

  is(await getInputEvents(), 0, "Before closed - number of input events");
  is(await getChangeEvents(), 0, "Before closed - number of change events");
  is(await getClickEvents(), 0, "Before closed - number of click events");

  EventUtils.synthesizeKey("a", { accelKey: true });
  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [{ isWindows }],
    function (args) {
      Assert.equal(
        String(content.getSelection()),
        args.isWindows ? "Text" : "",
        "Select all while popup is open"
      );
    }
  );

  // Backspace should not go back
  let handleKeyPress = function (event) {
    ok(false, "Should not get keypress event");
  };
  window.addEventListener("keypress", handleKeyPress);
  EventUtils.synthesizeKey("KEY_Backspace");
  window.removeEventListener("keypress", handleKeyPress);

  await hideSelectPopup();

  is(menulist.selectedIndex, 3, "Item 3 still selected");
  is(await getInputEvents(), 1, "After closed - number of input events");
  is(await getChangeEvents(), 1, "After closed - number of change events");
  is(await getClickEvents(), 0, "After closed - number of click events");

  // Opening and closing the popup without changing the value should not fire a change event.
  await openSelectPopup("click");
  await hideSelectPopup("escape");
  is(
    await getInputEvents(),
    1,
    "Open and close with no change - number of input events"
  );
  is(
    await getChangeEvents(),
    1,
    "Open and close with no change - number of change events"
  );
  is(
    await getClickEvents(),
    1,
    "Open and close with no change - number of click events"
  );
  EventUtils.synthesizeKey("KEY_Tab");
  EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true });
  is(
    await getInputEvents(),
    1,
    "Tab away from select with no change - number of input events"
  );
  is(
    await getChangeEvents(),
    1,
    "Tab away from select with no change - number of change events"
  );
  is(
    await getClickEvents(),
    1,
    "Tab away from select with no change - number of click events"
  );

  await openSelectPopup("click");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  await hideSelectPopup("escape");
  is(
    await getInputEvents(),
    isWindows ? 2 : 1,
    "Open and close with change - number of input events"
  );
  is(
    await getChangeEvents(),
    isWindows ? 2 : 1,
    "Open and close with change - number of change events"
  );
  is(
    await getClickEvents(),
    2,
    "Open and close with change - number of click events"
  );
  EventUtils.synthesizeKey("KEY_Tab");
  EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true });
  is(
    await getInputEvents(),
    isWindows ? 2 : 1,
    "Tab away from select with change - number of input events"
  );
  is(
    await getChangeEvents(),
    isWindows ? 2 : 1,
    "Tab away from select with change - number of change events"
  );
  is(
    await getClickEvents(),
    2,
    "Tab away from select with change - number of click events"
  );

  is(
    selectPopup.lastElementChild.previousElementSibling.label,
    "Seven",
    "Spaces collapsed"
  );
  is(
    selectPopup.lastElementChild.label,
    "\xA0\xA0Eight\xA0\xA0",
    "Non-breaking spaces not collapsed"
  );

  BrowserTestUtils.removeTab(tab);
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.forms.select.customstyling", true]],
  });
});

add_task(async function () {
  await doSelectTests("text/html", PAGECONTENT);
});

add_task(async function () {
  await doSelectTests("application/xhtml+xml", XHTML_DTD + "\n" + PAGECONTENT);
});

add_task(async function () {
  await doSelectTests("application/xml", XHTML_DTD + "\n" + PAGECONTENT_XSLT);
});

// This test opens a select popup and removes the content node of a popup while
// The popup should close if its node is removed.
add_task(async function () {
  const pageUrl = "data:text/html," + escape(PAGECONTENT_SMALL);
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, pageUrl);

  // First, try it when a different <select> element than the one that is open is removed
  const selectPopup = await openSelectPopup("click", "#one");

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    content.document.body.removeChild(content.document.getElementById("two"));
  });

  // Wait a bit just to make sure the popup won't close.
  await new Promise(resolve => setTimeout(resolve, 1000));

  is(selectPopup.state, "open", "Different popup did not affect open popup");

  await hideSelectPopup();

  // Next, try it when the same <select> element than the one that is open is removed
  await openSelectPopup("click", "#three");

  let popupHiddenPromise = BrowserTestUtils.waitForEvent(
    selectPopup,
    "popuphidden"
  );
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    content.document.body.removeChild(content.document.getElementById("three"));
  });
  await popupHiddenPromise;

  ok(true, "Popup hidden when select is removed");

  // Finally, try it when the tab is closed while the select popup is open.
  await openSelectPopup("click", "#one");

  popupHiddenPromise = BrowserTestUtils.waitForEvent(
    selectPopup,
    "popuphidden"
  );
  BrowserTestUtils.removeTab(tab);
  await popupHiddenPromise;

  ok(true, "Popup hidden when tab is closed");
});

// This test opens a select popup that is isn't a frame and has some translations applied.
add_task(async function () {
  const pageUrl = "data:text/html," + escape(PAGECONTENT_TRANSLATED);
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, pageUrl);

  // We need to explicitly call Element.focus() since dataURL is treated as
  // cross-origin, thus autofocus doesn't work there.
  const iframe = await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    return content.document.querySelector("iframe").browsingContext;
  });
  await SpecialPowers.spawn(iframe, [], async () => {
    const input = content.document.getElementById("select");
    const focusPromise = new Promise(resolve => {
      input.addEventListener("focus", resolve, { once: true });
    });
    input.focus();
    await focusPromise;
  });

  // First, get the position of the select popup when no translations have been applied.
  const selectPopup = await openSelectPopup();

  let rect = selectPopup.getBoundingClientRect();
  let expectedX = rect.left;
  let expectedY = rect.top;

  await hideSelectPopup();

  // Iterate through a set of steps which each add more translation to the select's expected position.
  let steps = [
    ["div", "transform: translateX(7px) translateY(13px);", 7, 13],
    [
      "frame",
      "border-top: 5px solid green; border-left: 10px solid red; border-right: 35px solid blue;",
      10,
      5,
    ],
    [
      "frame",
      "border: none; padding-left: 6px; padding-right: 12px; padding-top: 2px;",
      -4,
      -3,
    ],
    ["select", "margin: 9px; transform: translateY(-3px);", 9, 6],
  ];

  for (let stepIndex = 0; stepIndex < steps.length; stepIndex++) {
    let step = steps[stepIndex];

    await SpecialPowers.spawn(
      gBrowser.selectedBrowser,
      [step],
      async function (contentStep) {
        return new Promise(resolve => {
          let changedWin = content;

          let elem;
          if (contentStep[0] == "select") {
            changedWin = content.document.getElementById("frame").contentWindow;
            elem = changedWin.document.getElementById("select");
          } else {
            elem = content.document.getElementById(contentStep[0]);
          }

          changedWin.addEventListener(
            "MozAfterPaint",
            function () {
              resolve();
            },
            { once: true }
          );

          elem.style = contentStep[1];
          elem.getBoundingClientRect();
        });
      }
    );

    await openSelectPopup();

    expectedX += step[2];
    expectedY += step[3];

    let popupRect = selectPopup.getBoundingClientRect();
    is(popupRect.left, expectedX, "step " + (stepIndex + 1) + " x");
    is(popupRect.top, expectedY, "step " + (stepIndex + 1) + " y");

    await hideSelectPopup();
  }

  BrowserTestUtils.removeTab(tab);
});

// Test that we get the right events when a select popup is changed.
add_task(async function test_event_order() {
  const URL = "data:text/html," + escape(PAGECONTENT_SMALL);
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: URL,
    },
    async function (browser) {
      // According to https://html.spec.whatwg.org/#the-select-element,
      // we want to fire input, change, and then click events on the
      // <select> (in that order) when it has changed.
      let expectedEnter = [
        {
          type: "input",
          cancelable: false,
          targetIsOption: false,
          composed: true,
        },
        {
          type: "change",
          cancelable: false,
          targetIsOption: false,
          composed: false,
        },
      ];

      let expectedClick = [
        {
          type: "mousedown",
          cancelable: true,
          targetIsOption: true,
          composed: true,
        },
        {
          type: "mouseup",
          cancelable: true,
          targetIsOption: true,
          composed: true,
        },
        {
          type: "input",
          cancelable: false,
          targetIsOption: false,
          composed: true,
        },
        {
          type: "change",
          cancelable: false,
          targetIsOption: false,
          composed: false,
        },
        {
          type: "click",
          cancelable: true,
          targetIsOption: true,
          composed: true,
        },
      ];

      for (let mode of ["enter", "click"]) {
        let expected = mode == "enter" ? expectedEnter : expectedClick;
        await openSelectPopup("click", mode == "enter" ? "#one" : "#two");

        let eventsPromise = SpecialPowers.spawn(
          browser,
          [[mode, expected]],
          async function ([contentMode, contentExpected]) {
            return new Promise(resolve => {
              function onEvent(event) {
                select.removeEventListener(event.type, onEvent);
                Assert.ok(
                  contentExpected.length,
                  "Unexpected event " + event.type
                );
                let expectation = contentExpected.shift();
                Assert.equal(
                  event.type,
                  expectation.type,
                  "Expected the right event order"
                );
                Assert.ok(event.bubbles, "All of these events should bubble");
                Assert.equal(
                  event.cancelable,
                  expectation.cancelable,
                  "Cancellation property should match"
                );
                Assert.equal(
                  event.target.localName,
                  expectation.targetIsOption ? "option" : "select",
                  "Target matches"
                );
                Assert.equal(
                  event.composed,
                  expectation.composed,
                  "Composed property should match"
                );
                if (!contentExpected.length) {
                  resolve();
                }
              }

              let select = content.document.getElementById(
                contentMode == "enter" ? "one" : "two"
              );
              for (let event of [
                "input",
                "change",
                "mousedown",
                "mouseup",
                "click",
              ]) {
                select.addEventListener(event, onEvent);
              }
            });
          }
        );

        EventUtils.synthesizeKey("KEY_ArrowDown");
        await hideSelectPopup(mode);
        await eventsPromise;
      }
    }
  );
});

async function performSelectSearchTests(win) {
  let browser = win.gBrowser.selectedBrowser;
  await SpecialPowers.spawn(browser, [], async function () {
    let doc = content.document;
    let select = doc.getElementById("one");

    for (var i = 0; i < 40; i++) {
      select.add(new content.Option("Test" + i));
    }

    select.options[1].selected = true;
    select.focus();
  });

  let selectPopup = await openSelectPopup(false, "select", win);

  let searchElement = selectPopup.querySelector(
    ".contentSelectDropdown-searchbox"
  );
  searchElement.focus();

  EventUtils.synthesizeKey("O", {}, win);
  is(selectPopup.children[2].hidden, false, "First option should be visible");
  is(selectPopup.children[3].hidden, false, "Second option should be visible");

  EventUtils.synthesizeKey("3", {}, win);
  is(selectPopup.children[2].hidden, true, "First option should be hidden");
  is(selectPopup.children[3].hidden, true, "Second option should be hidden");
  is(selectPopup.children[4].hidden, false, "Third option should be visible");

  EventUtils.synthesizeKey("Z", {}, win);
  is(selectPopup.children[4].hidden, true, "Third option should be hidden");
  is(
    selectPopup.children[1].hidden,
    true,
    "First group header should be hidden"
  );

  EventUtils.synthesizeKey("KEY_Backspace", {}, win);
  is(selectPopup.children[4].hidden, false, "Third option should be visible");

  EventUtils.synthesizeKey("KEY_Backspace", {}, win);
  is(
    selectPopup.children[5].hidden,
    false,
    "Second group header should be visible"
  );

  EventUtils.synthesizeKey("KEY_Backspace", {}, win);
  EventUtils.synthesizeKey("O", {}, win);
  EventUtils.synthesizeKey("5", {}, win);
  is(
    selectPopup.children[5].hidden,
    false,
    "Second group header should be visible"
  );
  is(
    selectPopup.children[1].hidden,
    true,
    "First group header should be hidden"
  );

  EventUtils.synthesizeKey("KEY_Backspace", {}, win);
  is(
    selectPopup.children[1].hidden,
    false,
    "First group header should be shown"
  );

  EventUtils.synthesizeKey("KEY_Backspace", {}, win);
  is(
    selectPopup.children[8].hidden,
    true,
    "Option hidden by content should remain hidden"
  );

  await hideSelectPopup("escape", win);
}

// This test checks the functionality of search in select elements with groups
// and a large number of options.
add_task(async function test_select_search() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.forms.selectSearch", true]],
  });
  const pageUrl = "data:text/html," + escape(PAGECONTENT_GROUPS);
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, pageUrl);

  await performSelectSearchTests(window);

  BrowserTestUtils.removeTab(tab);

  await SpecialPowers.popPrefEnv();
});

// This test checks that a mousemove event is fired correctly at the menu and
// not at the browser, ensuring that any mouse capture has been cleared.
add_task(async function test_mousemove_correcttarget() {
  const pageUrl = "data:text/html," + escape(PAGECONTENT_SMALL);
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, pageUrl);

  const selectPopup = await openSelectPopup("mousedown");

  await new Promise(resolve => {
    window.addEventListener(
      "mousemove",
      function (event) {
        is(event.target.localName.indexOf("menu"), 0, "mouse over menu");
        resolve();
      },
      { capture: true, once: true }
    );

    EventUtils.synthesizeMouseAtCenter(selectPopup.firstElementChild, {
      type: "mousemove",
      buttons: 1,
    });
  });

  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#one",
    { type: "mouseup" },
    gBrowser.selectedBrowser
  );

  await hideSelectPopup();

  // The popup should be closed when fullscreen mode is entered or exited.
  for (let steps = 0; steps < 2; steps++) {
    await openSelectPopup("click");
    let popupHiddenPromise = BrowserTestUtils.waitForEvent(
      selectPopup,
      "popuphidden"
    );
    let sizeModeChanged = BrowserTestUtils.waitForEvent(
      window,
      "sizemodechange"
    );
    BrowserFullScreen();
    await sizeModeChanged;
    await popupHiddenPromise;
  }

  BrowserTestUtils.removeTab(tab);
});

// This test checks when a <select> element has some options with altered display values.
add_task(async function test_somehidden() {
  const pageUrl = "data:text/html," + escape(PAGECONTENT_SOMEHIDDEN);
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, pageUrl);

  let selectPopup = await openSelectPopup("click");

  // The exact number is not needed; just ensure the height is larger than 4 items to accommodate any popup borders.
  Assert.greaterOrEqual(
    selectPopup.getBoundingClientRect().height,
    selectPopup.lastElementChild.getBoundingClientRect().height * 4,
    "Height contains at least 4 items"
  );
  Assert.less(
    selectPopup.getBoundingClientRect().height,
    selectPopup.lastElementChild.getBoundingClientRect().height * 5,
    "Height doesn't contain 5 items"
  );

  // The label contains the substring 'Visible' for items that are visible.
  // Otherwise, it is expected to be display: none.
  is(selectPopup.parentNode.itemCount, 9, "Correct number of items");
  let child = selectPopup.firstElementChild;
  let idx = 1;
  while (child) {
    is(
      getComputedStyle(child).display,
      child.label.indexOf("Visible") > 0 ? "flex" : "none",
      "Item " + idx++ + " is visible"
    );
    child = child.nextElementSibling;
  }

  await hideSelectPopup("escape");
  BrowserTestUtils.removeTab(tab);
});

// This test checks that the popup is closed when the select element is blurred.
add_task(async function test_blur_hides_popup() {
  const pageUrl = "data:text/html," + escape(PAGECONTENT_SMALL);
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, pageUrl);

  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    content.addEventListener(
      "blur",
      function (event) {
        event.preventDefault();
        event.stopPropagation();
      },
      true
    );

    content.document.getElementById("one").focus();
  });

  let selectPopup = await openSelectPopup();

  let popupHiddenPromise = BrowserTestUtils.waitForEvent(
    selectPopup,
    "popuphidden"
  );

  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    content.document.getElementById("one").blur();
  });

  await popupHiddenPromise;

  ok(true, "Blur closed popup");

  BrowserTestUtils.removeTab(tab);
});

// Test zoom handling.
add_task(async function test_zoom() {
  const pageUrl = "data:text/html," + escape(PAGECONTENT_SMALL);
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, pageUrl);

  info("Opening the popup");
  const selectPopup = await openSelectPopup("click");

  info("Opened the popup");
  let nonZoomedFontSize = parseFloat(
    getComputedStyle(selectPopup.querySelector("menuitem")).fontSize,
    10
  );

  info("font-size is " + nonZoomedFontSize);
  await hideSelectPopup();

  info("Hide the popup");

  for (let i = 0; i < 2; ++i) {
    info("Testing with full zoom: " + ZoomManager.useFullZoom);

    // This is confusing, but does the right thing.
    FullZoom.setZoom(2.0, tab.linkedBrowser);

    info("Opening popup again");
    await openSelectPopup("click");

    let zoomedFontSize = parseFloat(
      getComputedStyle(selectPopup.querySelector("menuitem")).fontSize,
      10
    );
    info("Zoomed font-size is " + zoomedFontSize);

    Assert.less(
      Math.abs(zoomedFontSize - nonZoomedFontSize * 2.0),
      0.01,
      `Zoom should affect menu popup size, got ${zoomedFontSize}, ` +
        `expected ${nonZoomedFontSize * 2.0}`
    );

    await hideSelectPopup();
    info("Hid the popup again");

    ZoomManager.toggleZoom();
  }

  FullZoom.setZoom(1.0, tab.linkedBrowser); // make sure the zoom level is reset
  BrowserTestUtils.removeTab(tab);
});

// Test that input and change events are dispatched consistently (bug 1561882).
add_task(async function test_event_destroys_popup() {
  const PAGE_CONTENT = `
<!doctype html>
<select>
  <option>a</option>
  <option>b</option>
</select>
<script>
gChangeEvents = 0;
gInputEvents = 0;
let select = document.querySelector("select");
  select.addEventListener("input", function() {
    gInputEvents++;
    this.style.display = "none";
    this.getBoundingClientRect();
  })
  select.addEventListener("change", function() {
    gChangeEvents++;
  })
</script>`;

  const pageUrl = "data:text/html," + escape(PAGE_CONTENT);
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, pageUrl);

  // Test change and input events get handled consistently
  await openSelectPopup("click");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  await hideSelectPopup();

  is(
    await getChangeEvents(),
    1,
    "Should get change and input events consistently"
  );
  is(
    await getInputEvents(),
    1,
    "Should get change and input events consistently (input)"
  );

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_label_not_text() {
  const PAGE_CONTENT = `
<!doctype html>
<select>
  <option label="Some nifty Label">Some Element Text Instead</option>
  <option label="">Element Text</option>
</select>
`;

  const pageUrl = "data:text/html," + escape(PAGE_CONTENT);
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, pageUrl);

  const selectPopup = await openSelectPopup("click");

  is(
    selectPopup.children[0].label,
    "Some nifty Label",
    "Use the label not the text."
  );

  is(
    selectPopup.children[1].label,
    "Element Text",
    "Uses the text if the label is empty, like HTMLOptionElement::GetRenderedLabel."
  );

  BrowserTestUtils.removeTab(tab);
});
