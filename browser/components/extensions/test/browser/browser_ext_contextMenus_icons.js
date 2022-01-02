/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const PAGE =
  "http://mochi.test:8888/browser/browser/components/extensions/test/browser/context.html?test=icons";

add_task(async function test_root_icon() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE);

  let encodedImageData =
    "iVBORw0KGgoAAAANSUhEUgAAACQAAAAkCAYAAADhAJiYAAAC4klEQVRYhdWXLWzbQBSADQtDAwsHC1tUhUxqfL67lk2tdn+OJg0ODU0rLByqgqINBY6tmlbn7LMTJ5FaFVVBk1G0oUGjG2jT2Y7jxmmcbU/6iJ+f36fz+e5sGP9riCGm9hB37RG+scd4Yo/wsDXCZyIE2xuXsce4bY+wXkAsQtzYmExrfFgvkJkRbkzo1ehoxx5iXcgI/9iYUGt8WH9MqDXEcmNChmEYrRCf2SHWeYgQx3x0tLNRIeKQLTtEFyJEep4NTuhk8BC+yMrwEE3+iozo42d8gK7FAOkMsRiiN8QhW2ttSK5QTfRRV4QoymVeJMvPvDp7gCZigD613MN6yRFA3SWarow9QB9LCfG+NeF9qCtjAKOSQjCqVKhfVsiHEQ+grgx/lRGqUihAc1uL8EFD+KCRO+GrF4J61phcoRoPoEzkYhZYpykh5sMb7kOdIeY+jHKur4QI4Feh4AFX1nVeLxrAvQchGsBz5ls6wa2QdwcvIcE2863bTH79KOvsz/uUYJsp+J0pSzNlDckVqqVGUAF+n6uS7txcOl6wot4JVy70ufDLy4pWLUQVPE81pRI0mGe9oxLMHSeohHvMs/STUNaUK6vDPCvOyxMFDx4achehRDJmHnydnkPww5OFfLxrGIZBFDyYl4LpMzlTQFIP6AQx86w2UeYBccFpJrcKv5L9eGDtUAU6RIELqsB74uynjy/UBRF1gS5BTFxwQT1wTiXoUg9MH7m/3NZRRoi5IJytUbMgzv4Wc832+oQkiKgEehmyMkkpKsFkQV11QsRJL5rJYBLItQgRaUZEmnoZXsomz3vGiWw+I9KMF9SVFOqZEemZekli1jN3U/UOqhHHvC6oWWGElhfSpGdOk6+O9prdwvtLj5BjRsQxdRnot+Zeifpy/2/0stktKTRNLmbk0mwXyl8253fyojj+8rxOHNAhjjm5n0/5OOCGOKBzkrMO0Z75lvSAzKlrF32Z/3z8BqLAn+yMV7VhAAAAAElFTkSuQmCC";
  const IMAGE_ARRAYBUFFER = imageBufferFromDataURI(encodedImageData);

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "contextMenus icons",
      permissions: ["contextMenus"],
      icons: {
        "18": "extension.png",
      },
    },

    files: {
      "extension.png": IMAGE_ARRAYBUFFER,
    },

    background: function() {
      let menuitemId = browser.contextMenus.create({
        title: "child-to-delete",
        onclick: () => {
          browser.contextMenus.remove(menuitemId);
          browser.test.sendMessage("child-deleted");
        },
      });

      browser.contextMenus.create(
        {
          title: "child",
        },
        () => {
          browser.test.sendMessage("contextmenus-icons");
        }
      );
    },
  });

  let confirmContextMenuIcon = rootElements => {
    let expectedURL = new RegExp(
      String.raw`^moz-extension://[^/]+/extension\.png$`
    );
    is(rootElements.length, 1, "Found exactly one menu item");
    let imageUrl = rootElements[0].getAttribute("image");
    ok(
      expectedURL.test(imageUrl),
      "The context menu should display the extension icon next to the root element"
    );
  };

  await extension.startup();
  await extension.awaitMessage("contextmenus-icons");

  let extensionMenu = await openExtensionContextMenu();

  let contextMenu = document.getElementById("contentAreaContextMenu");
  let topLevelMenuItem = contextMenu.getElementsByAttribute(
    "ext-type",
    "top-level-menu"
  );
  confirmContextMenuIcon(topLevelMenuItem);

  let childToDelete = extensionMenu.getElementsByAttribute(
    "label",
    "child-to-delete"
  );
  is(childToDelete.length, 1, "Found exactly one child to delete");
  await closeExtensionContextMenu(childToDelete[0]);
  await extension.awaitMessage("child-deleted");

  await openExtensionContextMenu();

  contextMenu = document.getElementById("contentAreaContextMenu");
  topLevelMenuItem = contextMenu.getElementsByAttribute("label", "child");

  confirmContextMenuIcon(topLevelMenuItem);
  await closeContextMenu();

  await extension.unload();
  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_child_icon() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE);

  let blackIconData =
    "iVBORw0KGgoAAAANSUhEUgAAABIAAAASCAIAAADZrBkAAAAACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH4QYGEhkO2P07+gAAABl0RVh0Q29tbWVudABDcmVhdGVkIHdpdGggR0lNUFeBDhcAAAARSURBVCjPY2AYBaNgFAxPAAAD3gABo0ohTgAAAABJRU5ErkJggg==";
  const IMAGE_ARRAYBUFFER_BLACK = imageBufferFromDataURI(blackIconData);

  let redIconData =
    "iVBORw0KGgoAAAANSUhEUgAAABIAAAASCAIAAADZrBkAAAAACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH4QYGEgw1XkM0ygAAABl0RVh0Q29tbWVudABDcmVhdGVkIHdpdGggR0lNUFeBDhcAAAAYSURBVCjPY/zPQA5gYhjVNqptVNsg1wYAItkBI/GNR3YAAAAASUVORK5CYII=";
  const IMAGE_ARRAYBUFFER_RED = imageBufferFromDataURI(redIconData);

  let blueIconData =
    "iVBORw0KGgoAAAANSUhEUgAAABIAAAASCAIAAADZrBkAAAAACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH4QYGEg0QDFzRzAAAABl0RVh0Q29tbWVudABDcmVhdGVkIHdpdGggR0lNUFeBDhcAAAAbSURBVCjPY2SQ+89AOmBiIAuMahvVNqqNftoAlKMBQZXKX9kAAAAASUVORK5CYII=";
  const IMAGE_ARRAYBUFFER_BLUE = imageBufferFromDataURI(blueIconData);

  let greenIconData =
    "iVBORw0KGgoAAAANSUhEUgAAABIAAAASCAIAAADZrBkAAAAACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH4QYGEg0rvVc46AAAABl0RVh0Q29tbWVudABDcmVhdGVkIHdpdGggR0lNUFeBDhcAAAAaSURBVCjPY+Q8xkAGYGJgGNU2qm1U2+DWBgBolADz1beTnwAAAABJRU5ErkJggg==";
  const IMAGE_ARRAYBUFFER_GREEN = imageBufferFromDataURI(greenIconData);

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["contextMenus"],
      icons: {
        "18": "black_icon.png",
      },
    },

    files: {
      "black_icon.png": IMAGE_ARRAYBUFFER_BLACK,
      "red_icon.png": IMAGE_ARRAYBUFFER_RED,
      "blue_icon.png": IMAGE_ARRAYBUFFER_BLUE,
      "green_icon.png": IMAGE_ARRAYBUFFER_GREEN,
    },

    background: function() {
      browser.test.onMessage.addListener(msg => {
        if (msg !== "add-additional-contextmenu-items") {
          return;
        }

        browser.contextMenus.create({
          title: "child2",
          id: "contextmenu-child2",
          icons: {
            18: "blue_icon.png",
          },
        });

        browser.contextMenus.create(
          {
            title: "child3",
            id: "contextmenu-child3",
            icons: {
              18: "green_icon.png",
            },
          },
          () => {
            browser.test.sendMessage("extra-contextmenu-items-added");
          }
        );
      });

      browser.contextMenus.create(
        {
          title: "child1",
          id: "contextmenu-child1",
          icons: {
            18: "red_icon.png",
          },
        },
        () => {
          browser.test.sendMessage("single-contextmenu-item-added");
        }
      );
    },
  });

  let confirmContextMenuIcon = (element, imageName) => {
    let imageURL = element.getAttribute("image");
    ok(
      imageURL.endsWith(imageName),
      "The context menu should display the extension icon next to the child element"
    );
  };

  await extension.startup();

  await extension.awaitMessage("single-contextmenu-item-added");

  let contextMenu = await openContextMenu();
  let contextMenuChild1 = contextMenu.getElementsByAttribute(
    "label",
    "child1"
  )[0];
  confirmContextMenuIcon(contextMenuChild1, "black_icon.png");

  await closeContextMenu();

  extension.sendMessage("add-additional-contextmenu-items");
  await extension.awaitMessage("extra-contextmenu-items-added");

  contextMenu = await openExtensionContextMenu();

  contextMenuChild1 = contextMenu.getElementsByAttribute("label", "child1")[0];
  confirmContextMenuIcon(contextMenuChild1, "red_icon.png");

  let contextMenuChild2 = contextMenu.getElementsByAttribute(
    "label",
    "child2"
  )[0];
  confirmContextMenuIcon(contextMenuChild2, "blue_icon.png");

  let contextMenuChild3 = contextMenu.getElementsByAttribute(
    "label",
    "child3"
  )[0];
  confirmContextMenuIcon(contextMenuChild3, "green_icon.png");

  await closeContextMenu();

  await extension.unload();
  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_manifest_without_icons() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE);

  let redIconData =
    "iVBORw0KGgoAAAANSUhEUgAAABIAAAASCAIAAADZrBkAAAAACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH4QYGEgw1XkM0ygAAABl0RVh0Q29tbWVudABDcmVhdGVkIHdpdGggR0lNUFeBDhcAAAAYSURBVCjPY/zPQA5gYhjVNqptVNsg1wYAItkBI/GNR3YAAAAASUVORK5CYII=";
  const IMAGE_ARRAYBUFFER_RED = imageBufferFromDataURI(redIconData);

  let greenIconData =
    "iVBORw0KGgoAAAANSUhEUgAAABIAAAASCAIAAADZrBkAAAAACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH4QYGEg0rvVc46AAAABl0RVh0Q29tbWVudABDcmVhdGVkIHdpdGggR0lNUFeBDhcAAAAaSURBVCjPY+Q8xkAGYGJgGNU2qm1U2+DWBgBolADz1beTnwAAAABJRU5ErkJggg==";
  const IMAGE_ARRAYBUFFER_GREEN = imageBufferFromDataURI(greenIconData);

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "contextMenus icons",
      permissions: ["contextMenus"],
    },
    files: {
      "red.png": IMAGE_ARRAYBUFFER_RED,
      "green.png": IMAGE_ARRAYBUFFER_GREEN,
    },

    background() {
      browser.contextMenus.create(
        {
          title: "first item",
          icons: {
            18: "red.png",
          },
          onclick() {
            browser.contextMenus.create(
              {
                title: "second item",
                icons: {
                  18: "green.png",
                },
              },
              () => {
                browser.test.sendMessage("added-second-item");
              }
            );
          },
        },
        () => {
          browser.test.sendMessage("contextmenus-icons");
        }
      );
    },
  });

  await extension.startup();
  await extension.awaitMessage("contextmenus-icons");

  let menu = await openContextMenu();
  let items = menu.getElementsByAttribute("label", "first item");
  is(items.length, 1, "Found first item");
  // manifest.json does not declare icons, so the root menu item shouldn't have an icon either.
  is(items[0].getAttribute("image"), "", "Root menu must not have an icon");

  await closeExtensionContextMenu(items[0]);
  await extension.awaitMessage("added-second-item");

  menu = await openExtensionContextMenu();
  items = document.querySelectorAll(
    "#contentAreaContextMenu [ext-type='top-level-menu']"
  );
  is(items.length, 1, "Auto-generated root item exists");
  is(
    items[0].getAttribute("image"),
    "",
    "Auto-generated menu root must not have an icon"
  );

  items = menu.getElementsByAttribute("label", "first item");
  is(items.length, 1, "First child item should exist");
  is(
    items[0]
      .getAttribute("image")
      .split("/")
      .pop(),
    "red.png",
    "First item should have an icon"
  );

  items = menu.getElementsByAttribute("label", "second item");
  is(items.length, 1, "Secobnd child item should exist");
  is(
    items[0]
      .getAttribute("image")
      .split("/")
      .pop(),
    "green.png",
    "Second item should have an icon"
  );

  await closeExtensionContextMenu();
  await extension.unload();
  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_child_icon_update() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE);

  let blackIconData =
    "iVBORw0KGgoAAAANSUhEUgAAABIAAAASCAIAAADZrBkAAAAACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH4QYGEhkO2P07+gAAABl0RVh0Q29tbWVudABDcmVhdGVkIHdpdGggR0lNUFeBDhcAAAARSURBVCjPY2AYBaNgFAxPAAAD3gABo0ohTgAAAABJRU5ErkJggg==";
  const IMAGE_ARRAYBUFFER_BLACK = imageBufferFromDataURI(blackIconData);

  let redIconData =
    "iVBORw0KGgoAAAANSUhEUgAAABIAAAASCAIAAADZrBkAAAAACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH4QYGEgw1XkM0ygAAABl0RVh0Q29tbWVudABDcmVhdGVkIHdpdGggR0lNUFeBDhcAAAAYSURBVCjPY/zPQA5gYhjVNqptVNsg1wYAItkBI/GNR3YAAAAASUVORK5CYII=";
  const IMAGE_ARRAYBUFFER_RED = imageBufferFromDataURI(redIconData);

  let blueIconData =
    "iVBORw0KGgoAAAANSUhEUgAAABIAAAASCAIAAADZrBkAAAAACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH4QYGEg0QDFzRzAAAABl0RVh0Q29tbWVudABDcmVhdGVkIHdpdGggR0lNUFeBDhcAAAAbSURBVCjPY2SQ+89AOmBiIAuMahvVNqqNftoAlKMBQZXKX9kAAAAASUVORK5CYII=";
  const IMAGE_ARRAYBUFFER_BLUE = imageBufferFromDataURI(blueIconData);

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["contextMenus"],
      icons: {
        "18": "black_icon.png",
      },
    },

    files: {
      "black_icon.png": IMAGE_ARRAYBUFFER_BLACK,
      "red_icon.png": IMAGE_ARRAYBUFFER_RED,
      "blue_icon.png": IMAGE_ARRAYBUFFER_BLUE,
    },

    background: function() {
      browser.test.onMessage.addListener(msg => {
        if (msg === "update-contextmenu-item") {
          browser.contextMenus.update(
            "contextmenu-child2",
            {
              icons: {
                18: "blue_icon.png",
              },
            },
            () => {
              browser.test.sendMessage("contextmenu-item-updated");
            }
          );
        } else if (msg === "update-contextmenu-item-without-icons") {
          browser.contextMenus.update("contextmenu-child2", {}, () => {
            browser.test.sendMessage("contextmenu-item-updated-without-icons");
          });
        } else if (msg === "update-contextmenu-item-with-icons-as-null") {
          browser.contextMenus.update(
            "contextmenu-child2",
            {
              icons: null,
            },
            () => {
              browser.test.sendMessage(
                "contextmenu-item-updated-with-icons-as-null"
              );
            }
          );
        } else if (msg === "update-contextmenu-item-when-its-the-only-child") {
          browser.contextMenus.update(
            "contextmenu-child1",
            {
              icons: {
                18: "blue_icon.png",
              },
            },
            () => {
              browser.test.sendMessage(
                "contextmenu-item-updated-when-its-only-child"
              );
            }
          );
        }
      });

      browser.contextMenus.create({
        title: "child1",
        id: "contextmenu-child1",
        icons: {
          18: "blue_icon.png",
        },
      });

      let menuitemId = browser.contextMenus.create(
        {
          title: "child2",
          id: "contextmenu-child2",
          icons: {
            18: "red_icon.png",
          },
          onclick: async () => {
            await browser.contextMenus.remove(menuitemId);
            browser.test.sendMessage("child-deleted");
          },
        },
        () => {
          browser.test.sendMessage("contextmenu-items-added");
        }
      );
    },
  });

  let confirmContextMenuIcon = (element, imageName) => {
    let imageURL = element.getAttribute("image");
    ok(
      imageURL.endsWith(imageName),
      "The context menu should display the extension icon next to the child element"
    );
  };

  await extension.startup();

  await extension.awaitMessage("contextmenu-items-added");
  let contextMenu = await openExtensionContextMenu();

  let contextMenuChild1 = contextMenu.getElementsByAttribute(
    "label",
    "child1"
  )[0];
  confirmContextMenuIcon(contextMenuChild1, "blue_icon.png");

  let contextMenuChild2 = contextMenu.getElementsByAttribute(
    "label",
    "child2"
  )[0];
  confirmContextMenuIcon(contextMenuChild2, "red_icon.png");

  await closeContextMenu();

  extension.sendMessage("update-contextmenu-item");
  await extension.awaitMessage("contextmenu-item-updated");

  contextMenu = await openExtensionContextMenu();

  contextMenuChild2 = contextMenu.getElementsByAttribute("label", "child2")[0];
  confirmContextMenuIcon(contextMenuChild2, "blue_icon.png");

  await closeContextMenu();

  extension.sendMessage("update-contextmenu-item-without-icons");
  await extension.awaitMessage("contextmenu-item-updated-without-icons");

  contextMenu = await openExtensionContextMenu();

  contextMenuChild2 = contextMenu.getElementsByAttribute("label", "child2")[0];
  confirmContextMenuIcon(contextMenuChild2, "blue_icon.png");

  await closeContextMenu();

  extension.sendMessage("update-contextmenu-item-with-icons-as-null");
  await extension.awaitMessage("contextmenu-item-updated-with-icons-as-null");

  contextMenu = await openExtensionContextMenu();

  contextMenuChild2 = contextMenu.getElementsByAttribute("label", "child2")[0];
  is(
    contextMenuChild2.getAttribute("image"),
    "",
    "Second child should not have an icon"
  );

  await closeExtensionContextMenu(contextMenuChild2);
  await extension.awaitMessage("child-deleted");

  contextMenu = await openContextMenu();

  contextMenuChild1 = contextMenu.getElementsByAttribute("label", "child1")[0];
  confirmContextMenuIcon(contextMenuChild1, "black_icon.png");

  await closeContextMenu();

  extension.sendMessage("update-contextmenu-item-when-its-the-only-child");
  await extension.awaitMessage("contextmenu-item-updated-when-its-only-child");

  contextMenu = await openContextMenu();

  contextMenuChild1 = contextMenu.getElementsByAttribute("label", "child1")[0];
  confirmContextMenuIcon(contextMenuChild1, "black_icon.png");

  await closeContextMenu();

  await extension.unload();
  BrowserTestUtils.removeTab(tab);
});
