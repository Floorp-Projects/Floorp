/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

subscriptLoader.loadSubScript("resource://gre/modules/ril_consts.js", this);

function run_test() {
  run_next_test();
}

/**
 * Verify ICCRecordHelper.readPBR
 */
add_test(function test_read_pbr() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let helper = context.GsmPDUHelper;
  let record = context.ICCRecordHelper;
  let buf    = context.Buf;
  let io     = context.ICCIOHelper;

  io.loadLinearFixedEF = function fakeLoadLinearFixedEF(options) {
    let pbr_1 = [
      0xa8, 0x05, 0xc0, 0x03, 0x4f, 0x3a, 0x01
    ];

    // Write data size
    buf.writeInt32(pbr_1.length * 2);

    // Write pbr
    for (let i = 0; i < pbr_1.length; i++) {
      helper.writeHexOctet(pbr_1[i]);
    }

    // Write string delimiter
    buf.writeStringDelimiter(pbr_1.length * 2);

    options.totalRecords = 2;
    if (options.callback) {
      options.callback(options);
    }
  };

  io.loadNextRecord = function fakeLoadNextRecord(options) {
    let pbr_2 = [
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff
    ];

    options.p1++;
    if (options.callback) {
      options.callback(options);
    }
  };

  let successCb = function successCb(pbrs) {
    equal(pbrs[0].adn.fileId, 0x4f3a);
    equal(pbrs.length, 1);
  };

  let errorCb = function errorCb(errorMsg) {
    do_print("Reading EF_PBR failed, msg = " + errorMsg);
    ok(false);
  };

  record.readPBR(successCb, errorCb);

  // Check cache pbrs when 2nd call
  let ifLoadEF = false;
  io.loadLinearFixedEF = function fakeLoadLinearFixedEF(options)  {
    ifLoadEF = true;
  }
  record.readPBR(successCb, errorCb);
  ok(!ifLoadEF);

  run_next_test();
});

/**
 * Verify ICCRecordHelper.readEmail
 */
add_test(function test_read_email() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let helper = context.GsmPDUHelper;
  let record = context.ICCRecordHelper;
  let buf    = context.Buf;
  let io     = context.ICCIOHelper;
  let recordSize;

  io.loadLinearFixedEF = function fakeLoadLinearFixedEF(options)  {
    let email_1 = [
      0x65, 0x6D, 0x61, 0x69, 0x6C,
      0x00, 0x6D, 0x6F, 0x7A, 0x69,
      0x6C, 0x6C, 0x61, 0x2E, 0x63,
      0x6F, 0x6D, 0x02, 0x23];

    // Write data size
    buf.writeInt32(email_1.length * 2);

    // Write email
    for (let i = 0; i < email_1.length; i++) {
      helper.writeHexOctet(email_1[i]);
    }

    // Write string delimiter
    buf.writeStringDelimiter(email_1.length * 2);

    recordSize = email_1.length;
    options.recordSize = recordSize;
    if (options.callback) {
      options.callback(options);
    }
  };

  function doTestReadEmail(type, expectedResult) {
    let fileId = 0x6a75;
    let recordNumber = 1;

    // fileId and recordNumber are dummy arguments.
    record.readEmail(fileId, type, recordNumber, function(email) {
      equal(email, expectedResult);
    });
  };

  doTestReadEmail(ICC_USIM_TYPE1_TAG, "email@mozilla.com$#");
  doTestReadEmail(ICC_USIM_TYPE2_TAG, "email@mozilla.com");
  equal(record._emailRecordSize, recordSize);

  run_next_test();
});

/**
 * Verify ICCRecordHelper.updateEmail
 */
