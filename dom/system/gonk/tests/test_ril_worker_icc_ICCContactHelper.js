/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

subscriptLoader.loadSubScript("resource://gre/modules/ril_consts.js", this);

function run_test() {
  run_next_test();
}

/**
 * Test error message returned in onerror for readICCContacts.
 */
add_test(function test_error_message_read_icc_contact () {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let ril = context.RIL;

  function do_test(options, expectedErrorMsg) {
    ril.sendChromeMessage = function(message) {
      do_check_eq(message.errorMsg, expectedErrorMsg);
    }
    ril.readICCContacts(options);
  }

  // Error 1, didn't specify correct contactType.
  do_test({}, CONTACT_ERR_REQUEST_NOT_SUPPORTED);

  // Error 2, specifying a non-supported contactType.
  ril.appType = CARD_APPTYPE_USIM;
  do_test({contactType: "sdn"}, CONTACT_ERR_CONTACT_TYPE_NOT_SUPPORTED);

  // Error 3, suppose we update the supported PBR fields in USIM_PBR_FIELDS,
  // but forget to add implemenetations for it.
  USIM_PBR_FIELDS.push("pbc");
  do_test({contactType: "adn"}, CONTACT_ERR_FIELD_NOT_SUPPORTED);

  run_next_test();
});

/**
 * Test error message returned in onerror for updateICCContact.
 */
add_test(function test_error_message_update_icc_contact() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let ril = context.RIL;

  const ICCID = "123456789";
  ril.iccInfo.iccid = ICCID;

  function do_test(options, expectedErrorMsg) {
    ril.sendChromeMessage = function(message) {
      do_check_eq(message.errorMsg, expectedErrorMsg);
    }
    ril.updateICCContact(options);
  }

  // Error 1, didn't specify correct contactType.
  do_test({}, CONTACT_ERR_REQUEST_NOT_SUPPORTED);

  // Error 2, specifying a correct contactType, but without providing 'contact'.
  do_test({contactType: "adn"}, CONTACT_ERR_REQUEST_NOT_SUPPORTED);

  // Error 3, specifying a non-supported contactType.
  ril.appType = CARD_APPTYPE_USIM;
  do_test({contactType: "sdn", contact: {}}, CONTACT_ERR_CONTACT_TYPE_NOT_SUPPORTED);

  // Error 4, without supplying pin2.
  do_test({contactType: "fdn", contact: {contactId: ICCID + "1"}}, GECKO_ERROR_SIM_PIN2);

  // Error 5, No free record found in EF_ADN.
  let record = context.ICCRecordHelper;
  record.readPBR = function(onsuccess, onerror) {
    onsuccess([{adn: {fileId: 0x4f3a}}]);
  };

  let io = context.ICCIOHelper;
  io.loadLinearFixedEF = function(options) {
    options.totalRecords = 1;
    options.p1 = 1;
    options.callback(options);
  };

  do_test({contactType: "adn", contact: {}}, CONTACT_ERR_NO_FREE_RECORD_FOUND);

  // Error 6, ICC IO Error.
  io.loadLinearFixedEF = function(options) {
    ril[REQUEST_SIM_IO](0, {rilRequestError: ERROR_GENERIC_FAILURE});
  };
  do_test({contactType: "adn", contact: {contactId: ICCID + "1"}},
          GECKO_ERROR_GENERIC_FAILURE);

  // Error 7, suppose we update the supported PBR fields in USIM_PBR_FIELDS,
  // but forget to add implemenetations for it.
  USIM_PBR_FIELDS.push("pbc");
  do_test({contactType: "adn", contact: {contactId: ICCID + "1"}},
          CONTACT_ERR_FIELD_NOT_SUPPORTED);

  // Error 8, EF_PBR doesn't exist.
  record.readPBR = function(onsuccess, onerror) {
    onsuccess([]);
  };

  do_test({contactType: "adn", contact: {contactId: ICCID + "1"}},
          CONTACT_ERR_CANNOT_ACCESS_PHONEBOOK);

  run_next_test();
});

/**
 * Verify ICCContactHelper.readICCContacts
 */
