/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Unit tests for the PanelMultiView module.
 */

const { PanelMultiView } = ChromeUtils.import(
  "resource:///modules/PanelMultiView.jsm"
);

const PANELS_COUNT = 2;
let gPanelAnchors = [];
let gPanels = [];
let gPanelMultiViews = [];

const PANELVIEWS_COUNT = 4;
let gPanelViews = [];
let gPanelViewLabels = [];

const EVENT_TYPES = [
  "popupshown",
  "popuphidden",
  "PanelMultiViewHidden",
  "ViewShowing",
  "ViewShown",
  "ViewHiding",
];

/**
 * Checks that the element is displayed, including the state of the popup where
 * the element is located. This can trigger a synchronous reflow if necessary,
 * because even though the code under test is designed to avoid synchronous
 * reflows, it can raise completion events while a layout flush is still needed.
 *
 * In production code, event handlers for ViewShown have to wait for a flush if
 * they need to read style or layout information, like other code normally does.
 */
function is_visible(element) {
  var style = element.ownerGlobal.getComputedStyle(element);
  if (style.display == "none") {
    return false;
  }
  if (style.visibility != "visible") {
    return false;
  }
  if (style.display == "-moz-popup" && element.state != "open") {
    return false;
  }

  // Hiding a parent element will hide all its children
  if (element.parentNode != element.ownerDocument) {
    return is_visible(element.parentNode);
  }

  return true;
}

/**
 * Checks whether the label in the specified view is visible.
 */
function assertLabelVisible(viewIndex, expectedVisible) {
  Assert.equal(
    is_visible(gPanelViewLabels[viewIndex]),
    expectedVisible,
    `Visibility of label in view ${viewIndex}`
  );
}

/**
 * Opens the specified view as the main view in the specified panel.
 */
async function openPopup(panelIndex, viewIndex) {
  gPanelMultiViews[panelIndex].setAttribute(
    "mainViewId",
    gPanelViews[viewIndex].id
  );

  let promiseShown = BrowserTestUtils.waitForEvent(
    gPanelViews[viewIndex],
    "ViewShown"
  );
  PanelMultiView.openPopup(
    gPanels[panelIndex],
    gPanelAnchors[panelIndex],
    "bottomright topright"
  );
  await promiseShown;

  Assert.ok(PanelView.forNode(gPanelViews[viewIndex]).active);
  assertLabelVisible(viewIndex, true);
}

/**
 * Closes the specified panel.
 */
async function hidePopup(panelIndex) {
  gPanelMultiViews[panelIndex].setAttribute(
    "mainViewId",
    gPanelViews[panelIndex].id
  );

  let promiseHidden = BrowserTestUtils.waitForEvent(
    gPanels[panelIndex],
    "popuphidden"
  );
  PanelMultiView.hidePopup(gPanels[panelIndex]);
  await promiseHidden;
}

/**
 * Opens the specified subview in the specified panel.
 */
async function showSubView(panelIndex, viewIndex) {
  let promiseShown = BrowserTestUtils.waitForEvent(
    gPanelViews[viewIndex],
    "ViewShown"
  );
  gPanelMultiViews[panelIndex].showSubView(gPanelViews[viewIndex]);
  await promiseShown;

  Assert.ok(PanelView.forNode(gPanelViews[viewIndex]).active);
  assertLabelVisible(viewIndex, true);
}

/**
 * Navigates backwards to the specified view, which is displayed as a result.
 */
async function goBack(panelIndex, viewIndex) {
  let promiseShown = BrowserTestUtils.waitForEvent(
    gPanelViews[viewIndex],
    "ViewShown"
  );
  gPanelMultiViews[panelIndex].goBack();
  await promiseShown;

  Assert.ok(PanelView.forNode(gPanelViews[viewIndex]).active);
  assertLabelVisible(viewIndex, true);
}

/**
 * Records the specified events on an element into the specified array. An
 * optional callback can be used to respond to events and trigger nested events.
 */
function recordEvents(
  element,
  eventTypes,
  recordArray,
  eventCallback = () => {}
) {
  let nestedEvents = [];
  element.recorders = eventTypes.map(eventType => {
    let recorder = {
      eventType,
      listener(event) {
        let eventString =
          nestedEvents.join("") + `${event.originalTarget.id}: ${event.type}`;
        info(`Event on ${eventString}`);
        recordArray.push(eventString);
        // Any synchronous event triggered from within the given callback will
        // include information about the current event.
        nestedEvents.unshift(`${eventString} > `);
        eventCallback(event);
        nestedEvents.shift();
      },
    };
    element.addEventListener(recorder.eventType, recorder.listener);
    return recorder;
  });
}

