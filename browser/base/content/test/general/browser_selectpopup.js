/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This test checks that a <select> with an <optgroup> opens and can be navigated
// in a child process. This is different than single-process as a <menulist> is used
// to implement the dropdown list.

const XHTML_DTD = '<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.1//EN" "http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd">';

const PAGECONTENT =
  "<html xmlns='http://www.w3.org/1999/xhtml'>" +
  "<body onload='gChangeEvents = 0;gInputEvents = 0; document.body.firstChild.focus()'><select oninput='gInputEvents++' onchange='gChangeEvents++'>" +
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

const PAGECONTENT_TRANSLATED =
  "<html><body>" +
  "<div id='div'>" +
  "<iframe id='frame' width='320' height='295' style='border: none;'" +
  "        src='data:text/html,<select id=select autofocus><option>he he he</option><option>boo boo</option><option>baz baz</option></select>'" +
  "</iframe>" +
  "</div></body></html>";

function openSelectPopup(selectPopup, withMouse, selector = "select")
{
  let popupShownPromise = BrowserTestUtils.waitForEvent(selectPopup, "popupshown");

  if (withMouse) {
    return Promise.all([popupShownPromise,
                        BrowserTestUtils.synthesizeMouseAtCenter(selector, { }, gBrowser.selectedBrowser)]);
  }

  EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true, code: "ArrowDown" });
  return popupShownPromise;
}

function hideSelectPopup(selectPopup, mode = "enter")
{
  let popupHiddenPromise = BrowserTestUtils.waitForEvent(selectPopup, "popuphidden");

  if (mode == "escape") {
    EventUtils.synthesizeKey("KEY_Escape", { code: "Escape" });
  }
  else if (mode == "enter") {
    EventUtils.synthesizeKey("KEY_Enter", { code: "Enter" });
  }
  else if (mode == "click") {
    EventUtils.synthesizeMouseAtCenter(selectPopup.lastChild, { });
  }

  return popupHiddenPromise;
}

function getInputEvents()
{
  return ContentTask.spawn(gBrowser.selectedBrowser, {}, function() {
    return content.wrappedJSObject.gInputEvents;
  });
}

function getChangeEvents()
{
  return ContentTask.spawn(gBrowser.selectedBrowser, {}, function() {
    return content.wrappedJSObject.gChangeEvents;
  });
}