add_test(function test_read_icc_contacts() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let record = context.ICCRecordHelper;
  let contactHelper = context.ICCContactHelper;
  let ril = context.RIL;

  function do_test(aSimType, aContactType, aExpectedContact, aEnhancedPhoneBook) {
    ril.appType = aSimType;
    ril._isCdma = (aSimType === CARD_APPTYPE_RUIM);
    ril.iccInfoPrivate.cst = (aEnhancedPhoneBook) ?
                                    [0x0, 0x0C, 0x0, 0x0, 0x0]:
                                    [0x0, 0x00, 0x0, 0x0, 0x0];

    // Override some functions to test.
    contactHelper.getContactFieldRecordId = function(pbr, contact, field, onsuccess, onerror) {
      onsuccess(1);
    };

    record.readPBR = function readPBR(onsuccess, onerror) {
      onsuccess([{adn:{fileId: 0x6f3a}, email: {}, anr0: {}}]);
    };

    record.readADNLike = function readADNLike(fileId, onsuccess, onerror) {
      onsuccess([{recordId: 1, alphaId: "name", number: "111111"}])
    };

    record.readEmail = function readEmail(fileId, fileType, recordNumber, onsuccess, onerror) {
      onsuccess("hello@mail.com");
    };

    record.readANR = function readANR(fileId, fileType, recordNumber, onsuccess, onerror) {
      onsuccess("123456");
    };

    let onsuccess = function onsuccess(contacts) {
      let contact = contacts[0];
      for (let key in contact) {
        do_print("check " + key);
        if (Array.isArray(contact[key])) {
          do_check_eq(contact[key][0], aExpectedContact[key]);
        } else {
          do_check_eq(contact[key], aExpectedContact[key]);
        }
      }
    };

    let onerror = function onerror(errorMsg) {
      do_print("readICCContacts failed: " + errorMsg);
      do_check_true(false);
    };

    contactHelper.readICCContacts(aSimType, aContactType, onsuccess, onerror);
  }

  let expectedContact1 = {
    pbrIndex: 0,
    recordId: 1,
    alphaId:  "name",
    number:   "111111"
  };

  let expectedContact2 = {
    pbrIndex: 0,
    recordId: 1,
    alphaId:  "name",
    number:   "111111",
    email:    "hello@mail.com",
    anr:      "123456"
  };

  // SIM
  do_print("Test read SIM adn contacts");
  do_test(CARD_APPTYPE_SIM, "adn", expectedContact1);

  do_print("Test read SIM fdn contacts");
  do_test(CARD_APPTYPE_SIM, "fdn", expectedContact1);

  // USIM
  do_print("Test read USIM adn contacts");
  do_test(CARD_APPTYPE_USIM, "adn", expectedContact2);

  do_print("Test read USIM fdn contacts");
  do_test(CARD_APPTYPE_USIM, "fdn", expectedContact1);

  // RUIM
  do_print("Test read RUIM adn contacts");
  do_test(CARD_APPTYPE_RUIM, "adn", expectedContact1);

  do_print("Test read RUIM fdn contacts");
  do_test(CARD_APPTYPE_RUIM, "fdn", expectedContact1);

  // RUIM with enhanced phone book
  do_print("Test read RUIM adn contacts with enhanced phone book");
  do_test(CARD_APPTYPE_RUIM, "adn", expectedContact2, true);

  do_print("Test read RUIM fdn contacts with enhanced phone book");
  do_test(CARD_APPTYPE_RUIM, "fdn", expectedContact1, true);

  run_next_test();
});

/**
 * Verify ICCContactHelper.updateICCContact with appType is CARD_APPTYPE_USIM.
 */