/**
 * Stops recording events on an element.
 */
function stopRecordingEvents(element) {
  for (let recorder of element.recorders) {
    element.removeEventListener(recorder.eventType, recorder.listener);
  }
  delete element.recorders;
}

/**
 * Sets up the elements in the browser window that will be used by all the other
 * regression tests. Since the panel and view elements can live anywhere in the
 * document, they are simply added to the same toolbar as the panel anchors.
 *
 * <toolbar id="nav-bar">
 *     <toolbarbutton/>      ->  gPanelAnchors[panelIndex]
 *     <panel>               ->  gPanels[panelIndex]
 *       <panelmultiview/>   ->  gPanelMultiViews[panelIndex]
 *     </panel>
 *     <panelview>           ->  gPanelViews[viewIndex]
 *       <label/>            ->  gPanelViewLabels[viewIndex]
 *     </panelview>
 * </toolbar>
 */
add_task(async function test_setup() {
  let navBar = document.getElementById("nav-bar");

  for (let i = 0; i < PANELS_COUNT; i++) {
    gPanelAnchors[i] = document.createXULElement("toolbarbutton");
    gPanelAnchors[i].classList.add(
      "toolbarbutton-1",
      "chromeclass-toolbar-additional"
    );
    navBar.appendChild(gPanelAnchors[i]);

    gPanels[i] = document.createXULElement("panel");
    gPanels[i].id = "panel-" + i;
    gPanels[i].setAttribute("type", "arrow");
    gPanels[i].setAttribute("photon", true);
    navBar.appendChild(gPanels[i]);

    gPanelMultiViews[i] = document.createXULElement("panelmultiview");
    gPanelMultiViews[i].id = "panelmultiview-" + i;
    gPanels[i].appendChild(gPanelMultiViews[i]);
  }

  for (let i = 0; i < PANELVIEWS_COUNT; i++) {
    gPanelViews[i] = document.createXULElement("panelview");
    gPanelViews[i].id = "panelview-" + i;
    navBar.appendChild(gPanelViews[i]);

    gPanelViewLabels[i] = document.createXULElement("label");
    gPanelViewLabels[i].setAttribute("value", "PanelView " + i);
    gPanelViews[i].appendChild(gPanelViewLabels[i]);
  }

  registerCleanupFunction(() => {
    [...gPanelAnchors, ...gPanels, ...gPanelViews].forEach(e => e.remove());
  });
});

/**
 * Shows and hides all views in a panel with this static structure:
 *
 * - Panel 0
 *   - View 0
 *     - View 1
 *     - View 3
 *       - View 2
 */
add_task(async function test_simple() {
  // Show main view 0.
  await openPopup(0, 0);

  // Show and hide subview 1.
  await showSubView(0, 1);
  assertLabelVisible(0, false);
  await goBack(0, 0);
  assertLabelVisible(1, false);

  // Show subview 3.
  await showSubView(0, 3);
  assertLabelVisible(0, false);

  // Show and hide subview 2.
  await showSubView(0, 2);
  assertLabelVisible(3, false);
  await goBack(0, 3);
  assertLabelVisible(2, false);

  // Hide subview 3.
  await goBack(0, 0);
  assertLabelVisible(3, false);

  // Hide main view 0.
  await hidePopup(0);
  assertLabelVisible(0, false);
});

/**
 * Tests the event sequence in a panel with this static structure:
 *
 * - Panel 0
 *   - View 0
 *     - View 1
 *     - View 3
 *       - View 2
 */
add_task(async function test_simple_event_sequence() {
  let recordArray = [];
  recordEvents(gPanels[0], EVENT_TYPES, recordArray);

  await openPopup(0, 0);
  await showSubView(0, 1);
  await goBack(0, 0);
  await showSubView(0, 3);
  await showSubView(0, 2);
  await goBack(0, 3);
  await goBack(0, 0);
  await hidePopup(0);

  stopRecordingEvents(gPanels[0]);

  Assert.deepEqual(recordArray, [
    "panelview-0: ViewShowing",
    "panelview-0: ViewShown",
    "panel-0: popupshown",
    "panelview-1: ViewShowing",
    "panelview-1: ViewShown",
    "panelview-1: ViewHiding",
    "panelview-0: ViewShown",
    "panelview-3: ViewShowing",
    "panelview-3: ViewShown",
    "panelview-2: ViewShowing",
    "panelview-2: ViewShown",
    "panelview-2: ViewHiding",
    "panelview-3: ViewShown",
    "panelview-3: ViewHiding",
    "panelview-0: ViewShown",
    "panelview-0: ViewHiding",
    "panelmultiview-0: PanelMultiViewHidden",
    "panel-0: popuphidden",
  ]);
});

