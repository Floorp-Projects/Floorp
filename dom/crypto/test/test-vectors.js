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
    // This RSA private key has p < q
    jwk_priv_pLTq: {
      kty: "RSA",
      n:  "p0HwS3l58WCcN5MPJ3TDu2fMRFZdAFhItiuGMvfDGj0myIM2BhJixSsDleu0h" +
          "x8mSL4CP9c63-zTFdMjwHluYJ_ugK_jV5c4igfyyD2yQ9IAbaBtSh2GV_PrgM" +
          "l9XCMobLQonC0ktHcMojYMTNSmLBc1kXLxKq1PnY8ja8oNoNKmDzJt3Hx_KlJ" +
          "mORwrchrrVzKdrRuj37PmMKKl6MCNThrFFn4PtZ6e59cxgwSAWoOqvvTewCdI" +
          "H4uGuoEnafBcPsuMOLD-oS0CTml_AK3Wgwt-zJ9BSFSial_PSTg0hpUp8pqWv" +
          "1NZaxG1HiY3gS-8JHSqTVeoGznFmod3FqRG_Q",
      e:  "AQAB",
      d:  "SrxoBxGMr5KfuyV3DAZcv4yt9Ysxm0nXk673FCcpgrv4bHhU13m3sKp7u63Ky" +
          "OXeUXq1vpkJsa081O-3dfXMoFhWViJBz42-sc7DaT5IPY3EqzeYHhn7Qam4fZ" +
          "-K6HS9R3VpAAAb-peHiaPk8x_B8MmeIhPeN1ehz6F6DlwGoulDPOI3EMLoOCL" +
          "V_cus8AV8il5FaJxwuuo2xc4oEbwT24AN2hXVZekTgPkNSGXBMhagu82lDlx8" +
          "y7awiC1bchWMUJ88BLqgScbl4bpTLqos0fRXDugY957diwF_zRHdr2jsj8Dm_" +
          "0J1XdgaIeycIApU61PSUy8ZLZbDQ9mlajRlgQ",
      p:  "uQKh1mjslzbWrDBgqAWcCCPpV9tvA5AAotKuyDRe8ohj-WbKcClwQEXTLqT3Z" +
          "uirrHrqrmronElxdN22ukpmf_Kk301Kz1HU5qJZKTOzwiO8JSJ7wtLDDWkoyV" +
          "3Zj6On2N8ZX69cvwbo-S5trIv3iDjfsb0ZvmuGjEn-4dcYUxk",
      q:  "52957s9n0wOBwe5sCtttd_R-h-IX03mZ3Jie4K2GugCZy6H2Rs0hPoylOn0X9" +
          "eI7atHiP3laaHyEwTOQhdC_RUCLsS-ZMa8p0EaewF_Lk6eCL0pDmHpTZiGjeB" +
          "EwvftzoO_cEpbkRkF-OxjRs6ejppm3MKkgZZJT2-R5iSaQU4U",
      dp: "ijDlIXoN_oT_pG4eRGKsQYhRa0aEjWyqjPRBiVlU8mPeCRQ2ccECD4AYVebyx" +
          "PNWB-doFA_W36YcEObq7gtUtI1RiVn6XxEIrZzmbFgqFQEML9CqEMPM3d-Gj6" +
          "KCN0BOxzcdhNM_u5A1xKphUVja8-1HaUOOTyWRwogi0h4QFUE",
      dq: "KN1yNkzBFG1mGAw1X6VnKuss_Glbs6ehF2aLhzictXMttNsgVVgbKqRC-JTmC" +
          "jCsNSxiOrr-z7xM5KBqQHafj2baQ6sX7cH0LCaMGYPQun21aww960qON1ZxOt" +
          "4uMR2ZSS2ROmcSX6Vo2J6FSKetKdmykxEJ-2VfEVDCdQkuKtE",
      qi: "pDhm0DZADZiGIsjYMmpoDo4EmYHari-VfxjAqCgcec5nPfNt5BSKQow3_E_v0" +
          "Yik1qa-AGWuC8vTh8vUFsQ0rE1lVSgXYPalMTjLFNY_hCBXmsDfMS5vcsL0-G" +
          "Y8F2U_XRY3WEaoNPb9UZqzgp7xl6_XM__2U47LIoUpCgcN9RA",
    },
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

  // [pss-vect.txt] Example 1.1 from
  // <ftp://ftp.rsasecurity.com/pub/pkcs/pkcs-1/pkcs-1v2-1-vec.zip>
  rsapss: {
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
    spki: util.hex2abv(
      "30819f300d06092a864886f70d010101050003818d0030818902818100a56e4a" +
      "0e701017589a5187dc7ea841d156f2ec0e36ad52a44dfeb1e61f7ad991d8c510" +
      "56ffedb162b4c0f283a12a88a394dff526ab7291cbb307ceabfce0b1dfd5cd95" +
      "08096d5b2b8b6df5d671ef6377c0921cb23c270a70e2598e6ff89d19f105acc2" +
      "d3f0cb35f29280e1386b6f64c4ef22e1e1f20d0ce8cffb2249bd9a2137020301" +
      "0001"
    ),
    data: util.hex2abv(
      "cdc87da223d786df3b45e0bbbc721326d1ee2af806cc315475cc6f0d9c66e1b6" +
      "2371d45ce2392e1ac92844c310102f156a0d8d52c1f4c40ba3aa65095786cb76" +
      "9757a6563ba958fed0bcc984e8b517a3d5f515b23b8a41e74aa867693f90dfb0" +
      "61a6e86dfaaee64472c00e5f20945729cbebe77f06ce78e08f4098fba41f9d61" +
      "93c0317e8b60d4b6084acb42d29e3808a3bc372d85e331170fcbf7cc72d0b71c" +
      "296648b3a4d10f416295d0807aa625cab2744fd9ea8fd223c42537029828bd16" +
      "be02546f130fd2e33b936d2676e08aed1b73318b750a0167d0"
    ),
    sig: util.hex2abv(
      "9074308fb598e9701b2294388e52f971faac2b60a5145af185df5287b5ed2887" +
      "e57ce7fd44dc8634e407c8e0e4360bc226f3ec227f9d9e54638e8d31f5051215" +
      "df6ebb9c2f9579aa77598a38f914b5b9c1bd83c4e2f9f382a0d0aa3542ffee65" +
      "984a601bc69eb28deb27dca12c82c2d4c3f66cd500f1ff2b994d8a4e30cbb33c"
    ),
    saltLength: 20,
    jwk_priv: {
      kty: "RSA",
      n: "pW5KDnAQF1iaUYfcfqhB0Vby7A42rVKkTf6x5h962ZHYxRBW_-2xYrTA8oOhK" +
         "oijlN_1JqtykcuzB86r_OCx39XNlQgJbVsri2311nHvY3fAkhyyPCcKcOJZjm" +
         "_4nRnxBazC0_DLNfKSgOE4a29kxO8i4eHyDQzoz_siSb2aITc",
      e: "AQAB",
      d: "M6UEKpCyfU9UUcqbu9C0R3GhAa-IQ0Cu-YhfKku-kuiUpySsPFaMj5eFOtB8A" +
         "mbIxqPKCSnx6PESMYhEKfxNmuVf7olqEM5wfD7X5zTkRyejlXRQGlMmgxCcKr" +
         "rKuig8MbS9L1PD7jfjUs7jT55QO9gMBiKtecbc7og1R8ajsyU",
      p: "5-iUJyCod1Fyc6NWBT6iobwMlKpy1VxuhilrLfyWeUjApyy8zKfqyzVwbgmh3" +
         "1WhU1vZs8w0Fgs7bc0-2o5kQw",
      q: "tp3KHPfU1-yB51uQ_MqHSrzeEj_ScAGAqpBHm25I3o1n7ST58Z2FuidYdPVCz" +
         "SDccj5pYzZKH5QlRSsmmmeZ_Q",
      dp: "KPoTk4ZVvh-KFZy6ylpy6hkMMAieGc0nSlVvNsT24Z9VSzTAd3kEJ7vdjdPt" +
          "4kSDKPOF2Bsw6OQ7L_-gJ4YZeQ",
      dq: "Gos485j6cSBJiY1_t57gp3ZoeRKZzfoJ78DlB6yyHtdDAe9b_Ui-RV6utuFn" +
          "glWCdYCo5OjhQVHRUQqCo_LnKQ",
      qi: "JxVqukEm0kqB86Uoy_sn9WiG-ECp9uhuF6RLlP6TGVhLjiL93h5aLjvYqluo" +
          "2FhBlOshkKz4MrhH8To9JKefTQ",
    },
    jwk_pub: {
      kty: "RSA",
      n: "pW5KDnAQF1iaUYfcfqhB0Vby7A42rVKkTf6x5h962ZHYxRBW_-2xYrTA8oOhK" +
         "oijlN_1JqtykcuzB86r_OCx39XNlQgJbVsri2311nHvY3fAkhyyPCcKcOJZjm" +
         "_4nRnxBazC0_DLNfKSgOE4a29kxO8i4eHyDQzoz_siSb2aITc",
      e: "AQAB",
    },
  },

  // [pss-vect.txt] Example 1.4 from
  // <ftp://ftp.rsasecurity.com/pub/pkcs/pkcs-1/pkcs-1v2-1-vec.zip>
  rsapss2: {
    spki: util.hex2abv(
      "30819f300d06092a864886f70d010101050003818d0030818902818100a56e4a" +
      "0e701017589a5187dc7ea841d156f2ec0e36ad52a44dfeb1e61f7ad991d8c510" +
      "56ffedb162b4c0f283a12a88a394dff526ab7291cbb307ceabfce0b1dfd5cd95" +
      "08096d5b2b8b6df5d671ef6377c0921cb23c270a70e2598e6ff89d19f105acc2" +
      "d3f0cb35f29280e1386b6f64c4ef22e1e1f20d0ce8cffb2249bd9a2137020301" +
      "0001"
    ),
    data: util.hex2abv(
      "bc656747fa9eafb3f0"
    ),
    sig: util.hex2abv(
      "4609793b23e9d09362dc21bb47da0b4f3a7622649a47d464019b9aeafe53359c" +
      "178c91cd58ba6bcb78be0346a7bc637f4b873d4bab38ee661f199634c547a1ad" +
      "8442e03da015b136e543f7ab07c0c13e4225b8de8cce25d4f6eb8400f81f7e18" +
      "33b7ee6e334d370964ca79fdb872b4d75223b5eeb08101591fb532d155a6de87"
    ),
    saltLength: 20
  },

  // [SigVerPSS_186-3.rsp] from
  // <http://csrc.nist.gov/groups/STM/cavp/documents/dss/186-2rsatestvectors.zip>
  rsapss3: {
    spki: util.hex2abv(
      "30819d300d06092a864886f70d010101050003818b0030818702818100be499b" +
      "5e7f06c83fa0293e31465c8eb6b58af920bae52a7b5b9bfeb7aa72db1264112e" +
      "b3fd431d31a2a7e50941566929494a0e891ed5613918b4b51b0d1fb97783b26a" +
      "cf7d0f384cfb35f4d2824f5dd380623a26bf180b63961c619dcdb20cae406f22" +
      "f6e276c80a37259490cfeb72c1a71a84f1846d330877ba3e3101ec9c7b020111"
    ),
    data: util.hex2abv(
      "c7f5270fca725f9bd19f519a8d7cca3cc5c079024029f3bae510f9b02140fe23" +
      "8908e4f6c18f07a89c687c8684669b1f1db2baf9251a3c829faccb493084e16e" +
      "c9e28d58868074a5d6221667dd6e528d16fe2c9f3db4cfaf6c4dce8c8439af38" +
      "ceaaaa9ce2ecae7bc8f4a5a55e3bf96df9cd575c4f9cb327951b8cdfe4087168"
    ),
    sig: util.hex2abv(
      "11e169f2fd40b07641b9768a2ab19965fb6c27f10fcf0323fcc6d12eb4f1c06b" +
      "330ddaa1ea504407afa29de9ebe0374fe9d1e7d0ffbd5fc1cf3a3446e4145415" +
      "d2ab24f789b3464c5c43a256bbc1d692cf7f04801dac5bb401a4a03ab7d5728a" +
      "860c19e1a4dc797ca542c8203cec2e601eb0c51f567f2eda022b0b9ebddeeefa"
    ),
    saltLength: 10
  },

  // [SigVerPSS_186-3.rsp] from
  // <http://csrc.nist.gov/groups/STM/cavp/documents/dss/186-2rsatestvectors.zip>
  rsapss4: {
    spki: util.hex2abv(
      "30819d300d06092a864886f70d010101050003818b0030818702818100be499b" +
      "5e7f06c83fa0293e31465c8eb6b58af920bae52a7b5b9bfeb7aa72db1264112e" +
      "b3fd431d31a2a7e50941566929494a0e891ed5613918b4b51b0d1fb97783b26a" +
      "cf7d0f384cfb35f4d2824f5dd380623a26bf180b63961c619dcdb20cae406f22" +
      "f6e276c80a37259490cfeb72c1a71a84f1846d330877ba3e3101ec9c7b020111"
    ),
    data: util.hex2abv(
      "c7f5270fca725f9bd19f519a8d7cca3cc5c079024029f3bae510f9b02140fe23" +
      "8908e4f6c18f07a89c687c8684669b1f1db2baf9251a3c829faccb493084e16e" +
      "c9e28d58868074a5d6221667dd6e528d16fe2c9f3db4cfaf6c4dce8c8439af38" +
      "ceaaaa9ce2ecae7bc8f4a5a55e3bf96df9cd575c4f9cb327951b8cdfe4087168"
    ),
    sig: util.hex2abv(
      "b281ad934b2775c0cba5fb10aa574d2ed85c7f99b942b78e49702480069362ed" +
      "394baded55e56cfcbe7b0b8d2217a05a60e1acd725cb09060dfac585bc2132b9" +
      "9b41cdbd530c69d17cdbc84bc6b9830fc7dc8e1b2412cfe06dcf8c1a0cc3453f" +
      "93f25ebf10cb0c90334fac573f449138616e1a194c67f44efac34cc07a526267"
    ),
    saltLength: 10
  },

  // [SigVerPSS_186-3.rsp] from
  // <http://csrc.nist.gov/groups/STM/cavp/documents/dss/186-2rsatestvectors.zip>
  rsapss5: {
    spki: util.hex2abv(
      "30819d300d06092a864886f70d010101050003818b0030818702818100be499b" +
      "5e7f06c83fa0293e31465c8eb6b58af920bae52a7b5b9bfeb7aa72db1264112e" +
      "b3fd431d31a2a7e50941566929494a0e891ed5613918b4b51b0d1fb97783b26a" +
      "cf7d0f384cfb35f4d2824f5dd380623a26bf180b63961c619dcdb20cae406f22" +
      "f6e276c80a37259490cfeb72c1a71a84f1846d330877ba3e3101ec9c7b020111"
    ),
    data: util.hex2abv(
      "c7f5270fca725f9bd19f519a8d7cca3cc5c079024029f3bae510f9b02140fe23" +
      "8908e4f6c18f07a89c687c8684669b1f1db2baf9251a3c829faccb493084e16e" +
      "c9e28d58868074a5d6221667dd6e528d16fe2c9f3db4cfaf6c4dce8c8439af38" +
      "ceaaaa9ce2ecae7bc8f4a5a55e3bf96df9cd575c4f9cb327951b8cdfe4087168"
    ),
    sig: util.hex2abv(
      "8ffc38f9b820ef6b080fd2ec7de5626c658d79056f3edf610a295b7b0546f73e" +
      "01ffdf4d0070ebf79c33fd86c2d608be9438b3d420d09535b97cd3d846ecaf8f" +
      "6551cdf93197e9f8fb048044473ab41a801e9f7fc983c62b324361dade9f71a6" +
      "5952bd35c59faaa4d6ff462f68a6c4ec0b428aa47336f2178aeb276136563b7d"
    ),
    saltLength: 10
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
    ),

    jwk: {
      kty: "oct",
      k: "cGFzc3dvcmRQQVNTV09SRHBhc3N3b3Jk"
    }
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

  pbkdf2_sha256_no_salt: {
    password: util.hex2abv(
      "9c13a23bc58a52be8bb4fa1a2cbdff01747265736f7269745f64625f7265636f72645f6964"),
    length: 32 * 8,
    iterations: 1,

    derived: util.hex2abv(
      "ef29dd382fa66a83a95be7ccfb71f1ccfee494977855a4c260d90c2f8c91e062")
  },

  broken_pkcs8: {
    // A DH key with parameters p and g, and a private value.
    // This currently fails the key import due to the missing public value.
    // <https://stackoverflow.com/questions/6032675/diffie-hellman-test-vectors>
    dh: util.hex2abv(
      "308201340201003082011506072a8648ce3e02013082010802818100da3a8085" +
      "d372437805de95b88b675122f575df976610c6a844de99f1df82a06848bf7a42" +
      "f18895c97402e81118e01a00d0855d51922f434c022350861d58ddf60d65bc69" +
      "41fc6064b147071a4c30426d82fc90d888f94990267c64beef8c304a4b2b26fb" +
      "93724d6a9472fa16bc50c5b9b8b59afb62cfe9ea3ba042c73a6ade3502818100" +
      "a51883e9ac0539859df3d25c716437008bb4bd8ec4786eb4bc643299daef5e3e" +
      "5af5863a6ac40a597b83a27583f6a658d408825105b16d31b6ed088fc623f648" +
      "fd6d95e9cefcb0745763cddf564c87bcf4ba7928e74fd6a3080481f588d535e4" +
      "c026b58a21e1e5ec412ff241b436043e29173f1dc6cb943c09742de989547288" +
      "0416021442c6ee70beb7465928a1efe692d2281b8f7b53d6"
    )
  },

  // KASValidityTest_ECCEphemeralUnified_NOKC_ZZOnly_init.fax [EC]
  // <http://csrc.nist.gov/groups/STM/cavp/documents/keymgmt/kastestvectors.zip>
  ecdh_p256: {
    jwk_pub: {
      kty: "EC",
      crv: "P-256",
      x: "XOe4bjsyZgQD5jcS7wmY3q4QJ_rsPBvp92-TTf61jpg",
      y: "9M8HWzlAXdHxresJAQftz7K0ljc52HZ54wVssFV9Ct8"
    },

    jwk_priv: {
      kty: "EC",
      crv: "P-256",
      d: "qq_LEzeJpR00KM5DQvL2MNtJcbi0KcGVcoPIHNnwm2A",
      x: "FNwJHA-FwnSx5tKXFV_iLN408gbKUHRV06WnQlzTdN4",
      y: "is9pWAaneK4RdxmdLfsq5IwizDmUS2w8OGS99sKm3ek"
    },

    // vector with algorithm = id-ecDH
    spki: util.hex2abv(
      "3056301006042b81047006082a8648ce3d030107034200045ce7b86e3b326604" +
      "03e63712ef0998deae1027faec3c1be9f76f934dfeb58e98f4cf075b39405dd1" +
      "f1adeb090107edcfb2b4963739d87679e3056cb0557d0adf"
    ),

    // vector with algorithm = id-ecPublicKey
    spki_id_ecpk: util.hex2abv(
      "3059301306072a8648ce3d020106082a8648ce3d030107034200045ce7b86e3b" +
      "32660403e63712ef0998deae1027faec3c1be9f76f934dfeb58e98f4cf075b39" +
      "405dd1f1adeb090107edcfb2b4963739d87679e3056cb0557d0adf"
    ),

    raw: util.hex2abv(
      "045ce7b86e3b32660403e63712ef0998deae1027faec3c1be9f76f934dfeb58e" +
      "98f4cf075b39405dd1f1adeb090107edcfb2b4963739d87679e3056cb0557d0adf"
    ),

    secret: util.hex2abv(
      "35669cd5c244ba6c1ea89b8802c3d1db815cd769979072e6556eb98548c65f7d"
    )
  },

  // KASValidityTest_ECCEphemeralUnified_NOKC_ZZOnly_init.fax [ED]
  // <http://csrc.nist.gov/groups/STM/cavp/documents/keymgmt/kastestvectors.zip>
  ecdh_p384: {
    jwk_pub: {
      kty: "EC",
      crv: "P-384",
      x: "YoV6fhCph4kyt7sUkqiZOtbRs0rF6etPqlnrn1nzSB95NElaw4uTK7Pn2nlFFqqH",
      y: "bf3tRz6icq3-W6hhmoqDTBKjdOQUJ5xHr5kX4X-h5MZk_P_nCrG3IUVl1SAbhWDw"
    },

    jwk_priv: {
      kty: "EC",
      crv: "P-384",
      d: "RT8f0pRw4CL1Tgk4rwuNnNbFoQBNTTBkr7WVLLm4fDA3boYZpNB_t-rbMVLx0CRp",
      x: "_XwhXRnOzEfCsWIRCz3QLClaDkigQFvXmqYNdh_7vJdADykPbfGi1VgAu3XJdXoD",
      y: "S1P_FBCXYGE-5VPvTCRnFT7bPIPmUPV9qKTM24TQFYEUgIDfzCLsyGCWK-rhP6jU"
    },

    secret: util.hex2abv(
      "a3d28aa18f905a48a5f166b4ddbf5f6b499e43858ccdd80b869946aba2c5d461" +
      "db6a1e5b1137687801878ff0f8d9a7b3"
    )
  },

  // KASValidityTest_ECCEphemeralUnified_NOKC_ZZOnly_init.fax [EE]
  // <http://csrc.nist.gov/groups/STM/cavp/documents/keymgmt/kastestvectors.zip>
  ecdh_p521: {
    jwk_pub: {
      kty: "EC",
      crv: "P-521",
      x: "AeCLgRZ-BPqfhq4jt409-E26VHW5l29q74cHbIbQiS_-Gcqdo-087jHdPXUksGpr" +
         "Nyp_RcTZd94t3peXzQziQIqo",
      y: "AZIAp8QVnU9hBOkLScv0dy540uGtBWHkWj4DGh-Exh4iWZ0E-YBS8-HVx2eB-nfG" +
         "AGEy4-BzfpFFlfidOS1Tg77J"
    },

    jwk_priv: {
      kty: "EC",
      crv: "P-521",
      d: "ABtsfkDGFarQU4kb7e2gPszGCTT8GLDaaJbFQenFZce3qp_dh0qZarXHKBZ-BVic" +
         "NeIW5Sk661UoNfwykSvmh77S",
      x: "AcD_6Eb4A-8QdUM70c6F0WthN1kvV4fohS8QHbod6B4y1ZDU54mQuCR-3IBjcV1c" +
         "oh18uxbyUn5szMuCgjZUiD0y",
      y: "AU3WKJffztkhAQetBXaLvUSIHa87HMn8vZFB04lWipH-SrsrAu_4N-6iam0OD4EJ" +
         "0kOMH8iEh7yuivaKsFRzm2-m"
    },

    secret: util.hex2abv(
      "00561eb17d856552c21b8cbe7d3d60d1ea0db738b77d4050fa2dbd0773edc395" +
      "09854d9e30e843964ed3fd303339e338f31289120a38f94e9dc9ff7d4b3ea8f2" +
      "5e01"
    )
  },

  // Some test vectors that we should fail to import.
  ecdh_p256_negative: {
    // The given curve doesn't exist / isn't supported.
    jwk_bad_crv: {
      kty: "EC",
      crv: "P-123",
      x: "XOe4bjsyZgQD5jcS7wmY3q4QJ_rsPBvp92-TTf61jpg",
      y: "9M8HWzlAXdHxresJAQftz7K0ljc52HZ54wVssFV9Ct8"
    },

    // The crv parameter is missing.
    jwk_missing_crv: {
      kty: "EC",
      x: "XOe4bjsyZgQD5jcS7wmY3q4QJ_rsPBvp92-TTf61jpg",
      y: "9M8HWzlAXdHxresJAQftz7K0ljc52HZ54wVssFV9Ct8"
    },

    // The X coordinate is missing.
    jwk_missing_x: {
      kty: "EC",
      crv: "P-256",
      y: "9M8HWzlAXdHxresJAQftz7K0ljc52HZ54wVssFV9Ct8"
    },

    // The Y coordinate is missing.
    jwk_missing_y: {
      kty: "EC",
      crv: "P-256",
      x: "XOe4bjsyZgQD5jcS7wmY3q4QJ_rsPBvp92-TTf61jpg",
    },

    // Public point with Y not on the curve.
    raw_bad: util.hex2abv(
      "045ce7b86e3b32660403e63712ef0998deae1027faec3c1be9f76f934dfeb58e" +
      "98f4cf075b39405dd1f1adeb090106edcfb2b4963739d87679e3056cb0557d0adf"
    ),

    // Public point with Y a little too short.
    raw_short: util.hex2abv(
      "045ce7b86e3b32660403e63712ef0998deae1027faec3c1be9f76f934dfeb58e" +
      "98f4cf075b39405dd1f1adeb090107edcfb2b4963739d87679e3056cb0557d0a"
    ),

    // Public point with Y a little too long.
    raw_long: util.hex2abv(
      "045ce7b86e3b32660403e63712ef0998deae1027faec3c1be9f76f934dfeb58e" +
      "98f4cf075b39405dd1f1adeb090107edcfb2b4963739d87679e3056cb0557d0adfff"
    ),

    // Public point with EC_POINT_FORM_COMPRESSED_Y0.
    raw_compressed: util.hex2abv(
      "025ce7b86e3b32660403e63712ef0998deae1027faec3c1be9f76f934dfeb58e" +
      "98f4cf075b39405dd1f1adeb090107edcfb2b4963739d87679e3056cb0557d0adf"
    )
  },

  // NIST ECDSA test vectors
  // http://csrc.nist.gov/groups/STM/cavp/index.html
  ecdsa_verify: {
    pub_jwk: {
      "kty": "EC",
      "crv": "P-521",

      // 0061387fd6b95914e885f912edfbb5fb274655027f216c4091ca83e19336740fd8
      // 1aedfe047f51b42bdf68161121013e0d55b117a14e4303f926c8debb77a7fdaad1
      "x": "AGE4f9a5WRTohfkS7fu1-ydGVQJ_IWxAkcqD4ZM2dA_Y" +
           "Gu3-BH9RtCvfaBYRIQE-DVWxF6FOQwP5Jsjeu3en_arR",
      // 00e7d0c75c38626e895ca21526b9f9fdf84dcecb93f2b233390550d2b1463b7ee3
      // f58df7346435ff0434199583c97c665a97f12f706f2357da4b40288def888e59e6
      "y": "AOfQx1w4Ym6JXKIVJrn5_fhNzsuT8rIzOQVQ0rFGO37j" +
           "9Y33NGQ1_wQ0GZWDyXxmWpfxL3BvI1faS0Aoje-Ijlnm",
    },

    raw: util.hex2abv(
      "040061387fd6b95914e885f912edfbb5fb274655027f216c4091ca83e19336740fd" +
      "81aedfe047f51b42bdf68161121013e0d55b117a14e4303f926c8debb77a7fdaad1" +
      "00e7d0c75c38626e895ca21526b9f9fdf84dcecb93f2b233390550d2b1463b7ee3f" +
      "58df7346435ff0434199583c97c665a97f12f706f2357da4b40288def888e59e6"
    ),

    "data": util.hex2abv(
            "9ecd500c60e701404922e58ab20cc002651fdee7cbc9336adda33e4c1088fab1" +
            "964ecb7904dc6856865d6c8e15041ccf2d5ac302e99d346ff2f686531d255216" +
            "78d4fd3f76bbf2c893d246cb4d7693792fe18172108146853103a51f824acc62" +
            "1cb7311d2463c3361ea707254f2b052bc22cb8012873dcbb95bf1a5cc53ab89f"
          ),
    "sig": util.hex2abv(
            "004de826ea704ad10bc0f7538af8a3843f284f55c8b946af9235af5af74f2b76e0" +
            "99e4bc72fd79d28a380f8d4b4c919ac290d248c37983ba05aea42e2dd79fdd33e8" +
            "0087488c859a96fea266ea13bf6d114c429b163be97a57559086edb64aed4a1859" +
            "4b46fb9efc7fd25d8b2de8f09ca0587f54bd287299f47b2ff124aac566e8ee3b43"
          ),

    // Same as "sig", but with the last few octets set to 0
    "sig_tampered": util.hex2abv(
            "004de826ea704ad10bc0f7538af8a3843f284f55c8b946af9235af5af74f2b76e0" +
            "99e4bc72fd79d28a380f8d4b4c919ac290d248c37983ba05aea42e2dd79fdd33e8" +
            "0087488c859a96fea266ea13bf6d114c429b163be97a57559086edb64aed4a1859" +
            "4b46fb9efc7fd25d8b2de8f09ca0587f54bd287299f47b2ff124aac56600000000"
          )
  },

  ecdsa_bad: {
    pub_jwk: {
      "kty": "EC",
      "crv": "P-521",
      "x": "BhOH_WuVkU6IX5Eu37tfsnRlUCfyFsQJHKg-GTNnQP2B" +
           "rt_gR_UbQr32gWESEBPg1VsRehTkMD-SbI3rt3p_2q0B",
      "y": "AUNouOdGgHsraPNhXNeNdhpGTd15GPyN9R0iWWL98ePc" +
           "JD4mUQD/DsEzNZ4zLkTdSa/Y5fOP6GEzVzQy0zwC+goD"
    }
  },

  // RFC 2409 <http://tools.ietf.org/html/rfc2409#section-6.2>
  dh: {
    prime: util.hex2abv(
      "ffffffffffffffffc90fdaa22168c234c4c6628b80dc1cd129024e088a67cc74" +
      "020bbea63b139b22514a08798e3404ddef9519b3cd3a431b302b0a6df25f1437" +
      "4fe1356d6d51c245e485b576625e7ec6f44c42e9a637ed6b0bff5cb6f406b7ed" +
      "ee386bfb5a899fa5ae9f24117c4b1fe649286651ece65381ffffffffffffffff"
    ),

    prime2: util.hex2abv(
      "8b79f180cbd3f282de92e8b8f2d092674ffda61f01ed961f8ef04a1b7a3709ff" +
      "748c2abf6226cf0c4538e48838193da456e92ee530ef7aa703e741585e475b26" +
      "cd64fa97819181cef27de2449cd385c49c9b030f89873b5b7eaf063a788f00db" +
      "3cb670c73846bc4f76af062d672bde8f29806b81548411ab48b99aebfd9c2d09"
    ),
  },

  // KASValidityTest_FFCStatic_NOKC_ZZOnly_resp.fax [FA]
  // <http://csrc.nist.gov/groups/STM/cavp/documents/keymgmt/kastestvectors.zip>
  dh_nist: {
    prime: util.hex2abv(
      "8b79f180cbd3f282de92e8b8f2d092674ffda61f01ed961f8ef04a1b7a3709ff" +
      "748c2abf6226cf0c4538e48838193da456e92ee530ef7aa703e741585e475b26" +
      "cd64fa97819181cef27de2449cd385c49c9b030f89873b5b7eaf063a788f00db" +
      "3cb670c73846bc4f76af062d672bde8f29806b81548411ab48b99aebfd9c2d09"
    ),

    gen: util.hex2abv(
      "029843c81d0ea285c41a49b1a2f8e11a56a4b39040dfbc5ec040150c16f72f87" +
      "4152f9c44c659d86f7717b2425b62597e9a453b13da327a31cde2cced6009152" +
      "52d30262d1e54f4f864ace0e484f98abdbb37ebb0ba4106af5f0935b744677fa" +
      "2f7f3826dcef3a1586956105ebea805d871f34c46c25bc30fc66b2db26cb0a93"
    ),

    raw: util.hex2abv(
      "4fc9904887ac7fabff87f054003547c2d9458c1f6f584c140d7271f8b266bb39" +
      "0af7e3f625a629bec9c6a057a4cbe1a556d5e3eb2ff1c6ff677a08b0c7c50911" +
      "0b9e7c6dbc961ca4360362d3dbcffc5bf2bb7207e0a5922f77cf5464b316aa49" +
      "fb62b338ebcdb30bf573d07b663bb7777b69d6317df0a4f636ba3d9acbf9e8ac"
    ),

    spki: util.hex2abv(
      "308201a33082011806092a864886f70d01030130820109028181008b79f180cb" +
      "d3f282de92e8b8f2d092674ffda61f01ed961f8ef04a1b7a3709ff748c2abf62" +
      "26cf0c4538e48838193da456e92ee530ef7aa703e741585e475b26cd64fa9781" +
      "9181cef27de2449cd385c49c9b030f89873b5b7eaf063a788f00db3cb670c738" +
      "46bc4f76af062d672bde8f29806b81548411ab48b99aebfd9c2d090281800298" +
      "43c81d0ea285c41a49b1a2f8e11a56a4b39040dfbc5ec040150c16f72f874152" +
      "f9c44c659d86f7717b2425b62597e9a453b13da327a31cde2cced600915252d3" +
      "0262d1e54f4f864ace0e484f98abdbb37ebb0ba4106af5f0935b744677fa2f7f" +
      "3826dcef3a1586956105ebea805d871f34c46c25bc30fc66b2db26cb0a930000" +
      "038184000281804fc9904887ac7fabff87f054003547c2d9458c1f6f584c140d" +
      "7271f8b266bb390af7e3f625a629bec9c6a057a4cbe1a556d5e3eb2ff1c6ff67" +
      "7a08b0c7c509110b9e7c6dbc961ca4360362d3dbcffc5bf2bb7207e0a5922f77" +
      "cf5464b316aa49fb62b338ebcdb30bf573d07b663bb7777b69d6317df0a4f636" +
      "ba3d9acbf9e8ac"
    )
  },

  // Taken from appendix A of RFC 5869.
  // <https://tools.ietf.org/html/rfc5869>
  hkdf: [
    {
      prf: "SHA-256",
      key: util.hex2abv("0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b"),
      salt: util.hex2abv("000102030405060708090a0b0c"),
      info: util.hex2abv("f0f1f2f3f4f5f6f7f8f9"),
      data: util.hex2abv(
        "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf" +
        "34007208d5b887185865"
      ),
      jwk: {
        kty: "oct",
        k: "CwsLCwsLCwsLCwsLCwsLCwsLCwsLCw"
      }
    },
    {
      prf: "SHA-256",
      key: util.hex2abv(
        "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f" +
        "202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f" +
        "404142434445464748494a4b4c4d4e4f"
      ),
      salt: util.hex2abv(
        "606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f" +
        "808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f" +
        "a0a1a2a3a4a5a6a7a8a9aaabacadaeaf"
      ),
      info: util.hex2abv(
        "b0b1b2b3b4b5b6b7b8b9babbbcbdbebfc0c1c2c3c4c5c6c7c8c9cacbcccdcecf" +
        "d0d1d2d3d4d5d6d7d8d9dadbdcdddedfe0e1e2e3e4e5e6e7e8e9eaebecedeeef" +
        "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff"
      ),
      data: util.hex2abv(
        "b11e398dc80327a1c8e7f78c596a49344f012eda2d4efad8a050cc4c19afa97c" +
        "59045a99cac7827271cb41c65e590e09da3275600c2f09b8367793a9aca3db71" +
        "cc30c58179ec3e87c14c01d5c1f3434f1d87"
      )
    },
    {
      prf: "SHA-256",
      key: util.hex2abv("0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b"),
      salt: util.hex2abv(""),
      info: util.hex2abv(""),
      data: util.hex2abv(
        "8da4e775a563c18f715f802a063c5a31b8a11f5c5ee1879ec3454e5f3c738d2d" +
        "9d201395faa4b61a96c8"
      )
    },
    {
      prf: "SHA-1",
      key: util.hex2abv("0b0b0b0b0b0b0b0b0b0b0b"),
      salt: util.hex2abv("000102030405060708090a0b0c"),
      info: util.hex2abv("f0f1f2f3f4f5f6f7f8f9"),
      data: util.hex2abv(
        "085a01ea1b10f36933068b56efa5ad81a4f14b822f5b091568a9cdd4f155fda2" +
        "c22e422478d305f3f896"
      )
    },
    {
      prf: "SHA-1",
      key: util.hex2abv(
        "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f" +
        "202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f" +
        "404142434445464748494a4b4c4d4e4f"
      ),
      salt: util.hex2abv(
        "606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f" +
        "808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f" +
        "a0a1a2a3a4a5a6a7a8a9aaabacadaeaf"
      ),
      info: util.hex2abv(
        "b0b1b2b3b4b5b6b7b8b9babbbcbdbebfc0c1c2c3c4c5c6c7c8c9cacbcccdcecf" +
        "d0d1d2d3d4d5d6d7d8d9dadbdcdddedfe0e1e2e3e4e5e6e7e8e9eaebecedeeef" +
        "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff"
      ),
      data: util.hex2abv(
        "0bd770a74d1160f7c9f12cd5912a06ebff6adcae899d92191fe4305673ba2ffe" +
        "8fa3f1a4e5ad79f3f334b3b202b2173c486ea37ce3d397ed034c7f9dfeb15c5e" +
        "927336d0441f4c4300e2cff0d0900b52d3b4"
      )
    },
    {
      prf: "SHA-1",
      key: util.hex2abv("0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b"),
      salt: util.hex2abv(""),
      info: util.hex2abv(""),
      data: util.hex2abv(
        "0ac1af7002b3d761d1e55298da9d0506b9ae52057220a306e07b6b87e8df21d0" +
        "ea00033de03984d34918"
      )
    },
    {
      prf: "SHA-1",
      key: util.hex2abv("0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c"),
      salt: util.hex2abv(""),
      info: util.hex2abv(""),
      data: util.hex2abv(
        "2c91117204d745f3500d636a62f64f0ab3bae548aa53d423b0d1f27ebba6f5e5" +
        "673a081d70cce7acfc48"
      )
    }
  ]
}