add_test(function test_update_icc_contact() {
  const ADN_RECORD_ID   = 100;
  const ADN_SFI         = 1;
  const IAP_FILE_ID     = 0x4f17;
  const EMAIL_FILE_ID   = 0x4f50;
  const EMAIL_RECORD_ID = 20;
  const ANR0_FILE_ID    = 0x4f11;
  const ANR0_RECORD_ID  = 30;

  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let recordHelper = context.ICCRecordHelper;
  let contactHelper = context.ICCContactHelper;
  let ril = context.RIL;

  function do_test(aSimType, aContactType, aContact, aPin2, aFileType, aHaveIapIndex, aEnhancedPhoneBook) {
    ril.appType = aSimType;
    ril._isCdma = (aSimType === CARD_APPTYPE_RUIM);
    ril.iccInfoPrivate.cst = (aEnhancedPhoneBook) ? [0x0, 0x0C, 0x0, 0x0, 0x0]
                                                  : [0x0, 0x00, 0x0, 0x0, 0x0];

    recordHelper.readPBR = function(onsuccess, onerror) {
      if (aFileType === ICC_USIM_TYPE1_TAG) {
        onsuccess([{
          adn:   {fileId: ICC_EF_ADN},
          email: {fileId: EMAIL_FILE_ID,
                  fileType: ICC_USIM_TYPE1_TAG},
          anr0:  {fileId: ANR0_FILE_ID,
                  fileType: ICC_USIM_TYPE1_TAG}
        }]);
      } else if (aFileType === ICC_USIM_TYPE2_TAG) {
        onsuccess([{
          adn:   {fileId: ICC_EF_ADN,
                  sfi: ADN_SFI},
          iap:   {fileId: IAP_FILE_ID},
          email: {fileId: EMAIL_FILE_ID,
                  fileType: ICC_USIM_TYPE2_TAG,
                  indexInIAP: 0},
          anr0:  {fileId: ANR0_FILE_ID,
                  fileType: ICC_USIM_TYPE2_TAG,
                  indexInIAP: 1}
        }]);
      }
    };

    recordHelper.updateADNLike = function(fileId, contact, pin2, onsuccess, onerror) {
      if (aContactType === "fdn") {
        do_check_eq(fileId, ICC_EF_FDN);
      } else if (aContactType === "adn") {
        do_check_eq(fileId, ICC_EF_ADN);
      }
      do_check_eq(pin2, aPin2);
      do_check_eq(contact.alphaId, aContact.alphaId);
      do_check_eq(contact.number, aContact.number);
      onsuccess();
    };

    recordHelper.readIAP = function(fileId, recordNumber, onsuccess, onerror) {
      do_check_eq(fileId, IAP_FILE_ID);
      do_check_eq(recordNumber, ADN_RECORD_ID);
      onsuccess((aHaveIapIndex) ? [EMAIL_RECORD_ID, ANR0_RECORD_ID]
                                : [0xff, 0xff]);
    };

    recordHelper.updateIAP = function(fileId, recordNumber, iap, onsuccess, onerror) {
      do_check_eq(fileId, IAP_FILE_ID);
      do_check_eq(recordNumber, ADN_RECORD_ID);
      onsuccess();
    };

    recordHelper.updateEmail = function(pbr, recordNumber, email, adnRecordId, onsuccess, onerror) {
      do_check_eq(pbr.email.fileId, EMAIL_FILE_ID);
      if (pbr.email.fileType === ICC_USIM_TYPE1_TAG) {
        do_check_eq(recordNumber, ADN_RECORD_ID);
      } else if (pbr.email.fileType === ICC_USIM_TYPE2_TAG) {
        do_check_eq(recordNumber, EMAIL_RECORD_ID);
      }
      do_check_eq(email, aContact.email);
      onsuccess();
    };

    recordHelper.updateANR = function(pbr, recordNumber, number, adnRecordId, onsuccess, onerror) {
      do_check_eq(pbr.anr0.fileId, ANR0_FILE_ID);
      if (pbr.anr0.fileType === ICC_USIM_TYPE1_TAG) {
        do_check_eq(recordNumber, ADN_RECORD_ID);
      } else if (pbr.anr0.fileType === ICC_USIM_TYPE2_TAG) {
        do_check_eq(recordNumber, ANR0_RECORD_ID);
      }
      if (Array.isArray(aContact.anr)) {
        do_check_eq(number, aContact.anr[0]);
      }
      onsuccess();
    };

    recordHelper.findFreeRecordId = function(fileId, onsuccess, onerror) {
      let recordId = 0;
      if (fileId === EMAIL_FILE_ID) {
        recordId = EMAIL_RECORD_ID;
      } else if (fileId === ANR0_FILE_ID) {
        recordId = ANR0_RECORD_ID;
      }
      onsuccess(recordId);
    };

    let isSuccess = false;
    let onsuccess = function onsuccess() {
      do_print("updateICCContact success");
      isSuccess = true;
    };

    let onerror = function onerror(errorMsg) {
      do_print("updateICCContact failed: " + errorMsg);
    };

    contactHelper.updateICCContact(aSimType, aContactType, aContact, aPin2, onsuccess, onerror);
    do_check_true(isSuccess);
  }

  let contacts = [
    {
      pbrIndex: 0,
      recordId: ADN_RECORD_ID,
      alphaId:  "test",
      number:   "123456",
      email:    "test@mail.com",
      anr:      ["+654321"]
    },
    // a contact without email and anr.
    {
      pbrIndex: 0,
      recordId: ADN_RECORD_ID,
      alphaId:  "test2",
      number:   "123456",
    },
    // a contact with email but no anr.
    {
      pbrIndex: 0,
      recordId: ADN_RECORD_ID,
      alphaId:  "test3",
      number:   "123456",
      email:    "test@mail.com"
    },
    // a contact with anr but no email.
    {
      pbrIndex: 0,
      recordId: ADN_RECORD_ID,
      alphaId:  "test4",
      number:   "123456",
      anr:      ["+654321"]
    }];

  for (let i = 0; i < contacts.length; i++) {
    let contact = contacts[i];
    // SIM
    do_print("Test update SIM adn contacts");
    do_test(CARD_APPTYPE_SIM, "adn", contact);

    do_print("Test update SIM fdn contacts");
    do_test(CARD_APPTYPE_SIM, "fdn", contact, "1234");

    // USIM
    do_print("Test update USIM adn contacts");
    do_test(CARD_APPTYPE_USIM, "adn", contact, null, ICC_USIM_TYPE1_TAG);
    do_test(CARD_APPTYPE_USIM, "adn", contact, null, ICC_USIM_TYPE2_TAG, true);
    do_test(CARD_APPTYPE_USIM, "adn", contact, null, ICC_USIM_TYPE2_TAG, false);

    do_print("Test update USIM fdn contacts");
    do_test(CARD_APPTYPE_USIM, "fdn", contact, "1234");

    // RUIM
    do_print("Test update RUIM adn contacts");
    do_test(CARD_APPTYPE_RUIM, "adn", contact);

    do_print("Test update RUIM fdn contacts");
    do_test(CARD_APPTYPE_RUIM, "fdn", contact, "1234");

    // RUIM with enhanced phone book
    do_print("Test update RUIM adn contacts with enhanced phone book");
    do_test(CARD_APPTYPE_RUIM, "adn", contact, null, ICC_USIM_TYPE1_TAG, null, true);
    do_test(CARD_APPTYPE_RUIM, "adn", contact, null, ICC_USIM_TYPE2_TAG, true, true);
    do_test(CARD_APPTYPE_RUIM, "adn", contact, null, ICC_USIM_TYPE2_TAG, false, true);

    do_print("Test update RUIM fdn contacts with enhanced phone book");
    do_test(CARD_APPTYPE_RUIM, "fdn", contact, "1234", null, true);
  }

  run_next_test();
});

