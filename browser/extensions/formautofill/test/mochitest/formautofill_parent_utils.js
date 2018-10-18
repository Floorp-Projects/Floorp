// assert is available to chrome scripts loaded via SpecialPowers.loadChromeScript.
/* global assert */
/* eslint-env mozilla/frame-script */

"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://formautofill/FormAutofillUtils.jsm");

let {formAutofillStorage} = ChromeUtils.import("resource://formautofill/FormAutofillStorage.jsm", {});

const {ADDRESSES_COLLECTION_NAME, CREDITCARDS_COLLECTION_NAME} = FormAutofillUtils;

var ParentUtils = {
  async _getRecords(collectionName) {
    return new Promise(resolve => {
      Services.cpmm.addMessageListener("FormAutofill:Records", function getResult({data}) {
        Services.cpmm.removeMessageListener("FormAutofill:Records", getResult);
        resolve(data);
      });
      Services.cpmm.sendAsyncMessage("FormAutofill:GetRecords", {searchString: "", collectionName});
    });
  },

  async _storageChangeObserved({topic = "formautofill-storage-changed", type, times = 1}) {
    let count = times;

    return new Promise(resolve => {
      Services.obs.addObserver(function observer(subject, obsTopic, data) {
        if (type && data != type || !!--count) {
          return;
        }

        // every notification type should have the collection name.
        let allowedNames = [ADDRESSES_COLLECTION_NAME, CREDITCARDS_COLLECTION_NAME];
        assert.ok(allowedNames.includes(subject.wrappedJSObject.collectionName),
                  "should include the collection name");
        // every notification except removeAll should have a guid.
        if (data != "removeAll") {
          assert.ok(subject.wrappedJSObject.guid, "should have a guid");
        }
        Services.obs.removeObserver(observer, obsTopic);
        resolve();
      }, topic);
    });
  },

  async _operateRecord(collectionName, type, msgData, contentMsg) {
    let times, topic;

    if (collectionName == ADDRESSES_COLLECTION_NAME) {
      switch (type) {
        case "add": {
          Services.cpmm.sendAsyncMessage("FormAutofill:SaveAddress", msgData);
          break;
        }
        case "update": {
          Services.cpmm.sendAsyncMessage("FormAutofill:SaveAddress", msgData);
          break;
        }
        case "remove": {
          times = msgData.guids.length;
          Services.cpmm.sendAsyncMessage("FormAutofill:RemoveAddresses", msgData);
          break;
        }
      }
    } else {
      switch (type) {
        case "add": {
          const msgDataCloned = Object.assign({}, msgData);

          Services.cpmm.sendAsyncMessage("FormAutofill:SaveCreditCard", msgDataCloned);
          break;
        }
        case "remove": {
          times = msgData.guids.length;
          Services.cpmm.sendAsyncMessage("FormAutofill:RemoveCreditCards", msgData);
          break;
        }
      }
    }

    await this._storageChangeObserved({type, times, topic});
    sendAsyncMessage(contentMsg);
  },

  async operateAddress(type, msgData, contentMsg) {
    await this._operateRecord(ADDRESSES_COLLECTION_NAME, ...arguments);
  },

  async operateCreditCard(type, msgData, contentMsg) {
    await this._operateRecord(CREDITCARDS_COLLECTION_NAME, ...arguments);
  },

  async cleanUpAddresses() {
    const guids = (await this._getRecords(ADDRESSES_COLLECTION_NAME)).map(record => record.guid);

    if (guids.length == 0) {
      sendAsyncMessage("FormAutofillTest:AddressesCleanedUp");
      return;
    }

    await this.operateAddress("remove", {guids}, "FormAutofillTest:AddressesCleanedUp");
  },

  async cleanUpCreditCards() {
    const guids = (await this._getRecords(CREDITCARDS_COLLECTION_NAME)).map(record => record.guid);

    if (guids.length == 0) {
      sendAsyncMessage("FormAutofillTest:CreditCardsCleanedUp");
      return;
    }

    await this.operateCreditCard("remove", {guids}, "FormAutofillTest:CreditCardsCleanedUp");
  },

  async cleanup() {
    await this.cleanUpAddresses();
    await this.cleanUpCreditCards();
    Services.obs.removeObserver(this, "formautofill-storage-changed");
  },

  _areRecordsMatching(recordA, recordB, collectionName) {
    for (let field of formAutofillStorage[collectionName].VALID_FIELDS) {
      if (recordA[field] !== recordB[field]) {
        return false;
      }
    }
    // Check the internal field if both addresses have valid value.
    for (let field of formAutofillStorage.INTERNAL_FIELDS) {
      if (field in recordA && field in recordB && (recordA[field] !== recordB[field])) {
        return false;
      }
    }
    return true;
  },

  async _checkRecords(collectionName, expectedRecords) {
    const records = await this._getRecords(collectionName);

    if (records.length !== expectedRecords.length) {
      return false;
    }

    for (let record of records) {
      let matching = expectedRecords.some(expectedRecord => {
        return ParentUtils._areRecordsMatching(record, expectedRecord, collectionName);
      });

      if (!matching) {
        return false;
      }
    }

    return true;
  },

  async checkAddresses({expectedAddresses}) {
    const areMatched = await this._checkRecords(ADDRESSES_COLLECTION_NAME, expectedAddresses);

    sendAsyncMessage("FormAutofillTest:areAddressesMatching", areMatched);
  },

  async checkCreditCards({expectedCreditCards}) {
    const areMatched = await this._checkRecords(CREDITCARDS_COLLECTION_NAME, expectedCreditCards);

    sendAsyncMessage("FormAutofillTest:areCreditCardsMatching", areMatched);
  },

  observe(subject, topic, data) {
    assert.ok(topic === "formautofill-storage-changed");
    sendAsyncMessage("formautofill-storage-changed", {subject: null, topic, data});
  },
};

