/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for reordering with secondary toolbox such as Browser Content Toolbox.
// We test whether the ordering preference will not change when the secondary toolbox
// was closed without reordering.

const {
  gDevToolsBrowser,
} = require("devtools/client/framework/devtools-browser");

add_task(async function() {
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("devtools.toolbox.selectedTool");
  });

  // Keep initial tabs order preference of devtools so as to compare after re-ordering
  // tabs on browser content toolbox.
  const initialTabsOrderOnDevTools = Services.prefs.getCharPref(
    "devtools.toolbox.tabsOrder"
  );

  info("Prepare the toolbox on browser content toolbox");
  await addTab(`${URL_ROOT}doc_empty-tab-01.html`);
  // Select "memory" tool from first, because the webconsole might connect to the content.
  Services.prefs.setCharPref("devtools.toolbox.selectedTool", "memory");
  const toolbox = await gDevToolsBrowser.openContentProcessToolbox(gBrowser);

  info(
    "Check whether the value of devtools.toolbox.tabsOrder was not affected after closed"
  );
  const onToolboxDestroyed = toolbox.once("destroyed");
  toolbox.topWindow.close();
  await onToolboxDestroyed;
  is(
    Services.prefs.getCharPref("devtools.toolbox.tabsOrder"),
    initialTabsOrderOnDevTools,
    "The preference of devtools.toolbox.tabsOrder should not be affected"
  );
});
