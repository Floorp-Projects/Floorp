/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

var RIL = {};
Cu.import("resource://gre/modules/ril_consts.js", RIL);

/**
 * SmsSegmentHelper
 */
this.SmsSegmentHelper = {
  /**
   * Get valid SMS concatenation reference number.
   */
  _segmentRef: 0,
  get nextSegmentRef() {
    let ref = this._segmentRef++;

    this._segmentRef %= (this.segmentRef16Bit ? 65535 : 255);

    // 0 is not a valid SMS concatenation reference number.
    return ref + 1;
  },

  /**
   * Calculate encoded length using specified locking/single shift table
   *
   * @param aMessage
   *        message string to be encoded.
   * @param aLangTable
   *        locking shift table string.
   * @param aLangShiftTable
   *        single shift table string.
   * @param aStrict7BitEncoding [Optional]
   *        Enable Latin characters replacement with corresponding
   *        ones in GSM SMS 7-bit default alphabet.
   *
   * @return encoded length in septets.
   *
   * @note The algorithm used in this function must match exactly with
   *       GsmPDUHelper#writeStringAsSeptets.
   */
  countGsm7BitSeptets: function(aMessage, aLangTable, aLangShiftTable, aStrict7BitEncoding) {
    let length = 0;
    for (let msgIndex = 0; msgIndex < aMessage.length; msgIndex++) {
      let c = aMessage.charAt(msgIndex);
      if (aStrict7BitEncoding) {
        c = RIL.GSM_SMS_STRICT_7BIT_CHARMAP[c] || c;
      }

      let septet = aLangTable.indexOf(c);

      // According to 3GPP TS 23.038, section 6.1.1 General notes, "The
      // characters marked '1)' are not used but are displayed as a space."
      if (septet == RIL.PDU_NL_EXTENDED_ESCAPE) {
        continue;
      }

      if (septet >= 0) {
        length++;
        continue;
      }

      septet = aLangShiftTable.indexOf(c);
      if (septet < 0) {
        if (!aStrict7BitEncoding) {
          return -1;
        }

        // Bug 816082, when aStrict7BitEncoding is enabled, we should replace
        // characters that can't be encoded with GSM 7-Bit alphabets with '*'.
        c = "*";
        if (aLangTable.indexOf(c) >= 0) {
          length++;
        } else if (aLangShiftTable.indexOf(c) >= 0) {
          length += 2;
        } else {
          // We can't even encode a '*' character with current configuration.
          return -1;
        }

        continue;
      }

      // According to 3GPP TS 23.038 B.2, "This code represents a control
      // character and therefore must not be used for language specific
      // characters."
      if (septet == RIL.PDU_NL_RESERVED_CONTROL) {
        continue;
      }

      // The character is not found in locking shfit table, but could be
      // encoded as <escape><char> with single shift table. Note that it's
      // still possible for septet to has the value of PDU_NL_EXTENDED_ESCAPE,
      // but we can display it as a space in this case as said in previous
      // comment.
      length += 2;
    }

    return length;
  },

  /**
   * Calculate user data length of specified message string encoded in GSM 7Bit
   * alphabets.
   *
   * @param aMessage
   *        a message string to be encoded.
   * @param aStrict7BitEncoding [Optional]
   *        Enable Latin characters replacement with corresponding
   *        ones in GSM SMS 7-bit default alphabet.
   *
   * @return null or an options object with attributes `dcs`,
   *         `userDataHeaderLength`, `encodedFullBodyLength`, `langIndex`,
   *         `langShiftIndex`, `segmentMaxSeq` set.
   *
   * @see #calculateUserDataLength().
   *
   * |enabledGsmTableTuples|:
   *   List of tuples of national language identifier pairs.
   * |segmentRef16Bit|:
   *   Use 16-bit reference number for concatenated outgoint messages.
   *   TODO: Support static/runtime settings, see bug 1019443.
   */
  enabledGsmTableTuples: [
    [RIL.PDU_NL_IDENTIFIER_DEFAULT, RIL.PDU_NL_IDENTIFIER_DEFAULT],
  ],
  segmentRef16Bit: false,
  calculateUserDataLength7Bit: function(aMessage, aStrict7BitEncoding) {
    let options = null;
    let minUserDataSeptets = Number.MAX_VALUE;
    for (let i = 0; i < this.enabledGsmTableTuples.length; i++) {
      let [langIndex, langShiftIndex] = this.enabledGsmTableTuples[i];

      const langTable = RIL.PDU_NL_LOCKING_SHIFT_TABLES[langIndex];
      const langShiftTable = RIL.PDU_NL_SINGLE_SHIFT_TABLES[langShiftIndex];

      let bodySeptets = this.countGsm7BitSeptets(aMessage,
                                                  langTable,
                                                  langShiftTable,
                                                  aStrict7BitEncoding);
      if (bodySeptets < 0) {
        continue;
      }

      let headerLen = 0;
      if (langIndex != RIL.PDU_NL_IDENTIFIER_DEFAULT) {
        headerLen += 3; // IEI + len + langIndex
      }
      if (langShiftIndex != RIL.PDU_NL_IDENTIFIER_DEFAULT) {
        headerLen += 3; // IEI + len + langShiftIndex
      }

      // Calculate full user data length, note the extra byte is for header len
      let headerSeptets = Math.ceil((headerLen ? headerLen + 1 : 0) * 8 / 7);
      let segmentSeptets = RIL.PDU_MAX_USER_DATA_7BIT;
      if ((bodySeptets + headerSeptets) > segmentSeptets) {
        headerLen += this.segmentRef16Bit ? 6 : 5;
        headerSeptets = Math.ceil((headerLen + 1) * 8 / 7);
      }
      segmentSeptets -= headerSeptets;

      let segments = Math.ceil(bodySeptets / segmentSeptets);
      let userDataSeptets = bodySeptets + headerSeptets * segments;
      if (userDataSeptets >= minUserDataSeptets) {
        continue;
      }

      minUserDataSeptets = userDataSeptets;

      options = {
        dcs: RIL.PDU_DCS_MSG_CODING_7BITS_ALPHABET,
        encodedFullBodyLength: bodySeptets,
        userDataHeaderLength: headerLen,
        langIndex: langIndex,
        langShiftIndex: langShiftIndex,
        segmentMaxSeq: segments,
        segmentChars: segmentSeptets,
      };
    }

    return options;
  },

  /**
   * Calculate user data length of specified message string encoded in UCS2.
   *
   * @param aMessage
   *        a message string to be encoded.
   *
   * @return an options object with attributes `dcs`, `userDataHeaderLength`,
   *         `encodedFullBodyLength`, `segmentMaxSeq` set.
   *
   * @see #calculateUserDataLength().
   */
  calculateUserDataLengthUCS2: function(aMessage) {
    let bodyChars = aMessage.length;
    let headerLen = 0;
    let headerChars = Math.ceil((headerLen ? headerLen + 1 : 0) / 2);
    let segmentChars = RIL.PDU_MAX_USER_DATA_UCS2;
    if ((bodyChars + headerChars) > segmentChars) {
      headerLen += this.segmentRef16Bit ? 6 : 5;
      headerChars = Math.ceil((headerLen + 1) / 2);
      segmentChars -= headerChars;
    }

    let segments = Math.ceil(bodyChars / segmentChars);

    return {
      dcs: RIL.PDU_DCS_MSG_CODING_16BITS_ALPHABET,
      encodedFullBodyLength: bodyChars * 2,
      userDataHeaderLength: headerLen,
      segmentMaxSeq: segments,
      segmentChars: segmentChars,
    };
  },

  /**
   * Calculate user data length and its encoding.
   *
   * @param aMessage
   *        a message string to be encoded.
   * @param aStrict7BitEncoding [Optional]
   *        Enable Latin characters replacement with corresponding
   *        ones in GSM SMS 7-bit default alphabet.
   *
   * @return an options object with some or all of following attributes set:
   *
   * @param dcs
   *        Data coding scheme. One of the PDU_DCS_MSG_CODING_*BITS_ALPHABET
   *        constants.
   * @param userDataHeaderLength
   *        Length of embedded user data header, in bytes. The whole header
   *        size will be userDataHeaderLength + 1; 0 for no header.
   * @param encodedFullBodyLength
   *        Length of the message body when encoded with the given DCS. For
   *        UCS2, in bytes; for 7-bit, in septets.
   * @param langIndex
   *        Table index used for normal 7-bit encoded character lookup.
   * @param langShiftIndex
   *        Table index used for escaped 7-bit encoded character lookup.
   * @param segmentMaxSeq
   *        Max sequence number of a multi-part messages, or 1 for single one.
   *        This number might not be accurate for a multi-part message until
   *        it's processed by #fragmentText() again.
   */
  calculateUserDataLength: function(aMessage, aStrict7BitEncoding) {
    let options = this.calculateUserDataLength7Bit(aMessage, aStrict7BitEncoding);
    if (!options) {
      options = this.calculateUserDataLengthUCS2(aMessage);
    }

    return options;
  },

  /**
   * Fragment GSM 7-Bit encodable string for transmission.
   *
   * @param aText
   *        text string to be fragmented.
   * @param aLangTable
   *        locking shift table string.
   * @param aLangShiftTable
   *        single shift table string.
   * @param aSegmentSeptets
   *        Number of available spetets per segment.
   * @param aStrict7BitEncoding [Optional]
   *        Enable Latin characters replacement with corresponding
   *        ones in GSM SMS 7-bit default alphabet.
   *
   * @return an array of objects. See #fragmentText() for detailed definition.
   */
  fragmentText7Bit: function(aText, aLangTable, aLangShiftTable, aSegmentSeptets, aStrict7BitEncoding) {
    let ret = [];
    let body = "", len = 0;
    // If the message is empty, we only push the empty message to ret.
    if (aText.length === 0) {
      ret.push({
        body: aText,
        encodedBodyLength: aText.length,
      });
      return ret;
    }

    for (let i = 0, inc = 0; i < aText.length; i++) {
      let c = aText.charAt(i);
      if (aStrict7BitEncoding) {
        c = RIL.GSM_SMS_STRICT_7BIT_CHARMAP[c] || c;
      }

      let septet = aLangTable.indexOf(c);
      if (septet == RIL.PDU_NL_EXTENDED_ESCAPE) {
        continue;
      }

      if (septet >= 0) {
        inc = 1;
      } else {
        septet = aLangShiftTable.indexOf(c);
        if (septet == RIL.PDU_NL_RESERVED_CONTROL) {
          continue;
        }

        inc = 2;
        if (septet < 0) {
          if (!aStrict7BitEncoding) {
            throw new Error("Given text cannot be encoded with GSM 7-bit Alphabet!");
          }

          // Bug 816082, when aStrict7BitEncoding is enabled, we should replace
          // characters that can't be encoded with GSM 7-Bit alphabets with '*'.
          c = "*";
          if (aLangTable.indexOf(c) >= 0) {
            inc = 1;
          }
        }
      }

      if ((len + inc) > aSegmentSeptets) {
        ret.push({
          body: body,
          encodedBodyLength: len,
        });
        body = c;
        len = inc;
      } else {
        body += c;
        len += inc;
      }
    }

    if (len) {
      ret.push({
        body: body,
        encodedBodyLength: len,
      });
    }

    return ret;
  },

  /**
   * Fragment UCS2 encodable string for transmission.
   *
   * @param aText
   *        text string to be fragmented.
   * @param aSegmentChars
   *        Number of available characters per segment.
   *
   * @return an array of objects. See #fragmentText() for detailed definition.
   */
  fragmentTextUCS2: function(aText, aSegmentChars) {
    let ret = [];
    // If the message is empty, we only push the empty message to ret.
    if (aText.length === 0) {
      ret.push({
        body: aText,
        encodedBodyLength: aText.length,
      });
      return ret;
    }

    for (let offset = 0; offset < aText.length; offset += aSegmentChars) {
      let str = aText.substr(offset, aSegmentChars);
      ret.push({
        body: str,
        encodedBodyLength: str.length * 2,
      });
    }

    return ret;
  },

  /**
   * Fragment string for transmission.
   *
   * Fragment input text string into an array of objects that contains
   * attributes `body`, substring for this segment, `encodedBodyLength`,
   * length of the encoded segment body in septets.
   *
   * @param aText
   *        Text string to be fragmented.
   * @param aOptions [Optional]
   *        Optional pre-calculated option object. The output array will be
   *        stored at aOptions.segments if there are multiple segments.
   * @param aStrict7BitEncoding [Optional]
   *        Enable Latin characters replacement with corresponding
   *        ones in GSM SMS 7-bit default alphabet.
   *
   * @return Populated options object.
   */
  fragmentText: function(aText, aOptions, aStrict7BitEncoding) {
    if (!aOptions) {
      aOptions = this.calculateUserDataLength(aText, aStrict7BitEncoding);
    }

    if (aOptions.dcs == RIL.PDU_DCS_MSG_CODING_7BITS_ALPHABET) {
      const langTable = RIL.PDU_NL_LOCKING_SHIFT_TABLES[aOptions.langIndex];
      const langShiftTable = RIL.PDU_NL_SINGLE_SHIFT_TABLES[aOptions.langShiftIndex];
      aOptions.segments = this.fragmentText7Bit(aText,
                                                langTable, langShiftTable,
                                                aOptions.segmentChars,
                                                aStrict7BitEncoding);
    } else {
      aOptions.segments = this.fragmentTextUCS2(aText,
                                                aOptions.segmentChars);
    }

    // Re-sync aOptions.segmentMaxSeq with actual length of returning array.
    aOptions.segmentMaxSeq = aOptions.segments.length;

    if (aOptions.segmentMaxSeq > 1) {
      aOptions.segmentRef16Bit = this.segmentRef16Bit;
      aOptions.segmentRef = this.nextSegmentRef;
    }

    return aOptions;
  }
};

this.EXPORTED_SYMBOLS = [ 'SmsSegmentHelper' ];
