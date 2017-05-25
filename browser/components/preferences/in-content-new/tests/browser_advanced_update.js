/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { classes: Cc, interfaces: Ci, manager: Cm, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const uuidGenerator = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator);

const mockUpdateManager = {
  contractId: "@mozilla.org/updates/update-manager;1",

  _mockClassId: uuidGenerator.generateUUID(),

  _originalClassId: "",

  _originalFactory: null,

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIUpdateManager]),

  createInstance(outer, iiD) {
    if (outer) {
      throw Cr.NS_ERROR_NO_AGGREGATION;
    }
    return this.QueryInterface(iiD);
  },

  register() {
    let registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);
    if (!registrar.isCIDRegistered(this._mockClassId)) {
      this._originalClassId = registrar.contractIDToCID(this.contractId);
      this._originalFactory = Cm.getClassObject(Cc[this.contractId], Ci.nsIFactory);
      registrar.unregisterFactory(this._originalClassId, this._originalFactory);
      registrar.registerFactory(this._mockClassId, "Unregister after testing", this.contractId, this);
    }
  },

  unregister() {
    let registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);
    registrar.unregisterFactory(this._mockClassId, this);
    registrar.registerFactory(this._originalClassId, "", this.contractId, this._originalFactory);
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
      type: "minor",
      installDate: 1469763105156,
      detailsURL: "https://www.mozilla.org/firefox/aurora/"
    },
    {
      name: "Firefox Developer Edition 43.0a2",
      statusText: "The Update was successfully installed",
      buildID: "20150929004011",
      type: "minor",
      installDate: 1443585886224,
      detailsURL: "https://www.mozilla.org/firefox/aurora/"
    },
    {
      name: "Firefox Developer Edition 42.0a2",
      statusText: "The Update was successfully installed",
      buildID: "20150920004018",
      type: "major",
      installDate: 1442818147544,
      detailsURL: "https://www.mozilla.org/firefox/aurora/"
    }
  ]
};

function resetPreferences() {
  Services.prefs.clearUserPref("browser.search.update");
}

function formatInstallDate(sec) {
  var date = new Date(sec);
  const dtOptions = { year: "numeric", month: "long", day: "numeric",
                      hour: "numeric", minute: "numeric", second: "numeric" };
  return date.toLocaleString(undefined, dtOptions);
}

registerCleanupFunction(resetPreferences);

add_task(async function() {
  await openPreferencesViaOpenPreferencesAPI("advanced", { leaveOpen: true });
  resetPreferences();
  Services.prefs.setBoolPref("browser.search.update", false);

  let doc = gBrowser.selectedBrowser.contentDocument;
  let enableSearchUpdate = doc.getElementById("enableSearchUpdate");
  is_element_visible(enableSearchUpdate, "Check search update preference is visible");

  // Ensure that the update pref dialog reflects the actual pref value.
  ok(!enableSearchUpdate.checked, "Ensure search updates are disabled");
  Services.prefs.setBoolPref("browser.search.update", true);
  ok(enableSearchUpdate.checked, "Ensure search updates are enabled");

  gBrowser.removeCurrentTab();
});

add_task(async function() {
  await openPreferencesViaOpenPreferencesAPI("advanced", { leaveOpen: true });
  let doc = gBrowser.selectedBrowser.contentDocument;

  let showBtn = doc.getElementById("showUpdateHistory");
  let dialogOverlay = doc.getElementById("dialogOverlay");

  // XXX: For unknown reasons, this mock cannot be loaded by
  // XPCOMUtils.defineLazyServiceGetter() called in aboutDialog-appUpdater.js.
  // It is registered here so that we could assert update history subdialog
  // without stopping the preferences advanced pane from loading.
  // See bug 1361929.
  mockUpdateManager.register();

  // Test the dialog window opens
  is(dialogOverlay.style.visibility, "", "The dialog should be invisible");
  showBtn.doCommand();
  await promiseLoadSubDialog("chrome://mozapps/content/update/history.xul");
  is(dialogOverlay.style.visibility, "visible", "The dialog should be visible");

  let dialogFrame = doc.getElementById("dialogFrame");
  let frameDoc = dialogFrame.contentDocument;
  let updates = frameDoc.querySelectorAll("update");

  // Test the update history numbers are correct
  is(updates.length, mockUpdateManager.updateCount, "The update count is incorrect.");

  // Test the updates are displayed correctly
  let update = null;
  let updateData = null;
  for (let i = 0; i < updates.length; ++i) {
    update = updates[i];
    updateData = mockUpdateManager.getUpdateAt(i);

    is(update.name, updateData.name + " (" + updateData.buildID + ")", "Wrong update name");
    is(update.type, updateData.type == "major" ? "New Version" : "Security Update", "Wrong update type");
    is(update.installDate, formatInstallDate(updateData.installDate), "Wrong update installDate");
    is(update.detailsURL, updateData.detailsURL, "Wrong update detailsURL");
    is(update.status, updateData.statusText, "Wrong update status");
  }

  // Test the dialog window closes
  let closeBtn = doc.getElementById("dialogClose");
  closeBtn.doCommand();
  is(dialogOverlay.style.visibility, "", "The dialog should be invisible");

  mockUpdateManager.unregister();
  gBrowser.removeCurrentTab();
});