add_test(function test_update_email() {
  const recordSize = 0x20;
  const recordNumber = 1;
  const fileId = 0x4f50;
  const NUM_TESTS = 2;
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let pduHelper = context.GsmPDUHelper;
  let iccHelper = context.ICCPDUHelper;
  let ril = context.RIL;
  ril.appType = CARD_APPTYPE_USIM;
  let recordHelper = context.ICCRecordHelper;
  let buf = context.Buf;
  let ioHelper = context.ICCIOHelper;
  let pbr = {email: {fileId: fileId, fileType: ICC_USIM_TYPE1_TAG},
             adn: {sfi: 1}};
  let count = 0;

  // Override.
  ioHelper.updateLinearFixedEF = function(options) {
    options.pathId = context.ICCFileHelper.getEFPath(options.fileId);
    options.command = ICC_COMMAND_UPDATE_RECORD;
    options.p1 = options.recordNumber;
    options.p2 = READ_RECORD_ABSOLUTE_MODE;
    options.p3 = recordSize;
    ril.iccIO(options);
  };

  function do_test(pbr, expectedEmail, expectedAdnRecordId) {
    buf.sendParcel = function() {
      count++;

      // Request Type.
      equal(this.readInt32(), REQUEST_SIM_IO);

      // Token : we don't care
      this.readInt32();

      // command.
      equal(this.readInt32(), ICC_COMMAND_UPDATE_RECORD);

      // fileId.
      equal(this.readInt32(), fileId);

      // pathId.
      equal(this.readString(),
                  EF_PATH_MF_SIM + EF_PATH_DF_TELECOM + EF_PATH_DF_PHONEBOOK);

      // p1.
      equal(this.readInt32(), recordNumber);

      // p2.
      equal(this.readInt32(), READ_RECORD_ABSOLUTE_MODE);

      // p3.
      equal(this.readInt32(), recordSize);

      // data.
      let strLen = this.readInt32();
      let email;
      if (pbr.email.fileType === ICC_USIM_TYPE1_TAG) {
        email = iccHelper.read8BitUnpackedToString(recordSize);
      } else {
        email = iccHelper.read8BitUnpackedToString(recordSize - 2);
        equal(pduHelper.readHexOctet(), pbr.adn.sfi);
        equal(pduHelper.readHexOctet(), expectedAdnRecordId);
      }
      this.readStringDelimiter(strLen);
      equal(email, expectedEmail);

      // pin2.
      equal(this.readString(), null);

      // AID. Ignore because it's from modem.
      this.readInt32();

      if (count == NUM_TESTS) {
        run_next_test();
      }
    };
    recordHelper.updateEmail(pbr, recordNumber, expectedEmail, expectedAdnRecordId);
  }

  do_test(pbr, "test@mail.com");
  pbr.email.fileType = ICC_USIM_TYPE2_TAG;
  do_test(pbr, "test@mail.com", 1);
});

/**
 * Verify ICCRecordHelper.readANR
 */
add_test(function test_read_anr() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let helper = context.GsmPDUHelper;
  let record = context.ICCRecordHelper;
  let buf    = context.Buf;
  let io     = context.ICCIOHelper;
  let recordSize;

  io.loadLinearFixedEF = function fakeLoadLinearFixedEF(options)  {
    let anr_1 = [
      0x01, 0x05, 0x81, 0x10, 0x32,
      0x54, 0xF6, 0xFF, 0xFF];

    // Write data size
    buf.writeInt32(anr_1.length * 2);

    // Write anr
    for (let i = 0; i < anr_1.length; i++) {
      helper.writeHexOctet(anr_1[i]);
    }

    // Write string delimiter
    buf.writeStringDelimiter(anr_1.length * 2);

    recordSize = anr_1.length;
    options.recordSize = recordSize;
    if (options.callback) {
      options.callback(options);
    }
  };

  function doTestReadAnr(fileType, expectedResult) {
    let fileId = 0x4f11;
    let recordNumber = 1;

    // fileId and recordNumber are dummy arguments.
    record.readANR(fileId, fileType, recordNumber, function(anr) {
      equal(anr, expectedResult);
    });
  };

  doTestReadAnr(ICC_USIM_TYPE1_TAG, "0123456");
  equal(record._anrRecordSize, recordSize);

  run_next_test();
});

/**
 * Verify ICCRecordHelper.updateANR
 */
