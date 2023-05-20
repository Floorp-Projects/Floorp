/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Stolen from https://github.com/firefox-devtools/profiler/blob/52a7531662a08d96dc8bd03d25adcdb4c9653b92/src/utils/l10n-pseudo.js
// Which was stolen from https://hg.mozilla.org/mozilla-central/file/a1f74e8c8fb72390d22054d6b00c28b1a32f6c43/intl/l10n/L10nRegistry.jsm#l425

/**
 * Pseudolocalizations
 *
 * PSEUDO_STRATEGIES is a dict of strategies to be used to modify a
 * context in order to create pseudolocalizations.  These can be used by
 * developers to test the localizability of their code without having to
 * actually speak a foreign language.
 *
 * Currently, the following pseudolocales are supported:
 *
 *   accented - Ȧȧƈƈḗḗƞŧḗḗḓ Ḗḗƞɠŀīīşħ
 *
 *     In Accented English all Latin letters are replaced by accented
 *     Unicode counterparts which don't impair the readability of the content.
 *     This allows developers to quickly test if any given string is being
 *     correctly displayed in its 'translated' form.  Additionally, simple
 *     heuristics are used to make certain words longer to better simulate the
 *     experience of international users.
 *
 *   bidi - ɥsıʅƃuƎ ıpıԐ
 *
 *     Bidi English is a fake RTL locale.  All words are surrounded by
 *     Unicode formatting marks forcing the RTL directionality of characters.
 *     In addition, to make the reversed text easier to read, individual
 *     letters are flipped.
 *
 *     Note: The name above is hardcoded to be RTL in case code editors have
 *     trouble with the RLO and PDF Unicode marks.  In reality, it should be
 *     surrounded by those marks as well.
 *
 * See https://bugzil.la/1450781 for more information.
 *
 * In this implementation we use code points instead of inline unicode characters
 * because the encoding of JSM files mangles them otherwise.
 */

const ACCENTED_MAP = {
  // ȦƁƇḒḖƑƓĦĪĴĶĿḾȠǾƤɊŘŞŦŬṼẆẊẎẐ
  // prettier-ignore
  "caps": [550, 385, 391, 7698, 7702, 401, 403, 294, 298, 308, 310, 319, 7742, 544, 510, 420, 586, 344, 350, 358, 364, 7804, 7814, 7818, 7822, 7824],
  // ȧƀƈḓḗƒɠħīĵķŀḿƞǿƥɋřşŧŭṽẇẋẏẑ
  // prettier-ignore
  "small": [551, 384, 392, 7699, 7703, 402, 608, 295, 299, 309, 311, 320, 7743, 414, 511, 421, 587, 345, 351, 359, 365, 7805, 7815, 7819, 7823, 7825],
};

const FLIPPED_MAP = {
  // ∀ԐↃᗡƎℲ⅁HIſӼ⅂WNOԀÒᴚS⊥∩ɅMX⅄Z
  // prettier-ignore
  "caps": [8704, 1296, 8579, 5601, 398, 8498, 8513, 72, 73, 383, 1276, 8514, 87, 78, 79, 1280, 210, 7450, 83, 8869, 8745, 581, 77, 88, 8516, 90],
  // ɐqɔpǝɟƃɥıɾʞʅɯuodbɹsʇnʌʍxʎz
  // prettier-ignore
  "small": [592, 113, 596, 112, 477, 607, 387, 613, 305, 638, 670, 645, 623, 117, 111, 100, 98, 633, 115, 647, 110, 652, 653, 120, 654, 122],
};

function transformString(
  map = FLIPPED_MAP,
  elongate = false,
  prefix = "",
  postfix = "",
  msg
) {
  // Exclude access-keys and other single-char messages
  if (msg.length === 1) {
    return msg;
  }
  // XML entities (&#x202a;) and XML tags.
  const reExcluded = /(&[#\w]+;|<\s*.+?\s*>)/;

  const parts = msg.split(reExcluded);
  const modified = parts.map(part => {
    if (reExcluded.test(part)) {
      return part;
    }
    return (
      prefix +
      part.replace(/[a-z]/gi, ch => {
        const cc = ch.charCodeAt(0);
        if (cc >= 97 && cc <= 122) {
          const newChar = String.fromCodePoint(map.small[cc - 97]);
          // duplicate "a", "e", "o" and "u" to emulate ~30% longer text
          if (
            elongate &&
            (cc === 97 || cc === 101 || cc === 111 || cc === 117)
          ) {
            return newChar + newChar;
          }
          return newChar;
        }
        if (cc >= 65 && cc <= 90) {
          return String.fromCodePoint(map.caps[cc - 65]);
        }
        return ch;
      }) +
      postfix
    );
  });
  return modified.join("");
}

export const PSEUDO_STRATEGY_TRANSFORMS = {
  accented: transformString.bind(null, ACCENTED_MAP, true, "", ""),
  bidi: transformString.bind(null, FLIPPED_MAP, false, "\u202e", "\u202c"),
};
