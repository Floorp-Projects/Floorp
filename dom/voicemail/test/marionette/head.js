/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {Cc: Cc, Ci: Ci, Cr: Cr, Cu: Cu} = SpecialPowers;

let RIL = {};
Cu.import("resource://gre/modules/ril_consts.js", RIL);

// Emulate Promise.jsm semantics.
Promise.defer = function() { return new Deferred(); }
function Deferred()  {
  this.promise = new Promise(function(resolve, reject) {
    this.resolve = resolve;
    this.reject = reject;
  }.bind(this));
  Object.freeze(this);
}

const MWI_PDU_PREFIX = "0000";
const MWI_PDU_UDH_PREFIX = "0040";
const MWI_PID_DEFAULT = "00";
const MWI_DCS_DISCARD_INACTIVE = "C0";
const MWI_DCS_DISCARD_ACTIVE = "C8";
const MWI_TIMESTAMP = "00000000000000";

// Only bring in what we need from ril_worker/RadioInterfaceLayer here. Reusing
// that code turns out to be a nightmare, so there is some code duplication.
let PDUBuilder = {
  toHexString: function(n, length) {
    let str = n.toString(16);
    if (str.length < length) {
      for (let i = 0; i < length - str.length; i++) {
        str = "0" + str;
      }
    }
    return str.toUpperCase();
  },

  writeUint16: function(value) {
    this.buf += (value & 0xff).toString(16).toUpperCase();
    this.buf += ((value >> 8) & 0xff).toString(16).toUpperCase();
  },

  writeHexOctet: function(octet) {
    this.buf += this.toHexString(octet, 2);
  },

  writeSwappedNibbleBCD: function(data) {
    data = data.toString();
    let zeroCharCode = '0'.charCodeAt(0);

    for (let i = 0; i < data.length; i += 2) {
      let low = data.charCodeAt(i) - zeroCharCode;
      let high;
      if (i + 1 < data.length) {
        high = data.charCodeAt(i + 1) - zeroCharCode;
      } else {
        high = 0xF;
      }

      this.writeHexOctet((high << 4) | low);
    }
  },

  writeStringAsSeptets: function(message, paddingBits, langIndex,
                                 langShiftIndex) {
    const langTable = RIL.PDU_NL_LOCKING_SHIFT_TABLES[langIndex];
    const langShiftTable = RIL.PDU_NL_SINGLE_SHIFT_TABLES[langShiftIndex];

    let dataBits = paddingBits;
    let data = 0;
    for (let i = 0; i < message.length; i++) {
      let septet = langTable.indexOf(message[i]);
      if (septet == RIL.PDU_NL_EXTENDED_ESCAPE) {
        continue;
      }

      if (septet >= 0) {
        data |= septet << dataBits;
        dataBits += 7;
      } else {
        septet = langShiftTable.indexOf(message[i]);
        if (septet == -1) {
          throw new Error(message[i] + " not in 7 bit alphabet "
                          + langIndex + ":" + langShiftIndex + "!");
        }

        if (septet == RIL.PDU_NL_RESERVED_CONTROL) {
          continue;
        }

        data |= RIL.PDU_NL_EXTENDED_ESCAPE << dataBits;
        dataBits += 7;
        data |= septet << dataBits;
        dataBits += 7;
      }

      for (; dataBits >= 8; dataBits -= 8) {
        this.writeHexOctet(data & 0xFF);
        data >>>= 8;
      }
    }

    if (dataBits != 0) {
      this.writeHexOctet(data & 0xFF);
    }
  },

  buildAddress: function(address) {
    let addressFormat = RIL.PDU_TOA_ISDN; // 81
    if (address[0] == '+') {
      addressFormat = RIL.PDU_TOA_INTERNATIONAL | RIL.PDU_TOA_ISDN; // 91
      address = address.substring(1);
    }

    this.buf = "";
    this.writeHexOctet(address.length);
    this.writeHexOctet(addressFormat);
    this.writeSwappedNibbleBCD(address);

    return this.buf;
  },

  // assumes 7 bit encoding
  buildUserData: function(options) {
    let headerLength = 0;
    this.buf = "";
    if (options.headers) {
      for each (let header in options.headers) {
        headerLength += 2; // id + length octets
        if (header.octets) {
          headerLength += header.octets.length;
        }
      };
    }

    let encodedBodyLength = options.body.length;
    let headerOctets = (headerLength ? headerLength + 1 : 0);

    let paddingBits;
    let userDataLengthInSeptets;
    let headerSeptets = Math.ceil(headerOctets * 8 / 7);
    userDataLengthInSeptets = headerSeptets + encodedBodyLength;
    paddingBits = headerSeptets * 7 - headerOctets * 8;

    this.writeHexOctet(userDataLengthInSeptets);
    if (options.headers) {
      this.writeHexOctet(headerLength);

      for each (let header in options.headers) {
        this.writeHexOctet(header.id);
        this.writeHexOctet(header.length);

        if (header.octets) {
          for each (let octet in header.octets) {
            this.writeHexOctet(octet);
          }
        }
      }
    }

    this.writeStringAsSeptets(options.body, paddingBits,
                              RIL.PDU_NL_IDENTIFIER_DEFAULT,
                              RIL.PDU_NL_IDENTIFIER_DEFAULT);
    return this.buf;
  },

  buildLevel2DiscardMwi: function(aActive, aSender, aBody) {
    return MWI_PDU_PREFIX +
           this.buildAddress(aSender) +
           MWI_PID_DEFAULT +
           (aActive ? MWI_DCS_DISCARD_ACTIVE : MWI_DCS_DISCARD_INACTIVE) +
           MWI_TIMESTAMP +
           this.buildUserData({ body: aBody });
  },

  buildLevel3DiscardMwi: function(aMessageCount, aSender, aBody) {
    let options = {
      headers: [{
        id: RIL.PDU_IEI_SPECIAL_SMS_MESSAGE_INDICATION,
        length: 2,
          octets: [
          RIL.PDU_MWI_STORE_TYPE_DISCARD,
          aMessageCount || 0
        ]
      }],
      body: aBody
    };

    return MWI_PDU_UDH_PREFIX +
           this.buildAddress(aSender) +
           MWI_PID_DEFAULT +
           MWI_DCS_DISCARD_ACTIVE +
           MWI_TIMESTAMP +
           this.buildUserData(options);
  }
};

