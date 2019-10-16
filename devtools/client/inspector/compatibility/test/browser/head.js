/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from ../../../test/head.js */

// Import the inspector's head.js first (which itself imports shared-head.js).
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/test/head.js",
  this
);

async function openCompatibilityView() {
  info("Open the compatibility view");
  await pushPref("devtools.inspector.compatibility.enabled", true);

  const { inspector } = await openInspectorSidebarTab("compatibilityview");
  const panel = inspector.panelDoc.querySelector(
    "#compatibilityview-panel .inspector-tabpanel"
  );
  return { panel };
}
