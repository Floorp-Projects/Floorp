'use strict';

const {
  getCryptoParams,
  PushCrypto,
} = Cu.import('resource://gre/modules/PushCrypto.jsm', {});

function run_test() {
  run_next_test();
}

add_task(function* test_crypto_getCryptoParams() {
  let testData = [
  // These headers should parse correctly.
  {
    desc: 'aesgcm with multiple keys',
    headers: {
      encoding: 'aesgcm',
      crypto_key: 'keyid=p256dh;dh=Iy1Je2Kv11A,p256ecdsa=o2M8QfiEKuI',
      encryption: 'keyid=p256dh;salt=upk1yFkp1xI',
    },
    params: {
      dh: 'Iy1Je2Kv11A',
      salt: 'upk1yFkp1xI',
      rs: 4096,
      padSize: 2,
    },
  }, {
    desc: 'aesgcm with quoted key param',
    headers: {
      encoding: 'aesgcm',
      crypto_key: 'dh="byfHbUffc-k"',
      encryption: 'salt=C11AvAsp6Gc',
    },
    params: {
      dh: 'byfHbUffc-k',
      salt: 'C11AvAsp6Gc',
      rs: 4096,
      padSize: 2,
    },
  }, {
    desc: 'aesgcm with Crypto-Key and rs = 24',
    headers: {
      encoding: 'aesgcm',
      crypto_key: 'dh="ybuT4VDz-Bg"',
      encryption: 'salt=H7U7wcIoIKs; rs=24',
    },
    params: {
      dh: 'ybuT4VDz-Bg',
      salt: 'H7U7wcIoIKs',
      rs: 24,
      padSize: 2,
    },
  }, {
    desc: 'aesgcm128 with Encryption-Key and rs = 2',
    headers: {
      encoding: 'aesgcm128',
      encryption_key: 'keyid=legacy; dh=LqrDQuVl9lY',
      encryption: 'keyid=legacy; salt=YngI8B7YapM; rs=2',
    },
    params: {
      dh: 'LqrDQuVl9lY',
      salt: 'YngI8B7YapM',
      rs: 2,
      padSize: 1,
    },
  }, {
    desc: 'aesgcm128 with Encryption-Key',
    headers: {
      encoding: 'aesgcm128',
      encryption_key: 'keyid=v2; dh=VA6wmY1IpiE',
      encryption: 'keyid=v2; salt=khtpyXhpDKM',
    },
    params: {
      dh: 'VA6wmY1IpiE',
      salt: 'khtpyXhpDKM',
      rs: 4096,
      padSize: 1,
    }
  },

  // These headers should be rejected.
  {
    desc: 'aesgcm128 with Crypto-Key',
    headers: {
      encoding: 'aesgcm128',
      crypto_key: 'keyid=v2; dh=VA6wmY1IpiE',
      encryption: 'keyid=v2; salt=F0Im7RtGgNY',
    },
    params: null,
  },
  {
    desc: 'Invalid encoding',
    headers: {
      encoding: 'nonexistent',
    },
    params: null,
  }, {
    desc: 'Invalid record size',
    headers: {
      encoding: 'aesgcm',
      crypto_key: 'dh=pbmv1QkcEDY',
      encryption: 'dh=Esao8aTBfIk;rs=bad',
    },
    params: null,
  }, {
    desc: 'Insufficiently large record size',
    headers: {
      encoding: 'aesgcm',
      crypto_key: 'dh=fK0EXaw5IU8',
      encryption: 'salt=orbLLmlbJfM;rs=1',
    },
    params: null,
  }, {
    desc: 'aesgcm with Encryption-Key',
    headers: {
      encoding: 'aesgcm',
      encryption_key: 'dh=FplK5KkvUF0',
      encryption: 'salt=p6YHhFF3BQY',
    },
    params: null,
  }];

  for (let test of testData) {
    let params = getCryptoParams(test.headers);
    deepEqual(params, test.params, test.desc);
  }
});

