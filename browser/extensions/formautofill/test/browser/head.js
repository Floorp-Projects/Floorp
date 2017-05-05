/* exported MANAGE_PROFILES_DIALOG_URL, EDIT_PROFILE_DIALOG_URL,
            TEST_PROFILE_1, TEST_PROFILE_2, TEST_PROFILE_3,
            getProfiles, saveProfile, removeProfiles */

"use strict";

const MANAGE_PROFILES_DIALOG_URL = "chrome://formautofill/content/manageProfiles.xhtml";
const EDIT_PROFILE_DIALOG_URL = "chrome://formautofill/content/editProfile.xhtml";

const TEST_PROFILE_1 = {
  organization: "World Wide Web Consortium",
  "street-address": "32 Vassar Street\nMIT Room 32-G524",
  "address-level2": "Cambridge",
  "address-level1": "MA",
  "postal-code": "02139",
  country: "US",
  tel: "+1 617 253 5702",
  email: "timbl@w3.org",
};

const TEST_PROFILE_2 = {
  "street-address": "Some Address",
  country: "US",
};

const TEST_PROFILE_3 = {
  "street-address": "Other Address",
  "postal-code": "12345",
};

function getProfiles() {
  return new Promise(resolve => {
    Services.cpmm.addMessageListener("FormAutofill:Profiles", function getResult(result) {
      Services.cpmm.removeMessageListener("FormAutofill:Profiles", getResult);
      resolve(result.data);
    });
    Services.cpmm.sendAsyncMessage("FormAutofill:GetProfiles", {});
  });
}

function saveProfile(profile) {
  Services.cpmm.sendAsyncMessage("FormAutofill:SaveProfile", {profile});
  return TestUtils.topicObserved("formautofill-storage-changed");
}

function removeProfiles(guids) {
  Services.cpmm.sendAsyncMessage("FormAutofill:RemoveProfiles", {guids});
  return TestUtils.topicObserved("formautofill-storage-changed");
}
