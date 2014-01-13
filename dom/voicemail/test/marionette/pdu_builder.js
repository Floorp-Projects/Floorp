/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

let RIL = {};
SpecialPowers.Cu.import("resource://gre/modules/ril_consts.js", RIL);

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
  }
};
