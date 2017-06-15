/* exported MANAGE_PROFILES_DIALOG_URL, EDIT_PROFILE_DIALOG_URL,
            TEST_ADDRESS_1, TEST_ADDRESS_2, TEST_ADDRESS_3,
            getAddresses, saveAddress, removeAddresses */

"use strict";

const MANAGE_PROFILES_DIALOG_URL = "chrome://formautofill/content/manageProfiles.xhtml";
const EDIT_PROFILE_DIALOG_URL = "chrome://formautofill/content/editProfile.xhtml";

const TEST_ADDRESS_1 = {
  "given-name": "John",
  "additional-name": "R.",
  "family-name": "Smith",
  organization: "World Wide Web Consortium",
  "street-address": "32 Vassar Street\nMIT Room 32-G524",
  "address-level2": "Cambridge",
  "address-level1": "MA",
  "postal-code": "02139",
  country: "US",
  tel: "+1 617 253 5702",
  email: "timbl@w3.org",
};

const TEST_ADDRESS_2 = {
  "street-address": "Some Address",
  country: "US",
};

const TEST_ADDRESS_3 = {
  "street-address": "Other Address",
  "postal-code": "12345",
};

function getAddresses() {
  return new Promise(resolve => {
    Services.cpmm.addMessageListener("FormAutofill:Addresses", function getResult(result) {
      Services.cpmm.removeMessageListener("FormAutofill:Addresses", getResult);
      resolve(result.data);
    });
    Services.cpmm.sendAsyncMessage("FormAutofill:GetAddresses", {});
  });
}

function saveAddress(address) {
  Services.cpmm.sendAsyncMessage("FormAutofill:SaveAddress", {address});
  return TestUtils.topicObserved("formautofill-storage-changed");
}

function removeAddresses(guids) {
  Services.cpmm.sendAsyncMessage("FormAutofill:RemoveAddresses", {guids});
  return TestUtils.topicObserved("formautofill-storage-changed");
}
