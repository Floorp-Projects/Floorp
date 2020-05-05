/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const Cm = Components.manager;

const uuidGenerator = Cc["@mozilla.org/uuid-generator;1"].getService(
  Ci.nsIUUIDGenerator
);

const mockUpdateManager = {
  contractId: "@mozilla.org/updates/update-manager;1",

  _mockClassId: uuidGenerator.generateUUID(),

  _originalClassId: "",

  QueryInterface: ChromeUtils.generateQI([Ci.nsIUpdateManager]),

  createInstance(outer, iiD) {
    if (outer) {
      throw Components.Exception("", Cr.NS_ERROR_NO_AGGREGATION);
    }
    return this.QueryInterface(iiD);
  },

  register() {
    let registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);
    if (!registrar.isCIDRegistered(this._mockClassId)) {
      this._originalClassId = registrar.contractIDToCID(this.contractId);
      registrar.registerFactory(
        this._mockClassId,
        "Unregister after testing",
        this.contractId,
        this
      );
    }
  },

  unregister() {
    let registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);
    registrar.unregisterFactory(this._mockClassId, this);
    registrar.registerFactory(this._originalClassId, "", this.contractId, null);
  },

  get updateCount() {
    return this._updates.length;
  },

  getUpdateAt(index) {
    return this._updates[index];
  },

  _updates: [
    {
      name: "Firefox Developer Edition 49.0a2",
      statusText: "The Update was successfully installed",
      buildID: "20160728004010",
      installDate: 1469763105156,
      detailsURL: "https://www.mozilla.org/firefox/aurora/",
    },
    {
      name: "Firefox Developer Edition 43.0a2",
      statusText: "The Update was successfully installed",
      buildID: "20150929004011",
      installDate: 1443585886224,
      detailsURL: "https://www.mozilla.org/firefox/aurora/",
    },
    {
      name: "Firefox Developer Edition 42.0a2",
      statusText: "The Update was successfully installed",
      buildID: "20150920004018",
      installDate: 1442818147544,
      detailsURL: "https://www.mozilla.org/firefox/aurora/",
    },
  ],
};

function formatInstallDate(sec) {
  var date = new Date(sec);
  const dtOptions = {
    year: "numeric",
    month: "long",
    day: "numeric",
    hour: "numeric",
    minute: "numeric",
    second: "numeric",
  };
  return date.toLocaleString(undefined, dtOptions);
}

add_task(async function() {
  await openPreferencesViaOpenPreferencesAPI("general", { leaveOpen: true });
  let doc = gBrowser.selectedBrowser.contentDocument;

  let showBtn = doc.getElementById("showUpdateHistory");
  let dialogOverlay = content.gSubDialog._preloadDialog._overlay;

  // XXX: For unknown reasons, this mock cannot be loaded by
  // XPCOMUtils.defineLazyServiceGetter() called in aboutDialog-appUpdater.js.
  // It is registered here so that we could assert update history subdialog
  // without stopping the preferences advanced pane from loading.
  // See bug 1361929.
  mockUpdateManager.register();

  // Test the dialog window opens
  is(dialogOverlay.style.visibility, "", "The dialog should be invisible");
  let promiseSubDialogLoaded = promiseLoadSubDialog(
    "chrome://mozapps/content/update/history.xhtml"
  );
  showBtn.doCommand();
  await promiseSubDialogLoaded;
  is(dialogOverlay.style.visibility, "visible", "The dialog should be visible");

  let dialogFrame = dialogOverlay.querySelector(".dialogFrame");
  let frameDoc = dialogFrame.contentDocument;
  let updates = frameDoc.querySelectorAll("richlistitem.update");

  // Test the update history numbers are correct
  is(
    updates.length,
    mockUpdateManager.updateCount,
    "The update count is incorrect."
  );

  // Test the updates are displayed correctly
  let update = null;
  let updateData = null;
  for (let i = 0; i < updates.length; ++i) {
    update = updates[i];
    updateData = mockUpdateManager.getUpdateAt(i);

    let testcases = [
      {
        selector: ".update-name",
        id: "update-full-build-name",
        args: { name: updateData.name, buildID: updateData.buildID },
      },
      {
        selector: ".update-installedOn-label",
        id: "update-installed-on",
        args: { date: formatInstallDate(updateData.installDate) },
      },
      {
        selector: ".update-status-label",
        id: "update-status",
        args: { status: updateData.statusText },
      },
    ];

    for (let { selector, id, args } of testcases) {
      const element = update.querySelector(selector);
      const l10nAttrs = frameDoc.l10n.getAttributes(element);
      Assert.deepEqual(
        l10nAttrs,
        {
          id,
          args,
        },
        "Wrong " + id
      );
    }

    if (update.detailsURL) {
      is(
        update.detailsURL,
        update.querySelector(".text-link").href,
        "Wrong detailsURL"
      );
    }
  }

  // Test the dialog window closes
  let closeBtn = dialogOverlay.querySelector(".dialogClose");
  closeBtn.doCommand();
  is(dialogOverlay.style.visibility, "", "The dialog should be invisible");

  mockUpdateManager.unregister();
  gBrowser.removeCurrentTab();
});