let pendingEmulatorCmdCount = 0;

/**
 * Send emulator command with safe guard.
 *
 * We should only call |finish()| after all emulator command transactions
 * end, so here comes with the pending counter.  Resolve when the emulator
 * gives positive response, and reject otherwise.
 *
 * Fulfill params:
 *   result -- an array of emulator response lines.
 *
 * Reject params:
 *   result -- an array of emulator response lines.
 *
 * @return A deferred promise.
 */
function runEmulatorCmdSafe(aCommand) {
  let deferred = Promise.defer();

  ++pendingEmulatorCmdCount;
  runEmulatorCmd(aCommand, function(aResult) {
    --pendingEmulatorCmdCount;

    ok(true, "Emulator response: " + JSON.stringify(aResult));
    if (Array.isArray(aResult) && aResult[0] === "OK") {
      deferred.resolve(aResult);
    } else {
      deferred.reject(aResult);
    }
  });

  return deferred.promise;
}

/**
 * Promise wrapper for |SpecialPowers.pushPermissions|.
 *
 * Fulfill params: a MozVoicemail object.
 * Reject params: (none)
 *
 * @param aPermissions
 *        A permission operation description array. See
 *        |SpecialPowers.pushPermissions| for more details.
 *
 * @return A deferred promise.
 */
function pushPermissions(aPermissions) {
  let deferred = Promise.defer();

  SpecialPowers.pushPermissions(aPermissions, function() {
    ok(true, "permissions pushed: " + JSON.stringify(aPermissions));
    deferred.resolve();
  });

  return deferred.promise;
}

let voicemail;

/**
 * Add required permissions and test if |navigator.mozVoicemail| exists.
 *
 * Fulfill params: a MozVoicemail object.
 * Reject params: (none)
 *
 * @return A deferred promise.
 */
function ensureVoicemail() {
  let permissions = [{
    "type": "voicemail",
    "allow": 1,
    "context": document,
  }];

  return pushPermissions(permissions)
    .then(function() {
      voicemail = window.navigator.mozVoicemail;
      if (voicemail == null) {
        throw "navigator.mozVoicemail is undefined.";
      }

      if (!(voicemail instanceof MozVoicemail)) {
        throw "navigator.mozVoicemail is instance of " + voicemail.constructor;
      }

      return voicemail;
    });
}

