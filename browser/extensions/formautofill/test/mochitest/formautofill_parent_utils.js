// assert is available to chrome scripts loaded via SpecialPowers.loadChromeScript.
/* global assert */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");

var ParentUtils = {
  cleanUpAddress() {
    Services.cpmm.addMessageListener("FormAutofill:Addresses", function getResult(result) {
      Services.cpmm.removeMessageListener("FormAutofill:Addresses", getResult);

      let addresses = result.data;
      Services.cpmm.sendAsyncMessage("FormAutofill:RemoveAddresses",
                                     {guids: addresses.map(address => address.guid)});
    });

    Services.cpmm.sendAsyncMessage("FormAutofill:GetAddresses", {searchString: ""});
  },

  updateAddress(type, chromeMsg, msgData, contentMsg) {
    Services.cpmm.sendAsyncMessage(chromeMsg, msgData);
    Services.obs.addObserver(function observer(subject, topic, data) {
      if (data != type) {
        return;
      }

      Services.obs.removeObserver(observer, topic);
      sendAsyncMessage(contentMsg);
    }, "formautofill-storage-changed");
  },

  observe(subject, topic, data) {
    assert.ok(topic === "formautofill-storage-changed");
    sendAsyncMessage("formautofill-storage-changed", {subject: null, topic, data});
  },

  cleanup() {
    Services.obs.removeObserver(this, "formautofill-storage-changed");
    this.cleanUpAddress();
  },
};

ParentUtils.cleanUpAddress();
Services.obs.addObserver(ParentUtils, "formautofill-storage-changed");

addMessageListener("FormAutofillTest:AddAddress", (msg) => {
  ParentUtils.updateAddress("add", "FormAutofill:SaveAddress", msg, "FormAutofillTest:AddressAdded");
});

addMessageListener("FormAutofillTest:RemoveAddress", (msg) => {
  ParentUtils.updateAddress("remove", "FormAutofill:RemoveAddress", msg, "FormAutofillTest:AddressRemoved");
});

addMessageListener("FormAutofillTest:UpdateAddress", (msg) => {
  ParentUtils.updateAddress("update", "FormAutofill:SaveAddress", msg, "FormAutofillTest:AddressUpdated");
});

addMessageListener("cleanup", () => {
  ParentUtils.cleanup();
});
