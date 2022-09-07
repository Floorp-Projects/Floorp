/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/* import-globals-from helper-addons.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-addons.js", this);

add_task(async () => {
  const EXTENSION_NAME = "temporary-web-extension";
  const EXTENSION_ID = "test-devtools@mozilla.org";

  await enableExtensionDebugging();

  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  const { extension } = await installTemporaryExtensionFromXPI(
    {
      background() {
        const open = indexedDB.open("TestDatabase", 1);

        open.onupgradeneeded = function() {
          const db = open.result;
          db.createObjectStore("TestStore", { keyPath: "id" });
        };

        open.onsuccess = function() {
          const db = open.result;
          const tx = db.transaction("TestStore", "readwrite");
          const store = tx.objectStore("TestStore");

          store.put({ id: 1, name: "John", age: 12 });
          store.put({ id: 2, name: "Bob", age: 24 });
          tx.oncomplete = () => db.close();
        };
      },
      id: EXTENSION_ID,
      name: EXTENSION_NAME,
    },
    document
  );

  const { devtoolsTab, devtoolsWindow } = await openAboutDevtoolsToolbox(
    document,
    tab,
    window,
    EXTENSION_NAME
  );

  info("Select the storage panel");
  const toolbox = getToolbox(devtoolsWindow);
  await toolbox.selectTool("storage");
  const storage = toolbox.getCurrentPanel();

  info("Check the content of the storage panel treeview");
  const ids = [
    "indexedDB",
    `moz-extension://${extension.uuid}`,
    "TestDatabase (default)",
    "TestStore",
  ];
  ok(
    !!storage.panelWindow.document.querySelector(
      `[data-id='${JSON.stringify(ids)}']`
    ),
    "The indexedDB database for the extension is visible"
  );

  info("Select the indexedDB database for the extension");
  const updated = storage.UI.once("store-objects-updated");
  storage.UI.tree.selectedItem = ids;
  await updated;

  info("Wait until table populated");
  await waitUntil(() => storage.UI.table.items.size === 2);
  const items = storage.UI.table.items;

  info("Check the content of the storage panel table");
  is(items.size, 2);
  const user1 = JSON.parse(items.get(1).value);
  const user2 = JSON.parse(items.get(2).value);
  is(user1.name, "John", "user 1 has the expected name");
  is(user1.age, 12, "user 1 has the expected age");
  is(user2.name, "Bob", "user 2 has the expected name");
  is(user2.age, 24, "user 2 has the expected age");

  await closeAboutDevtoolsToolbox(document, devtoolsTab, window);
  await removeTemporaryExtension(EXTENSION_NAME, document);
  await removeTab(tab);
});
