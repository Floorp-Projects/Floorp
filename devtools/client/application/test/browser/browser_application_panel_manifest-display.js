/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that the manifest is being properly shown
 */

add_task(async function () {
  info("Test that we are displaying correctly a valid manifest");
  const url = URL_ROOT + "resources/manifest/load-ok.html";

  await enableApplicationPanel();
  const { panel, tab } = await openNewTabAndApplicationPanel(url);
  const doc = panel.panelWin.document;

  selectPage(panel, "manifest");

  info("Waiting for the manifest to be displayed");
  await waitUntil(() => doc.querySelector(".js-manifest") !== null);
  ok(true, "Manifest is being displayed");

  // assert manifest members are being properly displayed
  checkManifestMember(doc, "name", "Foo");
  checkManifestMember(doc, "background_color", "#ff0000");

  ok(
    doc.querySelector(".js-manifest-issues") === null,
    "No validation issues are being displayed"
  );

  // close the tab
  info("Closing the tab.");
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function () {
  info(
    "Test that we are displaying correctly a manifest with validation warnings"
  );
  const url = URL_ROOT + "resources/manifest/load-ok-warnings.html";

  await enableApplicationPanel();
  const { panel, tab } = await openNewTabAndApplicationPanel(url);
  const doc = panel.panelWin.document;

  selectPage(panel, "manifest");

  info("Waiting for the manifest to be displayed");
  await waitUntil(() => doc.querySelector(".js-manifest") !== null);
  ok(true, "Manifest is being displayed");

  // assert manifest members are being properly displayed
  checkManifestMember(doc, "name", "Foo");
  checkManifestMember(doc, "background_color", "");

  const issuesEl = doc.querySelector(".js-manifest-issues");
  ok(issuesEl !== null, "Validation issues are displayed");

  const warningEl = [...issuesEl.querySelectorAll(".js-manifest-issue")].find(
    x => x.textContent.includes("background_color")
  );
  ok(warningEl !== null, "A warning about background_color is displayed");

  // close the tab
  info("Closing the tab.");
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function () {
  info("Test that we are displaying correctly a manifest with JSON errors");
  const url = URL_ROOT + "resources/manifest/load-ok-json-error.html";

  await enableApplicationPanel();
  const { panel, tab } = await openNewTabAndApplicationPanel(url);
  const doc = panel.panelWin.document;

  selectPage(panel, "manifest");

  info("Waiting for the manifest to be displayed");
  await waitUntil(() => doc.querySelector(".js-manifest") !== null);
  ok(true, "Manifest is being displayed");

  const issuesEl = doc.querySelector(".js-manifest-issues");
  ok(issuesEl !== null, "Validation issues are displayed");

  const errorEl = [...issuesEl.querySelectorAll(".js-manifest-issue")].find(x =>
    x.textContent.includes("JSON")
  );
  ok(errorEl !== null, "An error about JSON parsing is displayed");

  // close the tab
  info("Closing the tab.");
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function () {
  info("Test that we are displaying correctly a manifest with icons");
  const url = URL_ROOT + "resources/manifest/load-ok-icons.html";

  await enableApplicationPanel();
  const { panel, tab } = await openNewTabAndApplicationPanel(url);
  const doc = panel.panelWin.document;

  selectPage(panel, "manifest");

  info("Waiting for the manifest to be displayed");
  await waitUntil(() => doc.querySelector(".js-manifest") !== null);
  ok(true, "Manifest is being displayed");

  // assert manifest icon is being displayed
  const iconEl = findMemberByLabel(doc, "128x128image/svg");
  ok(iconEl !== null, "Icon label is being displayed with size and image type");
  const imgEl = iconEl.querySelector(".js-manifest-item-content img");
  ok(imgEl !== null, "An image is displayed for the icon");
  is(
    imgEl.src,
    URL_ROOT + "resources/manifest/icon.svg",
    "The icon image has the the icon url as source"
  );
  const iconTextContent = iconEl.querySelector(
    ".js-manifest-item-content"
  ).textContent;
  ok(iconTextContent.includes("any"), "Purpose is being displayed");

  // close the tab
  info("Closing the tab.");
  await BrowserTestUtils.removeTab(tab);
});

function findMemberByLabel(doc, member) {
  return [...doc.querySelectorAll(".js-manifest-item")].find(x =>
    x.querySelector(".js-manifest-item-label").textContent.startsWith(member)
  );
}

function checkManifestMember(doc, member, expectedValue) {
  const itemEl = findMemberByLabel(doc, member);
  is(
    itemEl.querySelector(".js-manifest-item-content").textContent,
    expectedValue,
    `Manifest member ${member} displays the correct value`
  );
}