add_task(function* test_crypto_decodeMsg() {
  let privateKey = {
    crv: 'P-256',
    d: '4h23G_KkXC9TvBSK2v0Q7ImpS2YAuRd8hQyN0rFAwBg',
    ext: true,
    key_ops: ['deriveBits'],
    kty: 'EC',
    x: 'sd85ZCbEG6dEkGMCmDyGBIt454Qy-Yo-1xhbaT2Jlk4',
    y: 'vr3cKpQ-Sp1kpZ9HipNjUCwSA55yy0uM8N9byE8dmLs',
  };
  let publicKey = ChromeUtils.base64URLDecode('BLHfOWQmxBunRJBjApg8hgSLeOeEMvmKPtcYW2k9iZZOvr3cKpQ-Sp1kpZ9HipNjUCwSA55yy0uM8N9byE8dmLs', {
    padding: "reject",
  });

  let expectedSuccesses = [{
    desc: 'padSize = 2, rs = 24, pad = 0',
    result: 'Some message',
    data: 'Oo34w2F9VVnTMFfKtdx48AZWQ9Li9M6DauWJVgXU',
    senderPublicKey: 'BCHFVrflyxibGLlgztLwKelsRZp4gqX3tNfAKFaxAcBhpvYeN1yIUMrxaDKiLh4LNKPtj0BOXGdr-IQ-QP82Wjo',
    salt: 'zCU18Rw3A5aB_Xi-vfixmA',
    rs: 24,
    authSecret: 'aTDc6JebzR6eScy2oLo4RQ',
    padSize: 2,
  }, {
    desc: 'padSize = 2, rs = 8, pad = 16',
    result: 'Yet another message',
    data: 'uEC5B_tR-fuQ3delQcrzrDCp40W6ipMZjGZ78USDJ5sMj-6bAOVG3AK6JqFl9E6AoWiBYYvMZfwThVxmDnw6RHtVeLKFM5DWgl1EwkOohwH2EhiDD0gM3io-d79WKzOPZE9rDWUSv64JstImSfX_ADQfABrvbZkeaWxh53EG59QMOElFJqHue4dMURpsMXg',
    senderPublicKey: 'BEaA4gzA3i0JDuirGhiLgymS4hfFX7TNTdEhSk_HBlLpkjgCpjPL5c-GL9uBGIfa_fhGNKKFhXz1k9Kyens2ZpQ',
    salt: 'ZFhzj0S-n29g9P2p4-I7tA',
    rs: 8,
    authSecret: '6plwZnSpVUbF7APDXus3UQ',
    padSize: 2,
  }, {
    desc: 'padSize = 1, rs = 4096, pad = 2',
    result: 'aesgcm128 encrypted message',
    data: 'ljBJ44NPzJFH9EuyT5xWMU4vpZ90MdAqaq1TC1kOLRoPNHtNFXeJ0GtuSaE',
    senderPublicKey: 'BOmnfg02vNd6RZ7kXWWrCGFF92bI-rQ-bV0Pku3-KmlHwbGv4ejWqgasEdLGle5Rhmp6SKJunZw2l2HxKvrIjfI',
    salt: 'btxxUtclbmgcc30b9rT3Bg',
    rs: 4096,
    padSize: 1,
  }, {
    desc: 'padSize = 2, rs = 3, pad = 0',
    result: 'Small record size',
    data: 'oY4e5eDatDVt2fpQylxbPJM-3vrfhDasfPc8Q1PWt4tPfMVbz_sDNL_cvr0DXXkdFzS1lxsJsj550USx4MMl01ihjImXCjrw9R5xFgFrCAqJD3GwXA1vzS4T5yvGVbUp3SndMDdT1OCcEofTn7VC6xZ-zP8rzSQfDCBBxmPU7OISzr8Z4HyzFCGJeBfqiZ7yUfNlKF1x5UaZ4X6iU_TXx5KlQy_toV1dXZ2eEAMHJUcSdArvB6zRpFdEIxdcHcJyo1BIYgAYTDdAIy__IJVCPY_b2CE5W_6ohlYKB7xDyH8giNuWWXAgBozUfScLUVjPC38yJTpAUi6w6pXgXUWffende5FreQpnMFL1L4G-38wsI_-ISIOzdO8QIrXHxmtc1S5xzYu8bMqSgCinvCEwdeGFCmighRjj8t1zRWo0D14rHbQLPR_b1P5SvEeJTtS9Nm3iibM',
    senderPublicKey: 'BCg6ZIGuE2ZNm2ti6Arf4CDVD_8--aLXAGLYhpghwjl1xxVjTLLpb7zihuEOGGbyt8Qj0_fYHBP4ObxwJNl56bk',
    salt: '5LIDBXbvkBvvb7ZdD-T4PQ',
    rs: 3,
    authSecret: 'g2rWVHUCpUxgcL9Tz7vyeQ',
    padSize: 2,
  }];
  for (let test of expectedSuccesses) {
    let authSecret = test.authSecret ? ChromeUtils.base64URLDecode(test.authSecret, {
      padding: "reject",
    }) : null;
    let data = ChromeUtils.base64URLDecode(test.data, {
      padding: "reject",
    });
    let result = yield PushCrypto.decodeMsg(data,
                                            privateKey, publicKey,
                                            test.senderPublicKey, test.salt,
                                            test.rs, authSecret, test.padSize);
    let decoder = new TextDecoder('utf-8');
    equal(decoder.decode(new Uint8Array(result)), test.result, test.desc);
  }

  let expectedFailures = [{
    desc: 'padSize = 1, rs = 4096, auth secret, pad = 8',
    data: 'h0FmyldY8aT5EQ6CJrbfRn_IdDvytoLeHb9_q5CjtdFRfgDRknxLmOzavLaVG4oOiS0r',
    senderPublicKey: 'BCXHk7O8CE-9AOp6xx7g7c-NCaNpns1PyyHpdcmDaijLbT6IdGq0ezGatBwtFc34BBfscFxdk4Tjksa2Mx5rRCM',
    salt: 'aGBpoKklLtrLcAUCcCr7JQ',
    rs: 4096,
    authSecret: 'Sxb6u0gJIhGEogyLawjmCw',
    padSize: 1,
  }, {
    desc: 'Missing padding',
    data: 'anvsHj7oBQTPMhv7XSJEsvyMS4-8EtbC7HgFZsKaTg',
    senderPublicKey: 'BMSqfc3ohqw2DDgu3nsMESagYGWubswQPGxrW1bAbYKD18dIHQBUmD3ul_lu7MyQiT5gNdzn5JTXQvCcpf-oZE4',
    salt: 'Czx2i18rar8XWOXAVDnUuw',
    rs: 4096,
    padSize: 1,
  }, {
    desc: 'padSize > rs',
    data: 'Ct_h1g7O55e6GvuhmpjLsGnv8Rmwvxgw8iDESNKGxk_8E99iHKDzdV8wJPyHA-6b2E6kzuVa5UWiQ7s4Zms1xzJ4FKgoxvBObXkc_r_d4mnb-j245z3AcvRmcYGk5_HZ0ci26SfhAN3lCgxGzTHS4nuHBRkGwOb4Tj4SFyBRlLoTh2jyVK2jYugNjH9tTrGOBg7lP5lajLTQlxOi91-RYZSfFhsLX3LrAkXuRoN7G1CdiI7Y3_eTgbPIPabDcLCnGzmFBTvoJSaQF17huMl_UnWoCj2WovA4BwK_TvWSbdgElNnQ4CbArJ1h9OqhDOphVu5GUGr94iitXRQR-fqKPMad0ULLjKQWZOnjuIdV1RYEZ873r62Yyd31HoveJcSDb1T8l_QK2zVF8V4k0xmK9hGuC0rF5YJPYPHgl5__usknzxMBnRrfV5_MOL5uPZwUEFsu',
    senderPublicKey: 'BAcMdWLJRGx-kPpeFtwqR3GE1LWzd1TYh2rg6CEFu53O-y3DNLkNe_BtGtKRR4f7ZqpBMVS6NgfE2NwNPm3Ndls',
    salt: 'NQVTKhB0rpL7ZzKkotTGlA',
    rs: 1,
    authSecret: '6plwZnSpVUbF7APDXus3UQ',
    padSize: 2,
  }, {
    desc: 'Encrypted with padSize = 1, decrypted with padSize = 2 and auth secret',
    data: 'fwkuwTTChcLnrzsbDI78Y2EoQzfnbMI8Ax9Z27_rwX8',
    senderPublicKey: 'BCHn-I-J3dfPRLJBlNZ3xFoAqaBLZ6qdhpaz9W7Q00JW1oD-hTxyEECn6KYJNK8AxKUyIDwn6Icx_PYWJiEYjQ0',
    salt: 'c6JQl9eJ0VvwrUVCQDxY7Q',
    rs: 4096,
    authSecret: 'BhbpNTWyO5wVJmVKTV6XaA',
    padSize: 2,
  }, {
    desc: 'Truncated input',
    data: 'AlDjj6NvT5HGyrHbT8M5D6XBFSra6xrWS9B2ROaCIjwSu3RyZ1iyuv0',
    rs: 25,
  }];
  for (let test of expectedFailures) {
    let authSecret = test.authSecret ? ChromeUtils.base64URLDecode(test.authSecret, {
      padding: "reject",
    }) : null;
    let data = ChromeUtils.base64URLDecode(test.data, {
      padding: "reject",
    });
    yield rejects(
      PushCrypto.decodeMsg(data, privateKey, publicKey,
                           test.senderPublicKey, test.salt, test.rs,
                           authSecret, test.padSize),
      test.desc
    );
  }
});