add_test(function test_update_anr() {
  const recordSize = 0x20;
  const recordNumber = 1;
  const fileId = 0x4f11;
  const NUM_TESTS = 2;
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let pduHelper = context.GsmPDUHelper;
  let iccHelper = context.ICCPDUHelper;
  let ril = context.RIL;
  ril.appType = CARD_APPTYPE_USIM;
  let recordHelper = context.ICCRecordHelper;
  let buf = context.Buf;
  let ioHelper = context.ICCIOHelper;
  let pbr = {anr0: {fileId: fileId, fileType: ICC_USIM_TYPE1_TAG},
             adn: {sfi: 1}};
  let count = 0;

  // Override.
  ioHelper.updateLinearFixedEF = function(options) {
    options.pathId = context.ICCFileHelper.getEFPath(options.fileId);
    options.command = ICC_COMMAND_UPDATE_RECORD;
    options.p1 = options.recordNumber;
    options.p2 = READ_RECORD_ABSOLUTE_MODE;
    options.p3 = recordSize;
    ril.iccIO(options);
  };

  function do_test(pbr, expectedANR, expectedAdnRecordId) {
    buf.sendParcel = function() {
      count++;

      // Request Type.
      equal(this.readInt32(), REQUEST_SIM_IO);

      // Token : we don't care
      this.readInt32();

      // command.
      equal(this.readInt32(), ICC_COMMAND_UPDATE_RECORD);

      // fileId.
      equal(this.readInt32(), fileId);

      // pathId.
      equal(this.readString(),
                  EF_PATH_MF_SIM + EF_PATH_DF_TELECOM + EF_PATH_DF_PHONEBOOK);

      // p1.
      equal(this.readInt32(), recordNumber);

      // p2.
      equal(this.readInt32(), READ_RECORD_ABSOLUTE_MODE);

      // p3.
      equal(this.readInt32(), recordSize);

      // data.
      let strLen = this.readInt32();
      // EF_AAS, ignore.
      pduHelper.readHexOctet();
      equal(iccHelper.readNumberWithLength(), expectedANR);
      // EF_CCP, ignore.
      pduHelper.readHexOctet();
      // EF_EXT1, ignore.
      pduHelper.readHexOctet();
      if (pbr.anr0.fileType === ICC_USIM_TYPE2_TAG) {
        equal(pduHelper.readHexOctet(), pbr.adn.sfi);
        equal(pduHelper.readHexOctet(), expectedAdnRecordId);
      }
      this.readStringDelimiter(strLen);

      // pin2.
      equal(this.readString(), null);

      // AID. Ignore because it's from modem.
       this.readInt32();

      if (count == NUM_TESTS) {
        run_next_test();
      }
    };
    recordHelper.updateANR(pbr, recordNumber, expectedANR, expectedAdnRecordId);
  }

  do_test(pbr, "+123456789");
  pbr.anr0.fileType = ICC_USIM_TYPE2_TAG;
  do_test(pbr, "123456789", 1);
});

/**
 * Verify ICCRecordHelper.readIAP
 */
add_test(function test_read_iap() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let helper = context.GsmPDUHelper;
  let record = context.ICCRecordHelper;
  let buf    = context.Buf;
  let io     = context.ICCIOHelper;
  let recordSize;

  io.loadLinearFixedEF = function fakeLoadLinearFixedEF(options)  {
    let iap_1 = [0x01, 0x02];

    // Write data size/
    buf.writeInt32(iap_1.length * 2);

    // Write iap.
    for (let i = 0; i < iap_1.length; i++) {
      helper.writeHexOctet(iap_1[i]);
    }

    // Write string delimiter.
    buf.writeStringDelimiter(iap_1.length * 2);

    recordSize = iap_1.length;
    options.recordSize = recordSize;
    if (options.callback) {
      options.callback(options);
    }
  };

  function doTestReadIAP(expectedIAP) {
    const fileId = 0x4f17;
    const recordNumber = 1;

    let successCb = function successCb(iap) {
      for (let i = 0; i < iap.length; i++) {
        equal(expectedIAP[i], iap[i]);
      }
      run_next_test();
    }.bind(this);

    let errorCb = function errorCb(errorMsg) {
      do_print(errorMsg);
      ok(false);
      run_next_test();
    }.bind(this);

    record.readIAP(fileId, recordNumber, successCb, errorCb);
  };

  doTestReadIAP([1, 2]);
});

/**
 * Verify ICCRecordHelper.updateIAP
 */
