/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * First Test
 * Checks if buttons are disabled/enabled and visible/hidden correctly.
 */
add_task(async function testButtons() {
  // Let's make sure HTTPS-Only and HTTPS-First Mode is off.
  await setHttpsOnlyPref("off");
  await setHttpsFirstPref("off");

  // Open the privacy-pane in about:preferences
  await openPreferencesViaOpenPreferencesAPI("panePrivacy", {
    leaveOpen: true,
  });

  // Get button-element to open the exceptions-dialog
  const exceptionButton = gBrowser.contentDocument.getElementById(
    "httpsOnlyExceptionButton"
  );

  is(
    exceptionButton.disabled,
    true,
    "HTTPS-Only exception button should be disabled when HTTPS-Only Mode is disabled."
  );

  await setHttpsOnlyPref("private");
  is(
    exceptionButton.disabled,
    false,
    "HTTPS-Only exception button should be enabled when HTTPS-Only Mode is only enabled in private browsing."
  );

  await setHttpsOnlyPref("everywhere");
  is(
    exceptionButton.disabled,
    false,
    "HTTPS-Only exception button should be enabled when HTTPS-Only Mode enabled everywhere."
  );

  await setHttpsOnlyPref("off");
  is(
    exceptionButton.disabled,
    true,
    "Turning off HTTPS-Only should disable the exception button again."
  );

  await setHttpsFirstPref("private");
  is(
    exceptionButton.disabled,
    false,
    "HTTPS-Only exception button should be enabled when HTTPS-Only Mode is disabled and HTTPS-First Mode is only enabled in private browsing."
  );

  await setHttpsFirstPref("everywhere");
  is(
    exceptionButton.disabled,
    false,
    "HTTPS-Only exception button should be enabled when HTTPS-Only Mode is disabled and HTTPS-First Mode enabled everywhere."
  );

  // Now that the button is clickable, we open the dialog
  // to check if the correct buttons are visible
  let promiseSubDialogLoaded = promiseLoadSubDialog(
    "chrome://browser/content/preferences/dialogs/permissions.xhtml"
  );
  exceptionButton.doCommand();

  let win = await promiseSubDialogLoaded;

  const dialogDoc = win.document;

  is(
    dialogDoc.getElementById("btnBlock").hidden,
    true,
    "Block button should not be visible in HTTPS-Only Dialog."
  );
  is(
    dialogDoc.getElementById("btnCookieSession").hidden,
    true,
    "Cookie specific allow button should not be visible in HTTPS-Only Dialog."
  );
  is(
    dialogDoc.getElementById("btnAllow").hidden,
    true,
    "Allow button should not be visible in HTTPS-Only Dialog."
  );
  is(
    dialogDoc.getElementById("btnHttpsOnlyOff").hidden,
    false,
    "HTTPS-Only off button should be visible in HTTPS-Only Dialog."
  );
  is(
    dialogDoc.getElementById("btnHttpsOnlyOffTmp").hidden,
    false,
    "HTTPS-Only temporary off button should be visible in HTTPS-Only Dialog."
  );

  // Reset prefs and close the tab
  await SpecialPowers.flushPrefEnv();
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

/**
 * Second Test
 * Checks permissions are added and removed correctly.
 *
 * Each test opens a new dialog, performs an action (second argument),
 * then closes the dialog and checks if the changes were made (third argument).
 */
add_task(async function checkDialogFunctionality() {
  // Enable HTTPS-Only Mode for every window, so the exceptions dialog is accessible.
  await setHttpsOnlyPref("everywhere");

  // Open the privacy-pane in about:preferences
  await openPreferencesViaOpenPreferencesAPI("panePrivacy", {
    leaveOpen: true,
  });
  const preferencesDoc = gBrowser.contentDocument;

  // Test if we can add permanent exceptions
  await runTest(
    preferencesDoc,
    elements => {
      assertListContents(elements, []);

      elements.url.value = "test.com";
      elements.btnAllow.doCommand();

      assertListContents(elements, [["http://test.com", elements.allowL10nId]]);
    },
    () => [
      {
        type: "https-only-load-insecure",
        origin: "http://test.com",
        data: "added",
        capability: Ci.nsIPermissionManager.ALLOW_ACTION,
        expireType: Ci.nsIPermissionManager.EXPIRE_NEVER,
      },
    ]
  );

  // Test if items are retained, and if temporary exceptions are added correctly
  await runTest(
    preferencesDoc,
    elements => {
      assertListContents(elements, [["http://test.com", elements.allowL10nId]]);

      elements.url.value = "1.1.1.1:8080";
      elements.btnAllowSession.doCommand();

      assertListContents(elements, [
        ["http://test.com", elements.allowL10nId],
        ["http://1.1.1.1:8080", elements.allowSessionL10nId],
      ]);
    },
    () => [
      {
        type: "https-only-load-insecure",
        origin: "http://1.1.1.1:8080",
        data: "added",
        capability: Ci.nsIHttpsOnlyModePermission.LOAD_INSECURE_ALLOW_SESSION,
        expireType: Ci.nsIPermissionManager.EXPIRE_SESSION,
      },
    ]
  );

  // Test if we can remove the permissions one-by-one
  await runTest(
    preferencesDoc,
    elements => {
      while (elements.richlistbox.itemCount) {
        elements.richlistbox.selectedIndex = 0;
        elements.btnRemove.doCommand();
      }
      assertListContents(elements, []);
    },
    elements => {
      let richlistItems = elements.richlistbox.getElementsByAttribute(
        "origin",
        "*"
      );
      let observances = [];
      for (let item of richlistItems) {
        observances.push({
          type: "https-only-load-insecure",
          origin: item.getAttribute("origin"),
          data: "deleted",
        });
      }
      return observances;
    }
  );

  // Test if all inputs with an https scheme are added with an http scheme,
  // while other schemes are kept as they are. (Bug 1757297)
  await runTest(
    preferencesDoc,
    elements => {
      assertListContents(elements, []);

      elements.url.value = "http://test.com";
      elements.btnAllow.doCommand();

      assertListContents(elements, [["http://test.com", elements.allowL10nId]]);

      elements.url.value = "https://test.com";
      elements.btnAllow.doCommand();

      assertListContents(elements, [["http://test.com", elements.allowL10nId]]);

      elements.url.value = "https://test.org";
      elements.btnAllow.doCommand();

      assertListContents(elements, [
        ["http://test.com", elements.allowL10nId],
        ["http://test.org", elements.allowL10nId],
      ]);

      elements.url.value = "moz-extension://test";
      elements.btnAllow.doCommand();

      assertListContents(elements, [
        ["http://test.com", elements.allowL10nId],
        ["http://test.org", elements.allowL10nId],
        ["moz-extension://test", elements.allowL10nId],
      ]);
    },
    () => [
      {
        type: "https-only-load-insecure",
        origin: "http://test.com",
        data: "added",
        capability: Ci.nsIPermissionManager.ALLOW_ACTION,
        expireType: Ci.nsIPermissionManager.EXPIRE_NEVER,
      },
      {
        type: "https-only-load-insecure",
        origin: "http://test.org",
        data: "added",
        capability: Ci.nsIPermissionManager.ALLOW_ACTION,
        expireType: Ci.nsIPermissionManager.EXPIRE_NEVER,
      },
      {
        type: "https-only-load-insecure",
        origin: "moz-extension://test",
        data: "added",
        capability: Ci.nsIPermissionManager.ALLOW_ACTION,
        expireType: Ci.nsIPermissionManager.EXPIRE_NEVER,
      },
    ]
  );

  // Test if we can remove all permissions at once
  await runTest(
    preferencesDoc,
    elements => {
      elements.btnRemoveAll.doCommand();
      assertListContents(elements, []);
    },
    elements => {
      let richlistItems = elements.richlistbox.getElementsByAttribute(
        "origin",
        "*"
      );
      let observances = [];
      for (let item of richlistItems) {
        observances.push({
          type: "https-only-load-insecure",
          origin: item.getAttribute("origin"),
          data: "deleted",
        });
      }
      return observances;
    }
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

/**
 * Changes HTTPS-Only Mode pref
 * @param {string} state "everywhere", "private", "off"
 */
async function setHttpsOnlyPref(state) {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.security.https_only_mode", state === "everywhere"],
      ["dom.security.https_only_mode_pbm", state === "private"],
    ],
  });
}

/**
 * Changes HTTPS-First Mode pref
 * @param {string} state "everywhere", "private", "off"
 */
async function setHttpsFirstPref(state) {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.security.https_first", state === "everywhere"],
      ["dom.security.https_first_pbm", state === "private"],
    ],
  });
}