/**
 * Tests that further navigation is suppressed until the new view is shown.
 */
add_task(async function test_navigation_suppression() {
  await openPopup(0, 0);

  // Test re-entering the "showSubView" method.
  let promiseShown = BrowserTestUtils.waitForEvent(gPanelViews[1], "ViewShown");
  gPanelMultiViews[0].showSubView(gPanelViews[1]);
  Assert.ok(
    !PanelView.forNode(gPanelViews[0]).active,
    "The previous view should become inactive synchronously."
  );

  // The following call will have no effect.
  gPanelMultiViews[0].showSubView(gPanelViews[2]);
  await promiseShown;

  // Test re-entering the "goBack" method.
  promiseShown = BrowserTestUtils.waitForEvent(gPanelViews[0], "ViewShown");
  gPanelMultiViews[0].goBack();
  Assert.ok(
    !PanelView.forNode(gPanelViews[1]).active,
    "The previous view should become inactive synchronously."
  );

  // The following call will have no effect.
  gPanelMultiViews[0].goBack();
  await promiseShown;

  // Main view 0 should be displayed.
  assertLabelVisible(0, true);

  await hidePopup(0);
});

/**
 * Tests reusing views that are already open in another panel. In this test, the
 * structure of the first panel will change dynamically:
 *
 * - Panel 0
 *   - View 0
 *     - View 1
 * - Panel 1
 *   - View 1
 *     - View 2
 * - Panel 0
 *   - View 1
 *     - View 0
 */
add_task(async function test_switch_event_sequence() {
  let recordArray = [];
  recordEvents(gPanels[0], EVENT_TYPES, recordArray);
  recordEvents(gPanels[1], EVENT_TYPES, recordArray);

  // Show panel 0.
  await openPopup(0, 0);
  await showSubView(0, 1);

  // Show panel 1 with the view that is already open and visible in panel 0.
  // This will close panel 0 automatically.
  await openPopup(1, 1);
  await showSubView(1, 2);

  // Show panel 0 with a view that is already open but invisible in panel 1.
  // This will close panel 1 automatically.
  await openPopup(0, 1);
  await showSubView(0, 0);

  // Hide panel 0.
  await hidePopup(0);

  stopRecordingEvents(gPanels[0]);
  stopRecordingEvents(gPanels[1]);

  Assert.deepEqual(recordArray, [
    "panelview-0: ViewShowing",
    "panelview-0: ViewShown",
    "panel-0: popupshown",
    "panelview-1: ViewShowing",
    "panelview-1: ViewShown",
    "panelview-1: ViewHiding",
    "panelview-0: ViewHiding",
    "panelmultiview-0: PanelMultiViewHidden",
    "panel-0: popuphidden",
    "panelview-1: ViewShowing",
    "panel-1: popupshown",
    "panelview-1: ViewShown",
    "panelview-2: ViewShowing",
    "panelview-2: ViewShown",
    "panel-1: popuphidden",
    "panelview-2: ViewHiding",
    "panelview-1: ViewHiding",
    "panelmultiview-1: PanelMultiViewHidden",
    "panelview-1: ViewShowing",
    "panelview-1: ViewShown",
    "panel-0: popupshown",
    "panelview-0: ViewShowing",
    "panelview-0: ViewShown",
    "panelview-0: ViewHiding",
    "panelview-1: ViewHiding",
    "panelmultiview-0: PanelMultiViewHidden",
    "panel-0: popuphidden",
  ]);
});

/**
 * Tests the event sequence when opening the main view is canceled.
 */
