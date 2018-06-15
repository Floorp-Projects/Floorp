'use strict';

const {
  getCryptoParamsFromHeaders,
  PushCrypto,
} = ChromeUtils.import('resource://gre/modules/PushCrypto.jsm', {});

const REJECT_PADDING = { padding: 'reject' };

// A common key to decrypt some aesgcm and aesgcm128 messages. Other decryption
// tests have their own keys.
const LEGACY_PRIVATE_KEY = {
  kty: 'EC',
  crv: 'P-256',
  d: '4h23G_KkXC9TvBSK2v0Q7ImpS2YAuRd8hQyN0rFAwBg',
  x: 'sd85ZCbEG6dEkGMCmDyGBIt454Qy-Yo-1xhbaT2Jlk4',
  y: 'vr3cKpQ-Sp1kpZ9HipNjUCwSA55yy0uM8N9byE8dmLs',
  ext: true,
};

const LEGACY_PUBLIC_KEY = 'BLHfOWQmxBunRJBjApg8hgSLeOeEMvmKPtcYW2k9iZZOvr3cKpQ-Sp1kpZ9HipNjUCwSA55yy0uM8N9byE8dmLs';

async function assertDecrypts(test, headers) {
  let privateKey = test.privateKey;
  let publicKey = ChromeUtils.base64URLDecode(test.publicKey, REJECT_PADDING);
  let authSecret = null;
  if (test.authSecret) {
    authSecret = ChromeUtils.base64URLDecode(test.authSecret, REJECT_PADDING);
  }
  let payload = ChromeUtils.base64URLDecode(test.data, REJECT_PADDING);
  let result = await PushCrypto.decrypt(privateKey, publicKey, authSecret,
                                        headers, payload);
  let decoder = new TextDecoder('utf-8');
  equal(decoder.decode(new Uint8Array(result)), test.result, test.desc);
}

async function assertNotDecrypts(test, headers) {
  let authSecret = null;
  if (test.authSecret) {
    authSecret = ChromeUtils.base64URLDecode(test.authSecret, REJECT_PADDING);
  }
  let data = ChromeUtils.base64URLDecode(test.data, REJECT_PADDING);
  let publicKey = ChromeUtils.base64URLDecode(test.publicKey, REJECT_PADDING);
  let promise = PushCrypto.decrypt(test.privateKey, publicKey, authSecret,
                                   headers, data);
  await rejects(promise, test.expected, test.desc);
}

add_task(async function test_crypto_getCryptoParamsFromHeaders() {
  // These headers should parse correctly.
  let shouldParse = [{
    desc: 'aesgcm with multiple keys',
    headers: {
      encoding: 'aesgcm',
      crypto_key: 'keyid=p256dh;dh=Iy1Je2Kv11A,p256ecdsa=o2M8QfiEKuI',
      encryption: 'keyid=p256dh;salt=upk1yFkp1xI',
    },
    params: {
      senderKey: 'Iy1Je2Kv11A',
      salt: 'upk1yFkp1xI',
      rs: 4096,
    },
  }, {
    desc: 'aesgcm with quoted key param',
    headers: {
      encoding: 'aesgcm',
      crypto_key: 'dh="byfHbUffc-k"',
      encryption: 'salt=C11AvAsp6Gc',
    },
    params: {
      senderKey: 'byfHbUffc-k',
      salt: 'C11AvAsp6Gc',
      rs: 4096,
    },
  }, {
    desc: 'aesgcm with Crypto-Key and rs = 24',
    headers: {
      encoding: 'aesgcm',
      crypto_key: 'dh="ybuT4VDz-Bg"',
      encryption: 'salt=H7U7wcIoIKs; rs=24',
    },
    params: {
      senderKey: 'ybuT4VDz-Bg',
      salt: 'H7U7wcIoIKs',
      rs: 24,
    },
  }, {
    desc: 'aesgcm128 with Encryption-Key and rs = 2',
    headers: {
      encoding: 'aesgcm128',
      encryption_key: 'keyid=legacy; dh=LqrDQuVl9lY',
      encryption: 'keyid=legacy; salt=YngI8B7YapM; rs=2',
    },
    params: {
      senderKey: 'LqrDQuVl9lY',
      salt: 'YngI8B7YapM',
      rs: 2,
    },
  }, {
    desc: 'aesgcm128 with Encryption-Key',
    headers: {
      encoding: 'aesgcm128',
      encryption_key: 'keyid=v2; dh=VA6wmY1IpiE',
      encryption: 'keyid=v2; salt=khtpyXhpDKM',
    },
    params: {
      senderKey: 'VA6wmY1IpiE',
      salt: 'khtpyXhpDKM',
      rs: 4096,
    }
  }];
  for (let test of shouldParse) {
    let params = getCryptoParamsFromHeaders(test.headers);
    let senderKey = ChromeUtils.base64URLDecode(test.params.senderKey,
                                                REJECT_PADDING);
    let salt = ChromeUtils.base64URLDecode(test.params.salt, REJECT_PADDING);
    deepEqual(new Uint8Array(params.senderKey), new Uint8Array(senderKey),
      "Sender key should match for " + test.desc);
    deepEqual(new Uint8Array(params.salt), new Uint8Array(salt),
      "Salt should match for " + test.desc);
    equal(params.rs, test.params.rs,
      "Record size should match for " + test.desc);
  }

  // These headers should be rejected.
  let shouldThrow = [{
    desc: 'aesgcm128 with Crypto-Key',
    headers: {
      encoding: 'aesgcm128',
      crypto_key: 'keyid=v2; dh=VA6wmY1IpiE',
      encryption: 'keyid=v2; salt=F0Im7RtGgNY',
    },
    exception: /Missing Encryption-Key header/,
  }, {
    desc: 'Invalid encoding',
    headers: {
      encoding: 'nonexistent',
    },
    exception: /Missing encryption header/,
  }, {
    desc: 'Invalid record size',
    headers: {
      encoding: 'aesgcm',
      crypto_key: 'dh=pbmv1QkcEDY',
      encryption: 'dh=Esao8aTBfIk;rs=bad',
    },
    exception: /Invalid salt parameter/,
  }, {
    desc: 'aesgcm with Encryption-Key',
    headers: {
      encoding: 'aesgcm',
      encryption_key: 'dh=FplK5KkvUF0',
      encryption: 'salt=p6YHhFF3BQY',
    },
    exception: /Missing Crypto-Key header/,
  }];
  for (let test of shouldThrow) {
    throws(() => getCryptoParamsFromHeaders(test.headers), test.exception, test.desc);
  }
});

