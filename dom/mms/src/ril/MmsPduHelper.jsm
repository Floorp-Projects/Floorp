/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

let WSP = {};
Cu.import("resource://gre/modules/WspPduHelper.jsm", WSP);

Cu.import("resource://gre/modules/mms_consts.js");

let DEBUG; // set to true to see debug messages

function translatePduErrorToStatus(error) {
  switch (error) {
    case MMS_PDU_ERROR_OK:
      return MMS_PDU_STATUS_RETRIEVED;
    case MMS_PDU_ERROR_TRANSIENT_FAILURE:
    case MMS_PDU_ERROR_TRANSIENT_MESSAGE_NOT_FOUND:
    case MMS_PDU_ERROR_TRANSIENT_NETWORK_PROBLEM:
      return MMS_PDU_STATUS_DEFERRED;
    default:
      return MMS_PDU_STATUS_UNRECOGNISED;
  }
}

/**
 * Internal decoding function for boolean values.
 *
 * Boolean-value = Yes | No
 * Yes = <Octet 128>
 * No = <Octet 129>
 */
let BooleanValue = {
  /**
   * @param data
   *        A wrapped object containing raw PDU data.
   *
   * @return Boolean true or false.
   *
   * @throws CodeError if read octet equals to neither 128 nor 129.
   */
  decode: function decode(data) {
    let value = WSP.Octet.decode(data);
    if ((value != 128) && (value != 129)) {
      throw new WSP.CodeError("Boolean-value: invalid value " + value);
    }

    return value == 128;
  },

  /**
   * @param data
   *        A wrapped object to store encoded raw data.
   * @param value
   *        A boolean value to be encoded.
   */
  encode: function encode(data, value) {
    WSP.Octet.encode(data, value ? 128 : 129);
  },
};

/**
 * MMS Address
 *
 * address = email | device-address | alphanum-shortcode | num-shortcode
 *
 * @see OMA-TS-MMS_ENC-V1_3-20110913-A section 8
 */
