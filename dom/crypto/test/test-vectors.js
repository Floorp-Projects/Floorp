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
    key_jwk: {
      kty: "oct",
      k: "_v_pkoZlcxxtao-UZzCDCP7_6ZKGZXMcbWqPlGcwgwg"
    },
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

  // RSA test vectors, Example 1.3
  // <ftp://ftp.rsa.com/pub/rsalabs/tmp/pkcs1v15crypt-vectors.txt>
  rsaes: {
    pkcs8: util.hex2abv(
      "30820276020100300d06092a864886f70d0101010500048202603082025c0201" +
      "0002818100a8b3b284af8eb50b387034a860f146c4919f318763cd6c5598c8ae" +
      "4811a1e0abc4c7e0b082d693a5e7fced675cf4668512772c0cbc64a742c6c630" +
      "f533c8cc72f62ae833c40bf25842e984bb78bdbf97c0107d55bdb662f5c4e0fa" +
      "b9845cb5148ef7392dd3aaff93ae1e6b667bb3d4247616d4f5ba10d4cfd226de" +
      "88d39f16fb020301000102818053339cfdb79fc8466a655c7316aca85c55fd8f" +
      "6dd898fdaf119517ef4f52e8fd8e258df93fee180fa0e4ab29693cd83b152a55" +
      "3d4ac4d1812b8b9fa5af0e7f55fe7304df41570926f3311f15c4d65a732c4831" +
      "16ee3d3d2d0af3549ad9bf7cbfb78ad884f84d5beb04724dc7369b31def37d0c" +
      "f539e9cfcdd3de653729ead5d1024100d32737e7267ffe1341b2d5c0d150a81b" +
      "586fb3132bed2f8d5262864a9cb9f30af38be448598d413a172efb802c21acf1" +
      "c11c520c2f26a471dcad212eac7ca39d024100cc8853d1d54da630fac004f471" +
      "f281c7b8982d8224a490edbeb33d3e3d5cc93c4765703d1dd791642f1f116a0d" +
      "d852be2419b2af72bfe9a030e860b0288b5d7702400e12bf1718e9cef5599ba1" +
      "c3882fe8046a90874eefce8f2ccc20e4f2741fb0a33a3848aec9c9305fbecbd2" +
      "d76819967d4671acc6431e4037968db37878e695c102410095297b0f95a2fa67" +
      "d00707d609dfd4fc05c89dafc2ef6d6ea55bec771ea333734d9251e79082ecda" +
      "866efef13c459e1a631386b7e354c899f5f112ca85d7158302404f456c502493" +
      "bdc0ed2ab756a3a6ed4d67352a697d4216e93212b127a63d5411ce6fa98d5dbe" +
      "fd73263e3728142743818166ed7dd63687dd2a8ca1d2f4fbd8e1"
    ),
    spki: util.hex2abv(
      "30819f300d06092a864886f70d010101050003818d0030818902818100a8b3b2" +
      "84af8eb50b387034a860f146c4919f318763cd6c5598c8ae4811a1e0abc4c7e0" +
      "b082d693a5e7fced675cf4668512772c0cbc64a742c6c630f533c8cc72f62ae8" +
      "33c40bf25842e984bb78bdbf97c0107d55bdb662f5c4e0fab9845cb5148ef739" +
      "2dd3aaff93ae1e6b667bb3d4247616d4f5ba10d4cfd226de88d39f16fb020301" +
      "0001"
    ),
    data: util.hex2abv(
      "d94ae0832e6445ce42331cb06d531a82b1db4baad30f746dc916df24d4e3c245" +
      "1fff59a6423eb0e1d02d4fe646cf699dfd818c6e97b051"
    ),
    result: util.hex2abv(
      "709c7d2d4598c96065b6588da2f89fa87f062d7241ef6595898f637ada57eae9" +
      "0173f0fb4bf6a91ebd96506907c853dacf208494be94d313a04185d474a90741" +
      "2effc3e024d07e4d09aa245fbcb130219bfa5de02d4f7e2ec9e62e8ad32dee5f" +
      "f4d8e4cfecbc5033a1c2c61c5233ae16192a481d0075bfc7ce028212cd27bebe"
    ),
  },

  // RSA test vectors, Example 1.3 (sig256 generated with PyCrypto)
  // <ftp://ftp.rsa.com/pub/rsalabs/tmp/pkcs1v15sign-vectors.txt>
  rsassa: {
    pkcs8: util.hex2abv(
      "30820275020100300d06092a864886f70d01010105000482025f3082025b0201" +
      "0002818100a56e4a0e701017589a5187dc7ea841d156f2ec0e36ad52a44dfeb1" +
      "e61f7ad991d8c51056ffedb162b4c0f283a12a88a394dff526ab7291cbb307ce" +
      "abfce0b1dfd5cd9508096d5b2b8b6df5d671ef6377c0921cb23c270a70e2598e" +
      "6ff89d19f105acc2d3f0cb35f29280e1386b6f64c4ef22e1e1f20d0ce8cffb22" +
      "49bd9a2137020301000102818033a5042a90b27d4f5451ca9bbbd0b44771a101" +
      "af884340aef9885f2a4bbe92e894a724ac3c568c8f97853ad07c0266c8c6a3ca" +
      "0929f1e8f11231884429fc4d9ae55fee896a10ce707c3ed7e734e44727a39574" +
      "501a532683109c2abacaba283c31b4bd2f53c3ee37e352cee34f9e503bd80c06" +
      "22ad79c6dcee883547c6a3b325024100e7e8942720a877517273a356053ea2a1" +
      "bc0c94aa72d55c6e86296b2dfc967948c0a72cbccca7eacb35706e09a1df55a1" +
      "535bd9b3cc34160b3b6dcd3eda8e6443024100b69dca1cf7d4d7ec81e75b90fc" +
      "ca874abcde123fd2700180aa90479b6e48de8d67ed24f9f19d85ba275874f542" +
      "cd20dc723e6963364a1f9425452b269a6799fd024028fa13938655be1f8a159c" +
      "baca5a72ea190c30089e19cd274a556f36c4f6e19f554b34c077790427bbdd8d" +
      "d3ede2448328f385d81b30e8e43b2fffa02786197902401a8b38f398fa712049" +
      "898d7fb79ee0a77668791299cdfa09efc0e507acb21ed74301ef5bfd48be455e" +
      "aeb6e1678255827580a8e4e8e14151d1510a82a3f2e729024027156aba4126d2" +
      "4a81f3a528cbfb27f56886f840a9f6e86e17a44b94fe9319584b8e22fdde1e5a" +
      "2e3bd8aa5ba8d8584194eb2190acf832b847f13a3d24a79f4d"
    ),
    jwk_priv: {
      kty: "RSA",
      n:  "pW5KDnAQF1iaUYfcfqhB0Vby7A42rVKkTf6x5h962ZHYxRBW_-2xYrTA8oOhK" +
          "oijlN_1JqtykcuzB86r_OCx39XNlQgJbVsri2311nHvY3fAkhyyPCcKcOJZjm" +
          "_4nRnxBazC0_DLNfKSgOE4a29kxO8i4eHyDQzoz_siSb2aITc",
      e:  "AQAB",
      d:  "M6UEKpCyfU9UUcqbu9C0R3GhAa-IQ0Cu-YhfKku-kuiUpySsPFaMj5eFOtB8A" +
          "mbIxqPKCSnx6PESMYhEKfxNmuVf7olqEM5wfD7X5zTkRyejlXRQGlMmgxCcKr" +
          "rKuig8MbS9L1PD7jfjUs7jT55QO9gMBiKtecbc7og1R8ajsyU",
      p:  "5-iUJyCod1Fyc6NWBT6iobwMlKpy1VxuhilrLfyWeUjApyy8zKfqyzVwbgmh31W" +
          "hU1vZs8w0Fgs7bc0-2o5kQw",
      q:  "tp3KHPfU1-yB51uQ_MqHSrzeEj_ScAGAqpBHm25I3o1n7ST58Z2FuidYdPVCz" +
          "SDccj5pYzZKH5QlRSsmmmeZ_Q",
      dp: "KPoTk4ZVvh-KFZy6ylpy6hkMMAieGc0nSlVvNsT24Z9VSzTAd3kEJ7vdjdPt4" +
          "kSDKPOF2Bsw6OQ7L_-gJ4YZeQ",
      dq: "Gos485j6cSBJiY1_t57gp3ZoeRKZzfoJ78DlB6yyHtdDAe9b_Ui-RV6utuFng" +
          "lWCdYCo5OjhQVHRUQqCo_LnKQ",
      qi: "JxVqukEm0kqB86Uoy_sn9WiG-ECp9uhuF6RLlP6TGVhLjiL93h5aLjvYqluo2" +
          "FhBlOshkKz4MrhH8To9JKefTQ"
    },
    spki: util.hex2abv(
      "30819f300d06092a864886f70d010101050003818d0030818902818100a56e4a" +
      "0e701017589a5187dc7ea841d156f2ec0e36ad52a44dfeb1e61f7ad991d8c510" +
      "56ffedb162b4c0f283a12a88a394dff526ab7291cbb307ceabfce0b1dfd5cd95" +
      "08096d5b2b8b6df5d671ef6377c0921cb23c270a70e2598e6ff89d19f105acc2" +
      "d3f0cb35f29280e1386b6f64c4ef22e1e1f20d0ce8cffb2249bd9a2137020301" +
      "0001"
    ),
    jwk_pub: {
      kty: "RSA",
      n:  "pW5KDnAQF1iaUYfcfqhB0Vby7A42rVKkTf6x5h962ZHYxRBW_-2xYrTA8oOhK" +
          "oijlN_1JqtykcuzB86r_OCx39XNlQgJbVsri2311nHvY3fAkhyyPCcKcOJZjm" +
          "_4nRnxBazC0_DLNfKSgOE4a29kxO8i4eHyDQzoz_siSb2aITc",
      e:  "AQAB",
    },
    data: util.hex2abv(
      "a4b159941761c40c6a82f2b80d1b94f5aa2654fd17e12d588864679b54cd04ef" +
      "8bd03012be8dc37f4b83af7963faff0dfa225477437c48017ff2be8191cf3955" +
      "fc07356eab3f322f7f620e21d254e5db4324279fe067e0910e2e81ca2cab31c7" +
      "45e67a54058eb50d993cdb9ed0b4d029c06d21a94ca661c3ce27fae1d6cb20f4" +
      "564d66ce4767583d0e5f060215b59017be85ea848939127bd8c9c4d47b51056c" +
      "031cf336f17c9980f3b8f5b9b6878e8b797aa43b882684333e17893fe9caa6aa" +
      "299f7ed1a18ee2c54864b7b2b99b72618fb02574d139ef50f019c9eef4169713" +
      "38e7d470"
    ),
    sig1: util.hex2abv(
      "0b1f2e5180e5c7b4b5e672929f664c4896e50c35134b6de4d5a934252a3a245f" +
      "f48340920e1034b7d5a5b524eb0e1cf12befef49b27b732d2c19e1c43217d6e1" +
      "417381111a1d36de6375cf455b3c9812639dbc27600c751994fb61799ecf7da6" +
      "bcf51540afd0174db4033188556675b1d763360af46feeca5b60f882829ee7b2"
    ),
    sig256: util.hex2abv(
      "558af496a9900ec497a51723a0bf1be167a3fdd0e40c95764575bcc93d35d415" +
      "94aef08cd8d339272387339fe5faa5635a1c4ad6c9b622f8c38edce6b26d9b76" +
      "e3fec5b567e5b996624c4aeef74191c4349e5ac9e29b848c54bcfa538fec58d5" +
      "9368253f0ff9a7ba0637918dd16b2c95f8c73ad7484482ba4387655f2f7d4b00"
    ),
    sig_fail: util.hex2abv(
      "8000000080e5c7b4b5e672929f664c4896e50c35134b6de4d5a934252a3a245f" +
      "f48340920e1034b7d5a5b524eb0e1cf12befef49b27b732d2c19e1c43217d6e1" +
      "417381111a1d36de6375cf455b3c9812639dbc27600c751994fb61799ecf7da6" +
      "bcf51540afd0174db4033188556675b1d763360af46feeca5b60f882829ee7b2"
    ),
  },

  // RSA test vectors, oaep-vect.txt, Example 1.1: A 1024-bit RSA Key Pair
  // <ftp://ftp.rsasecurity.com/pub/pkcs/pkcs-1/pkcs-1v2-1-vec.zip>
  rsaoaep: {
    pkcs8: util.hex2abv(
      "30820276020100300d06092a864886f70d0101010500048202603082025c0201" +
      "0002818100a8b3b284af8eb50b387034a860f146c4919f318763cd6c5598c8ae" +
      "4811a1e0abc4c7e0b082d693a5e7fced675cf4668512772c0cbc64a742c6c630" +
      "f533c8cc72f62ae833c40bf25842e984bb78bdbf97c0107d55bdb662f5c4e0fa" +
      "b9845cb5148ef7392dd3aaff93ae1e6b667bb3d4247616d4f5ba10d4cfd226de" +
      "88d39f16fb020301000102818053339cfdb79fc8466a655c7316aca85c55fd8f" +
      "6dd898fdaf119517ef4f52e8fd8e258df93fee180fa0e4ab29693cd83b152a55" +
      "3d4ac4d1812b8b9fa5af0e7f55fe7304df41570926f3311f15c4d65a732c4831" +
      "16ee3d3d2d0af3549ad9bf7cbfb78ad884f84d5beb04724dc7369b31def37d0c" +
      "f539e9cfcdd3de653729ead5d1024100d32737e7267ffe1341b2d5c0d150a81b" +
      "586fb3132bed2f8d5262864a9cb9f30af38be448598d413a172efb802c21acf1" +
      "c11c520c2f26a471dcad212eac7ca39d024100cc8853d1d54da630fac004f471" +
      "f281c7b8982d8224a490edbeb33d3e3d5cc93c4765703d1dd791642f1f116a0d" +
      "d852be2419b2af72bfe9a030e860b0288b5d7702400e12bf1718e9cef5599ba1" +
      "c3882fe8046a90874eefce8f2ccc20e4f2741fb0a33a3848aec9c9305fbecbd2" +
      "d76819967d4671acc6431e4037968db37878e695c102410095297b0f95a2fa67" +
      "d00707d609dfd4fc05c89dafc2ef6d6ea55bec771ea333734d9251e79082ecda" +
      "866efef13c459e1a631386b7e354c899f5f112ca85d7158302404f456c502493" +
      "bdc0ed2ab756a3a6ed4d67352a697d4216e93212b127a63d5411ce6fa98d5dbe" +
      "fd73263e3728142743818166ed7dd63687dd2a8ca1d2f4fbd8e1"
    ),
    spki: util.hex2abv(
      "30819f300d06092a864886f70d010101050003818d0030818902818100a8b3b2" +
      "84af8eb50b387034a860f146c4919f318763cd6c5598c8ae4811a1e0abc4c7e0" +
      "b082d693a5e7fced675cf4668512772c0cbc64a742c6c630f533c8cc72f62ae8" +
      "33c40bf25842e984bb78bdbf97c0107d55bdb662f5c4e0fab9845cb5148ef739" +
      "2dd3aaff93ae1e6b667bb3d4247616d4f5ba10d4cfd226de88d39f16fb020301" +
      "0001"
    ),
    data: util.hex2abv(
      "6628194e12073db03ba94cda9ef9532397d50dba79b987004afefe34"
    ),
    result: util.hex2abv(
      "354fe67b4a126d5d35fe36c777791a3f7ba13def484e2d3908aff722fad468fb" +
      "21696de95d0be911c2d3174f8afcc201035f7b6d8e69402de5451618c21a535f" +
      "a9d7bfc5b8dd9fc243f8cf927db31322d6e881eaa91a996170e657a05a266426" +
      "d98c88003f8477c1227094a0d9fa1e8c4024309ce1ecccb5210035d47ac72e8a"
    ),
  },

  key_wrap_known_answer: {
    key:          util.hex2abv("0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a"),
    wrapping_key: util.hex2abv("0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b"),
    wrapping_iv:  util.hex2abv("0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c"),
    wrapped_key:  util.hex2abv("9ed0283a9a2b7e4292ebc5135e6342cc" +
                               "8a7f65802a1f6fd41bd3251c4da0c138")
  },

  // AES Key Wrap
  // From RFC 3394, "Wrap 128 bits of Key Data with a 256-bit KEK"
  // http://tools.ietf.org/html/rfc3394#section-4.3
  aes_kw: {
    wrapping_key: {
      kty: "oct",
      alg: "A256KW",
      k:   "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8"
    },
    key: {
      kty: "oct",
      k:   "ABEiM0RVZneImaq7zN3u_w"
    },
    wrapped_key: util.hex2abv("64e8c3f9ce0f5ba263e9777905818a2a"+
                              "93c8191e7d6e8ae7")
  },

  // RFC 6070 <http://tools.ietf.org/html/rfc6070>
  pbkdf2_sha1: {
    password: new TextEncoder("utf-8").encode("passwordPASSWORDpassword"),
    salt: new TextEncoder("utf-8").encode("saltSALTsaltSALTsaltSALTsaltSALTsalt"),
    iterations: 4096,
    length: 25 * 8,

    derived: util.hex2abv(
      "3d2eec4fe41c849b80c8d83662c0e44a8b291a964cf2f07038"
    )
  },

  // https://stackoverflow.com/questions/5130513/pbkdf2-hmac-sha2-test-vectors
  pbkdf2_sha256: {
    password: new TextEncoder("utf-8").encode("passwordPASSWORDpassword"),
    salt: new TextEncoder("utf-8").encode("saltSALTsaltSALTsaltSALTsaltSALTsalt"),
    iterations: 4096,
    length: 40 * 8,

    derived: util.hex2abv(
      "348c89dbcbd32b2f32d814b8116e84cf2b17347ebc1800181c4e2a1fb8dd53e1" +
      "c635518c7dac47e9"
    )
  },
}
