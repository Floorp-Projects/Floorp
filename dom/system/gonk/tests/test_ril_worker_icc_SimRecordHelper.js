/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

subscriptLoader.loadSubScript("resource://gre/modules/ril_consts.js", this);

function run_test() {
  run_next_test();
}

/**
 * Verify reading EF_AD and parsing MCC/MNC
 */
add_test(function test_reading_ad_and_parsing_mcc_mnc() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let record = context.SimRecordHelper;
  let helper = context.GsmPDUHelper;
  let ril    = context.RIL;
  let buf    = context.Buf;
  let io     = context.ICCIOHelper;

  function do_test(mncLengthInEf, imsi, expectedMcc, expectedMnc) {
    ril.iccInfoPrivate.imsi = imsi;

    io.loadTransparentEF = function fakeLoadTransparentEF(options) {
      let ad = [0x00, 0x00, 0x00];
      if (typeof mncLengthInEf === 'number') {
        ad.push(mncLengthInEf);
      }

      // Write data size
      buf.writeInt32(ad.length * 2);

      // Write data
      for (let i = 0; i < ad.length; i++) {
        helper.writeHexOctet(ad[i]);
      }

      // Write string delimiter
      buf.writeStringDelimiter(ad.length * 2);

      if (options.callback) {
        options.callback(options);
      }
    };

    record.readAD();

    do_check_eq(ril.iccInfo.mcc, expectedMcc);
    do_check_eq(ril.iccInfo.mnc, expectedMnc);
  }

  do_test(undefined, "466923202422409", "466", "92" );
  do_test(0x00,      "466923202422409", "466", "92" );
  do_test(0x01,      "466923202422409", "466", "92" );
  do_test(0x02,      "466923202422409", "466", "92" );
  do_test(0x03,      "466923202422409", "466", "923");
  do_test(0x04,      "466923202422409", "466", "92" );
  do_test(0xff,      "466923202422409", "466", "92" );

  do_test(undefined, "310260542718417", "310", "260");
  do_test(0x00,      "310260542718417", "310", "260");
  do_test(0x01,      "310260542718417", "310", "260");
  do_test(0x02,      "310260542718417", "310", "26" );
  do_test(0x03,      "310260542718417", "310", "260");
  do_test(0x04,      "310260542718417", "310", "260");
  do_test(0xff,      "310260542718417", "310", "260");

  run_next_test();
});

add_test(function test_reading_optional_efs() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let record = context.SimRecordHelper;
  let gsmPdu = context.GsmPDUHelper;
  let ril    = context.RIL;
  let buf    = context.Buf;
  let io     = context.ICCIOHelper;

  function buildSST(supportedEf) {
    let sst = [];
    let len = supportedEf.length;
    for (let i = 0; i < len; i++) {
      let index, bitmask, iccService;
      if (ril.appType === CARD_APPTYPE_SIM) {
        iccService = GECKO_ICC_SERVICES.sim[supportedEf[i]];
        iccService -= 1;
        index = Math.floor(iccService / 4);
        bitmask = 2 << ((iccService % 4) << 1);
      } else if (ril.appType === CARD_APPTYPE_USIM){
        iccService = GECKO_ICC_SERVICES.usim[supportedEf[i]];
        iccService -= 1;
        index = Math.floor(iccService / 8);
        bitmask = 1 << ((iccService % 8) << 0);
      }

      if (sst) {
        sst[index] |= bitmask;
      }
    }
    return sst;
  }

  ril.updateCellBroadcastConfig = function fakeUpdateCellBroadcastConfig() {
    // Ignore updateCellBroadcastConfig after reading SST
  };

  function do_test(sst, supportedEf) {
    // Clone supportedEf to local array for testing
    let testEf = supportedEf.slice(0);

    record.readMSISDN = function fakeReadMSISDN() {
      testEf.splice(testEf.indexOf("MSISDN"), 1);
    };

    record.readMBDN = function fakeReadMBDN() {
      testEf.splice(testEf.indexOf("MDN"), 1);
    };

    record.readMWIS = function fakeReadMWIS() {
      testEf.splice(testEf.indexOf("MWIS"), 1);
    };

    io.loadTransparentEF = function fakeLoadTransparentEF(options) {
      // Write data size
      buf.writeInt32(sst.length * 2);

      // Write data
      for (let i = 0; i < sst.length; i++) {
         gsmPdu.writeHexOctet(sst[i] || 0);
      }

      // Write string delimiter
      buf.writeStringDelimiter(sst.length * 2);

      if (options.callback) {
        options.callback(options);
      }

      if (testEf.length !== 0) {
        do_print("Un-handled EF: " + JSON.stringify(testEf));
        do_check_true(false);
      }
    };

    record.readSST();
  }

  // TODO: Add all necessary optional EFs eventually
  let supportedEf = ["MSISDN", "MDN", "MWIS"];
  ril.appType = CARD_APPTYPE_SIM;
  do_test(buildSST(supportedEf), supportedEf);
  ril.appType = CARD_APPTYPE_USIM;
  do_test(buildSST(supportedEf), supportedEf);

  run_next_test();
});

