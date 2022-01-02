# Updating this library

1. Replace the contents of validator.js with the contents of https://github.com/chriso/validator.js/blob/master/validator.js.

2. Add the following methods:
   ```
   // see http://isrc.ifpi.org/en/isrc-standard/code-syntax
   var isrc = /^[A-Z]{2}[0-9A-Z]{3}\d{2}\d{5}$/;

   function isISRC(str) {
     assertString(str);
     return isrc.test(str);
   }
   ```

   ```
   var cultureCodes = new Set(["ar", "bg", "ca", "zh-Hans", "cs", "da", "de",
   "el", "en", "es", "fi", "fr", "he", "hu", "is", "it", "ja", "ko", "nl", "no",
   "pl", "pt", "rm", "ro", "ru", "hr", "sk", "sq", "sv", "th", "tr", "ur", "id",
   "uk", "be", "sl", "et", "lv", "lt", "tg", "fa", "vi", "hy", "az", "eu", "hsb",
   "mk", "tn", "xh", "zu", "af", "ka", "fo", "hi", "mt", "se", "ga", "ms", "kk",
   "ky", "sw", "tk", "uz", "tt", "bn", "pa", "gu", "or", "ta", "te", "kn", "ml",
   "as", "mr", "sa", "mn", "bo", "cy", "km", "lo", "gl", "kok", "syr", "si", "iu",
   "am", "tzm", "ne", "fy", "ps", "fil", "dv", "ha", "yo", "quz", "nso", "ba", "lb",
   "kl", "ig", "ii", "arn", "moh", "br", "ug", "mi", "oc", "co", "gsw", "sah",
   "qut", "rw", "wo", "prs", "gd", "ar-SA", "bg-BG", "ca-ES", "zh-TW", "cs-CZ",
   "da-DK", "de-DE", "el-GR", "en-US", "fi-FI", "fr-FR", "he-IL", "hu-HU", "is-IS",
   "it-IT", "ja-JP", "ko-KR", "nl-NL", "nb-NO", "pl-PL", "pt-BR", "rm-CH", "ro-RO",
   "ru-RU", "hr-HR", "sk-SK", "sq-AL", "sv-SE", "th-TH", "tr-TR", "ur-PK", "id-ID",
   "uk-UA", "be-BY", "sl-SI", "et-EE", "lv-LV", "lt-LT", "tg-Cyrl-TJ", "fa-IR",
   "vi-VN", "hy-AM", "az-Latn-AZ", "eu-ES", "hsb-DE", "mk-MK", "tn-ZA", "xh-ZA",
   "zu-ZA", "af-ZA", "ka-GE", "fo-FO", "hi-IN", "mt-MT", "se-NO", "ms-MY", "kk-KZ",
   "ky-KG", "sw-KE", "tk-TM", "uz-Latn-UZ", "tt-RU", "bn-IN", "pa-IN", "gu-IN",
   "or-IN", "ta-IN", "te-IN", "kn-IN", "ml-IN", "as-IN", "mr-IN", "sa-IN", "mn-MN",
   "bo-CN", "cy-GB", "km-KH", "lo-LA", "gl-ES", "kok-IN", "syr-SY", "si-LK",
   "iu-Cans-CA", "am-ET", "ne-NP", "fy-NL", "ps-AF", "fil-PH", "dv-MV",
   "ha-Latn-NG", "yo-NG", "quz-BO", "nso-ZA", "ba-RU", "lb-LU", "kl-GL", "ig-NG",
   "ii-CN", "arn-CL", "moh-CA", "br-FR", "ug-CN", "mi-NZ", "oc-FR", "co-FR",
   "gsw-FR", "sah-RU", "qut-GT", "rw-RW", "wo-SN", "prs-AF", "gd-GB", "ar-IQ",
   "zh-CN", "de-CH", "en-GB", "es-MX", "fr-BE", "it-CH", "nl-BE", "nn-NO", "pt-PT",
   "sr-Latn-CS", "sv-FI", "az-Cyrl-AZ", "dsb-DE", "se-SE", "ga-IE", "ms-BN",
   "uz-Cyrl-UZ", "bn-BD", "mn-Mong-CN", "iu-Latn-CA", "tzm-Latn-DZ", "quz-EC",
   "ar-EG", "zh-HK", "de-AT", "en-AU", "es-ES", "fr-CA", "sr-Cyrl-CS", "se-FI",
   "quz-PE", "ar-LY", "zh-SG", "de-LU", "en-CA", "es-GT", "fr-CH", "hr-BA",
   "smj-NO", "ar-DZ", "zh-MO", "de-LI", "en-NZ", "es-CR", "fr-LU", "bs-Latn-BA",
   "smj-SE", "ar-MA", "en-IE", "es-PA", "fr-MC", "sr-Latn-BA", "sma-NO", "ar-TN",
   "en-ZA", "es-DO", "sr-Cyrl-BA", "sma-SE", "ar-OM", "en-JM", "es-VE",
   "bs-Cyrl-BA", "sms-FI", "ar-YE", "en-029", "es-CO", "sr-Latn-RS", "smn-FI",
   "ar-SY", "en-BZ", "es-PE", "sr-Cyrl-RS", "ar-JO", "en-TT", "es-AR", "sr-Latn-ME",
   "ar-LB", "en-ZW", "es-EC", "sr-Cyrl-ME", "ar-KW", "en-PH", "es-CL", "ar-AE",
   "es-UY", "ar-BH", "es-PY", "ar-QA", "en-IN", "es-BO", "en-MY", "es-SV", "en-SG",
   "es-HN", "es-NI", "es-PR", "es-US", "bs-Cyrl", "bs-Latn", "sr-Cyrl", "sr-Latn",
   "smn", "az-Cyrl", "sms", "zh", "nn", "bs", "az-Latn", "sma", "uz-Cyrl",
   "mn-Cyrl", "iu-Cans", "zh-Hant", "nb", "sr", "tg-Cyrl", "dsb", "smj", "uz-Latn",
   "mn-Mong", "iu-Latn", "tzm-Latn", "ha-Latn", "zh-CHS", "zh-CHT"]);

   function isRFC5646(str) {
     assertString(str);
     // According to the spec these codes are case sensitive so we can check the
     // string directly.
     return cultureCodes.has(str);
   }
   ```

   ```
   var semver =  /^v?(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)(-(0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*)(\.(0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*))*)?(\+[0-9a-zA-Z-]+(\.[0-9a-zA-Z-]+)*)?$/i

   function isSemVer(str) {
     assertString(str);
     return semver.test(str);
   }
   ```

   ```
   var rgbcolor =  /^rgb?\(\s*(0|[1-9]\d?|1\d\d?|2[0-4]\d|25[0-5])\s*,\s*(0|[1-9]\d?|1\d\d?|2[0-4]\d|25[0-5])\s*,\s*(0|[1-9]\d?|1\d\d?|2[0-4]\d|25[0-5])\s*\)$/i

   function isRGBColor(str) {
     assertString(str);
     return rgbcolor.test(str);
   }
   ```

