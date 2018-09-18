/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-env browser */
/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* import-globals-from ../../../shared/test/shared-head.js */

"use strict";

// Load the shared-head file first.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-head.js",
  this);

/**
 * Enable the new about:debugging panel.
 */
async function enableNewAboutDebugging() {
  await pushPref("devtools.aboutdebugging.new-enabled", true);
}

async function openAboutDebugging(page, win) {
  await enableNewAboutDebugging();

  info("opening about:debugging");
  const tab = await addTab("about:debugging", { window: win });
  const browser = tab.linkedBrowser;
  const document = browser.contentDocument;
  const window = browser.contentWindow;

  info("Wait until the main about debugging container is available");
  await waitUntil(() => document.querySelector(".app"));

  info("Wait until the client connection was established");
  await waitUntil(() => document.querySelector(".js-runtime-page"));

  return { tab, document, window };
}
