/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["TelURIParser"];

/**
 * Singleton providing functionality for parsing tel: and sms: URIs
 */
this.TelURIParser = {
  parseURI: function(scheme, uri) {
    // https://www.ietf.org/rfc/rfc2806.txt
    let subscriber = decodeURIComponent(uri.slice((scheme + ':').length));

    if (!subscriber.length) {
      return null;
    }

    let number = '';
    let pos = 0;
    let len = subscriber.length;

    // visual-separator
    let visualSeparator = [ ' ', '-', '.', '(', ')' ];
    let digits = [ '0', '1', '2', '3', '4', '5', '6', '7', '8', '9' ];
    let dtmfDigits = [ '*', '#', 'A', 'B', 'C', 'D' ];
    let pauseCharacter = [ 'p', 'w' ];

    // global-phone-number
    if (subscriber[pos] == '+') {
      number += '+';
      for (++pos; pos < len; ++pos) {
        if (visualSeparator.indexOf(subscriber[pos]) != -1) {
          number += subscriber[pos];
        } else if (digits.indexOf(subscriber[pos]) != -1) {
          number += subscriber[pos];
        } else {
          break;
        }
      }
    }
    // local-phone-number
    else {
      for (; pos < len; ++pos) {
        if (visualSeparator.indexOf(subscriber[pos]) != -1) {
          number += subscriber[pos];
        } else if (digits.indexOf(subscriber[pos]) != -1) {
          number += subscriber[pos];
        } else if (dtmfDigits.indexOf(subscriber[pos]) != -1) {
          number += subscriber[pos];
        } else if (pauseCharacter.indexOf(subscriber[pos]) != -1) {
          number += subscriber[pos];
        } else {
          break;
        }
      }

      // this means error
      if (!number.length) {
        return null;
      }

      // isdn-subaddress
      if (subscriber.substring(pos, pos + 6) == ';isub=') {
        let subaddress = '';

        for (pos += 6; pos < len; ++pos) {
          if (visualSeparator.indexOf(subscriber[pos]) != -1) {
            subaddress += subscriber[pos];
          } else if (digits.indexOf(subscriber[pos]) != -1) {
            subaddress += subscriber[pos];
          } else {
            break;
          }
        }

        // FIXME: ignore subaddress - Bug 795242
      }

      // post-dial
      if (subscriber.substring(pos, pos + 7) == ';postd=') {
        let subaddress = '';

        for (pos += 7; pos < len; ++pos) {
          if (visualSeparator.indexOf(subscriber[pos]) != -1) {
            subaddress += subscriber[pos];
          } else if (digits.indexOf(subscriber[pos]) != -1) {
            subaddress += subscriber[pos];
          } else if (dtmfDigits.indexOf(subscriber[pos]) != -1) {
            subaddress += subscriber[pos];
          } else if (pauseCharacter.indexOf(subscriber[pos]) != -1) {
            subaddress += subscriber[pos];
          } else {
            break;
          }
        }

        // FIXME: ignore subaddress - Bug 795242
      }

      // area-specific
      if (subscriber.substring(pos, pos + 15) == ';phone-context=') {
        pos += 15;

        // global-network-prefix | local-network-prefix | private-prefi
        number = subscriber.substring(pos, subscriber.length) + number;
      }
    }

    // Ignore MWI and USSD codes. See 794034.
    if (number.match(/[#\*]/) && !number.match(/^[#\*]\d+$/)) {
      return null;
    }

    return number || null;
  }
};