let Address = {
  decode: function (data) {
    let str = EncodedStringValue.decode(data);

    let result;
    if (((result = str.match(/^(\+?[\d.-]+)\/TYPE=(PLMN)$/)) != null)
        || ((result = str.match(/^(\d{1,3}(?:\.\d{1,3}){3})\/TYPE=(IPv4)$/)) != null)
        || ((result = str.match(/^([\da-fA-F]{4}(?::[\da-fA-F]{4}){7})\/TYPE=(IPv6)$/)) != null)
        || ((result = str.match(/^([\w\+\-.%]+)\/TYPE=(\w+)$/)) != null)) {
      return {address: result[1], type: result[2]};
    }

    let type;
    if (str.match(/^[\+*#]\d+$/)) {
      type = "num";
    } else if (str.match(/^\w+$/)) {
      type = "alphanum";
    } else {
      type = "unknown";
    }

    return {address: str, type: type};
  },
};

/**
 * Header-field = MMS-header | Application-header
 *
 * @see OMA-TS-MMS_ENC-V1_3-20110913-A clause 7.2
 */
let HeaderField = {
  /**
   * @param data
   *        A wrapped object containing raw PDU data.
   * @param options
   *        Extra context for decoding.
   *
   * @return A decoded object containing `name` and `value` properties or null
   *         in case of a failed parsing. The `name` property must be a string,
   *         but the `value` property can be many different types depending on
   *         `name`.
   */
  decode: function decode(data, options) {
    return WSP.decodeAlternatives(data, options,
                                  MmsHeader, WSP.ApplicationHeader);
  },

  /**
   * @param data
   *        A wrapped object to store encoded raw data.
   * @param octet
   *        Octet value to be encoded.
   * @param options
   *        Extra context for encoding.
   */
  encode: function encode(data, value, options) {
    WSP.encodeAlternatives(data, value, options,
                           MmsHeader, WSP.ApplicationHeader);
  },
};

/**
 * MMS-header = MMS-field-name MMS-value
 * MMS-field-name = Short-integer
 *
 * @see OMA-TS-MMS_ENC-V1_3-20110913-A clause 7.2
 */
let MmsHeader = {
  /**
   * @param data
   *        A wrapped object containing raw PDU data.
   * @param options
   *        Extra context for decoding.
   *
   * @return A decoded object containing `name` and `value` properties or null
   *         in case of a failed parsing. The `name` property must be a string,
   *         but the `value` property can be many different types depending on
   *         `name`.
   *
   * @throws NotWellKnownEncodingError if decoded well-known header field
   *         number is not registered or supported.
   */
  decode: function decode(data, options) {
    let index = WSP.ShortInteger.decode(data);

    let entry = MMS_HEADER_FIELDS[index];
    if (!entry) {
      throw new WSP.NotWellKnownEncodingError(
        "MMS-header: not well known header " + index);
    }

    let cur = data.offset, value;
    try {
      value = entry.coder.decode(data, options);
    } catch (e) {
      data.offset = cur;

      value = WSP.skipValue(data);
      debug("Skip malformed well known header: "
            + JSON.stringify({name: entry.name, value: value}));

      return null;
    }

    return {
      name: entry.name,
      value: value,
    };
  },

  /**
   * @param data
   *        A wrapped object to store encoded raw data.
   * @param header
   *        An object containing two attributes: a string-typed `name` and a
   *        `value` of arbitrary type.
   *
   * @throws CodeError if got an empty header name.
   * @throws NotWellKnownEncodingError if the well-known header field number is
   *         not registered or supported.
   */
  encode: function encode(data, header) {
    if (!header.name) {
      throw new WSP.CodeError("MMS-header: empty header name");
    }

    let entry = MMS_HEADER_FIELDS[header.name.toLowerCase()];
    if (!entry) {
      throw new WSP.NotWellKnownEncodingError(
        "MMS-header: not well known header " + header.name);
    }

    WSP.ShortInteger.encode(data, entry.number);
    entry.coder.encode(data, header.value);
  },
};

/**
 * Content-class-value = text | image-basic| image-rich | video-basic |
 *                       video-rich | megapixel | content-basic | content-rich
 *
 * @see OMA-TS-MMS_ENC-V1_3-20110913-A clause 7.3.9
 */
let ContentClassValue = {
  /**
   * @param data
   *        A wrapped object containing raw PDU data.
   *
   * @return A integer value for each class.
   *
   * @throws CodeError if decoded value is not in range 128..135.
   */
  decode: function decode(data) {
    let value = WSP.Octet.decode(data);
    if ((value >= 128) && (value <= 135)) {
      return value;
    }

    throw new WSP.CodeError("Content-class-value: invalid class " + value);
  },
};

/**
 * When used in a PDU other than M-Mbox-Delete.conf and M-Delete.conf:
 *
 *   Content-location-value = Uri-value
 *
 * When used in the M-Mbox-Delete.conf and M-Delete.conf PDU:
 *
 *   Content-location-Del-value = Value-length Status-count-value Content-location-value
 *   Status-count-value = Integer-value
 *
 * @see OMA-TS-MMS_ENC-V1_3-20110913-A clause 7.3.10
 */
let ContentLocationValue = {
  /**
   * @param data
   *        A wrapped object containing raw PDU data.
   * @param options
   *        Extra context for decoding.
   *
   * @return A decoded object containing `uri` and conditional `statusCount`
   *         properties.
   */
  decode: function decode(data, options) {
    let type = WSP.ensureHeader(options, "x-mms-message-type");

    let result = {};
    if ((type == MMS_PDU_TYPE_MBOX_DELETE_CONF)
        || (type == MMS_PDU_TYPE_DELETE_CONF)) {
      let length = WSP.ValueLength.decode(data);
      let end = data.offset + length;

      result.statusCount = WSP.IntegerValue.decode(data);
      result.uri = WSP.UriValue.decode(data);

      if (data.offset != end) {
        data.offset = end;
      }
    } else {
      result.uri = WSP.UriValue.decode(data);
    }

    return result;
  },
};

/**
 * Element-Descriptor-value = Value-length Content-Reference-value *(Parameter)
 * Content-Reference-value = Text-string
 *
 * @see OMA-TS-MMS_ENC-V1_3-20110913-A clause 7.3.18
 */
let ElementDescriptorValue = {
  /**
   * @param data
   *        A wrapped object containing raw PDU data.
   *
   * @return A decoded object containing a string property `contentReference`
   *         and an optinal `params` name-value map.
   */
  decode: function decode(data) {
    let length = WSP.ValueLength.decode(data);
    let end = data.offset + length;

    let result = {};
    result.contentReference = WSP.TextString.decode(data);
    if (data.offset < end) {
      result.params = Parameter.decodeMultiple(data, end);
    }

    if (data.offset != end) {
      // Explicitly seek to end in case of skipped parameters.
      data.offset = end;
    }

    return result;
  },
};

/**
 * OMA-TS-MMS_ENC-V1_3-20110913-A clause 7.3.18:
 * `For well-known parameter names binary tokens MUST be used as defined in
 * Table 27.` So we can't reuse that of WSP.
 *
 *   Parameter = Parameter-name Parameter-value
 *   Parameter-name = Short-integer | Text-string
 *   Parameter-value = Constrained-encoding | Text-string
 *
 * @see OMA-TS-MMS_ENC-V1_3-20110913-A clause 7.3.18
 */
let Parameter = {
  /**
   * @param data
   *        A wrapped object containing raw PDU data.
   *
   * @return A decoded string.
   *
   * @throws NotWellKnownEncodingError if decoded well-known parameter number
   *         is not registered or supported.
   */
  decodeParameterName: function decodeParameterName(data) {
    let begin = data.offset;
    let number;
    try {
      number = WSP.ShortInteger.decode(data);
    } catch (e) {
      data.offset = begin;
      return WSP.TextString.decode(data).toLowerCase();
    }

    let entry = MMS_WELL_KNOWN_PARAMS[number];
    if (!entry) {
      throw new WSP.NotWellKnownEncodingError(
        "Parameter-name: not well known parameter " + number);
    }

    return entry.name;
  },

  /**
   * @param data
   *        A wrapped object containing raw PDU data.
   *
   * @return A decoded object containing `name` and `value` properties or null
   *         in case of a failed parsing. The `name` property must be a string,
   *         but the `value` property can be many different types depending on
   *         `name`.
   */
  decode: function decode(data) {
    let name = this.decodeParameterName(data);
    let value = WSP.decodeAlternatives(data, null,
                                       WSP.ConstrainedEncoding, WSP.TextString);
    return {
      name: name,
      value: value,
    };
  },

  /**
   * @param data
   *        A wrapped object containing raw PDU data.
   * @param end
   *        Ending offset of following parameters.
   *
   * @return An array of decoded objects.
   */
  decodeMultiple: function decodeMultiple(data, end) {
    let params, param;

    while (data.offset < end) {
      try {
        param = this.decode(data);
      } catch (e) {
        break;
      }
      if (param) {
        if (!params) {
          params = {};
        }
        params[param.name] = param.value;
      }
    }

    return params;
  },
};

/**
 * Encoded-string-value = Text-string | Value-length Char-set Text-string
 * The Char-set values are registered by IANA as MIBEnum value.
 *
 * @see OMA-TS-MMS_ENC-V1_3-20110913-A clause 7.3.19
 */
let EncodedStringValue = {
  /**
   * @param data
   *        A wrapped object containing raw PDU data.
   *
   * @return Decoded string.
   */
  decode: function decode(data) {
    return WSP.decodeAlternatives(data, null,
                                  WSP.TextString, CharsetEncodedString);
  },
};

/**
 * Charset-encoded-string = Value-length Char-set Text-string
 *
 * @see OMA-TS-MMS_ENC-V1_3-20110913-A clause 7.3.19
 */
let CharsetEncodedString = {
  /**
   * @param data
   *        A wrapped object containing raw PDU data.
   *
   * @return Decoded string.
   *
   * @throws CodeError if the raw octets cannot be converted.
   * @throws NotWellKnownEncodingError if decoded well-known charset number is
   *         not registered or supported.
   */
  decode: function decode(data) {
    let length = WSP.ValueLength.decode(data);
    let end = data.offset + length;

    let charset = WSP.ShortInteger.decode(data);
    let entry = WSP.WSP_WELL_KNOWN_CHARSETS[charset];
    if (!entry) {
      throw new WSP.NotWellKnownEncodingError(
        "Charset-encoded-string: not well known charset " + charset);
    }

    let str;
    if (entry.converter) {
      // Read a possible string quote(<Octet 127>).
      let begin = data.offset;
      if (WSP.Octet.decode(data) != 127) {
        data.offset = begin;
      }

      let raw = WSP.Octet.decodeMultiple(data, end - 1);
      // Read NUL character.
      WSP.Octet.decodeEqualTo(data, 0);

      if (!raw) {
        str = "";
      } else {
        let conv = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                   .createInstance(Ci.nsIScriptableUnicodeConverter);
        conv.charset = entry.converter;
        try {
          str = conv.convertFromByteArray(raw, raw.length);
        } catch (e) {
          throw new WSP.CodeError("Charset-encoded-string: " + e.message);
        }
      }
    } else {
      str = WSP.TextString.decode(data);
    }

    if (data.offset != end) {
      data.offset = end;
    }

    return str;
  },
};

/**
 * Expiry-value = Value-length (Absolute-token Date-value | Relative-token Delta-seconds-value)
 * Address-token = <Octet 128>
 * Relative-token = <Octet 129>
 *
 * @see OMA-TS-MMS_ENC-V1_3-20110913-A clause 7.3.20
 */
let ExpiryValue = {
  /**
   * @param data
   *        A wrapped object containing raw PDU data.
   *
   * @return A Date object for absolute expiry or an integer for relative one.
   *
   * @throws CodeError if decoded token equals to neither 128 nor 129.
   */
  decode: function decode(data) {
    let length = WSP.ValueLength.decode(data);
    let end = data.offset + length;

    let token = WSP.Octet.decode(data);
    if ((token != 128) && (token != 129)) {
      throw new WSP.CodeError("Expiry-value: invalid token " + token);
    }

    let result;
    if (token == 128) {
      result = WSP.DateValue.decode(data);
    } else {
      result = WSP.DeltaSecondsValue.decode(data);
    }

    if (data.offset != end) {
      data.offset = end;
    }

    return result;
  },
};

/**
 * From-value = Value-length (Address-present-token Address | Insert-address-token)
 * Address-present-token = <Octet 128>
 * Insert-address-token = <Octet 129>
 *
 * @see OMA-TS-MMS_ENC-V1_3-20110913-A clause 7.3.21
 */
let FromValue = {
  /**
   * @param data
   *        A wrapped object containing raw PDU data.
   *
   * @return Decoded string or null for MMS Proxy-Relay Insert-Address mode.
   *
   * @throws CodeError if decoded token equals to neither 128 nor 129.
   */
  decode: function decode(data) {
    let length = WSP.ValueLength.decode(data);
    let end = data.offset + length;

    let token = WSP.Octet.decode(data);
    if ((token != 128) && (token != 129)) {
      throw new WSP.CodeError("From-value: invalid token " + token);
    }

    let result = null;
    if (token == 128) {
      result = Address.decode(data);
    }

    if (data.offset != end) {
      data.offset = end;
    }

    return result;
  },
};

/**
 * Previously-sent-by-value = Value-length Forwarded-count-value Address
 * Forwarded-count-value = Integer-value
 *
 * @see OMA-TS-MMS_ENC-V1_3-20110913-A clause 7.3.23
 */
let PreviouslySentByValue = {
  /**
   * @param data
   *        A wrapped object containing raw PDU data.
   *
   * @return Decoded object containing an integer `forwardedCount` and an
   *         string-typed `originator` attributes.
   */
  decode: function decode(data) {
    let length = WSP.ValueLength.decode(data);
    let end = data.offset + length;

    let result = {};
    result.forwardedCount = WSP.IntegerValue.decode(data);
    result.originator = Address.decode(data);

    if (data.offset != end) {
      data.offset = end;
    }

    return result;
  },
};

/**
 * Previously-sent-date-value = Value-length Forwarded-count-value Date-value
 *
 * @see OMA-TS-MMS_ENC-V1_3-20110913-A clause 7.3.23
 * @see OMA-TS-MMS_ENC-V1_3-20110913-A clause 7.3.24
 */
let PreviouslySentDateValue = {
  /**
   * @param data
   *        A wrapped object containing raw PDU data.
   *
   * @return Decoded object containing an integer `forwardedCount` and an
   *         Date-typed `timestamp` attributes.
   */
  decode: function decode(data) {
    let length = WSP.ValueLength.decode(data);
    let end = data.offset + length;

    let result = {};
    result.forwardedCount = WSP.IntegerValue.decode(data);
    result.timestamp = WSP.DateValue.decode(data);

    if (data.offset != end) {
      data.offset = end;
    }

    return result;
  },
};

/**
 * Message-class-value = Class-identifier | Token-text
 *
 * @see OMA-TS-MMS_ENC-V1_3-20110913-A clause 7.3.27
 */
let MessageClassValue = {
  /**
   * @param data
   *        A wrapped object containing raw PDU data.
   *
   * @return A decoded string.
   */
  decode: function decode(data) {
    return WSP.decodeAlternatives(data, null,
                                  ClassIdentifier, WSP.TokenText);
  },
};

/**
 * Class-identifier = Personal | Advertisement | Informational | Auto
 * Personal = <Octet 128>
 * Advertisement = <Octet 129>
 * Informational = <Octet 130>
 * Auto = <Octet 131>
 *
 * @see OMA-TS-MMS_ENC-V1_3-20110913-A clause 7.3.27
 */
let ClassIdentifier = {
  /**
   * @param data
   *        A wrapped object containing raw PDU data.
   *
   * @return A decoded string.
   *
   * @throws CodeError if decoded value is not in the range 128..131.
   */
  decode: function decode(data) {
    let value = WSP.Octet.decode(data);
    switch (value) {
      case 128: return "personal";
      case 129: return "advertisement";
      case 130: return "informational";
      case 131: return "auto";
    }

    throw new WSP.CodeError("Class-identifier: invalid id " + value);
  },
};

 /**
 * Message-type-value = <Octet 128..151>
 *
 * @see OMA-TS-MMS_ENC-V1_3-20110913-A clause 7.3.30
 */
let MessageTypeValue = {
  /**
   * @param data
   *        A wrapped object containing raw PDU data.
   *
   * @return A decoded integer.
   *
   * @throws CodeError if decoded value is not in the range 128..151.
   */
  decode: function decode(data) {
    let type = WSP.Octet.decode(data);
    if ((type >= 128) && (type <= 151)) {
      return type;
    }

    throw new WSP.CodeError("Message-type-value: invalid type " + type);
  },

  /**
   * @param data
   *        A wrapped object to store encoded raw data.
   * @param type
   *        A numeric message type value to be encoded.
   *
   * @throws CodeError if the value is not in the range 128..151.
   */
  encode: function encode(data, type) {
    if ((type < 128) || (type > 151)) {
      throw new WSP.CodeError("Message-type-value: invalid type " + type);
    }

    WSP.Octet.encode(data, type);
  },
};

/**
 * MM-flags-value = Value-length ( Add-token | Remove-token | Filter-token ) Encoded-string-value
 * Add-token = <Octet 128>
 * Remove-token = <Octet 129>
 * Filter-token = <Octet 130>
 *
 * @see OMA-TS-MMS_ENC-V1_3-20110913-A clause 7.3.32
 */
let MmFlagsValue = {
  /**
   * @param data
   *        A wrapped object containing raw PDU data.
   *
   * @return Decoded object containing an integer `type` and an string-typed
   *         `address` attributes.
   *
   * @throws CodeError if decoded value is not in the range 128..130.
   */
  decode: function decode(data) {
    let length = WSP.ValueLength.decode(data);
    let end = data.offset + length;

    let result = {};
    result.type = WSP.Octet.decode(data);
    if ((result.type < 128) || (result.type > 130)) {
      throw new WSP.CodeError("MM-flags-value: invalid type " + result.type);
    }
    result.text = EncodedStringValue.decode(data);

    if (data.offset != end) {
      data.offset = end;
    }

    return result;
  },
};

/**
 * MM-state-value = Draft | Sent | New | Retrieved | Forwarded
 * Draft = <Octet 128>
 * Sent = <Octet 129>
 * New = <Octet 130>
 * Retrieved = <Octet 131>
 * Forwarded = <Octet 132>
 *
 * @see OMA-TS-MMS_ENC-V1_3-20110913-A clause 7.3.33
 */
let MmStateValue = {
  /**
   * @param data
   *        A wrapped object containing raw PDU data.
   *
   * @return A decoded integer.
   *
   * @throws CodeError if decoded value is not in the range 128..132.
   */
  decode: function decode(data) {
    let state = WSP.Octet.decode(data);
    if ((state >= 128) && (state <= 132)) {
      return state;
    }

    throw new WSP.CodeError("MM-state-value: invalid state " + state);
  },
};

/**
 * Priority-value = Low | Normal | High
 * Low = <Octet 128>
 * Normal = <Octet 129>
 * High = <Octet 130>
 *
 * @see OMA-TS-MMS_ENC-V1_3-20110913-A clause 7.3.35
 */
let PriorityValue = {
  /**
   * @param data
   *        A wrapped object containing raw PDU data.
   *
   * @return A decoded integer.
   *
   * @throws CodeError if decoded value is not in the range 128..130.
   */
  decode: function decode(data) {
    let priority = WSP.Octet.decode(data);
    if ((priority >= 128) && (priority <= 130)) {
      return priority;
    }

    throw new WSP.CodeError("Priority-value: invalid priority " + priority);
  },
};

/**
 * Recommended-Retrieval-Mode-value = Manual
 * Manual = <Octet 128>
 *
 * @see OMA-TS-MMS_ENC-V1_3-20110913-A clause 7.3.39
 */
let RecommendedRetrievalModeValue = {
  /**
   * @param data
   *        A wrapped object containing raw PDU data.
   *
   * @return A decoded integer.
   */
  decode: function decode(data) {
    return WSP.Octet.decodeEqualTo(data, 128);
  },
};

/**
 * Reply-charging-value = Requested | Requested text only | Accepted |
 *                        Accepted text only
 * Requested = <Octet 128>
 * Requested text only = <Octet 129>
 * Accepted = <Octet 130>
 * Accepted text only = <Octet 131>
 *
 * @see OMA-TS-MMS_ENC-V1_3-20110913-A clause 7.3.43
 */
let ReplyChargingValue = {
  /**
   * @param data
   *        A wrapped object containing raw PDU data.
   *
   * @return A decoded integer.
   *
   * @throws CodeError if decoded value is not in the range 128..131.
   */
  decode: function decode(data) {
    let value = WSP.Octet.decode(data);
    if ((value >= 128) && (value <= 131)) {
      return value;
    }

    throw new WSP.CodeError("Reply-charging-value: invalid value " + value);
  },
};

/**
 * Retrieve-status-value = Ok | Error-transient-failure |
 *                         Error-transient-message-not-found |
 *                         Error-transient-network-problem |
 *                         Error-permanent-failure |
 *                         Error-permanent-service-denied |
 *                         Error-permanent-message-not-found |
 *                         Error-permanent-content-unsupported
 * Ok = <Octet 128>
 * Error-transient-failure = <Octet 192>
 * Error-transient-message-not-found = <Octet 193>
 * Error-transient-network-problem = <Octet 194>
 * Error-permanent-failure = <Octet 224>
 * Error-permanent-service-denied = <Octet 225>
 * Error-permanent-message-not-found = <Octet 226>
 * Error-permanent-content-unsupported = <Octet 227>
 *
 * @see OMA-TS-MMS_ENC-V1_3-20110913-A clause 7.3.50
 */
let RetrieveStatusValue = {
  /**
   * @param data
   *        A wrapped object containing raw PDU data.
   *
   * @return A decoded integer.
   */
  decode: function decode(data) {
    let value = WSP.Octet.decode(data);
    if ((value == 128)
        || ((value >= 192) && (value <= 194))
        || ((value >= 224) && (value <= 227))) {
      return value;
    }

    if ((value >= 195) && (value <= 223)) {
      // The values 195 through 223 are reserved for future use to indicate
      // other transient failures. An MMS Client MUST react the same to a value
      // in range 195 to 223 as it does to the value 192
      // (Error-transient-failure).
      return MMS_PDU_ERROR_TRANSIENT_FAILURE;
    }

    // The values 228 through 255 are reserved for future use to indicate
    // other permanent failures. An MMS Client MUST react the same to a value
    // in range 228 to 255 as it does to the value 224
    // (Error-permanent-failure).

    // Any other values SHALL NOT be used. They are reserved for future use.
    // An MMS Client that receives such a reserved value MUST react the same
    // as it does to the value 224 (Error-permanent-failure).
    return MMS_PDU_ERROR_PERMANENT_FAILURE;
  },
};

/**
 * Status-value = Expired | Retrieved | Rejected | Deferred | Unrecognised |
 *                Indeterminate | Forwarded | Unreachable
 * Expired = <Octet 128>
 * Retrieved = <Octet 129>
 * Rejected = <Octet 130>
 * Deferred = <Octet 131>
 * Unrecognised = <Octet 132>
 * Indeterminate = <Octet 133>
 * Forwarded = <Octet 134>
 * Unreachable = <Octet 135>
 *
 * @see OMA-TS-MMS_ENC-V1_3-20110913-A clause 7.3.54
 */
let StatusValue = {
  /**
   * @param data
   *        A wrapped object containing raw PDU data.
   *
   * @return A decoded integer.
   *
   * @throws CodeError if decoded value is not in the range 128..135.
   */
  decode: function decode(data) {
    let status = WSP.Octet.decode(data);
    if ((status >= 128) && (status <= 135)) {
      return status;
    }

    throw new WSP.CodeError("Status-value: invalid status " + status);
  },

  /**
   * @param data
   *        A wrapped object to store encoded raw data.
   * @param value
   *        A numeric status value to be encoded.
   *
   * @throws CodeError if the value is not in the range 128..135.
   */
  encode: function encode(data, value) {
    if ((value < 128) || (value > 135)) {
      throw new WSP.CodeError("Status-value: invalid status " + value);
    }

    WSP.Octet.encode(data, value);
  },
};

let PduHelper = {
  /**
   * @param data
   *        A wrapped object containing raw PDU data.
   * @param headers
   *        An optional object to store parsed header fields. Created
   *        automatically if undefined.
   *
   * @return A boolean value indicating whether it's followed by message body.
   */
  parseHeaders: function parseHeaders(data, headers) {
    if (!headers) {
      headers = {};
    }

    let header;
    while (data.offset < data.array.length) {
      // There is no `header length` information in MMS PDU. If we just got
      // something wrong in parsing header fields, we might not be able to
      // determine the correct header-content boundary.
      header = HeaderField.decode(data, headers);

      if (header) {
        let orig = headers[header.name];
        if (Array.isArray(orig)) {
          headers[header.name].push(header.value);
        } else if (orig) {
          headers[header.name] = [orig, header.value];
        } else {
          headers[header.name] = header.value;
        }
        if (header.name == "content-type") {
          // `... if the PDU contains a message body the Content Type MUST be
          // the last header field, followed by message body.` See
          // OMA-TS-MMS_ENC-V1_3-20110913-A section 7.
          break;
        }
      }
    }

    return headers;
  },

  /**
   * @param data
   *        A wrapped object containing raw PDU data.
   * @param msg
   *        A message object to store decoded multipart or octet array content.
   */
  parseContent: function parseContent(data, msg) {
    let contentType = msg.headers["content-type"].media;
    if ((contentType == "application/vnd.wap.multipart.related")
        || (contentType == "application/vnd.wap.multipart.mixed")) {
      msg.parts = WSP.PduHelper.parseMultiPart(data);
      return;
    }

    if (data.offset >= data.array.length) {
      return;
    }

    msg.content = WSP.Octet.decodeMultiple(data, data.array.length);
    if (false) {
      for (let begin = 0; begin < msg.content.length; begin += 20) {
        debug("content: " + JSON.stringify(msg.content.subarray(begin, begin + 20)));
      }
    }
  },

  /**
   * Check existences of all mandatory fields of a MMS message. Also sets `type`
   * and `typeinfo` for convient access.
   *
   * @param msg
   *        A MMS message object.
   *
   * @throws FatalCodeError if the PDU type is not supported yet.
   */
  checkMandatoryFields: function checkMandatoryFields(msg) {
    let type = WSP.ensureHeader(msg.headers, "x-mms-message-type");
    let entry = MMS_PDU_TYPES[type];
    if (!entry) {
      throw new WSP.FatalCodeError(
        "checkMandatoryFields: unsupported message type " + type);
    }

    entry.mandatoryFields.forEach(function (name) {
      WSP.ensureHeader(msg.headers, name);
    });

    // Setup convient alias that referenced frequently.
    msg.type = type;
    msg.typeinfo = entry;
  },

  /**
   * @param data
   *        A wrapped object containing raw PDU data.
   * @param msg [optional]
   *        Optional target object for decoding.
   *
   * @return A MMS message object or null in case of errors found.
   */
  parse: function parse(data, msg) {
    if (!msg) {
      msg = {};
    }

    try {
      msg.headers = this.parseHeaders(data, msg.headers);

      // Validity checks
      this.checkMandatoryFields(msg);

      if (msg.typeinfo.hasContent) {
        this.parseContent(data, msg);
      }
    } catch (e) {
      debug("Failed to parse MMS message, error message: " + e.message);
      return null;
    }

    return msg;
  },

  /**
   * Convert javascript Array to an nsIInputStream.
   */
  convertArrayToInputStream: function convertDataToInputStream(array) {
    let storageStream = Cc["@mozilla.org/storagestream;1"]
                        .createInstance(Ci.nsIStorageStream);
    storageStream.init(4096, array.length, null);

    let boStream = Cc["@mozilla.org/binaryoutputstream;1"]
                   .createInstance(Ci.nsIBinaryOutputStream);
    boStream.setOutputStream(storageStream.getOutputStream(0));
    boStream.writeByteArray(array, array.length)
    boStream.close();

    return storageStream.newInputStream(0);
  },

  /**
   * @param data [optional]
   *        A wrapped object to store encoded raw data. Created if undefined.
   * @param headers
   *        A dictionary object containing multiple name/value mapping.
   *
   * @return the passed data parameter or a created one.
   */
  encodeHeaders: function encodeHeaders(data, headers) {
    if (!data) {
      data = {array: [], offset: 0};
    }

    function encodeHeader(name) {
      HeaderField.encode(data, {name: name, value: headers[name]});
    }

    function encodeHeaderIfExists(name) {
      // Header value could be zero or null.
      if (headers[name] !== undefined) {
        encodeHeader(name);
      }
    }

    // `In the encoding of the header fields, the order of the fields is not
    // significant, except that X-Mms-Message-Type, X-Mms-Transaction-ID (when
    // present) and X-Mms-MMS-Version MUST be at the beginning of the message
    // headers, in that order, and if the PDU contains a message body the
    // Content Type MUST be the last header field, followed by message body.`
    // ~ OMA-TS-MMS_ENC-V1_3-20110913-A section 7
    encodeHeader("x-mms-message-type");
    encodeHeaderIfExists("x-mms-transaction-id");
    encodeHeaderIfExists("x-mms-mms-version");

    for (let key in headers) {
      if ((key == "x-mms-message-type")
          || (key == "x-mms-transaction-id")
          || (key == "x-mms-mms-version")
          || (key == "content-type")) {
        continue;
      }
      encodeHeader(key);
    }

    encodeHeaderIfExists("content-type");

    // Remove extra space consumed during encoding.
    while (data.array.length > data.offset) {
      data.array.pop();
    }

    return data;
  },

  /**
   * @param multiStream
   *        An exsiting nsIMultiplexInputStream.
   * @param msg
   *        A MMS message object.
   *
   * @return An instance of nsIMultiplexInputStream or null in case of errors.
   */
  compose: function compose(multiStream, msg) {
    if (!multiStream) {
      multiStream = Cc["@mozilla.org/io/multiplex-input-stream;1"]
                    .createInstance(Ci.nsIMultiplexInputStream);
    }

    try {
      // Validity checks
      this.checkMandatoryFields(msg);

      let data = this.encodeHeaders(null, msg.headers);
      debug("Composed PDU Header: " + JSON.stringify(data.array));
      let headerStream = this.convertArrayToInputStream(data.array);
      multiStream.appendStream(headerStream);

      return multiStream;
    } catch (e) {
      debug("Failed to compose MMS message, error message: " + e.message);
      return null;
    }
  },
};

const MMS_PDU_TYPES = (function () {
  let pdus = {};
  function add(number, hasContent, mandatoryFields) {
    pdus[number] = {
      number: number,
      hasContent: hasContent,
      mandatoryFields: mandatoryFields,
    };
  }

  add(MMS_PDU_TYPE_NOTIFICATION_IND, false, ["x-mms-message-type",
                                             "x-mms-transaction-id",
                                             "x-mms-mms-version",
                                             "x-mms-message-class",
                                             "x-mms-message-size",
                                             "x-mms-expiry",
                                             "x-mms-content-location"]);
  add(MMS_PDU_TYPE_RETRIEVE_CONF, true, ["x-mms-message-type",
                                         "x-mms-mms-version",
                                         "content-type"]);
  add(MMS_PDU_TYPE_NOTIFYRESP_IND, false, ["x-mms-message-type",
                                           "x-mms-transaction-id",
                                           "x-mms-mms-version",
                                           "x-mms-status"]);

  return pdus;
})();

/**
 * Header field names and assigned numbers.
 *
 * @see OMA-TS-MMS_ENC-V1_3-20110913-A clause 7.3.4
 */
const MMS_HEADER_FIELDS = (function () {
  let names = {};
  function add(name, number, coder) {
    let entry = {
      name: name,
      number: number,
      coder: coder,
    };
    names[name] = names[number] = entry;
  }

  add("bcc",                                     0x01, Address);
  add("cc",                                      0x02, Address);
  add("x-mms-content-location",                  0x03, ContentLocationValue);
  add("content-type",                            0x04, WSP.ContentTypeValue);
  add("date",                                    0x05, WSP.DateValue);
  add("x-mms-delivery-report",                   0x06, BooleanValue);
  //add("x-mms-delivery-time", 0x07);
  add("x-mms-expiry",                            0x08, ExpiryValue);
  add("from",                                    0x09, FromValue);
  add("x-mms-message-class",                     0x0A, MessageClassValue);
  add("message-id",                              0x0B, WSP.TextString);
  add("x-mms-message-type",                      0x0C, MessageTypeValue);
  add("x-mms-mms-version",                       0x0D, WSP.ShortInteger);
  add("x-mms-message-size",                      0x0E, WSP.LongInteger);
  add("x-mms-priority",                          0x0F, PriorityValue);
  add("x-mms-read-report",                       0x10, BooleanValue);
  add("x-mms-report-allowed",                    0x11, BooleanValue);
  //add("x-mms-response-status", 0x12);
  //add("x-mms-response-text", 0x13);
  //add("x-mms-sender-visibility", 0x14);
  add("x-mms-status",                            0x15, StatusValue);
  add("subject",                                 0x16, EncodedStringValue);
  add("to",                                      0x17, Address);
  add("x-mms-transaction-id",                    0x18, WSP.TextString);
  add("x-mms-retrieve-status",                   0x19, RetrieveStatusValue);
  add("x-mms-retrieve-text",                     0x1A, EncodedStringValue);
  //add("x-mms-read-status", 0x1B);
  add("x-mms-reply-charging",                    0x1C, WSP.ReplyChargingValue);
  add("x-mms-reply-charging-deadline",           0x1D, ExpiryValue);
  add("x-mms-reply-charging-id",                 0x1E, WSP.TextString);
  add("x-mms-reply-charging-size",               0x1F, WSP.LongInteger);
  add("x-mms-previously-sent-by",                0x20, PreviouslySentByValue);
  add("x-mms-previously-sent-date",              0x21, PreviouslySentDateValue);
  add("x-mms-store",                             0x22, BooleanValue);
  add("x-mms-mm-state",                          0x23, MmStateValue);
  add("x-mms-mm-flags",                          0x24, MmFlagsValue);
  //add("x-mms-store-status", 0x25);
  //add("x-mms-store-status-text", 0x26);
  add("x-mms-stored",                            0x27, BooleanValue);
  //add("x-mms-attributes", 0x28);
  add("x-mms-totals",                            0x29, BooleanValue);
  //add("x-mms-mbox-totals", 0x2A);
  add("x-mms-quotas",                            0x2B, BooleanValue);
  //add("x-mms-mbox-quotas", 0x2C);
  add("x-mms-message-count",                     0x2D, WSP.IntegerValue);
  //add("content", 0x2E);
  add("x-mms-start",                             0x2F, WSP.IntegerValue);
  //add("additional-headers", 0x30);
  add("x-mms-distribution-indicator",            0x31, BooleanValue);
  add("x-mms-element-descriptor",                0x32, ElementDescriptorValue);
  add("x-mms-limit",                             0x33, WSP.IntegerValue);
  add("x-mms-recommended-retrieval-mode",        0x34, RecommendedRetrievalModeValue);
  add("x-mms-recommended-retrieval-mode-text",   0x35, EncodedStringValue);
  //add("x-mms-status-text", 0x36);
  add("x-mms-applic-id",                         0x37, WSP.TextString);
  add("x-mms-reply-applic-id",                   0x38, WSP.TextString);
  add("x-mms-aux-applic-id",                     0x39, WSP.TextString);
  add("x-mms-content-class",                     0x3A, ContentClassValue);
  add("x-mms-drm-content",                       0x3B, BooleanValue);
  add("x-mms-adaptation-allowed",                0x3C, BooleanValue);
  add("x-mms-replace-id",                        0x3D, WSP.TextString);
  add("x-mms-cancel-id",                         0x3E, WSP.TextString);
  add("x-mms-cancel-status",                     0x3F, BooleanValue);

  return names;
})();

// @see OMA-TS-MMS_ENC-V1_3-20110913-A Table 27: Parameter Name Assignments
const MMS_WELL_KNOWN_PARAMS = (function () {
  let params = {};

  function add(name, number, coder) {
    let entry = {
      name: name,
      number: number,
      coder: coder,
    };
    params[name] = params[number] = entry;
  }

  add("type", 0x02, WSP.ConstrainedEncoding);

  return params;
})();

let debug;
if (DEBUG) {
  debug = function (s) {
    dump("-$- MmsPduHelper: " + s + "\n");
  };
} else {
  debug = function (s) {};
}

const EXPORTED_SYMBOLS = ALL_CONST_SYMBOLS.concat([
  // Utility functions
  "translatePduErrorToStatus",

  // Decoders
  "BooleanValue",
  "Address",
  "HeaderField",
  "MmsHeader",
  "ContentClassValue",
  "ContentLocationValue",
  "ElementDescriptorValue",
  "Parameter",
  "EncodedStringValue",
  "ExpiryValue",
  "FromValue",
  "MessageClassValue",
  "ClassIdentifier",
  "MessageTypeValue",
  "PriorityValue",
  "RecommendedRetrievalModeValue",
  "ReplyChargingValue",
  "StatusValue",

  // Parser
  "PduHelper",
]);

