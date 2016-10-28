/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// The WebIDL binder places static methods on the prototype, rather than
// on the constructor, which is a bit clumsy, and is definitely not
// idiomatic.
LanguageInfo.detectLanguage = LanguageInfo.prototype.detectLanguage;

// Closure is overzealous in its function call optimization, and tries
// to turn these singleton methods into unbound function calls.
ensureCache.alloc = ensureCache.alloc.bind(ensureCache);
ensureCache.prepare = ensureCache.prepare.bind(ensureCache);

// From public/encodings.h. Unfortunately, the WebIDL binder doesn't
// allow us to define or automatically derive these in the IDL.
var Encodings = {
  'ISO_8859_1'           :  0,
  'ISO_8859_2'           :  1,
  'ISO_8859_3'           :  2,
  'ISO_8859_4'           :  3,
  'ISO_8859_5'           :  4,
  'ISO_8859_6'           :  5,
  'ISO_8859_7'           :  6,
  'ISO_8859_8'           :  7,
  'ISO_8859_9'           :  8,
  'ISO_8859_10'          :  9,
  'JAPANESE_EUC_JP'      : 10,
  'EUC_JP'               : 10,
  'JAPANESE_SHIFT_JIS'   : 11,
  'SHIFT_JIS'            : 11,
  'JAPANESE_JIS'         : 12,
  'JIS'                  : 12,
  'CHINESE_BIG5'         : 13,
  'BIG5'                 : 13,
  'CHINESE_GB'           : 14,
  'CHINESE_EUC_CN'       : 15,
  'EUC_CN'               : 15,
  'KOREAN_EUC_KR'        : 16,
  'EUC_KR'               : 16,
  'UNICODE_UNUSED'       : 17,
  'CHINESE_EUC_DEC'      : 18,
  'EUC_DEC'              : 18,
  'CHINESE_CNS'          : 19,
  'CNS'                  : 19,
  'CHINESE_BIG5_CP950'   : 20,
  'BIG5_CP950'           : 20,
  'JAPANESE_CP932'       : 21,
  'CP932'                : 21,
  'UTF8'                 : 22,
  'UNKNOWN_ENCODING'     : 23,
  'ASCII_7BIT'           : 24,
  'RUSSIAN_KOI8_R'       : 25,
  'KOI8_R'               : 25,
  'RUSSIAN_CP1251'       : 26,
  'CP1251'               : 26,
  'MSFT_CP1252'          : 27,
  'CP1252'               : 27,
  'RUSSIAN_KOI8_RU'      : 28,
  'KOI8_RU'              : 28,
  'MSFT_CP1250'          : 29,
  'CP1250'               : 29,
  'ISO_8859_15'          : 30,
  'MSFT_CP1254'          : 31,
  'CP1254'               : 31,
  'MSFT_CP1257'          : 32,
  'CP1257'               : 32,
  'ISO_8859_11'          : 33,
  'MSFT_CP874'           : 34,
  'CP874'                : 34,
  'MSFT_CP1256'          : 35,
  'CP1256'               : 35,
  'MSFT_CP1255'          : 36,
  'CP1255'               : 36,
  'ISO_8859_8_I'         : 37,
  'HEBREW_VISUAL'        : 38,
  'CZECH_CP852'          : 39,
  'CP852'                : 39,
  'CZECH_CSN_369103'     : 40,
  'CSN_369103'           : 40,
  'MSFT_CP1253'          : 41,
  'CP1253'               : 41,
  'RUSSIAN_CP866'        : 42,
  'CP866'                : 42,
  'ISO_8859_13'          : 43,
  'ISO_2022_KR'          : 44,
  'GBK'                  : 45,
  'GB18030'              : 46,
  'BIG5_HKSCS'           : 47,
  'ISO_2022_CN'          : 48,
  'TSCII'                : 49,
  'TAMIL_MONO'           : 50,
  'TAMIL_BI'             : 51,
  'JAGRAN'               : 52,
  'MACINTOSH_ROMAN'      : 53,
  'UTF7'                 : 54,
  'BHASKAR'              : 55,
  'HTCHANAKYA'           : 56,
  'UTF16BE'              : 57,
  'UTF16LE'              : 58,
  'UTF32BE'              : 59,
  'UTF32LE'              : 60,
  'BINARYENC'            : 61,
  'HZ_GB_2312'           : 62,
  'UTF8UTF8'             : 63,
  'TAM_ELANGO'           : 64,
  'TAM_LTTMBARANI'       : 65,
  'TAM_SHREE'            : 66,
  'TAM_TBOOMIS'          : 67,
  'TAM_TMNEWS'           : 68,
  'TAM_WEBTAMIL'         : 69,
  'KDDI_SHIFT_JIS'       : 70,
  'DOCOMO_SHIFT_JIS'     : 71,
  'SOFTBANK_SHIFT_JIS'   : 72,
  'KDDI_ISO_2022_JP'     : 73,
  'ISO_2022_JP'          : 73,
  'SOFTBANK_ISO_2022_JP' : 74,
};

// Accept forms both with and without underscores/hypens.
for (let code of Object.keys(Encodings)) {
  if (code['includes']("_"))
    Encodings[code.replace(/_/g, "")] = Encodings[code];
}

addOnPreMain(function() {

  onmessage = function(aMsg) {
    let data = aMsg['data'];

    let langInfo;
    if (data['tld'] == undefined && data['encoding'] == undefined && data['language'] == undefined) {
      langInfo = LanguageInfo.detectLanguage(data['text'], !data['isHTML']);
    } else {
      // Do our best to find the given encoding in the encodings table.
      // Otherwise, just fall back to unknown.
      let enc = String(data['encoding']).toUpperCase().replace(/[_-]/g, "");

      let encoding;
      if (Encodings.hasOwnProperty(enc))
        encoding = Encodings[enc];
      else
        encoding = Encodings['UNKNOWN_ENCODING'];

      langInfo = LanguageInfo.detectLanguage(data['text'], !data['isHTML'],
                                             data['tld'] || null,
                                             encoding,
                                             data['language'] || null);
    }

    postMessage({
      'language': langInfo.getLanguageCode(),
      'confident': langInfo.getIsReliable(),

      'languages': new Array(3).fill(0).map((_, index) => {
        let lang = langInfo.get_languages(index);
        return {
          'languageCode': lang.getLanguageCode(),
          'percent': lang.getPercent(),
        };
      }).filter(lang => {
        // Ignore empty results.
        return lang['languageCode'] != "un" || lang['percent'] > 0;
      }),
    });

    Module.destroy(langInfo);
  };

  postMessage("ready");
});
