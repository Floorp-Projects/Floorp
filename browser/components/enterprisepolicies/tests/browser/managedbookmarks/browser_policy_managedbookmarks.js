/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_policy_managedbookmarks() {
  let managedBookmarksMenu = window.document.getElementById(
    "managed-bookmarks"
  );

  is(
    managedBookmarksMenu.hidden,
    false,
    "Managed bookmarks button should be visible."
  );
  is(
    managedBookmarksMenu.label,
    "Folder 1",
    "Managed bookmarks buttons should have correct label"
  );

  let popupShownPromise = BrowserTestUtils.waitForEvent(
    managedBookmarksMenu.menupopup,
    "popupshown",
    false
  );
  let popupHiddenPromise = BrowserTestUtils.waitForEvent(
    managedBookmarksMenu.menupopup,
    "popuphidden",
    false
  );
  managedBookmarksMenu.open = true;
  await popupShownPromise;

  is(
    managedBookmarksMenu.menupopup.children[0].label,
    "Bookmark 1",
    "Bookmark should have correct label"
  );
  is(
    managedBookmarksMenu.menupopup.children[0].link,
    "https://example.com/",
    "Bookmark should have correct link"
  );
  is(
    managedBookmarksMenu.menupopup.children[1].label,
    "Bookmark 2",
    "Bookmark should have correct label"
  );
  is(
    managedBookmarksMenu.menupopup.children[1].link,
    "https://bookmark2.example.com/",
    "Bookmark should have correct link"
  );
  let subFolder = managedBookmarksMenu.menupopup.children[2];
  is(subFolder.label, "Folder 2", "Subfolder should have correct label");
  is(
    subFolder.menupopup.children[0].label,
    "Bookmark 3",
    "Bookmark should have correct label"
  );
  is(
    subFolder.menupopup.children[0].link,
    "https://bookmark3.example.com/",
    "Bookmark should have correct link"
  );
  is(
    subFolder.menupopup.children[1].label,
    "Bookmark 4",
    "Bookmark should have correct link"
  );
  is(
    subFolder.menupopup.children[1].link,
    "https://bookmark4.example.com/",
    "Bookmark should have correct label"
  );
  subFolder = managedBookmarksMenu.menupopup.children[3];
  await TestUtils.waitForCondition(() => {
    // Need to wait for Fluent to translate
    return subFolder.label == "Subfolder";
  }, "Subfolder should have correct label");
  is(
    subFolder.menupopup.children[0].label,
    "Bookmark 5",
    "Bookmark should have correct label"
  );
  is(
    subFolder.menupopup.children[0].link,
    "https://bookmark5.example.com/",
    "Bookmark should have correct link"
  );
  is(
    subFolder.menupopup.children[1].label,
    "Bookmark 6",
    "Bookmark should have correct link"
  );
  is(
    subFolder.menupopup.children[1].link,
    "https://bookmark6.example.com/",
    "Bookmark should have correct label"
  );

  managedBookmarksMenu.open = false;
  await popupHiddenPromise;
});

add_task(async function test_open_managedbookmark() {
  let managedBookmarksMenu = window.document.getElementById(
    "managed-bookmarks"
  );

  let promise = BrowserTestUtils.waitForEvent(
    managedBookmarksMenu.menupopup,
    "popupshown",
    false
  );
  managedBookmarksMenu.open = true;
  await promise;

  let context = document.getElementById("placesContext");
  let openContextMenuPromise = BrowserTestUtils.waitForEvent(
    context,
    "popupshown"
  );
  EventUtils.synthesizeMouseAtCenter(
    managedBookmarksMenu.menupopup.children[0],
    {
      button: 2,
      type: "contextmenu",
    }
  );
  await openContextMenuPromise;
  info("Opened context menu");

  let tabCreatedPromise = BrowserTestUtils.waitForNewTab(gBrowser, null, true);

  let openInNewTabOption = document.getElementById("placesContext_open:newtab");
  context.activateItem(openInNewTabOption);
  info("Click open in new tab");

  let lastOpenedTab = await tabCreatedPromise;
  Assert.equal(
    lastOpenedTab.linkedBrowser.currentURI.spec,
    "https://example.com/",
    "Should have opened the correct URI"
  );
  await BrowserTestUtils.removeTab(lastOpenedTab);
});

add_task(async function test_copy_managedbookmark() {
  let managedBookmarksMenu = window.document.getElementById(
    "managed-bookmarks"
  );

  let promise = BrowserTestUtils.waitForEvent(
    managedBookmarksMenu.menupopup,
    "popupshown",
    false
  );
  managedBookmarksMenu.open = true;
  await promise;

  let context = document.getElementById("placesContext");
  let openContextMenuPromise = BrowserTestUtils.waitForEvent(
    context,
    "popupshown"
  );
  EventUtils.synthesizeMouseAtCenter(
    managedBookmarksMenu.menupopup.children[0],
    {
      button: 2,
      type: "contextmenu",
    }
  );
  await openContextMenuPromise;
  info("Opened context menu");

  let copyOption = document.getElementById("placesContext_copy");

  await new Promise((resolve, reject) => {
    SimpleTest.waitForClipboard(
      "https://example.com/",
      () => {
        context.activateItem(copyOption);
      },
      resolve,
      () => {
        ok(false, "Clipboard copy failed");
        reject();
      }
    );
  });
});
