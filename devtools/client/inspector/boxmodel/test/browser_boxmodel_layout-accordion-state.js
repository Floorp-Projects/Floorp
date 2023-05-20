/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the box model's accordion state is persistent through hide/show in the
// layout view.

const TEST_URI = `
  <style>
    #div1 {
      margin: 10px;
      padding: 3px;
    }
  </style>
  <div id="div1"></div>
`;

const BOXMODEL_OPENED_PREF = "devtools.layout.boxmodel.opened";
const ACCORDION_HEADER_SELECTOR = ".accordion-header";
const ACCORDION_CONTENT_SELECTOR = ".accordion-content";

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, boxmodel, toolbox } = await openLayoutView();
  const { document: doc } = boxmodel;

  await testAccordionStateAfterClickingHeader(doc);
  await testAccordionStateAfterSwitchingSidebars(inspector, doc);
  await testAccordionStateAfterReopeningLayoutView(toolbox);

  Services.prefs.clearUserPref(BOXMODEL_OPENED_PREF);
});

function testAccordionStateAfterClickingHeader(doc) {
  const item = doc.querySelector("#layout-section-boxmodel");
  const header = item.querySelector(ACCORDION_HEADER_SELECTOR);
  const content = item.querySelector(ACCORDION_CONTENT_SELECTOR);

  info("Checking initial state of the box model panel.");
  ok(
    !content.hidden && content.childElementCount > 0,
    "The box model panel content is visible."
  );
  ok(
    Services.prefs.getBoolPref(BOXMODEL_OPENED_PREF),
    `${BOXMODEL_OPENED_PREF} is pref on by default.`
  );

  info("Clicking the box model header to hide the box model panel.");
  header.click();

  info("Checking the new state of the box model panel.");
  ok(content.hidden, "The box model panel content is hidden.");
  ok(
    !Services.prefs.getBoolPref(BOXMODEL_OPENED_PREF),
    `${BOXMODEL_OPENED_PREF} is pref off.`
  );
}

function testAccordionStateAfterSwitchingSidebars(inspector, doc) {
  info(
    "Checking the box model accordion state is persistent after switching sidebars."
  );

  info("Selecting the computed view.");
  inspector.sidebar.select("computedview");

  info("Selecting the layout view.");
  inspector.sidebar.select("layoutview");

  info("Checking the state of the box model panel.");
  const item = doc.querySelector("#layout-section-boxmodel");
  const content = item.querySelector(ACCORDION_CONTENT_SELECTOR);

  ok(content.hidden, "The box model panel content is hidden.");
  ok(
    !Services.prefs.getBoolPref(BOXMODEL_OPENED_PREF),
    `${BOXMODEL_OPENED_PREF} is pref off.`
  );
}

async function testAccordionStateAfterReopeningLayoutView(toolbox) {
  info(
    "Checking the box model accordion state is persistent after closing and " +
      "re-opening the layout view."
  );

  info("Closing the toolbox.");
  await toolbox.destroy();

  info("Re-opening the layout view.");
  const { boxmodel } = await openLayoutView();
  const item = boxmodel.document.querySelector("#layout-section-boxmodel");
  const content = item.querySelector(ACCORDION_CONTENT_SELECTOR);

  info("Checking the state of the box model panel.");
  ok(content.hidden, "The box model panel content is hidden.");
  ok(
    !Services.prefs.getBoolPref(BOXMODEL_OPENED_PREF),
    `${BOXMODEL_OPENED_PREF} is pref off.`
  );
}