add_task(async function test_cancel_mainview_event_sequence() {
  let recordArray = [];
  recordEvents(gPanels[0], EVENT_TYPES, recordArray, event => {
    if (event.type == "ViewShowing") {
      event.preventDefault();
    }
  });

  gPanelMultiViews[0].setAttribute("mainViewId", gPanelViews[0].id);

  let promiseHidden = BrowserTestUtils.waitForEvent(gPanels[0], "popuphidden");
  PanelMultiView.openPopup(
    gPanels[0],
    gPanelAnchors[0],
    "bottomright topright"
  );
  await promiseHidden;

  stopRecordingEvents(gPanels[0]);

  Assert.deepEqual(recordArray, [
    "panelview-0: ViewShowing",
    "panelview-0: ViewHiding",
    "panelmultiview-0: PanelMultiViewHidden",
    "panelmultiview-0: popuphidden",
  ]);
});

/**
 * Tests the event sequence when opening a subview is canceled.
 */
add_task(async function test_cancel_subview_event_sequence() {
  let recordArray = [];
  recordEvents(gPanels[0], EVENT_TYPES, recordArray, event => {
    if (
      event.type == "ViewShowing" &&
      event.originalTarget.id == gPanelViews[1].id
    ) {
      event.preventDefault();
    }
  });

  await openPopup(0, 0);

  let promiseHiding = BrowserTestUtils.waitForEvent(
    gPanelViews[1],
    "ViewHiding"
  );
  gPanelMultiViews[0].showSubView(gPanelViews[1]);
  await promiseHiding;

  // Only the subview should have received the hidden event at this point.
  Assert.deepEqual(recordArray, [
    "panelview-0: ViewShowing",
    "panelview-0: ViewShown",
    "panel-0: popupshown",
    "panelview-1: ViewShowing",
    "panelview-1: ViewHiding",
  ]);
  recordArray.length = 0;

  await hidePopup(0);

  stopRecordingEvents(gPanels[0]);

  Assert.deepEqual(recordArray, [
    "panelview-0: ViewHiding",
    "panelmultiview-0: PanelMultiViewHidden",
    "panel-0: popuphidden",
  ]);
});

/**
 * Tests the event sequence when closing the panel while opening the main view.
 */
add_task(async function test_close_while_showing_mainview_event_sequence() {
  let recordArray = [];
  recordEvents(gPanels[0], EVENT_TYPES, recordArray, event => {
    if (event.type == "ViewShowing") {
      PanelMultiView.hidePopup(gPanels[0]);
    }
  });

  gPanelMultiViews[0].setAttribute("mainViewId", gPanelViews[0].id);

  let promiseHidden = BrowserTestUtils.waitForEvent(gPanels[0], "popuphidden");
  let promiseHiding = BrowserTestUtils.waitForEvent(
    gPanelViews[0],
    "ViewHiding"
  );
  PanelMultiView.openPopup(
    gPanels[0],
    gPanelAnchors[0],
    "bottomright topright"
  );
  await promiseHiding;
  await promiseHidden;

  stopRecordingEvents(gPanels[0]);

  Assert.deepEqual(recordArray, [
    "panelview-0: ViewShowing",
    "panelview-0: ViewShowing > panelview-0: ViewHiding",
    "panelview-0: ViewShowing > panelmultiview-0: PanelMultiViewHidden",
    "panelview-0: ViewShowing > panelmultiview-0: popuphidden",
  ]);
});

/**
 * Tests the event sequence when closing the panel while opening a subview.
 */
add_task(async function test_close_while_showing_subview_event_sequence() {
  let recordArray = [];
  recordEvents(gPanels[0], EVENT_TYPES, recordArray, event => {
    if (
      event.type == "ViewShowing" &&
      event.originalTarget.id == gPanelViews[1].id
    ) {
      PanelMultiView.hidePopup(gPanels[0]);
    }
  });

  await openPopup(0, 0);

  let promiseHidden = BrowserTestUtils.waitForEvent(gPanels[0], "popuphidden");
  gPanelMultiViews[0].showSubView(gPanelViews[1]);
  await promiseHidden;

  stopRecordingEvents(gPanels[0]);

  Assert.deepEqual(recordArray, [
    "panelview-0: ViewShowing",
    "panelview-0: ViewShown",
    "panel-0: popupshown",
    "panelview-1: ViewShowing",
    "panelview-1: ViewShowing > panelview-1: ViewHiding",
    "panelview-1: ViewShowing > panelview-0: ViewHiding",
    "panelview-1: ViewShowing > panelmultiview-0: PanelMultiViewHidden",
    "panelview-1: ViewShowing > panel-0: popuphidden",
  ]);
});