function doSelectTests(contentType, dtd)
{
  const pageUrl = "data:" + contentType + "," + escape(dtd + "\n" + PAGECONTENT);
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, pageUrl);

  let menulist = document.getElementById("ContentSelectDropdown");
  let selectPopup = menulist.menupopup;

  yield openSelectPopup(selectPopup);

  let isWindows = navigator.platform.indexOf("Win") >= 0;

  is(menulist.selectedIndex, 1, "Initial selection");
  is(selectPopup.firstChild.localName, "menucaption", "optgroup is caption");
  is(selectPopup.firstChild.getAttribute("label"), "First Group", "optgroup label");
  is(selectPopup.childNodes[1].localName, "menuitem", "option is menuitem");
  is(selectPopup.childNodes[1].getAttribute("label"), "One", "option label");

  EventUtils.synthesizeKey("KEY_ArrowDown", { code: "ArrowDown" });
  is(menulist.menuBoxObject.activeChild, menulist.getItemAtIndex(2), "Select item 2");
  is(menulist.selectedIndex, isWindows ? 2 : 1, "Select item 2 selectedIndex");

  EventUtils.synthesizeKey("KEY_ArrowDown", { code: "ArrowDown" });
  is(menulist.menuBoxObject.activeChild, menulist.getItemAtIndex(3), "Select item 3");
  is(menulist.selectedIndex, isWindows ? 3 : 1, "Select item 3 selectedIndex");

  EventUtils.synthesizeKey("KEY_ArrowDown", { code: "ArrowDown" });

  // On Windows, one can navigate on disabled menuitems
  let expectedIndex = isWindows ? 5 : 9;

  is(menulist.menuBoxObject.activeChild, menulist.getItemAtIndex(expectedIndex),
     "Skip optgroup header and disabled items select item 7");
  is(menulist.selectedIndex, isWindows ? 5 : 1, "Select or skip disabled item selectedIndex");

  for (let i = 0; i < 10; i++) {
    is(menulist.getItemAtIndex(i).disabled, i >= 4 && i <= 7, "item " + i + " disabled")
  }

  EventUtils.synthesizeKey("KEY_ArrowUp", { code: "ArrowUp" });
  is(menulist.menuBoxObject.activeChild, menulist.getItemAtIndex(3), "Select item 3 again");
  is(menulist.selectedIndex, isWindows ? 3 : 1, "Select item 3 selectedIndex");

  is((yield getInputEvents()), 0, "Before closed - number of input events");
  is((yield getChangeEvents()), 0, "Before closed - number of change events");

  EventUtils.synthesizeKey("a", { accelKey: true });
  yield ContentTask.spawn(gBrowser.selectedBrowser, { isWindows }, function(args) {
    Assert.equal(String(content.getSelection()), args.isWindows ? "Text" : "",
      "Select all while popup is open");
  });

  yield hideSelectPopup(selectPopup);

  is(menulist.selectedIndex, 3, "Item 3 still selected");
  is((yield getInputEvents()), 1, "After closed - number of input events");
  is((yield getChangeEvents()), 1, "After closed - number of change events");

  // Opening and closing the popup without changing the value should not fire a change event.
  yield openSelectPopup(selectPopup, true);
  yield hideSelectPopup(selectPopup, "escape");
  is((yield getInputEvents()), 1, "Open and close with no change - number of input events");
  is((yield getChangeEvents()), 1, "Open and close with no change - number of change events");
  EventUtils.synthesizeKey("VK_TAB", { });
  EventUtils.synthesizeKey("VK_TAB", { shiftKey: true });
  is((yield getInputEvents()), 1, "Tab away from select with no change - number of input events");
  is((yield getChangeEvents()), 1, "Tab away from select with no change - number of change events");

  yield openSelectPopup(selectPopup, true);
  EventUtils.synthesizeKey("KEY_ArrowDown", { code: "ArrowDown" });
  yield hideSelectPopup(selectPopup, "escape");
  is((yield getInputEvents()), isWindows ? 2 : 1, "Open and close with change - number of input events");
  is((yield getChangeEvents()), isWindows ? 2 : 1, "Open and close with change - number of change events");
  EventUtils.synthesizeKey("VK_TAB", { });
  EventUtils.synthesizeKey("VK_TAB", { shiftKey: true });
  is((yield getInputEvents()), isWindows ? 2 : 1, "Tab away from select with change - number of input events");
  is((yield getChangeEvents()), isWindows ? 2 : 1, "Tab away from select with change - number of change events");

  is(selectPopup.lastChild.previousSibling.label, "Seven", "Spaces collapsed");
  is(selectPopup.lastChild.label, "\xA0\xA0Eight\xA0\xA0", "Non-breaking spaces not collapsed");

  yield BrowserTestUtils.removeTab(tab);
}

add_task(function*() {
  yield doSelectTests("text/html", "");
});

add_task(function*() {
  yield doSelectTests("application/xhtml+xml", XHTML_DTD);
});

// This test opens a select popup and removes the content node of a popup while
// The popup should close if its node is removed.
add_task(function*() {
  const pageUrl = "data:text/html," + escape(PAGECONTENT_SMALL);
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, pageUrl);

  let menulist = document.getElementById("ContentSelectDropdown");
  let selectPopup = menulist.menupopup;

  // First, try it when a different <select> element than the one that is open is removed
  yield openSelectPopup(selectPopup, true, "#one");

  yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function() {
    content.document.body.removeChild(content.document.getElementById("two"));
  });

  // Wait a bit just to make sure the popup won't close.
  yield new Promise(resolve => setTimeout(resolve, 1000));

  is(selectPopup.state, "open", "Different popup did not affect open popup");

  yield hideSelectPopup(selectPopup);

  // Next, try it when the same <select> element than the one that is open is removed
  yield openSelectPopup(selectPopup, true, "#three");

  let popupHiddenPromise = BrowserTestUtils.waitForEvent(selectPopup, "popuphidden");
  yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function() {
    content.document.body.removeChild(content.document.getElementById("three"));
  });
  yield popupHiddenPromise;

  ok(true, "Popup hidden when select is removed");

  // Finally, try it when the tab is closed while the select popup is open.
  yield openSelectPopup(selectPopup, true, "#one");

  popupHiddenPromise = BrowserTestUtils.waitForEvent(selectPopup, "popuphidden");
  yield BrowserTestUtils.removeTab(tab);
  yield popupHiddenPromise;

  ok(true, "Popup hidden when tab is closed");
});