Services.obs.addObserver(ParentUtils, "formautofill-storage-changed");

Services.mm.addMessageListener("FormAutofill:FieldsIdentified", () => {
  sendAsyncMessage("FormAutofillTest:FieldsIdentified");
});

addMessageListener("FormAutofillTest:AddAddress", (msg) => {
  ParentUtils.operateAddress("add", msg, "FormAutofillTest:AddressAdded");
});

addMessageListener("FormAutofillTest:RemoveAddress", (msg) => {
  ParentUtils.operateAddress("remove", msg, "FormAutofillTest:AddressRemoved");
});

addMessageListener("FormAutofillTest:UpdateAddress", (msg) => {
  ParentUtils.operateAddress("update", msg, "FormAutofillTest:AddressUpdated");
});

addMessageListener("FormAutofillTest:CheckAddresses", (msg) => {
  ParentUtils.checkAddresses(msg);
});

addMessageListener("FormAutofillTest:CleanUpAddresses", (msg) => {
  ParentUtils.cleanUpAddresses();
});

addMessageListener("FormAutofillTest:AddCreditCard", (msg) => {
  ParentUtils.operateCreditCard("add", msg, "FormAutofillTest:CreditCardAdded");
});

addMessageListener("FormAutofillTest:RemoveCreditCard", (msg) => {
  ParentUtils.operateCreditCard("remove", msg, "FormAutofillTest:CreditCardRemoved");
});

addMessageListener("FormAutofillTest:CheckCreditCards", (msg) => {
  ParentUtils.checkCreditCards(msg);
});

addMessageListener("FormAutofillTest:CleanUpCreditCards", (msg) => {
  ParentUtils.cleanUpCreditCards();
});

addMessageListener("cleanup", () => {
  ParentUtils.cleanup().then(() => {
    sendAsyncMessage("cleanup-finished", {});
  });
});