add_test(function test_update_iap() {
  const recordSize = 2;
  const recordNumber = 1;
  const fileId = 0x4f17;
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let pduHelper = context.GsmPDUHelper;
  let ril = context.RIL;
  ril.appType = CARD_APPTYPE_USIM;
  let recordHelper = context.ICCRecordHelper;
  let buf = context.Buf;
  let ioHelper = context.ICCIOHelper;
  let count = 0;

  // Override.
  ioHelper.updateLinearFixedEF = function(options) {
    options.pathId = context.ICCFileHelper.getEFPath(options.fileId);
    options.command = ICC_COMMAND_UPDATE_RECORD;
    options.p1 = options.recordNumber;
    options.p2 = READ_RECORD_ABSOLUTE_MODE;
    options.p3 = recordSize;
    ril.iccIO(options);
  };

  function do_test(expectedIAP) {
    buf.sendParcel = function() {
      // Request Type.
      equal(this.readInt32(), REQUEST_SIM_IO);

      // Token : we don't care
      this.readInt32();

      // command.
      equal(this.readInt32(), ICC_COMMAND_UPDATE_RECORD);

      // fileId.
      equal(this.readInt32(), fileId);

      // pathId.
      equal(this.readString(),
                  EF_PATH_MF_SIM + EF_PATH_DF_TELECOM + EF_PATH_DF_PHONEBOOK);

      // p1.
      equal(this.readInt32(), recordNumber);

      // p2.
      equal(this.readInt32(), READ_RECORD_ABSOLUTE_MODE);

      // p3.
      equal(this.readInt32(), recordSize);

      // data.
      let strLen = this.readInt32();
      for (let i = 0; i < recordSize; i++) {
        equal(expectedIAP[i], pduHelper.readHexOctet());
      }
      this.readStringDelimiter(strLen);

      // pin2.
      equal(this.readString(), null);

      // AID. Ignore because it's from modem.
      this.readInt32();

      run_next_test();
    };
    recordHelper.updateIAP(fileId, recordNumber, expectedIAP);
  }

  do_test([1, 2]);
});

/**
 * Verify ICCRecordHelper.updateADNLike.
 */
add_test(function test_update_adn_like() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let ril = context.RIL;
  let record = context.ICCRecordHelper;
  let io = context.ICCIOHelper;
  let pdu = context.ICCPDUHelper;
  let buf = context.Buf;

  ril.appType = CARD_APPTYPE_SIM;
  const recordSize = 0x20;
  let fileId;

  // Override.
  io.updateLinearFixedEF = function(options) {
    options.pathId = context.ICCFileHelper.getEFPath(options.fileId);
    options.command = ICC_COMMAND_UPDATE_RECORD;
    options.p1 = options.recordNumber;
    options.p2 = READ_RECORD_ABSOLUTE_MODE;
    options.p3 = recordSize;
    ril.iccIO(options);
  };

  buf.sendParcel = function() {
    // Request Type.
    equal(this.readInt32(), REQUEST_SIM_IO);

    // Token : we don't care
    this.readInt32();

    // command.
    equal(this.readInt32(), ICC_COMMAND_UPDATE_RECORD);

    // fileId.
    equal(this.readInt32(), fileId);

    // pathId.
    equal(this.readString(), EF_PATH_MF_SIM + EF_PATH_DF_TELECOM);

    // p1.
    equal(this.readInt32(), 1);

    // p2.
    equal(this.readInt32(), READ_RECORD_ABSOLUTE_MODE);

    // p3.
    equal(this.readInt32(), 0x20);

    // data.
    let contact = pdu.readAlphaIdDiallingNumber(0x20);
    equal(contact.alphaId, "test");
    equal(contact.number, "123456");

    // pin2.
    if (fileId == ICC_EF_ADN) {
      equal(this.readString(), null);
    } else {
      equal(this.readString(), "1111");
    }

    // AID. Ignore because it's from modem.
    this.readInt32();

    if (fileId == ICC_EF_FDN) {
      run_next_test();
    }
  };

  fileId = ICC_EF_ADN;
  record.updateADNLike(fileId,
                       {recordId: 1, alphaId: "test", number: "123456"});

  fileId = ICC_EF_FDN;
  record.updateADNLike(fileId,
                       {recordId: 1, alphaId: "test", number: "123456"},
                       "1111");
});

/**
 * Verify ICCRecordHelper.findFreeRecordId.
 */