add_task(async function test_aes128gcm_ok() {
  let expectedSuccesses = [{
    desc: 'Example from draft-ietf-webpush-encryption-latest',
    result: 'When I grow up, I want to be a watermelon',
    data: 'DGv6ra1nlYgDCS1FRnbzlwAAEABBBP4z9KsN6nGRTbVYI_c7VJSPQTBtkgcy27mlmlMoZIIgDll6e3vCYLocInmYWAmS6TlzAC8wEqKK6PBru3jl7A_yl95bQpu6cVPTpK4Mqgkf1CXztLVBSt2Ks3oZwbuwXPXLWyouBWLVWGNWQexSgSxsj_Qulcy4a-fN',
    authSecret: 'BTBZMqHH6r4Tts7J_aSIgg',
    privateKey: {
      kty: 'EC',
      crv: 'P-256',
      d: 'q1dXpw3UpT5VOmu_cf_v6ih07Aems3njxI-JWgLcM94',
      x: 'JXGyvs3942BVGq8e0PTNNmwRzr5VX4m8t7GGpTM5FzE',
      y: 'aOzi6-AYWXvTBHm4bjyPjs7Vd8pZGH6SRpkNtoIAiw4',
      ext: true,
    },
    publicKey: 'BCVxsr7N_eNgVRqvHtD0zTZsEc6-VV-JvLexhqUzORcxaOzi6-AYWXvTBHm4bjyPjs7Vd8pZGH6SRpkNtoIAiw4',
  }, {
    desc: 'rs = 24, pad = 0',
    result: "I am the very model of a modern Major-General; I've information vegetable, animal, and mineral",
    data: 'goagSH7PP0ZGwUsgShmdkwAAABhBBDJVyIuUJbOSVMeWHP8VNPnxY-dZSw86doqOkEzZZZY1ALBWVXTVf0rUDH3oi68I9Hrp-01zA-mr8XKWl5kcH8cX0KiV2PtCwdkEyaQ73YF5fsDxgoWDiaTA3wPqMvuLDqGsZWHnE9Psnfoy7UMEqKlh2a1nE7ZOXiXcOBHLNj260jYzSJnEPV2eXixSXfyWpaSJHAwfj4wVdAAocmViIg6ywk8wFB1hgJpnX2UVEU_qIOcaP6AOIOr1UUQPfosQqC2MEHe5u9gHXF5pi-E267LAlkoYefq01KV_xK_vjbxpw8GAYfSjQEm0L8FG-CN37c8pnQ2Yf61MkihaXac9OctfNeWq_22cN6hn4qsOq0F7QoWIiZqWhB1vS9cJ3KUlyPQvKI9cvevDxw0fJHWeTFzhuwT9BjdILjjb2Vkqc0-qTDOawqD4c8WXsvdGDQCec5Y1x3UhdQXdjR_mhXypxFM37OZTvKJBr1vPCpRXl-bI6iOd7KScgtMM1x5luKhGzZyz25HyuFyj1ec82A',
    authSecret: '_tK2LDGoIt6be6agJ_nvGA',
    privateKey: {
      kty: 'EC',
      crv: 'P-256',
      d: 'bGViEe3PvjjFJg8lcnLsqu71b2yqWGnZN9J2MTed-9s',
      x: 'auB0GHF0AZ2LAocFnvOXDS7EeCMopnzbg-tS21FMHrU',
      y: 'GpbhrW-_xKj3XhhXA-kDZSicKZ0kn0BuVhqzhLOB-Cc',
      ext: true,
    },
    publicKey: 'BGrgdBhxdAGdiwKHBZ7zlw0uxHgjKKZ824PrUttRTB61GpbhrW-_xKj3XhhXA-kDZSicKZ0kn0BuVhqzhLOB-Cc',
  }, {
    desc: 'rs = 49, pad = 84; ciphertext length falls on record boundary',
    result: 'Hello, world',
    data: '-yiDzsHE_K3W0TcfbqSR4AAAADFBBC1EHuf5_2oDKaZJJ9BST9vnsixvtl4Qq0_cA4-UQgoMo_oo2tNshOyRoWLq4Hj6rSwc7XjegRPhlgKyDolPSXa5c-L89oL6DIzNmvPVv_Ht4W-tWjHOGdOLXh_h94pPrYQrvBAlTCxs3ZaitVKE2XLFPK2MO6yxD19X6w1KQzO2BBAroRfK4pEI-9n2Kai6aWDdAZRbOe03unBsQ0oQ_SvSCU_5JJvNrUUTX1_kX804Bx-LLTlBr9pDmBDXeqyvfOULVDJb9YyVAzN9BzeFoyPfo0M',
    authSecret: 'lfF1cOUI72orKtG09creMw',
    privateKey: {
      kty: 'EC',
      crv: 'P-256',
      d: 'ZwBKTqgg3u2OSdtelIDmPT6jzOGujhpgYJcT1SfQAe8',
      x: 'AU6PFLktoHzgg7k_ljZ-h7IXpH9-8u6TqdNDqgY-V1o',
      y: 'nzDVnGkMajmz_IFbFQyn3RSWAXQTN7U1B6UfQbFzpyE',
      ext: true,
    },
    publicKey: 'BAFOjxS5LaB84IO5P5Y2foeyF6R_fvLuk6nTQ6oGPldanzDVnGkMajmz_IFbFQyn3RSWAXQTN7U1B6UfQbFzpyE',
  }, {
    desc: 'rs = 18, pad = 0',
    result: '1',
    data: 'fK69vCCTjuNAqUbxvU9o8QAAABJBBDfP21Ij2fleqgL27ZQP8i6vBbNiLpSdw86fM15u-bJq6qzKD3QICos2RZLyzMbV7d1DAEtwuRiH0UTZ-pPxbDvH6mj0_VR6lOyoSxbhOKYIAXc',
    authSecret: '1loE35Xy215gSDn3F9zeeQ',
    privateKey: {
      kty: 'EC',
      crv: 'P-256',
      d: 'J0M_q4lws8tShLYRg--0YoZWLNKnMw2MrpYJEaVXHQw',
      x: 'UV1DJjVWUjmdoksr6SQeYztc8U-vDPOm_WAxe5VMCi8',
      y: 'SEhUgASyewz3SAvIEMa-wDqPt5yOoA_IsF4A-INFY-8',
      ext: true,
    },
    publicKey: 'BFFdQyY1VlI5naJLK-kkHmM7XPFPrwzzpv1gMXuVTAovSEhUgASyewz3SAvIEMa-wDqPt5yOoA_IsF4A-INFY-8',
  }];
  for (let test of expectedSuccesses) {
    let privateKey = test.privateKey;
    let publicKey = ChromeUtils.base64URLDecode(test.publicKey, {
      padding: 'reject',
    });
    let authSecret = ChromeUtils.base64URLDecode(test.authSecret, {
      padding: 'reject',
    });
    let payload = ChromeUtils.base64URLDecode(test.data, {
      padding: 'reject',
    });
    let result = await PushCrypto.decrypt(privateKey, publicKey, authSecret, {
      encoding: 'aes128gcm',
    }, payload);
    let decoder = new TextDecoder('utf-8');
    equal(decoder.decode(new Uint8Array(result)), test.result, test.desc);
  }
});