/**
 * Wait for one named voicemail event.
 *
 * Resolve if that named event occurs.  Never reject.
 *
 * Fulfill params: the DOMEvent passed.
 *
 * @param aEventName
 *        A string event name.
 * @param aMatchFunc [optional]
 *        An additional callback function to match the interested event
 *        before removing the listener and going to resolve the promise.
 *
 * @return A deferred promise.
 */
function waitForManagerEvent(aEventName, aMatchFunc) {
  let deferred = Promise.defer();

  voicemail.addEventListener(aEventName, function onevent(aEvent) {
    if (aMatchFunc && !aMatchFunc(aEvent)) {
      ok(true, "MozVoicemail event '" + aEventName + "' got" +
               " but is not interested.");
      return;
    }

    ok(true, "MozVoicemail event '" + aEventName + "' got.");
    voicemail.removeEventListener(aEventName, onevent);
    deferred.resolve(aEvent);
  });

  return deferred.promise;
}

/**
 * Send raw voicemail indicator PDU. Resolve if the indicator PDU was sent with
 * success.
 *
 * Fulfill params: (none)
 * Reject params: (none)
 *
 * @return A deferred promise.
 */
function sendIndicatorPDU(aPDU) {
  return runEmulatorCmdSafe("sms pdu " + aPDU);
}

/**
 * Send raw voicemail indicator PDU and wait for "statuschanged" event. Resolve
 * if the indicator was sent and a "statuschanged" event was dispatched to
 * |navigator.mozVoicemail|.
 *
 * Fulfill params: (none)
 * Reject params: (none)
 *
 * @return A deferred promise.
 */
function sendIndicatorPDUAndWait(aPDU) {
  let promises = [];

  promises.push(waitForManagerEvent("statuschanged"));
  promises.push(sendIndicatorPDU(aPDU));

  return Promise.all(promises);
}

/**
 * Check equalities of all attributes of two VoicemailStatus instances.
 */
function compareVoicemailStatus(aStatus1, aStatus2) {
  is(aStatus1.serviceId, aStatus2.serviceId, "VoicemailStatus::serviceId");
  is(aStatus1.hasMessages, aStatus2.hasMessages, "VoicemailStatus::hasMessages");
  is(aStatus1.messageCount, aStatus2.messageCount, "VoicemailStatus::messageCount");
  is(aStatus1.returnNumber, aStatus2.returnNumber, "VoicemailStatus::returnNumber");
  is(aStatus1.returnMessage, aStatus2.returnMessage, "VoicemailStatus::returnMessage");
}

/**
 * Check if attributs of a VoicemailStatus match our expectations.
 */
function checkVoicemailStatus(aStatus, aServiceId, aHasMessages, aMessageCount,
                              aReturnNumber, aReturnMessage) {
  compareVoicemailStatus(aStatus, {
    serviceId: aServiceId,
    hasMessages: aHasMessages,
    messageCount: aMessageCount,
    returnNumber: aReturnNumber,
    returnMessage: aReturnMessage
  });
}

/**
 * Wait for pending emulator transactions and call |finish()|.
 */
function cleanUp() {
  ok(true, ":: CLEANING UP ::");

  waitFor(finish, function() {
    return pendingEmulatorCmdCount === 0;
  });
}

/**
 * Basic test routine helper for voicemail tests.
 *
 * This helper does nothing but clean-ups.
 *
 * @param aTestCaseMain
 *        A function that takes no parameter.
 */
function startTestBase(aTestCaseMain) {
  Promise.resolve()
         .then(aTestCaseMain)
         .then(cleanUp, function() {
           ok(false, 'promise rejects during test.');
           cleanUp();
         });
}

/**
 * Common test routine helper for voicemail tests.
 *
 * This function ensures global |voicemail| variable is available during the
 * process and performs clean-ups as well.
 *
 * @param aTestCaseMain
 *        A function that takes no parameter.
 */
function startTestCommon(aTestCaseMain) {
  startTestBase(function() {
    return ensureVoicemail()
      .then(aTestCaseMain);
  });
}
