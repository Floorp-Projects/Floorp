/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */

// Don't modify this code. Please use:
// https://github.com/andreasgal/PhoneNumber.js

"use strict";

this.EXPORTED_SYMBOLS = ["PhoneNumber"];

const Cu = Components.utils;

Cu.import('resource://gre/modules/XPCOMUtils.jsm');
XPCOMUtils.defineLazyModuleGetter(this, "PHONE_NUMBER_META_DATA",
                                  "resource://gre/modules/PhoneNumberMetaData.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PhoneNumberNormalizer",
                                  "resource://gre/modules/PhoneNumberNormalizer.jsm");
this.PhoneNumber = (function (dataBase) {
  // Use strict in our context only - users might not want it
  'use strict';

  const MAX_PHONE_NUMBER_LENGTH = 50;
  const NON_ALPHA_CHARS = /[^a-zA-Z]/g;
  const NON_DIALABLE_CHARS = /[^,#+\*\d]/g;
  const NON_DIALABLE_CHARS_ONCE = new RegExp(NON_DIALABLE_CHARS.source);
  const BACKSLASH = /\\/g;
  const SPLIT_FIRST_GROUP = /^(\d+)(.*)$/;
  const LEADING_PLUS_CHARS_PATTERN = /^[+\uFF0B]+/g;

  // Format of the string encoded meta data. If the name contains "^" or "$"
  // we will generate a regular expression from the value, with those special
  // characters as prefix/suffix.
  const META_DATA_ENCODING = ["region",
                              "^(?:internationalPrefix)",
                              "nationalPrefix",
                              "^(?:nationalPrefixForParsing)",
                              "nationalPrefixTransformRule",
                              "nationalPrefixFormattingRule",
                              "^possiblePattern$",
                              "^nationalPattern$",
                              "formats"];

  const FORMAT_ENCODING = ["^pattern$",
                           "nationalFormat",
                           "^leadingDigits",
                           "nationalPrefixFormattingRule",
                           "internationalFormat"];

  var regionCache = Object.create(null);

  // Parse an array of strings into a convenient object. We store meta
  // data as arrays since thats much more compact than JSON.
  function ParseArray(array, encoding, obj) {
    for (var n = 0; n < encoding.length; ++n) {
      var value = array[n];
      if (!value)
        continue;
      var field = encoding[n];
      var fieldAlpha = field.replace(NON_ALPHA_CHARS, "");
      if (field != fieldAlpha)
        value = new RegExp(field.replace(fieldAlpha, value));
      obj[fieldAlpha] = value;
    }
    return obj;
  }

  // Parse string encoded meta data into a convenient object
  // representation.
  function ParseMetaData(countryCode, md) {
    var array = eval(md.replace(BACKSLASH, "\\\\"));
    md = ParseArray(array,
                    META_DATA_ENCODING,
                    { countryCode: countryCode });
    regionCache[md.region] = md;
    return md;
  }

  // Parse string encoded format data into a convenient object
  // representation.
  function ParseFormat(md) {
    var formats = md.formats;
    if (!formats) {
      return null;
    }
    // Bail if we already parsed the format definitions.
    if (!(Array.isArray(formats[0])))
      return;
    for (var n = 0; n < formats.length; ++n) {
      formats[n] = ParseArray(formats[n],
                              FORMAT_ENCODING,
                              {});
    }
  }

  // Search for the meta data associated with a region identifier ("US") in
  // our database, which is indexed by country code ("1"). Since we have
  // to walk the entire database for this, we cache the result of the lookup
  // for future reference.
  function FindMetaDataForRegion(region) {
    // Check in the region cache first. This will find all entries we have
    // already resolved (parsed from a string encoding).
    var md = regionCache[region];
    if (md)
      return md;
    for (var countryCode in dataBase) {
      var entry = dataBase[countryCode];
      // Each entry is a string encoded object of the form '["US..', or
      // an array of strings. We don't want to parse the string here
      // to save memory, so we just substring the region identifier
      // and compare it. For arrays, we compare against all region
      // identifiers with that country code. We skip entries that are
      // of type object, because they were already resolved (parsed into
      // an object), and their country code should have been in the cache.
      if (Array.isArray(entry)) {
        for (var n = 0; n < entry.length; n++) {
          if (typeof entry[n] == "string" && entry[n].substr(2,2) == region) {
            if (n > 0) {
              // Only the first entry has the formats field set.
              // Parse the main country if we haven't already and use
              // the formats field from the main country.
              if (typeof entry[0] == "string")
                entry[0] = ParseMetaData(countryCode, entry[0]);
              let formats = entry[0].formats;
              let current = ParseMetaData(countryCode, entry[n]);
              current.formats = formats;
              return entry[n] = current;
            }

            entry[n] = ParseMetaData(countryCode, entry[n]);
            return entry[n];
          }
        }
        continue;
      }
      if (typeof entry == "string" && entry.substr(2,2) == region)
        return dataBase[countryCode] = ParseMetaData(countryCode, entry);
    }
  }

  // Format a national number for a given region. The boolean flag "intl"
  // indicates whether we want the national or international format.
  function FormatNumber(regionMetaData, number, intl) {
    // We lazily parse the format description in the meta data for the region,
    // so make sure to parse it now if we haven't already done so.
    ParseFormat(regionMetaData);
    var formats = regionMetaData.formats;
    if (!formats) {
      return null;
    }
    for (var n = 0; n < formats.length; ++n) {
      var format = formats[n];
      // The leading digits field is optional. If we don't have it, just
      // use the matching pattern to qualify numbers.
      if (format.leadingDigits && !format.leadingDigits.test(number))
        continue;
      if (!format.pattern.test(number))
        continue;
      if (intl) {
        // If there is no international format, just fall back to the national
        // format.
        var internationalFormat = format.internationalFormat;
        if (!internationalFormat)
          internationalFormat = format.nationalFormat;
        // Some regions have numbers that can't be dialed from outside the
        // country, indicated by "NA" for the international format of that
        // number format pattern.
        if (internationalFormat == "NA")
          return null;
        // Prepend "+" and the country code.
        number = "+" + regionMetaData.countryCode + " " +
                 number.replace(format.pattern, internationalFormat);
      } else {
        number = number.replace(format.pattern, format.nationalFormat);
        // The region has a national prefix formatting rule, and it can be overwritten
        // by each actual number format rule.
        var nationalPrefixFormattingRule = regionMetaData.nationalPrefixFormattingRule;
        if (format.nationalPrefixFormattingRule)
          nationalPrefixFormattingRule = format.nationalPrefixFormattingRule;
        if (nationalPrefixFormattingRule) {
          // The prefix formatting rule contains two magic markers, "$NP" and "$FG".
          // "$NP" will be replaced by the national prefix, and "$FG" with the
          // first group of numbers.
          var match = number.match(SPLIT_FIRST_GROUP);
          if (match) {
            var firstGroup = match[1];
            var rest = match[2];
            var prefix = nationalPrefixFormattingRule;
            prefix = prefix.replace("$NP", regionMetaData.nationalPrefix);
            prefix = prefix.replace("$FG", firstGroup);
            number = prefix + rest;
          }
        }
      }
      return (number == "NA") ? null : number;
    }
    return null;
  }

  function NationalNumber(regionMetaData, number) {
    this.region = regionMetaData.region;
    this.regionMetaData = regionMetaData;
    this.nationalNumber = number;
  }

  // NationalNumber represents the result of parsing a phone number. We have
  // three getters on the prototype that format the number in national and
  // international format. Once called, the getters put a direct property
  // onto the object, caching the result.
  NationalNumber.prototype = {
    // +1 949-726-2896
    get internationalFormat() {
      var value = FormatNumber(this.regionMetaData, this.nationalNumber, true);
      Object.defineProperty(this, "internationalFormat", { value: value, enumerable: true });
      return value;
    },
    // (949) 726-2896
    get nationalFormat() {
      var value = FormatNumber(this.regionMetaData, this.nationalNumber, false);
      Object.defineProperty(this, "nationalFormat", { value: value, enumerable: true });
      return value;
    },
    // +19497262896
    get internationalNumber() {
      var value = this.internationalFormat ? this.internationalFormat.replace(NON_DIALABLE_CHARS, "")
                                           : null;
      Object.defineProperty(this, "internationalNumber", { value: value, enumerable: true });
      return value;
    },
    // country name 'US'
    get countryName() {
      var value = this.region ? this.region : null;
      Object.defineProperty(this, "countryName", { value: value, enumerable: true });
      return value;
    }
  };

  // Check whether the number is valid for the given region.
  function IsValidNumber(number, md) {
    return md.possiblePattern.test(number);
  }

  // Check whether the number is a valid national number for the given region.
  function IsNationalNumber(number, md) {
    return IsValidNumber(number, md) && md.nationalPattern.test(number);
  }

  // Determine the country code a number starts with, or return null if
  // its not a valid country code.
  function ParseCountryCode(number) {
    for (var n = 1; n <= 3; ++n) {
      var cc = number.substr(0,n);
      if (dataBase[cc])
        return cc;
    }
    return null;
  }

  // Parse an international number that starts with the country code. Return
  // null if the number is not a valid international number.
  function ParseInternationalNumber(number) {
    var ret;

    // Parse and strip the country code.
    var countryCode = ParseCountryCode(number);
    if (!countryCode)
      return null;
    number = number.substr(countryCode.length);

    // Lookup the meta data for the region (or regions) and if the rest of
    // the number parses for that region, return the parsed number.
    var entry = dataBase[countryCode];
    if (Array.isArray(entry)) {
      for (var n = 0; n < entry.length; ++n) {
        if (typeof entry[n] == "string")
          entry[n] = ParseMetaData(countryCode, entry[n]);
        if (n > 0)
          entry[n].formats = entry[0].formats;
        ret = ParseNationalNumber(number, entry[n])
        if (ret)
          return ret;
      }
      return null;
    }
    if (typeof entry == "string")
      entry = dataBase[countryCode] = ParseMetaData(countryCode, entry);
    return ParseNationalNumber(number, entry);
  }

  // Parse a national number for a specific region. Return null if the
  // number is not a valid national number (it might still be a possible
  // number for parts of that region).
  function ParseNationalNumber(number, md) {
    if (!md.possiblePattern.test(number) ||
        !md.nationalPattern.test(number)) {
      return null;
    }
    // Success.
    return new NationalNumber(md, number);
  }

  // Parse a number and transform it into the national format, removing any
  // international dial prefixes and country codes.
  function ParseNumber(number, defaultRegion) {
    var ret;

    // Remove formating characters and whitespace.
    number = PhoneNumberNormalizer.Normalize(number);

    // If there is no defaultRegion, we can't parse international access codes.
    if (!defaultRegion && number[0] !== '+')
      return null;

    // Detect and strip leading '+'.
    if (number[0] === '+')
      return ParseInternationalNumber(number.replace(LEADING_PLUS_CHARS_PATTERN, ""));

    // Lookup the meta data for the given region.
    var md = FindMetaDataForRegion(defaultRegion.toUpperCase());

    // See if the number starts with an international prefix, and if the
    // number resulting from stripping the code is valid, then remove the
    // prefix and flag the number as international.
    if (md.internationalPrefix.test(number)) {
      var possibleNumber = number.replace(md.internationalPrefix, "");
      ret = ParseInternationalNumber(possibleNumber)
      if (ret)
        return ret;
    }

    // This is not an international number. See if its a national one for
    // the current region. National numbers can start with the national
    // prefix, or without.
    if (md.nationalPrefixForParsing) {
      // Some regions have specific national prefix parse rules. Apply those.
      var withoutPrefix = number.replace(md.nationalPrefixForParsing,
                                         md.nationalPrefixTransformRule || '');
      ret = ParseNationalNumber(withoutPrefix, md)
      if (ret)
        return ret;
    } else {
      // If there is no specific national prefix rule, just strip off the
      // national prefix from the beginning of the number (if there is one).
      var nationalPrefix = md.nationalPrefix;
      if (nationalPrefix && number.indexOf(nationalPrefix) == 0 &&
          (ret = ParseNationalNumber(number.substr(nationalPrefix.length), md))) {
        return ret;
      }
    }
    ret = ParseNationalNumber(number, md)
    if (ret)
      return ret;

    // Now lets see if maybe its an international number after all, but
    // without '+' or the international prefix.
    ret = ParseInternationalNumber(number)
    if (ret)
      return ret;

    // If the number matches the possible numbers of the current region,
    // return it as a possible number.
    if (md.possiblePattern.test(number))
      return new NationalNumber(md, number);

    // We couldn't parse the number at all.
    return null;
  }

  function IsPlainPhoneNumber(number) {
    if (typeof number !== 'string') {
      return false;
    }

    var length = number.length;
    var isTooLong = (length > MAX_PHONE_NUMBER_LENGTH);
    var isEmpty = (length === 0);
    return !(isTooLong || isEmpty || NON_DIALABLE_CHARS_ONCE.test(number));
  }

  return {
    IsPlain: IsPlainPhoneNumber,
    Parse: ParseNumber,
  };
})(PHONE_NUMBER_META_DATA);
