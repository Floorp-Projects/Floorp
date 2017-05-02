// assert is available to chrome scripts loaded via SpecialPowers.loadChromeScript.
/* global assert */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");

var ParentUtils = {
  cleanUpProfile() {
    Services.cpmm.addMessageListener("FormAutofill:Profiles", function getResult(result) {
      Services.cpmm.removeMessageListener("FormAutofill:Profiles", getResult);

      let profiles = result.data;
      Services.cpmm.sendAsyncMessage("FormAutofill:RemoveProfiles",
                                     {guids: profiles.map(profile => profile.guid)});
    });

    Services.cpmm.sendAsyncMessage("FormAutofill:GetProfiles", {searchString: ""});
  },

  updateProfile(type, chromeMsg, msgData, contentMsg) {
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
    this.cleanUpProfile();
  },
};

ParentUtils.cleanUpProfile();
Services.obs.addObserver(ParentUtils, "formautofill-storage-changed");

addMessageListener("FormAutofillTest:AddProfile", (msg) => {
  ParentUtils.updateProfile("add", "FormAutofill:SaveProfile", msg, "FormAutofillTest:ProfileAdded");
});

addMessageListener("FormAutofillTest:RemoveProfile", (msg) => {
  ParentUtils.updateProfile("remove", "FormAutofill:Removefile", msg, "FormAutofillTest:ProfileRemoved");
});

addMessageListener("FormAutofillTest:UpdateProfile", (msg) => {
  ParentUtils.updateProfile("update", "FormAutofill:SaveProfile", msg, "FormAutofillTest:ProfileUpdated");
});

addMessageListener("cleanup", () => {
  ParentUtils.cleanup();
});