3. Add the following to the validator object towards the end of the file:
   ```
   isISRC: isISRC,
   isRFC5646: isRFC5646,
   isSemVer: isSemVer,
   isRGBColor: isRGBColor,
   ```

4. Look for the phones array just above the isMobilePhone() method.

   1. Replace the en-HK regex with:
      ```
      // According to http://www.ofca.gov.hk/filemanager/ofca/en/content_311/no_plan.pdf
      'en-HK': /^(\+?852-?)?((4(04[01]|06\d|09[3-9]|20\d|2[2-9]\d|3[3-9]\d|[467]\d{2}|5[1-9]\d|81\d|82[1-9]|8[69]\d|92[3-9]|95[2-9]|98\d)|5([1-79]\d{2})|6(0[1-9]\d|[1-9]\d{2})|7(0[1-9]\d|10[4-79]|11[458]|1[24578]\d|13[24-9]|16[0-8]|19[24579]|21[02-79]|2[456]\d|27[13-6]|3[456]\d|37[4578]|39[0146])|8(1[58]\d|2[45]\d|267|27[5-9]|2[89]\d|3[15-9]\d|32[5-8]|[46-9]\d{2}|5[013-9]\d)|9(0[1-9]\d|1[02-9]\d|[2-8]\d{2}))-?\d{4}|7130-?[0124-8]\d{3}|8167-?2\d{3})$/,
      ```
   2. Add:
      ```
      'ko-KR': /^((\+?82)[ \-]?)?0?1([0|1|6|7|8|9]{1})[ \-]?\d{3,4}[ \-]?\d{4}$/,
      'lt-LT': /^(\+370|8)\d{8}$/,
      ```

5. Replace the isMobilePhone() method with:
   ```
   function isMobilePhone(str, locale) {
     assertString(str);
     if (locale in phones) {
       return phones[locale].test(str);
     } else if (locale === 'any') {
       return !!Object.values(phones).find(phone => phone.test(str));
     }
     return false;
   }
   ```

6. Delete the notBase64 regex and replace the isBase64 with:
   ```
   function isBase64(str) {
     assertString(str);
     // Value length must be divisible by 4.
     var len = str.length;
     if (!len || len % 4 !== 0) {
       return false;
     }

     try {
       if (atob(str)) {
         return true;
       }
     } catch (e) {
       return false;
     }
   }
   ```

7. Do not replace the test files as they have been converted to xpcshell tests. If there are new methods then add their tests to the `test_sanitizers.js` or `test_validators.js` files as appropriate.

8. To test the library please run the following:
   ```
   ./mach xpcshell-test devtools/client/shared/vendor/stringvalidator/
   ```
