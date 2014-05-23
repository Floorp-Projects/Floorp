/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

tv = {
  raw: util.hex2abv("f3095c4fe5e299477643c2310b44f0aa"),

  // this key had an inappropriate length (18 octets)
  negative_raw: util.hex2abv("f3095c4fe5e299477643c2310b44f0aabbcc"),

  // openssl genrsa 512 | openssl pkcs8 -topk8 -nocrypt
  pkcs8: util.hex2abv(
    "30820154020100300d06092a864886f70d01010105000482013e3082013a0201" +
    "00024100a240ceb54e70dc14825b587d2f5dfd463c4b8250b696004a1acaafe4" +
    "9bcf384a46aa9fb4d9c7ee88e9ef0a315f53868f63680b58347249baedd93415" +
    "16c4cab70203010001024034e6dc7ed0ec8b55448b73f69d1310196e5f5045f0" +
    "c247a5e1c664432d6a0af7e7da40b83af047dd01f5e0a90e47c224d7b5133a35" +
    "4d11aa5003b3e8546c9901022100cdb2d7a7435bcb45e50e86f6c14e97ed781f" +
    "0956cd26e6f75ed9fc88125f8407022100c9ee30af6cb95ac9c1149ed84b3338" +
    "481741359409f369c497be177d950fb7d10221008b0ef98d611320639b0b6c20" +
    "4ae4a7fee8f30a6c3cfaacafd4d6c74af228d26702206b0e1dbf935bbd774327" +
    "2483b572a53f0b1d2643a2f6eab7305fb6627cf9855102203d2263156b324146" +
    "4478b713eb854c4f6b3ef052f0463b65d8217daec0099834"
  ),

  // Truncated form of the PKCS#8 stucture above
  negative_pkcs8: util.hex2abv("30820154020100300d06092a864886f70d010"),

  // Extracted from a cert via http://lapo.it/asn1js/
  spki: util.hex2abv(
    "30819F300D06092A864886F70D010101050003818D0030818902818100895309" +
    "7086EE6147C5F4D5FFAF1B498A3D11EC5518E964DC52126B2614F743883F64CA" +
    "51377ABB530DFD20464A48BD67CD27E7B29AEC685C5D10825E605C056E4AB8EE" +
    "A460FA27E55AA62C498B02D7247A249838A12ECDF37C6011CF4F0EDEA9CEE687" +
    "C1CB4A51C6AE62B2EFDB000723A01C99D6C23F834880BA8B42D5414E6F020301" +
    "0001"
  ),

  // Truncated form of the PKCS#8 stucture above
  negative_spki: util.hex2abv("30819F300D06092A864886F70D010101050003"),

  // From the NESSIE project
  // https://www.cosic.esat.kuleuven.be/nessie/testvectors/hash
  //        /sha/Sha-2-256.unverified.test-vectors
  // Set 1, vector# 5
  sha256: {
    data: util.hex2abv("616263646263646563646566646566676566676866676" +
                       "8696768696a68696a6b696a6b6c6a6b6c6d6b6c6d6e6c" +
                       "6d6e6f6d6e6f706e6f7071"),
    result: util.hex2abv("248D6A61D20638B8E5C026930C3E6039A33CE45964F" +
                         "F2167F6ECEDD419DB06C1"),
  },

  // Test vector 2 from:
  // <https://github.com/geertj/bluepass/blob/master/tests/vectors/aes-cbc-pkcs7.txt>
  aes_cbc_enc: {
    /*
    key: util.hex2abv("893123f2d57b6e2c39e2f10d3ff818d1"),
    iv: util.hex2abv("64be1b06ea7453ed2df9a79319d5edc5"),
    data: util.hex2abv("44afb9a64ac896c2"),
    result: util.hex2abv("7067c4cb6dfc69df949c2f39903c9310"),
    */
    key: util.hex2abv("893123f2d57b6e2c39e2f10d3ff818d1"),
    iv: util.hex2abv("64be1b06ea7453ed2df9a79319d5edc5"),
    data: util.hex2abv("44afb9a64ac896c2"),
    result: util.hex2abv("7067c4cb6dfc69df949c2f39903c9310"),
  },

  // Test vector 11 from:
  // <https://github.com/geertj/bluepass/blob/master/tests/vectors/aes-cbc-pkcs7.txt>
  aes_cbc_dec: {
    key: util.hex2abv("04952c3fcf497a4d449c41e8730c5d9a"),
    iv: util.hex2abv("53549bf7d5553b727458c1abaf0ba167"),
    data: util.hex2abv("7fa290322ca7a1a04b61a1147ff20fe6" +
                       "6fde58510a1d0289d11c0ddf6f4decfd"),
    result: util.hex2abv("c9a44f6f75e98ddbca7332167f5c45e3"),
  },

  // Test vector 2 from:
  // <http://tools.ietf.org/html/rfc3686#section-6>
  aes_ctr_enc: {
    key: util.hex2abv("7E24067817FAE0D743D6CE1F32539163"),
    iv: util.hex2abv("006CB6DBC0543B59DA48D90B00000001"),
    data: util.hex2abv("000102030405060708090A0B0C0D0E0F" +
                       "101112131415161718191A1B1C1D1E1F"),
    result: util.hex2abv("5104A106168A72D9790D41EE8EDAD3" +
                         "88EB2E1EFC46DA57C8FCE630DF9141BE28"),
  },

  // Test vector 3 from:
  // <http://tools.ietf.org/html/rfc3686#section-6>
  aes_ctr_dec: {
    key: util.hex2abv("7691BE035E5020A8AC6E618529F9A0DC"),
    iv: util.hex2abv("00E0017B27777F3F4A1786F000000001"),
    data: util.hex2abv("000102030405060708090A0B0C0D0E0F" +
                       "101112131415161718191A1B1C1D1E1F20212223"),
    result: util.hex2abv("C1CF48A89F2FFDD9CF4652E9EFDB72D7" +
                         "4540A42BDE6D7836D59A5CEAAEF3105325B2072F"),
  },

  // Test case #18 from McGrew and Viega
  // <http://csrc.nist.gov/groups/ST/toolkit/BCM/documents/proposedmodes/gcm/gcm-revised-spec.pdf>
  aes_gcm_enc: {
    key: util.hex2abv("feffe9928665731c6d6a8f9467308308" +
                      "feffe9928665731c6d6a8f9467308308"),
    iv: util.hex2abv("9313225df88406e555909c5aff5269aa" +
                     "6a7a9538534f7da1e4c303d2a318a728" +
                     "c3c0c95156809539fcf0e2429a6b5254" +
                     "16aedbf5a0de6a57a637b39b"),
    adata: util.hex2abv("feedfacedeadbeeffeedfacedeadbeefabaddad2"),
    data: util.hex2abv("d9313225f88406e5a55909c5aff5269a" +
                       "86a7a9531534f7da2e4c303d8a318a72" +
                       "1c3c0c95956809532fcf0e2449a6b525" +
                       "b16aedf5aa0de657ba637b39"),
    result: util.hex2abv("5a8def2f0c9e53f1f75d7853659e2a20" +
                         "eeb2b22aafde6419a058ab4f6f746bf4" +
                         "0fc0c3b780f244452da3ebf1c5d82cde" +
                         "a2418997200ef82e44ae7e3f" +
                         "a44a8266ee1c8eb0c8b5d4cf5ae9f19a"),
  },


  // Test case #17 from McGrew and Viega
  // <http://csrc.nist.gov/groups/ST/toolkit/BCM/documents/proposedmodes/gcm/gcm-revised-spec.pdf>
  aes_gcm_dec: {
    key: util.hex2abv("feffe9928665731c6d6a8f9467308308" +
                      "feffe9928665731c6d6a8f9467308308"),
    iv: util.hex2abv("cafebabefacedbad"),
    adata: util.hex2abv("feedfacedeadbeeffeedfacedeadbeefabaddad2"),
    data: util.hex2abv("c3762df1ca787d32ae47c13bf19844cb" +
                       "af1ae14d0b976afac52ff7d79bba9de0" +
                       "feb582d33934a4f0954cc2363bc73f78" +
                       "62ac430e64abe499f47c9b1f" +
                       "3a337dbf46a792c45e454913fe2ea8f2"),
    result: util.hex2abv("d9313225f88406e5a55909c5aff5269a" +
                       "86a7a9531534f7da2e4c303d8a318a72" +
                       "1c3c0c95956809532fcf0e2449a6b525" +
                       "b16aedf5aa0de657ba637b39"),
  },

  // Test case #17 from McGrew and Viega
  // ... but with part of the authentication tag zeroed
  // <http://csrc.nist.gov/groups/ST/toolkit/BCM/documents/proposedmodes/gcm/gcm-revised-spec.pdf>
  aes_gcm_dec_fail: {
    key: util.hex2abv("feffe9928665731c6d6a8f9467308308" +
                      "feffe9928665731c6d6a8f9467308308"),
    iv: util.hex2abv("cafebabefacedbad"),
    adata: util.hex2abv("feedfacedeadbeeffeedfacedeadbeefabaddad2"),
    data: util.hex2abv("c3762df1ca787d32ae47c13bf19844cb" +
                       "af1ae14d0b976afac52ff7d79bba9de0" +
                       "feb582d33934a4f0954cc2363bc73f78" +
                       "62ac430e64abe499f47c9b1f" +
                       "00000000000000005e454913fe2ea8f2"),
    result: util.hex2abv("d9313225f88406e5a55909c5aff5269a" +
                       "86a7a9531534f7da2e4c303d8a318a72" +
                       "1c3c0c95956809532fcf0e2449a6b525" +
                       "b16aedf5aa0de657ba637b39"),
  },

  // RFC 4231 <http://tools.ietf.org/html/rfc4231>, Test Case 7
  hmac_sign: {
    key: util.hex2abv("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" +
                      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" +
                      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" +
                      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" +
                      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" +
                      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" +
                      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" +
                      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" +
                      "aaaaaa"),
    data: util.hex2abv("54686973206973206120746573742075" +
                       "73696e672061206c6172676572207468" +
                       "616e20626c6f636b2d73697a65206b65" +
                       "7920616e642061206c61726765722074" +
                       "68616e20626c6f636b2d73697a652064" +
                       "6174612e20546865206b6579206e6565" +
                       "647320746f2062652068617368656420" +
                       "6265666f7265206265696e6720757365" +
                       "642062792074686520484d414320616c" +
                       "676f726974686d2e"),
    result: util.hex2abv("9b09ffa71b942fcb27635fbcd5b0e944" +
                         "bfdc63644f0713938a7f51535c3a35e2"),
  },

  // RFC 4231 <http://tools.ietf.org/html/rfc4231>, Test Case 6
  hmac_verify: {
    key: util.hex2abv("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" +
                      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" +
                      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" +
                      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" +
                      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" +
                      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" +
                      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" +
                      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" +
                      "aaaaaa"),
    data: util.hex2abv("54657374205573696e67204c61726765" +
                       "72205468616e20426c6f636b2d53697a" +
                       "65204b6579202d2048617368204b6579" +
                       "204669727374"),
    sig: util.hex2abv("60e431591ee0b67f0d8a26aacbf5b77f" +
                      "8e0bc6213728c5140546040f0ee37f54"),
    sig_fail: util.hex2abv("000000001ee0b67f0d8a26aacbf5b77f" +
                           "8e0bc6213728c5140546040f0ee37f54"),
  },
}