/**
 * Verify updateICCContact with removal of anr and email with File Type 1.
 */
add_test(function test_update_icc_contact_with_remove_type1_attr() {
  const ADN_RECORD_ID   = 100;
  const IAP_FILE_ID     = 0x4f17;
  const EMAIL_FILE_ID   = 0x4f50;
  const EMAIL_RECORD_ID = 20;
  const ANR0_FILE_ID    = 0x4f11;
  const ANR0_RECORD_ID  = 30;

  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let recordHelper = context.ICCRecordHelper;
  let contactHelper = context.ICCContactHelper;

  recordHelper.updateADNLike = function(fileId, contact, pin2, onsuccess, onerror) {
    onsuccess();
  };

  let contact = {
    pbrIndex: 0,
    recordId: ADN_RECORD_ID,
    alphaId:  "test2",
    number:   "123456",
  };

  recordHelper.readIAP = function(fileId, recordNumber, onsuccess, onerror) {
    onsuccess([EMAIL_RECORD_ID, ANR0_RECORD_ID]);
  };

  recordHelper.updateEmail = function(pbr, recordNumber, email, adnRecordId, onsuccess, onerror) {
    do_check_true(email == null);
    onsuccess();
  };

  recordHelper.updateANR = function(pbr, recordNumber, number, adnRecordId, onsuccess, onerror) {
    do_check_true(number == null);
    onsuccess();
  };

  function do_test(type) {
    recordHelper.readPBR = function(onsuccess, onerror) {
      if (type == ICC_USIM_TYPE1_TAG) {
        onsuccess([{
          adn:   {fileId: ICC_EF_ADN},
          email: {fileId: EMAIL_FILE_ID,
                  fileType: ICC_USIM_TYPE1_TAG},
          anr0:  {fileId: ANR0_FILE_ID,
                  fileType: ICC_USIM_TYPE1_TAG}}]);
      } else {
        onsuccess([{
          adn:   {fileId: ICC_EF_ADN},
          iap:   {fileId: IAP_FILE_ID},
          email: {fileId: EMAIL_FILE_ID,
                  fileType: ICC_USIM_TYPE2_TAG,
                  indexInIAP: 0},
          anr0:  {fileId: ANR0_FILE_ID,
                  fileType: ICC_USIM_TYPE2_TAG,
                  indexInIAP: 1}}]);
      }
    };

    let successCb = function() {
      do_check_true(true);
    };

    let errorCb = function(errorMsg) {
      do_print(errorMsg);
      do_check_true(false);
    };

    contactHelper.updateICCContact(CARD_APPTYPE_USIM, "adn", contact, null, successCb, errorCb);
  }

  do_test(ICC_USIM_TYPE1_TAG);
  do_test(ICC_USIM_TYPE2_TAG);

  run_next_test();
});