add_task(async function test_aes128gcm_err() {
  let expectedFailures = [{
    // Just the payload; no header at all.
    desc: 'Missing header block',
    data: 'RbdNK2m-mwdN47NaqH58FWEd',
    privateKey: {
      kty: 'EC',
      crv: 'P-256',
      d: 'G-g_ODMu8JaB-vPzB7H_LhDKt4zHzatoOsDukqw_buE',
      x: '26mRyiFTQ_Nr3T6FfK_ePRi_V_GDWygzutQU8IhBYgU',
      y: 'GslqCyRJADfQfPUo5OGOEAoaZOt0R0hUS_HiINq6zyw',
      ext: true,
    },
    publicKey: 'BNupkcohU0Pza90-hXyv3j0Yv1fxg1soM7rUFPCIQWIFGslqCyRJADfQfPUo5OGOEAoaZOt0R0hUS_HiINq6zyw',
    authSecret: 'NHG7mEgeAlM785VCvPPbpA',
    expected: /Truncated header/,
  }, {
    // The sender key should be 65 bytes; this header contains an invalid key
    // that's only 1 byte.
    desc: 'Truncated sender key',
    data: '3ltpa4fxoVy2revdedb5ngAAABIBALa8GCbDfJ9z3WtIWcK1BRgZUg',
    privateKey: {
      kty: 'EC',
      crv: 'P-256',
      d: 'zojo4LMFekdS60yPqTHrYhwwLaWtA7ga9FnPZzVWDK4',
      x: 'oyXZkITEDeDOcioELESNlKMmkXIcp54890XnjGmIYZQ',
      y: 'sCzqGSJBdnlanU27sgc68szW-m8KTHxJaFVr5QKjuoE',
      ext: true,
    },
    publicKey: 'BKMl2ZCExA3gznIqBCxEjZSjJpFyHKeePPdF54xpiGGUsCzqGSJBdnlanU27sgc68szW-m8KTHxJaFVr5QKjuoE',
    authSecret: 'XDHg2W2aE5iZrAlp01n3QA',
    expected: /Invalid sender public key/,
  }, {
    // The message is encrypted with only the first 12 bytes of the 16-byte
    // auth secret, so the derived decryption key and nonce won't match.
    desc: 'Encrypted with mismatched auth secret',
    data: 'gRX0mIuMOSp7rLQ8jxrFZQAAABJBBBmUSDxUHpvDmmrwP_cTqndFwoThOKQqJDW3l7IMS2mM9RGLT4VVMXwZDqvr-rdJwWTT9r3r4NRBcZExo1fYiQoTxNvUsW_z3VqD98ka1uBArEJzCn8LPNMkXp-Nb_McdR1BDP0',
    privateKey: {
      kty: 'EC',
      crv: 'P-256',
      d: 'YMdjalF95wOaCsLQ4wZEAHlMeOfgSTmBKaInzuD5qAE',
      x: '_dBBKKhcBYltf4H-EYvcuIe588H_QYOtxMgk0ShgcwA',
      y: '6Yay37WmEOWvQ-QIoAcwWE-T49_d_ERzfV8I-y1viRY',
      ext: true,
    },
    publicKey: 'BP3QQSioXAWJbX-B_hGL3LiHufPB_0GDrcTIJNEoYHMA6Yay37WmEOWvQ-QIoAcwWE-T49_d_ERzfV8I-y1viRY',
    authSecret: 'NVo4zW2b7xWZDi0zCNvWAA',
    expected: /Bad encryption/,
  }, {
    // Multiple records; the first has padding delimiter = 2, but should be 1.
    desc: 'Early final record',
    data: '2-IVUH0a09Lq6r6ubodNjwAAABJBBHvEND80qDSM3E5GL_x8QKpqjGGnOcTEHUUSVQX3Dp_F-e-oaFLdSI3Pjo6iyvt14Hq9XufJ1cA4uv7weVcbC9opRBHOmMdt0DHA5YBXekmAo3XkXtMEKb4OLunafm34aW0BuOw',
    privateKey: {
      kty: 'EC',
      crv: 'P-256',
      d: 'XdodkYvEB7o82hLLgBTUmqfgJpACggMERmvIADTKkkA',
      x: 'yVxlINrRHo9qG_gDGkDCpO4QRcGQO-BqHfp_gpzOst4',
      y: 'Akga5r0EdhIbEsVTLQsjF4gHfvoGg6W_4NYjObJRyzU',
      ext: true,
    },
    publicKey: 'BMlcZSDa0R6Pahv4AxpAwqTuEEXBkDvgah36f4KczrLeAkga5r0EdhIbEsVTLQsjF4gHfvoGg6W_4NYjObJRyzU',
    authSecret: 'QMJB_eQmnuHm1yVZLZgnGA',
    expected: /Padding is wrong!/,
  }];
  for (let test of expectedFailures) {
    await assertNotDecrypts(test, { encoding: 'aes128gcm' });
  }
});