/**
 * Verify fetchSimRecords.
 */
add_test(function test_fetch_sim_recodes() {
  let worker = newWorker();
  let context = worker.ContextPool._contexts[0];
  let RIL = context.RIL;
  let iccRecord = context.ICCRecordHelper;
  let simRecord = context.SimRecordHelper;

  function testFetchSimRecordes(expectCalled) {
    let ifCalled = [];

    RIL.getIMSI = function() {
      ifCalled.push("getIMSI");
    };

    simRecord.readAD = function() {
      ifCalled.push("readAD");
    };

    simRecord.readSST = function() {
      ifCalled.push("readSST");
    };

    simRecord.fetchSimRecords();

    for (let i = 0; i < expectCalled.length; i++ ) {
      if (ifCalled[i] != expectCalled[i]) {
        do_print(expectCalled[i] + " is not called.");
        do_check_true(false);
      }
    }
  }

  let expectCalled = ["getIMSI", "readAD", "readSST"];
  testFetchSimRecordes(expectCalled);

  run_next_test();
});

/**
 * Verify SimRecordHelper.readMWIS
 */
add_test(function test_read_mwis() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let helper = context.GsmPDUHelper;
  let recordHelper = context.SimRecordHelper;
  let buf    = context.Buf;
  let io     = context.ICCIOHelper;
  let mwisData;
  let postedMessage;

  worker.postMessage = function fakePostMessage(message) {
    postedMessage = message;
  };

  io.loadLinearFixedEF = function fakeLoadLinearFixedEF(options) {
    if (mwisData) {
      // Write data size
      buf.writeInt32(mwisData.length * 2);

      // Write MWIS
      for (let i = 0; i < mwisData.length; i++) {
        helper.writeHexOctet(mwisData[i]);
      }

      // Write string delimiter
      buf.writeStringDelimiter(mwisData.length * 2);

      options.recordSize = mwisData.length;
      if (options.callback) {
        options.callback(options);
      }
    } else {
      do_print("mwisData[] is not set.");
    }
  };

  function buildMwisData(isActive, msgCount) {
    if (msgCount < 0 || msgCount === GECKO_VOICEMAIL_MESSAGE_COUNT_UNKNOWN) {
      msgCount = 0;
    } else if (msgCount > 255) {
      msgCount = 255;
    }

    mwisData =  [ (isActive) ? 0x01 : 0x00,
                  msgCount,
                  0xFF, 0xFF, 0xFF ];
  }

  function do_test(isActive, msgCount) {
    buildMwisData(isActive, msgCount);
    recordHelper.readMWIS();

    do_check_eq("iccmwis", postedMessage.rilMessageType);
    do_check_eq(isActive, postedMessage.mwi.active);
    do_check_eq((isActive) ? msgCount : 0, postedMessage.mwi.msgCount);
  }

  do_test(true, GECKO_VOICEMAIL_MESSAGE_COUNT_UNKNOWN);
  do_test(true, 1);
  do_test(true, 255);

  do_test(false, 0);
  do_test(false, 255); // Test the corner case when mwi is disable with incorrect msgCount.

  run_next_test();
});

/**
 * Verify SimRecordHelper.updateMWIS
 */
add_test(function test_update_mwis() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let pduHelper = context.GsmPDUHelper;
  let ril = context.RIL;
  ril.appType = CARD_APPTYPE_USIM;
  ril.iccInfoPrivate.mwis = [0x00, 0x00, 0x00, 0x00, 0x00];
  let recordHelper = context.SimRecordHelper;
  let buf = context.Buf;
  let ioHelper = context.ICCIOHelper;
  let recordSize = ril.iccInfoPrivate.mwis.length;
  let recordNum = 1;

  ioHelper.updateLinearFixedEF = function(options) {
    options.pathId = context.ICCFileHelper.getEFPath(options.fileId);
    options.command = ICC_COMMAND_UPDATE_RECORD;
    options.p1 = options.recordNumber;
    options.p2 = READ_RECORD_ABSOLUTE_MODE;
    options.p3 = recordSize;
    ril.iccIO(options);
  };

  function do_test(isActive, count) {
    let mwis = ril.iccInfoPrivate.mwis;
    let isUpdated = false;

    function buildMwisData() {
      let result = mwis.slice(0);
      result[0] = isActive? (mwis[0] | 0x01) : (mwis[0] & 0xFE);
      result[1] = (count === GECKO_VOICEMAIL_MESSAGE_COUNT_UNKNOWN) ? 0 : count;

      return result;
    }

    buf.sendParcel = function() {
      isUpdated = true;

      // Request Type.
      do_check_eq(this.readInt32(), REQUEST_SIM_IO);

      // Token : we don't care
      this.readInt32();

      // command.
      do_check_eq(this.readInt32(), ICC_COMMAND_UPDATE_RECORD);

      // fileId.
      do_check_eq(this.readInt32(), ICC_EF_MWIS);

      // pathId.
      do_check_eq(this.readString(),
                  EF_PATH_MF_SIM + ((ril.appType === CARD_APPTYPE_USIM) ? EF_PATH_ADF_USIM : EF_PATH_DF_GSM));

      // p1.
      do_check_eq(this.readInt32(), recordNum);

      // p2.
      do_check_eq(this.readInt32(), READ_RECORD_ABSOLUTE_MODE);

      // p3.
      do_check_eq(this.readInt32(), recordSize);

      // data.
      let strLen = this.readInt32();
      do_check_eq(recordSize * 2, strLen);
      let expectedMwis = buildMwisData();
      for (let i = 0; i < recordSize; i++) {
        do_check_eq(expectedMwis[i], pduHelper.readHexOctet());
      }
      this.readStringDelimiter(strLen);

      // pin2.
      do_check_eq(this.readString(), null);

      if (!ril.v5Legacy) {
        // AID. Ignore because it's from modem.
        this.readInt32();
      }
    };

    do_check_false(isUpdated);

    recordHelper.updateMWIS({ active: isActive,
                              msgCount: count });

    do_check_true((ril.iccInfoPrivate.mwis) ? isUpdated : !isUpdated);
  }

  do_test(true, GECKO_VOICEMAIL_MESSAGE_COUNT_UNKNOWN);
  do_test(true, 1);
  do_test(true, 255);

  do_test(false, 0);

  // Test if Path ID is correct for SIM.
  ril.appType = CARD_APPTYPE_SIM;
  do_test(false, 0);

  // Test if loadLinearFixedEF() is not invoked in updateMWIS() when
  // EF_MWIS is not loaded/available.
  delete ril.iccInfoPrivate.mwis;
  do_test(false, 0);

  run_next_test();
});