// This test opens a select popup that is isn't a frame and has some translations applied.
add_task(function*() {
  const pageUrl = "data:text/html," + escape(PAGECONTENT_TRANSLATED);
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, pageUrl);

  let menulist = document.getElementById("ContentSelectDropdown");
  let selectPopup = menulist.menupopup;

  // First, get the position of the select popup when no translations have been applied.
  yield openSelectPopup(selectPopup, false);

  let rect = selectPopup.getBoundingClientRect();
  let expectedX = rect.left;
  let expectedY = rect.top;

  yield hideSelectPopup(selectPopup);

  // Iterate through a set of steps which each add more translation to the select's expected position.
  let steps = [
    [ "div", "transform: translateX(7px) translateY(13px);", 7, 13 ],
    [ "frame", "border-top: 5px solid green; border-left: 10px solid red; border-right: 35px solid blue;", 10, 5 ],
    [ "frame", "border: none; padding-left: 6px; padding-right: 12px; padding-top: 2px;", -4, -3 ],
    [ "select", "margin: 9px; transform: translateY(-3px);", 9, 6 ],
  ];

  for (let stepIndex = 0; stepIndex < steps.length; stepIndex++) {
    let step = steps[stepIndex];

    yield ContentTask.spawn(gBrowser.selectedBrowser, step, function*(step) {
      return new Promise(resolve => {
        let changedWin = content;

        let elem;
        if (step[0] == "select") {
          changedWin = content.document.getElementById("frame").contentWindow;
          elem = changedWin.document.getElementById("select");
        }
        else {
          elem = content.document.getElementById(step[0]);
        }

        changedWin.addEventListener("MozAfterPaint", function onPaint() {
          changedWin.removeEventListener("MozAfterPaint", onPaint);
          resolve();
        });

        elem.style = step[1];
      });
    });

    yield openSelectPopup(selectPopup, false);

    expectedX += step[2];
    expectedY += step[3];

    let rect = selectPopup.getBoundingClientRect();
    is(rect.left, expectedX, "step " + (stepIndex + 1) + " x");
    is(rect.top, expectedY, "step " + (stepIndex + 1) + " y");

    yield hideSelectPopup(selectPopup);
  }

  yield BrowserTestUtils.removeTab(tab);
});

// Test that we get the right events when a select popup is changed.
add_task(function* test_event_order() {
  const URL = "data:text/html," + escape(PAGECONTENT_SMALL);
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: URL,
  }, function*(browser) {
    let menulist = document.getElementById("ContentSelectDropdown");
    let selectPopup = menulist.menupopup;

    // According to https://html.spec.whatwg.org/#the-select-element,
    // we want to fire input, change, and then click events on the
    // <select> (in that order) when it has changed.
    let expectedEnter = [
      {
        type: "input",
        cancelable: false,
        targetIsOption: false,
      },
      {
        type: "change",
        cancelable: false,
        targetIsOption: false,
      },
    ];

    let expectedClick = [
      {
        type: "input",
        cancelable: false,
        targetIsOption: false,
      },
      {
        type: "change",
        cancelable: false,
        targetIsOption: false,
      },
      {
        type: "mousedown",
        cancelable: true,
        targetIsOption: true,
      },
      {
        type: "mouseup",
        cancelable: true,
        targetIsOption: true,
      },
      {
        type: "click",
        cancelable: true,
        targetIsOption: true,
      },
    ];

    for (let mode of ["enter", "click"]) {
      let expected = mode == "enter" ? expectedEnter : expectedClick;
      yield openSelectPopup(selectPopup, true, mode == "enter" ? "#one" : "#two");

      let eventsPromise = ContentTask.spawn(browser, { mode, expected }, function*(args) {
        let expected = args.expected;

        return new Promise((resolve) => {
          function onEvent(event) {
            select.removeEventListener(event.type, onEvent);
            Assert.ok(expected.length, "Unexpected event " + event.type);
            let expectation = expected.shift();
            Assert.equal(event.type, expectation.type,
                         "Expected the right event order");
            Assert.ok(event.bubbles, "All of these events should bubble");
            Assert.equal(event.cancelable, expectation.cancelable,
                         "Cancellation property should match");
            Assert.equal(event.target.localName,
                         expectation.targetIsOption ? "option" : "select",
                         "Target matches");
            if (!expected.length) {
              resolve();
            }
          }

          let select = content.document.getElementById(args.mode == "enter" ? "one" : "two");
          for (let event of ["input", "change", "mousedown", "mouseup", "click"]) {
            select.addEventListener(event, onEvent);
          }
        });
      });

      EventUtils.synthesizeKey("KEY_ArrowDown", { code: "ArrowDown" });
      yield hideSelectPopup(selectPopup, mode);
      yield eventsPromise;
    }
  });
});

