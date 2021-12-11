"use strict";

const { ExtensionPermissions } = ChromeUtils.import(
  "resource://gre/modules/ExtensionPermissions.jsm"
);
const { AboutNewTab } = ChromeUtils.import(
  "resource:///modules/AboutNewTab.jsm"
);

const NEWTAB_PRIVATE_ALLOWED = "browser.newtab.privateAllowed";
const NEWTAB_EXTENSION_CONTROLLED = "browser.newtab.extensionControlled";
const NEWTAB_URI = "webext-newtab-1.html";

function promisePrefChange(pref) {
  return new Promise((resolve, reject) => {
    Services.prefs.addObserver(pref, function observer() {
      Services.prefs.removeObserver(pref, observer);
      resolve(arguments);
    });
  });
}

function verifyPrefSettings(controlled, allowed) {
  is(
    Services.prefs.getBoolPref(NEWTAB_EXTENSION_CONTROLLED, false),
    controlled,
    "newtab extension controlled"
  );
  is(
    Services.prefs.getBoolPref(NEWTAB_PRIVATE_ALLOWED, false),
    allowed,
    "newtab private permission after permission change"
  );

  if (controlled) {
    ok(
      AboutNewTab.newTabURL.endsWith(NEWTAB_URI),
      "Newtab url is overridden by the extension."
    );
  }
  if (controlled && allowed) {
    ok(
      BROWSER_NEW_TAB_URL.endsWith(NEWTAB_URI),
      "active newtab url is overridden by the extension."
    );
  } else {
    let expectednewTab = controlled ? "about:privatebrowsing" : "about:newtab";
    is(BROWSER_NEW_TAB_URL, expectednewTab, "active newtab url is default.");
  }
}

async function promiseUpdatePrivatePermission(allowed, extension) {
  info(`update private allowed permission`);
  let ext = WebExtensionPolicy.getByID(extension.id).extension;
  await Promise.all([
    promisePrefChange(NEWTAB_PRIVATE_ALLOWED),
    ExtensionPermissions[allowed ? "add" : "remove"](
      extension.id,
      { permissions: ["internal:privateBrowsingAllowed"], origins: [] },
      ext
    ),
  ]);

  verifyPrefSettings(true, allowed);
}

add_task(async function test_new_tab_private() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: {
        gecko: {
          id: "@private-newtab",
        },
      },
      chrome_url_overrides: {
        newtab: NEWTAB_URI,
      },
    },
    files: {
      NEWTAB_URI: `
        <!DOCTYPE html>
        <head>
          <meta charset="utf-8"/></head>
        <html>
          <body>
          </body>
        </html>
      `,
    },
    useAddonManager: "permanent",
  });
  await extension.startup();

  verifyPrefSettings(true, false);

  await promiseUpdatePrivatePermission(true, extension);

  await extension.unload();
});
