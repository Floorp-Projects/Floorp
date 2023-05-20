/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that the frames list gets updated as iframes are added/removed from the document,
// and during navigation.

const TEST_COM_URL =
  "https://example.com/document-builder.sjs?html=<div id=com>com";
const TEST_ORG_URL =
  `https://example.org/document-builder.sjs?html=<div id=org>org</div>` +
  `<iframe src="https://example.org/document-builder.sjs?html=example.org iframe"></iframe>` +
  `<iframe src="https://example.com/document-builder.sjs?html=example.com iframe"></iframe>`;

add_task(async function () {
  // Enable the frames button.
  await pushPref("devtools.command-button-frames.enabled", true);

  const tab = await addTab(TEST_COM_URL);

  const toolbox = await gDevTools.showToolboxForTab(tab, {
    toolId: "webconsole",
  });

  ok(
    !getFramesButton(toolbox),
    "Frames button is not rendered when there's no iframes in the page"
  );
  await checkFramesList(toolbox, []);

  info("Create a same origin (example.com) iframe");
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    const comIframe = content.document.createElement("iframe");
    comIframe.src =
      "https://example.com/document-builder.sjs?html=example.com iframe";
    content.document.body.appendChild(comIframe);
  });

  await waitFor(() => getFramesButton(toolbox));
  ok(true, "Button is displayed when adding an iframe");

  info("Check the content of the frames list");
  await checkFramesList(toolbox, [
    TEST_COM_URL,
    "https://example.com/document-builder.sjs?html=example.com iframe",
  ]);

  info("Create a cross-process origin (example.org) iframe");
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    const orgIframe = content.document.createElement("iframe");
    orgIframe.src =
      "https://example.org/document-builder.sjs?html=example.org iframe";
    content.document.body.appendChild(orgIframe);
  });

  info("Check that the content of the frames list was updated");
  try {
    await checkFramesList(toolbox, [
      TEST_COM_URL,
      "https://example.com/document-builder.sjs?html=example.com iframe",
      "https://example.org/document-builder.sjs?html=example.org iframe",
    ]);

    // If Fission is enabled and EFT is not, we shouldn't hit this line as `checkFramesList`
    // should throw (as remote frames are only displayed when EFT is enabled).
    ok(
      !isFissionEnabled() || isEveryFrameTargetEnabled(),
      "iframe picker should only display remote frames when EFT is enabled"
    );
  } catch (e) {
    ok(
      isFissionEnabled() && !isEveryFrameTargetEnabled(),
      "iframe picker displays remote frames only when EFT is enabled"
    );
    return;
  }

  info("Reload and check that the frames list is cleared");
  await reloadBrowser();
  await waitFor(() => !getFramesButton(toolbox));
  ok(
    true,
    "The button was hidden when reloading as the page does not have iframes"
  );
  await checkFramesList(toolbox, []);

  info("Navigate to a different origin, on a page with iframes");
  await navigateTo(TEST_ORG_URL);
  await checkFramesList(toolbox, [
    TEST_ORG_URL,
    "https://example.org/document-builder.sjs?html=example.org iframe",
    "https://example.com/document-builder.sjs?html=example.com iframe",
  ]);

  info("Check that frames list is updated when removing same-origin iframe");
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    content.document.querySelector("iframe").remove();
  });
  await checkFramesList(toolbox, [
    TEST_ORG_URL,
    "https://example.com/document-builder.sjs?html=example.com iframe",
  ]);

  info("Check that frames list is updated when removing cross-origin iframe");
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    content.document.querySelector("iframe").remove();
  });
  await waitFor(() => !getFramesButton(toolbox));
  ok(true, "The button was hidden when removing the last iframe on the page");
  await checkFramesList(toolbox, []);

  info("Check that the list does have expected items after reloading");
  await reloadBrowser();
  await waitFor(() => getFramesButton(toolbox));
  ok(true, "button is displayed after reloading");
  await checkFramesList(toolbox, [
    TEST_ORG_URL,
    "https://example.org/document-builder.sjs?html=example.org iframe",
    "https://example.com/document-builder.sjs?html=example.com iframe",
  ]);
});

function getFramesButton(toolbox) {
  return toolbox.doc.getElementById("command-button-frames");
}

async function checkFramesList(toolbox, expectedFrames) {
  const frames = await waitFor(() => {
    // items might be added in the list before their url is known, so exclude empty items.
    const f = getFramesLabels(toolbox).filter(t => t !== "");
    if (f.length !== expectedFrames.length) {
      return false;
    }

    return f;
  });

  is(
    JSON.stringify(frames.sort()),
    JSON.stringify(expectedFrames.sort()),
    "The expected frames are displayed"
  );
}

function getFramesLabels(toolbox) {
  return Array.from(
    toolbox.doc.querySelectorAll("#toolbox-frame-menu .command .label")
  ).map(el => el.textContent);
}