/**
 * Opens new exceptions dialog, runs test function
 * @param {HTMLElement} preferencesDoc document of about:preferences tab
 * @param {function} test function to call when dialog is open
 * @param {Array} observances permission changes to observe (order is important)
 */
async function runTest(preferencesDoc, test, observancesFn) {
  // Click on exception-button and wait for dialog to open
  let promiseSubDialogLoaded = promiseLoadSubDialog(
    "chrome://browser/content/preferences/dialogs/permissions.xhtml"
  );
  preferencesDoc.getElementById("httpsOnlyExceptionButton").doCommand();

  let win = await promiseSubDialogLoaded;

  // Create a bunch of references to UI-elements for the test-function
  const doc = win.document;
  let elements = {
    richlistbox: doc.getElementById("permissionsBox"),
    url: doc.getElementById("url"),
    btnAllow: doc.getElementById("btnHttpsOnlyOff"),
    btnAllowSession: doc.getElementById("btnHttpsOnlyOffTmp"),
    btnRemove: doc.getElementById("removePermission"),
    btnRemoveAll: doc.getElementById("removeAllPermissions"),
    allowL10nId: win.gPermissionManager._getCapabilityL10nId(
      Ci.nsIPermissionManager.ALLOW_ACTION
    ),
    allowSessionL10nId: win.gPermissionManager._getCapabilityL10nId(
      Ci.nsIHttpsOnlyModePermission.LOAD_INSECURE_ALLOW_SESSION
    ),
  };

  // Some observances need to be computed based on the current state.
  const observances = observancesFn(elements);

  // Run test function
  await test(elements);

  // Click on "Save changes" and wait for permission changes.
  let btnApplyChanges = doc.querySelector("dialog").getButton("accept");
  let observeAllPromise = createObserveAllPromise(observances);

  btnApplyChanges.doCommand();
  await observeAllPromise;
}

function assertListContents(elements, expected) {
  is(
    elements.richlistbox.itemCount,
    expected.length,
    "Richlistbox should match the expected amount of exceptions."
  );

  for (let i = 0; i < expected.length; i++) {
    let website = expected[i][0];
    let listItem = elements.richlistbox.getElementsByAttribute(
      "origin",
      website
    );
    is(listItem.length, 1, "Each origin should be unique");
    is(
      listItem[0]
        .querySelector(".website-capability-value")
        .getAttribute("data-l10n-id"),
      expected[i][1],
      "List item capability should match expected l10n-id"
    );
  }
}
