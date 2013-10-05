/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Buffer, TextEncoder, TextDecoder } = require('sdk/io/buffer');

exports.testIsEncoding = function (assert) {
  Object.keys(ENCODINGS).forEach(encoding => {
    assert.ok(Buffer.isEncoding(encoding),
      'Buffer.isEncoding ' + encoding + ' truthy');
  });
  ['not-encoding', undefined, null, 100, {}].forEach(encoding => {
    assert.ok(!Buffer.isEncoding(encoding),
      'Buffer.isEncoding ' + encoding + ' falsy');
  });
};

exports.testIsBuffer = function (assert) {
  let buffer = new Buffer('content', 'utf8');
  assert.ok(Buffer.isBuffer(buffer), 'isBuffer truthy on buffers');
  assert.ok(!Buffer.isBuffer({}), 'isBuffer falsy on objects');
  assert.ok(!Buffer.isBuffer(new Uint8Array()),
    'isBuffer falsy on Uint8Array');
};

exports.testBufferByteLength = function (assert) {
  let str = '\u00bd + \u00bc = \u00be';
  assert.equal(Buffer.byteLength(str), 12,
    'correct byteLength of string');
};

exports.testTextEncoderDecoder = function (assert) {
  assert.ok(TextEncoder, 'TextEncoder exists');
  assert.ok(TextDecoder, 'TextDecoder exists');
};

// List of supported encodings taken from:
// http://mxr.mozilla.org/mozilla-central/source/dom/encoding/labelsencodings.properties
const ENCODINGS = { "unicode-1-1-utf-8": 1, "utf-8": 1, "utf8": 1,
  "866": 1, "cp866": 1, "csibm866": 1, "ibm866": 1, "csisolatin2": 1,
  "iso-8859-2": 1, "iso-ir-101": 1, "iso8859-2": 1, "iso88592": 1,
  "iso_8859-2": 1, "iso_8859-2:1987": 1, "l2": 1, "latin2": 1, "csisolatin3": 1,
  "iso-8859-3": 1, "iso-ir-109": 1, "iso8859-3": 1, "iso88593": 1,
  "iso_8859-3": 1, "iso_8859-3:1988": 1, "l3": 1, "latin3": 1, "csisolatin4": 1,
  "iso-8859-4": 1, "iso-ir-110": 1, "iso8859-4": 1, "iso88594": 1,
  "iso_8859-4": 1, "iso_8859-4:1988": 1, "l4": 1, "latin4": 1,
  "csisolatincyrillic": 1, "cyrillic": 1, "iso-8859-5": 1, "iso-ir-144": 1,
  "iso8859-5": 1, "iso88595": 1, "iso_8859-5": 1, "iso_8859-5:1988": 1,
  "arabic": 1, "asmo-708": 1, "csiso88596e": 1, "csiso88596i": 1,
  "csisolatinarabic": 1, "ecma-114": 1, "iso-8859-6": 1, "iso-8859-6-e": 1,
  "iso-8859-6-i": 1, "iso-ir-127": 1, "iso8859-6": 1, "iso88596": 1,
  "iso_8859-6": 1, "iso_8859-6:1987": 1, "csisolatingreek": 1, "ecma-118": 1,
  "elot_928": 1, "greek": 1, "greek8": 1, "iso-8859-7": 1, "iso-ir-126": 1,
  "iso8859-7": 1, "iso88597": 1, "iso_8859-7": 1, "iso_8859-7:1987": 1,
  "sun_eu_greek": 1, "csiso88598e": 1, "csisolatinhebrew": 1, "hebrew": 1,
  "iso-8859-8": 1, "iso-8859-8-e": 1, "iso-ir-138": 1, "iso8859-8": 1,
  "iso88598": 1, "iso_8859-8": 1, "iso_8859-8:1988": 1, "visual": 1,
  "csiso88598i": 1, "iso-8859-8-i": 1, "logical": 1, "csisolatin6": 1,
  "iso-8859-10": 1, "iso-ir-157": 1, "iso8859-10": 1, "iso885910": 1,
  "l6": 1, "latin6": 1, "iso-8859-13": 1, "iso8859-13": 1, "iso885913": 1,
  "iso-8859-14": 1, "iso8859-14": 1, "iso885914": 1, "csisolatin9": 1,
  "iso-8859-15": 1, "iso8859-15": 1, "iso885915": 1, "iso_8859-15": 1,
  "l9": 1, "iso-8859-16": 1, "cskoi8r": 1, "koi": 1, "koi8": 1, "koi8-r": 1,
  "koi8_r": 1, "koi8-u": 1, "csmacintosh": 1, "mac": 1, "macintosh": 1,
  "x-mac-roman": 1, "dos-874": 1, "iso-8859-11": 1, "iso8859-11": 1,
  "iso885911": 1, "tis-620": 1, "windows-874": 1, "cp1250": 1,
  "windows-1250": 1, "x-cp1250": 1, "cp1251": 1, "windows-1251": 1,
  "x-cp1251": 1, "ansi_x3.4-1968": 1, "ascii": 1, "cp1252": 1, "cp819": 1,
  "csisolatin1": 1, "ibm819": 1, "iso-8859-1": 1, "iso-ir-100": 1,
  "iso8859-1": 1, "iso88591": 1, "iso_8859-1": 1, "iso_8859-1:1987": 1,
  "l1": 1, "latin1": 1, "us-ascii": 1, "windows-1252": 1, "x-cp1252": 1,
  "cp1253": 1, "windows-1253": 1, "x-cp1253": 1, "cp1254": 1, "csisolatin5": 1,
  "iso-8859-9": 1, "iso-ir-148": 1, "iso8859-9": 1, "iso88599": 1,
  "iso_8859-9": 1, "iso_8859-9:1989": 1, "l5": 1, "latin5": 1,
  "windows-1254": 1, "x-cp1254": 1, "cp1255": 1, "windows-1255": 1,
  "x-cp1255": 1, "cp1256": 1, "windows-1256": 1, "x-cp1256": 1, "cp1257": 1,
  "windows-1257": 1, "x-cp1257": 1, "cp1258": 1, "windows-1258": 1,
  "x-cp1258": 1, "x-mac-cyrillic": 1, "x-mac-ukrainian": 1, "chinese": 1,
  "csgb2312": 1, "csiso58gb231280": 1, "gb2312": 1, "gb_2312": 1,
  "gb_2312-80": 1, "gbk": 1, "iso-ir-58": 1, "x-gbk": 1, "gb18030": 1,
  "hz-gb-2312": 1, "big5": 1, "big5-hkscs": 1, "cn-big5": 1, "csbig5": 1,
  "x-x-big5": 1, "cseucpkdfmtjapanese": 1, "euc-jp": 1, "x-euc-jp": 1,
  "csiso2022jp": 1, "iso-2022-jp": 1, "csshiftjis": 1, "ms_kanji": 1,
  "shift-jis": 1, "shift_jis": 1, "sjis": 1, "windows-31j": 1, "x-sjis": 1,
  "cseuckr": 1, "csksc56011987": 1, "euc-kr": 1, "iso-ir-149": 1, "korean": 1,
  "ks_c_5601-1987": 1, "ks_c_5601-1989": 1, "ksc5601": 1, "ksc_5601": 1,
  "windows-949": 1, "csiso2022kr": 1, "iso-2022-kr": 1, "utf-16": 1,
  "utf-16le": 1, "utf-16be": 1, "x-user-defined": 1 };

require('sdk/test').run(exports);