add_task(async function test_aesgcm_ok() {
  let expectedSuccesses = [{
    desc: 'padSize = 2, rs = 24, pad = 0',
    result: 'Some message',
    data: 'Oo34w2F9VVnTMFfKtdx48AZWQ9Li9M6DauWJVgXU',
    authSecret: 'aTDc6JebzR6eScy2oLo4RQ',
    privateKey: LEGACY_PRIVATE_KEY,
    publicKey: LEGACY_PUBLIC_KEY,
    headers: {
      crypto_key: 'dh=BCHFVrflyxibGLlgztLwKelsRZp4gqX3tNfAKFaxAcBhpvYeN1yIUMrxaDKiLh4LNKPtj0BOXGdr-IQ-QP82Wjo',
      encryption: 'salt=zCU18Rw3A5aB_Xi-vfixmA; rs=24',
      encoding: 'aesgcm',
    },
  }, {
    desc: 'padSize = 2, rs = 8, pad = 16',
    result: 'Yet another message',
    data: 'uEC5B_tR-fuQ3delQcrzrDCp40W6ipMZjGZ78USDJ5sMj-6bAOVG3AK6JqFl9E6AoWiBYYvMZfwThVxmDnw6RHtVeLKFM5DWgl1EwkOohwH2EhiDD0gM3io-d79WKzOPZE9rDWUSv64JstImSfX_ADQfABrvbZkeaWxh53EG59QMOElFJqHue4dMURpsMXg',
    authSecret: '6plwZnSpVUbF7APDXus3UQ',
    privateKey: LEGACY_PRIVATE_KEY,
    publicKey: LEGACY_PUBLIC_KEY,
    headers: {
      crypto_key: 'dh=BEaA4gzA3i0JDuirGhiLgymS4hfFX7TNTdEhSk_HBlLpkjgCpjPL5c-GL9uBGIfa_fhGNKKFhXz1k9Kyens2ZpQ',
      encryption: 'salt=ZFhzj0S-n29g9P2p4-I7tA; rs=8',
      encoding: 'aesgcm',
    },
  }, {
    desc: 'padSize = 2, rs = 3, pad = 0',
    result: 'Small record size',
    data: 'oY4e5eDatDVt2fpQylxbPJM-3vrfhDasfPc8Q1PWt4tPfMVbz_sDNL_cvr0DXXkdFzS1lxsJsj550USx4MMl01ihjImXCjrw9R5xFgFrCAqJD3GwXA1vzS4T5yvGVbUp3SndMDdT1OCcEofTn7VC6xZ-zP8rzSQfDCBBxmPU7OISzr8Z4HyzFCGJeBfqiZ7yUfNlKF1x5UaZ4X6iU_TXx5KlQy_toV1dXZ2eEAMHJUcSdArvB6zRpFdEIxdcHcJyo1BIYgAYTDdAIy__IJVCPY_b2CE5W_6ohlYKB7xDyH8giNuWWXAgBozUfScLUVjPC38yJTpAUi6w6pXgXUWffende5FreQpnMFL1L4G-38wsI_-ISIOzdO8QIrXHxmtc1S5xzYu8bMqSgCinvCEwdeGFCmighRjj8t1zRWo0D14rHbQLPR_b1P5SvEeJTtS9Nm3iibM',
    authSecret: 'g2rWVHUCpUxgcL9Tz7vyeQ',
    privateKey: LEGACY_PRIVATE_KEY,
    publicKey: LEGACY_PUBLIC_KEY,
    headers: {
      crypto_key: 'dh=BCg6ZIGuE2ZNm2ti6Arf4CDVD_8--aLXAGLYhpghwjl1xxVjTLLpb7zihuEOGGbyt8Qj0_fYHBP4ObxwJNl56bk',
      encryption: 'salt=5LIDBXbvkBvvb7ZdD-T4PQ; rs=3',
      encoding: 'aesgcm',
    },
  }, {
    desc: 'Example from draft-ietf-httpbis-encryption-encoding-02',
    result: 'I am the walrus',
    data: '6nqAQUME8hNqw5J3kl8cpVVJylXKYqZOeseZG8UueKpA',
    authSecret: 'R29vIGdvbyBnJyBqb29iIQ',
    privateKey: {
      kty: 'EC',
      crv: 'P-256',
      d: '9FWl15_QUQAWDaD3k3l50ZBZQJ4au27F1V4F0uLSD_M',
      x: 'ISQGPMvxncL6iLZDugTm3Y2n6nuiyMYuD3epQ_TC-pE',
      y: 'T21EEWyf0cQDQcakQMqz4hQKYOQ3il2nNZct4HgAUQU',
      ext: true,
    },
    publicKey: 'BCEkBjzL8Z3C-oi2Q7oE5t2Np-p7osjGLg93qUP0wvqRT21EEWyf0cQDQcakQMqz4hQKYOQ3il2nNZct4HgAUQU',
    headers: {
      crypto_key: 'keyid="dhkey"; dh="BNoRDbb84JGm8g5Z5CFxurSqsXWJ11ItfXEWYVLE85Y7CYkDjXsIEc4aqxYaQ1G8BqkXCJ6DPpDrWtdWj_mugHU"',
      encryption: 'keyid="dhkey"; salt="lngarbyKfMoi9Z75xYXmkg"',
      encoding: 'aesgcm',
    },
  }];
  for (let test of expectedSuccesses) {
    await assertDecrypts(test, test.headers);
  }
});

