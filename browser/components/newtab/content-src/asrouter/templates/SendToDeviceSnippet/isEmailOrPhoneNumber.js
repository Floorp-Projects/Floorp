/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Checks if a given string is an email or phone number or neither
 * @param {string} val The user input
 * @param {ASRMessageContent} content .content property on ASR message
 * @returns {"email"|"phone"|""} The type of the input
 */
export function isEmailOrPhoneNumber(val, content) {
  const { locale } = content;
  // http://emailregex.com/
  const email_re =
    /^(([^<>()[\]\\.,;:\s@"]+(\.[^<>()[\]\\.,;:\s@"]+)*)|(".+"))@((\[[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}])|(([a-zA-Z\-0-9]+\.)+[a-zA-Z]{2,}))$/;
  const check_email = email_re.test(val);
  let check_phone; // depends on locale
  switch (locale) {
    case "en-US":
    case "en-CA":
      // allow 10-11 digits in case user wants to enter country code
      check_phone = val.length >= 10 && val.length <= 11 && !isNaN(val);
      break;
    case "de":
      // allow between 2 and 12 digits for german phone numbers
      check_phone = val.length >= 2 && val.length <= 12 && !isNaN(val);
      break;
    // this case should never be hit, but good to have a fallback just in case
    default:
      check_phone = !isNaN(val);
      break;
  }
  if (check_email) {
    return "email";
  } else if (check_phone) {
    return "phone";
  }
  return "";
}