/**
 * Verify ICCContactHelper.findFreeICCContact in SIM
 */
add_test(function test_find_free_icc_contact_sim() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let recordHelper = context.ICCRecordHelper;
  let contactHelper = context.ICCContactHelper;
  // Correct record Id starts with 1, so put a null element at index 0.
  let records = [null];
  const MAX_RECORDS = 3;
  const PBR_INDEX = 0;

  recordHelper.findFreeRecordId = function(fileId, onsuccess, onerror) {
    if (records.length > MAX_RECORDS) {
      onerror("No free record found.");
      return;
    }

    onsuccess(records.length);
  };

  let successCb = function(pbrIndex, recordId) {
    do_check_eq(pbrIndex, PBR_INDEX);
    records[recordId] = {};
  };

  let errorCb = function(errorMsg) {
    do_print(errorMsg);
    do_check_true(false);
  };

  for (let i = 0; i < MAX_RECORDS; i++) {
    contactHelper.findFreeICCContact(CARD_APPTYPE_SIM, "adn", successCb, errorCb);
  }
  // The 1st element, records[0], is null.
  do_check_eq(records.length - 1, MAX_RECORDS);

  // Now the EF is full, so finding a free one should result failure.
  successCb = function(pbrIndex, recordId) {
    do_check_true(false);
  };

  errorCb = function(errorMsg) {
    do_check_true(errorMsg === "No free record found.");
  };
  contactHelper.findFreeICCContact(CARD_APPTYPE_SIM, "adn", successCb, errorCb);

  run_next_test();
});

/**
 * Verify ICCContactHelper.findFreeICCContact in USIM
 */
add_test(function test_find_free_icc_contact_usim() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let recordHelper = context.ICCRecordHelper;
  let contactHelper = context.ICCContactHelper;
  const ADN1_FILE_ID = 0x6f3a;
  const ADN2_FILE_ID = 0x6f3b;
  const MAX_RECORDS = 3;

  // The adn in the first phonebook set has already two records, which means
  // only 1 free record remained.
  let pbrs = [{adn: {fileId: ADN1_FILE_ID, records: [null, {}, {}]}},
              {adn: {fileId: ADN2_FILE_ID, records: [null]}}];

  recordHelper.readPBR = function readPBR(onsuccess, onerror) {
    onsuccess(pbrs);
  };

  recordHelper.findFreeRecordId = function(fileId, onsuccess, onerror) {
    let pbr = (fileId == ADN1_FILE_ID ? pbrs[0]: pbrs[1]);
    if (pbr.adn.records.length > MAX_RECORDS) {
      onerror("No free record found.");
      return;
    }

    onsuccess(pbr.adn.records.length);
  };

  let successCb = function(pbrIndex, recordId) {
    do_check_eq(pbrIndex, 0);
    pbrs[pbrIndex].adn.records[recordId] = {};
  };

  let errorCb = function(errorMsg) {
    do_check_true(false);
  };

  contactHelper.findFreeICCContact(CARD_APPTYPE_USIM, "adn", successCb, errorCb);

  // Now the EF_ADN in the 1st phonebook set is full, so the next free contact
  // will come from the 2nd phonebook set.
  successCb = function(pbrIndex, recordId) {
    do_check_eq(pbrIndex, 1);
    do_check_eq(recordId, 1);
  }
  contactHelper.findFreeICCContact(CARD_APPTYPE_USIM, "adn", successCb, errorCb);

  run_next_test();
});