/**
 * Verify the call flow of receiving Class 2 SMS stored in SIM:
 * 1. UNSOLICITED_RESPONSE_NEW_SMS_ON_SIM.
 * 2. SimRecordHelper.readSMS().
 * 3. sendChromeMessage() with rilMessageType == "sms-received".
 */
add_test(function test_read_new_sms_on_sim() {
  // Instead of reusing newUint8Worker defined in this file,
  // we define our own worker to fake the methods in WorkerBuffer dynamically.
  function newSmsOnSimWorkerHelper() {
    let _postedMessage;
    let _worker = newWorker({
      postRILMessage: function(data) {
      },
      postMessage: function(message) {
        _postedMessage = message;
      }
    });

    _worker.debug = do_print;

    return {
      get postedMessage() {
        return _postedMessage;
      },
      get worker() {
        return _worker;
      },
      fakeWokerBuffer: function() {
        let context = _worker.ContextPool._contexts[0];
        let index = 0; // index for read
        let buf = [];
        context.Buf.writeUint8 = function(value) {
          buf.push(value);
        };
        context.Buf.readUint8 = function() {
          return buf[index++];
        };
        context.Buf.seekIncoming = function(offset) {
          index += offset;
        };
        context.Buf.getReadAvailable = function() {
          return buf.length - index;
        };
      }
    };
  }

  let workerHelper = newSmsOnSimWorkerHelper();
  let worker = workerHelper.worker;
  let context = worker.ContextPool._contexts[0];

  context.ICCIOHelper.loadLinearFixedEF = function fakeLoadLinearFixedEF(options) {
      // SimStatus: Unread, SMSC:+0123456789, Sender: +9876543210, Text: How are you?
      let SimSmsPduHex = "0306911032547698040A9189674523010000208062917314080CC8F71D14969741F977FD07"
                       // In 4.2.25 EF_SMS Short Messages of 3GPP TS 31.102:
                       // 1. Record length == 176 bytes.
                       // 2. Any bytes in the record following the TPDU shall be filled with 'FF'.
                       + "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
                       + "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
                       + "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
                       + "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF";

      workerHelper.fakeWokerBuffer();

      context.Buf.writeString(SimSmsPduHex);

      options.recordSize = 176; // Record length is fixed to 176 bytes.
      if (options.callback) {
        options.callback(options);
      }
  };

  function newSmsOnSimParcel() {
    let data = new Uint8Array(4 + 4); // Int32List with 1 element.
    let offset = 0;

    function writeInt(value) {
      data[offset++] = value & 0xFF;
      data[offset++] = (value >>  8) & 0xFF;
      data[offset++] = (value >> 16) & 0xFF;
      data[offset++] = (value >> 24) & 0xFF;
    }

    writeInt(1); // Length of Int32List
    writeInt(1); // RecordNum = 1.

    return newIncomingParcel(-1,
                             RESPONSE_TYPE_UNSOLICITED,
                             UNSOLICITED_RESPONSE_NEW_SMS_ON_SIM,
                             data);
  }

  function do_test() {
    worker.onRILMessage(0, newSmsOnSimParcel());

    let postedMessage = workerHelper.postedMessage;

    do_check_eq("sms-received", postedMessage.rilMessageType);
    do_check_eq("+0123456789", postedMessage.SMSC);
    do_check_eq("+9876543210", postedMessage.sender);
    do_check_eq("How are you?", postedMessage.body);
  }

  do_test();

  run_next_test();
});