// This test checks select elements with a large number of options to ensure that
// the popup appears within the browser area.
add_task(function* test_large_popup() {
  const pageUrl = "data:text/html," + escape(PAGECONTENT_SMALL);
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, pageUrl);

  yield ContentTask.spawn(tab.linkedBrowser, null, function*() {
    let doc = content.document;
    let select = doc.getElementById("one");
    for (var i = 0; i < 180; i++) {
      select.add(new content.Option("Test" + i));
    }

    select.options[60].selected = true;
    select.focus();
  });

  let selectPopup = document.getElementById("ContentSelectDropdown").menupopup;
  let browserRect = tab.linkedBrowser.getBoundingClientRect();

  let positions = [
    "margin-top: 300px;",
    "position: fixed; bottom: 100px;",
    "width: 100%; height: 9999px;"
  ];

  let position;
  while (true) {
    yield openSelectPopup(selectPopup, false);

    let rect = selectPopup.getBoundingClientRect();
    ok(rect.top >= browserRect.top, "Popup top position in within browser area");
    ok(rect.bottom <= browserRect.bottom, "Popup bottom position in within browser area");

    // Don't check the scroll position for the last step as the popup will be cut off.
    if (positions.length == 1) {
      let cs = window.getComputedStyle(selectPopup);
      let bpBottom = parseFloat(cs.paddingBottom) + parseFloat(cs.borderBottomWidth);

      is(selectPopup.childNodes[60].getBoundingClientRect().bottom,
         selectPopup.getBoundingClientRect().bottom - bpBottom,
         "Popup scroll at correct position " + bpBottom);
    }

    yield hideSelectPopup(selectPopup);

    position = positions.shift();
    if (!position) {
      break;
    }

    let contentPainted = BrowserTestUtils.contentPainted(tab.linkedBrowser);
    yield ContentTask.spawn(tab.linkedBrowser, position, function*(position) {
      let select = content.document.getElementById("one");
      select.setAttribute("style", position);
    });
    yield contentPainted;
  }

  yield BrowserTestUtils.removeTab(tab);
});

// This test checks that a mousemove event is fired correctly at the menu and
// not at the browser, ensuring that any mouse capture has been cleared.
add_task(function* test_mousemove_correcttarget() {
  const pageUrl = "data:text/html," + escape(PAGECONTENT_SMALL);
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, pageUrl);

  let selectPopup = document.getElementById("ContentSelectDropdown").menupopup;

  let popupShownPromise = BrowserTestUtils.waitForEvent(selectPopup, "popupshown");
  yield BrowserTestUtils.synthesizeMouseAtCenter("#one", { type: "mousedown" }, gBrowser.selectedBrowser);
  yield popupShownPromise;

  yield new Promise(resolve => {
    window.addEventListener("mousemove", function checkForMouseMove(event) {
      window.removeEventListener("mousemove", checkForMouseMove, true);
      is(event.target.localName.indexOf("menu"), 0, "mouse over menu");
      resolve();
    }, true);

    EventUtils.synthesizeMouseAtCenter(selectPopup.firstChild, { type: "mousemove" });
  });

  yield BrowserTestUtils.synthesizeMouseAtCenter("#one", { type: "mouseup" }, gBrowser.selectedBrowser);

  yield hideSelectPopup(selectPopup);

  yield BrowserTestUtils.removeTab(tab);
});