add_task(async function test_aesgcm_err() {
  let expectedFailures = [{
    desc: 'aesgcm128 message decrypted as aesgcm',
    data: 'fwkuwTTChcLnrzsbDI78Y2EoQzfnbMI8Ax9Z27_rwX8',
    authSecret: 'BhbpNTWyO5wVJmVKTV6XaA',
    privateKey: LEGACY_PRIVATE_KEY,
    publicKey: LEGACY_PUBLIC_KEY,
    headers: {
      crypto_key: 'dh=BCHn-I-J3dfPRLJBlNZ3xFoAqaBLZ6qdhpaz9W7Q00JW1oD-hTxyEECn6KYJNK8AxKUyIDwn6Icx_PYWJiEYjQ0',
      encryption: 'salt=c6JQl9eJ0VvwrUVCQDxY7Q',
      encoding: 'aesgcm',
    },
    expected: /Bad encryption/,
  }, {
    // The plaintext is "O hai". The ciphertext is exactly `rs + 16` bytes,
    // but we didn't include the empty trailing block that aesgcm requires for
    // exact multiples.
    desc: 'rs = 7, no trailing block',
    data: 'YG4F-b06y590hRlnSsw_vuOw62V9Iz8',
    authSecret: 'QoDi0u6vcslIVJKiouXMXw',
    privateKey: {
      kty: 'EC',
      crv: 'P-256',
      d: '2bu4paOAZbL2ef1u-wTzONuTIcDPc00o0zUJgg46XTc',
      x: 'uEvLZUMVn1my0cwnLdcFT0mj1gSU5uzI3HeGwXC7jX8',
      y: 'SfNVLGL-FurydsuzciDfw8K1cUHyoDWnJJ_16UG6Dbo',
      ext: true,
    },
    publicKey: 'BLhLy2VDFZ9ZstHMJy3XBU9Jo9YElObsyNx3hsFwu41_SfNVLGL-FurydsuzciDfw8K1cUHyoDWnJJ_16UG6Dbo',
    headers: {
      crypto_key: 'dh=BD_bsTUpxBMvSv8eksith3vijMLj44D4jhJjO51y7wK1ytbUlsyYBBYYyB5AAe5bnREA_WipTgemDVz00LiWcfM',
      encryption: 'salt=xKWvs_jWWeg4KOsot_uBhA; rs=7',
      encoding: 'aesgcm',
    },
    expected: /Encrypted data truncated/,
  }, {
    // The last block is only 1 byte, but valid blocks must be at least 2 bytes.
    desc: 'Pad size > last block length',
    data: 'JvX9HsJ4lL5gzP8_uCKc6s15iRIaNhD4pFCgq5-dfwbUqEcNUkqv',
    authSecret: 'QtGZeY8MQfCaq-XwKOVGBQ',
    privateKey: {
      kty: 'EC',
      crv: 'P-256',
      d: 'CosERAVXgvTvoh7UkrRC2V-iXoNs0bXle9I68qzkles',
      x: '_D0YqEwirvTJQJdjG6xXrjstMVpeAzf221cUqZz6hgY',
      y: '9MnFbM7U14uiYMDI5e2I4jN29tYmsM9F66QodhKmA-c',
      ext: true,
    },
    publicKey: 'BPw9GKhMIq70yUCXYxusV647LTFaXgM39ttXFKmc-oYG9MnFbM7U14uiYMDI5e2I4jN29tYmsM9F66QodhKmA-c',
    headers: {
      crypto_key: 'dh=BBNZNEi5Ew_ID5S4Y9jWBi1NeVDje6Mjs7SDLViUn6A8VAZj-6X3QAuYQ3j20BblqjwTgYst7PRnY6UGrKyLbmU',
      encryption: 'salt=ot8hzbwOo6CYe6ZhdlwKtg; rs=6',
      encoding: 'aesgcm',
    },
    expected: /Decoded array is too short/,
  }, {
    // The last block is 3 bytes (2 bytes for the pad length; 1 byte of data),
    // but claims its pad length is 2.
    desc: 'Padding length > last block length',
    data: 'oWSOFA-UO5oWq-kI79RHaFfwAejLiQJ4C7eTmrSTBl4gArLXfx7lZ-Y',
    authSecret: 'gKG_P6-de5pyzS8hyH_NyQ',
    privateKey: {
      kty: 'EC',
      crv: 'P-256',
      d: '9l-ahcBM-I0ykwbWiDS9KRrPdhyvTZ0SxKiPpj2aeaI',
      x: 'qx0tU4EDaQv6ayFA3xvLLBdMmn4mLxjn7SK6mIeIxeg',
      y: 'ymbMcmUOEyh_-rLrBsi26NG4UFCis2MTDs5FG2VdDPI',
      ext: true,
    },
    publicKey: 'BKsdLVOBA2kL-mshQN8byywXTJp-Ji8Y5-0iupiHiMXoymbMcmUOEyh_-rLrBsi26NG4UFCis2MTDs5FG2VdDPI',
    headers: {
      crypto_key: 'dh=BKe2IBO_cwmEzQyTVscSbQcj0Y3uBSzGZ_mHlANMciS8uGpb7U8_Bw7TNdlYfpwWDLd0cxM8YYWNDbNJ_p2Rp4o',
      encryption: 'salt=z7QJ6UR89SiFRkd4RsC4Vg; rs=6',
      encoding: 'aesgcm',
    },
    expected: /Padding is wrong/,
  }, {
    // The first block has no padding, but claims its pad length is 1.
    desc: 'Non-zero padding',
    data: 'Qdvjh0LkZXKu_1Hvv56D0rOSF6Mww3y0F8xkxUNlwVu2U1iakOUUGRs',
    authSecret: 'cMpWQW58BrpDbJ8KqbS9ig',
    privateKey: {
      kty: 'EC',
      crv: 'P-256',
      d: 'IzuaxLqFJmjSu8GjLCo2oEaDZjDButW4m4T0qx02XsM',
      x: 'Xy7vt_TJTynxwWsQyY069BcKmrhkRjhKPFuTi-AphoY',
      y: '0M10IVM1ourR7Q5AUX2b2fgdmGyTWcYsdHcdFK_b4Hk',
      ext: true,
    },
    publicKey: 'BF8u77f0yU8p8cFrEMmNOvQXCpq4ZEY4Sjxbk4vgKYaG0M10IVM1ourR7Q5AUX2b2fgdmGyTWcYsdHcdFK_b4Hk',
    headers: {
      crypto_key: 'dh=BBicj01QI0ryiFzAaty9VpW_crgq9XbU1bOCtEZI9UNE6tuOgp4lyN_UN0N905ECnLWK5v_sCPUIxnQgOuCseSo',
      encryption: 'salt=SbkGHONbQBBsBcj9dLyIUw; rs=6',
      encoding: 'aesgcm',
    },
    expected: /Padding is wrong/,
  }, {
    // The first record is 22 bytes: 2 bytes for the pad length, 4 bytes of
    // data, and a 16-byte auth tag. The second "record" is missing the pad
    // and data, and contains just the auth tag.
    desc: 'rs = 6, second record truncated to only auth tag',
    data: 'C7u3j5AL4Yzh2yYB_umN6tzrVHxrt7D5baTEW9DE1Bk3up9fY4w',
    authSecret: '3rWhsRCU_KdaqfKPbd3zBQ',
    privateKey: {
      kty: 'EC',
      crv: 'P-256',
      d: 'nhOT9171xuoQBJGkiZ3aqT5qw_ILJ94_PPiVNu1LFSY',
      x: 'lCj7ctQTmRfwzTMcODlNfHjFMAHmgdI44OhTQXX_xpE',
      y: 'WBdgz4GWGtGAisC63O9DtP5l--hnCzPZiV-YZ-a6Lcw',
      ext: true,
    },
    publicKey: 'BJQo-3LUE5kX8M0zHDg5TXx4xTAB5oHSOODoU0F1_8aRWBdgz4GWGtGAisC63O9DtP5l--hnCzPZiV-YZ-a6Lcw',
    headers: {
      crypto_key: 'dh=BI38Qs_OhDmQIxbszc6Nako-MrX3FzAE_8HzxM1wgoEIG4ocxyF-YAAVhfkpJUvDpRyKW2LDHIaoylaZuxQfRhE',
      encryption: 'salt=QClh48OlvGpSjZ0Mg0e8rg; rs=6',
      encoding: 'aesgcm',
    },
    expected: /Decoded array is too short/,
  }];
  for (let test of expectedFailures) {
    await assertNotDecrypts(test, test.headers);
  }
});