add_test(function test_find_free_record_id() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let pduHelper = context.GsmPDUHelper;
  let recordHelper = context.ICCRecordHelper;
  let buf = context.Buf;
  let io  = context.ICCIOHelper;
  let ril = context.RIL;

  function writeRecord(record) {
    // Write data size
    buf.writeInt32(record.length * 2);

    for (let i = 0; i < record.length; i++) {
      pduHelper.writeHexOctet(record[i]);
    }

    // Write string delimiter
    buf.writeStringDelimiter(record.length * 2);
  }

  io.loadLinearFixedEF = function fakeLoadLinearFixedEF(options)  {
    // Some random data.
    let record = [0x12, 0x34, 0x56, 0x78, 0x90];
    options.p1 = 1;
    options.totalRecords = 2;
    writeRecord(record);
    if (options.callback) {
      options.callback(options);
    }
  };

  ril.iccIO = function fakeIccIO(options) {
    // Unused bytes.
    let record = [0xff, 0xff, 0xff, 0xff, 0xff];
    writeRecord(record);
    if (options.callback) {
      options.callback(options);
    }
  };

  let fileId = 0x0000; // Dummy.
  recordHelper.findFreeRecordId(
    fileId,
    function(recordId) {
      equal(recordId, 2);
      run_next_test();
    }.bind(this),
    function(errorMsg) {
      do_print(errorMsg);
      ok(false);
      run_next_test();
    }.bind(this));
});

/**
 * Verify ICCRecordHelper.fetchICCRecords.
 */
add_test(function test_fetch_icc_recodes() {
  let worker = newWorker();
  let context = worker.ContextPool._contexts[0];
  let RIL = context.RIL;
  let iccRecord = context.ICCRecordHelper;
  let simRecord = context.SimRecordHelper;
  let ruimRecord = context.RuimRecordHelper;
  let fetchTag = 0x00;

  simRecord.fetchSimRecords = function() {
    fetchTag = 0x01;
  };

  ruimRecord.fetchRuimRecords = function() {
    fetchTag = 0x02;
  };

  RIL.appType = CARD_APPTYPE_SIM;
  iccRecord.fetchICCRecords();
  equal(fetchTag, 0x01);

  RIL.appType = CARD_APPTYPE_RUIM;
  iccRecord.fetchICCRecords();
  equal(fetchTag, 0x02);

  RIL.appType = CARD_APPTYPE_USIM;
  iccRecord.fetchICCRecords();
  equal(fetchTag, 0x01);

  run_next_test();
});

/**
 * Verify reading EF_ICCID.
 */
add_test(function test_handling_iccid() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let record = context.ICCRecordHelper;
  let helper = context.GsmPDUHelper;
  let ril = context.RIL;
  let buf = context.Buf;
  let io = context.ICCIOHelper;

  ril.reportStkServiceIsRunning = function fakeReportStkServiceIsRunning() {
  };

  function do_test(rawICCID, expectedICCID) {
    io.loadTransparentEF = function fakeLoadTransparentEF(options) {
      // Write data size
      buf.writeInt32(rawICCID.length);

      // Write data
      for (let i = 0; i < rawICCID.length; i += 2) {
        helper.writeHexOctet(parseInt(rawICCID.substr(i, 2), 16));
      }

      // Write string delimiter
      buf.writeStringDelimiter(rawICCID.length);

      if (options.callback) {
        options.callback(options);
      }
    };

    record.readICCID();

    equal(ril.iccInfo.iccid, expectedICCID);
  }

  // Invalid value 0xE at high nibbile + low nibbile contains 0xF.
  do_test("9868002E90909F001519", "89860020909");
  // Invalid value 0xD at low nibbile.
  do_test("986800D2909090001519", "8986002090909005191");
  // Invalid value 0xC at low nibbile.
  do_test("986800C2909090001519", "8986002090909005191");
  // Invalid value 0xB at low nibbile.
  do_test("986800B2909090001519", "8986002090909005191");
  // Invalid value 0xA at low nibbile.
  do_test("986800A2909090001519", "8986002090909005191");
  // Valid ICCID.
  do_test("98101430121181157002", "89014103211118510720");

  run_next_test();
});