add_task(async function test_aesgcm128_ok() {
  let expectedSuccesses = [{
    desc: 'padSize = 1, rs = 4096, pad = 2',
    result: 'aesgcm128 encrypted message',
    data: 'ljBJ44NPzJFH9EuyT5xWMU4vpZ90MdAqaq1TC1kOLRoPNHtNFXeJ0GtuSaE',
    privateKey: LEGACY_PRIVATE_KEY,
    publicKey: LEGACY_PUBLIC_KEY,
    headers: {
      encryption_key: 'dh=BOmnfg02vNd6RZ7kXWWrCGFF92bI-rQ-bV0Pku3-KmlHwbGv4ejWqgasEdLGle5Rhmp6SKJunZw2l2HxKvrIjfI',
      encryption: 'salt=btxxUtclbmgcc30b9rT3Bg; rs=4096',
      encoding: 'aesgcm128',
    },
  }];
  for (let test of expectedSuccesses) {
    await assertDecrypts(test, test.headers);
  }
});

add_task(async function test_aesgcm128_err() {
  let expectedFailures = [{
    // aesgcm128 doesn't use an auth secret, but we've mixed one in during
    // encryption, so the decryption key and nonce won't match.
    desc: 'padSize = 1, rs = 4096, auth secret, pad = 8',
    data: 'h0FmyldY8aT5EQ6CJrbfRn_IdDvytoLeHb9_q5CjtdFRfgDRknxLmOzavLaVG4oOiS0r',
    authSecret: 'Sxb6u0gJIhGEogyLawjmCw',
    privateKey: LEGACY_PRIVATE_KEY,
    publicKey: LEGACY_PUBLIC_KEY,
    headers: {
      crypto_key: 'dh=BCXHk7O8CE-9AOp6xx7g7c-NCaNpns1PyyHpdcmDaijLbT6IdGq0ezGatBwtFc34BBfscFxdk4Tjksa2Mx5rRCM',
      encryption: 'salt=aGBpoKklLtrLcAUCcCr7JQ',
      encoding: 'aesgcm128',
    },
    expected: /Missing Encryption-Key header/,
  }, {
    // The first byte of each record must be the pad length.
    desc: 'Missing padding',
    data: 'anvsHj7oBQTPMhv7XSJEsvyMS4-8EtbC7HgFZsKaTg',
    privateKey: LEGACY_PRIVATE_KEY,
    publicKey: LEGACY_PUBLIC_KEY,
    headers: {
      crypto_key: 'dh=BMSqfc3ohqw2DDgu3nsMESagYGWubswQPGxrW1bAbYKD18dIHQBUmD3ul_lu7MyQiT5gNdzn5JTXQvCcpf-oZE4',
      encryption: 'salt=Czx2i18rar8XWOXAVDnUuw',
      encoding: 'aesgcm128',
    },
    expected: /Missing Encryption-Key header/,
  }, {
    desc: 'Truncated input',
    data: 'AlDjj6NvT5HGyrHbT8M5D6XBFSra6xrWS9B2ROaCIjwSu3RyZ1iyuv0',
    privateKey: LEGACY_PRIVATE_KEY,
    publicKey: LEGACY_PUBLIC_KEY,
    headers: {
      crypto_key: 'dh=BCHn-I-J3dfPRLJBlNZ3xFoAqaBLZ6qdhpaz9W7Q00JW1oD-hTxyEECn6KYJNK8AxKUyIDwn6Icx_PYWJiEYjQ0',
      encryption: 'salt=c6JQl9eJ0VvwrUVCQDxY7Q; rs=25',
      encoding: 'aesgcm128',
    },
    expected: /Missing Encryption-Key header/,
  }, {
    desc: 'Padding length > rs',
    data: 'Ct_h1g7O55e6GvuhmpjLsGnv8Rmwvxgw8iDESNKGxk_8E99iHKDzdV8wJPyHA-6b2E6kzuVa5UWiQ7s4Zms1xzJ4FKgoxvBObXkc_r_d4mnb-j245z3AcvRmcYGk5_HZ0ci26SfhAN3lCgxGzTHS4nuHBRkGwOb4Tj4SFyBRlLoTh2jyVK2jYugNjH9tTrGOBg7lP5lajLTQlxOi91-RYZSfFhsLX3LrAkXuRoN7G1CdiI7Y3_eTgbPIPabDcLCnGzmFBTvoJSaQF17huMl_UnWoCj2WovA4BwK_TvWSbdgElNnQ4CbArJ1h9OqhDOphVu5GUGr94iitXRQR-fqKPMad0ULLjKQWZOnjuIdV1RYEZ873r62Yyd31HoveJcSDb1T8l_QK2zVF8V4k0xmK9hGuC0rF5YJPYPHgl5__usknzxMBnRrfV5_MOL5uPZwUEFsu',
    privateKey: LEGACY_PRIVATE_KEY,
    publicKey: LEGACY_PUBLIC_KEY,
    headers: {
      crypto_key: 'dh=BAcMdWLJRGx-kPpeFtwqR3GE1LWzd1TYh2rg6CEFu53O-y3DNLkNe_BtGtKRR4f7ZqpBMVS6NgfE2NwNPm3Ndls',
      encryption: 'salt=NQVTKhB0rpL7ZzKkotTGlA; rs=1',
      encoding: 'aesgcm128',
    },
    expected: /Missing Encryption-Key header/,
  }];
  for (let test of expectedFailures) {
    await assertNotDecrypts(test, test.headers);
  }
});
