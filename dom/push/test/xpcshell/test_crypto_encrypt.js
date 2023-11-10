// Test PushCrypto.encrypt()
"use strict";

const { PushCrypto } = ChromeUtils.importESModule(
  "resource://gre/modules/PushCrypto.sys.mjs"
);

let from64 = v => {
  // allow whitespace in the strings.
  let stripped = v.replace(/ |\t|\r|\n/g, "");
  return new Uint8Array(
    ChromeUtils.base64URLDecode(stripped, { padding: "reject" })
  );
};

let to64 = v => ChromeUtils.base64URLEncode(v, { pad: false });

// A helper function to take a public key (as a buffer containing a 65-byte
// buffer of uncompressed EC points) and a private key (32byte buffer) and
// return 2 crypto keys.
async function importKeyPair(publicKeyBuffer, privateKeyBuffer) {
  let jwk = {
    kty: "EC",
    crv: "P-256",
    x: to64(publicKeyBuffer.slice(1, 33)),
    y: to64(publicKeyBuffer.slice(33, 65)),
    ext: true,
  };
  let publicKey = await crypto.subtle.importKey(
    "jwk",
    jwk,
    { name: "ECDH", namedCurve: "P-256" },
    true,
    []
  );
  jwk.d = to64(privateKeyBuffer);
  let privateKey = await crypto.subtle.importKey(
    "jwk",
    jwk,
    { name: "ECDH", namedCurve: "P-256" },
    true,
    ["deriveBits"]
  );
  return { publicKey, privateKey };
}

// The example from draft-ietf-webpush-encryption-09.
add_task(async function static_aes128gcm() {
  let fixture = {
    ciphertext:
      from64(`DGv6ra1nlYgDCS1FRnbzlwAAEABBBP4z9KsN6nGRTbVYI_c7VJSPQTBtkgcy27ml
                        mlMoZIIgDll6e3vCYLocInmYWAmS6TlzAC8wEqKK6PBru3jl7A_yl95bQpu6cVPT
                        pK4Mqgkf1CXztLVBSt2Ks3oZwbuwXPXLWyouBWLVWGNWQexSgSxsj_Qulcy4a-fN`),
    plaintext: new TextEncoder().encode(
      "When I grow up, I want to be a watermelon"
    ),
    authSecret: from64("BTBZMqHH6r4Tts7J_aSIgg"),
    receiver: {
      private: from64("q1dXpw3UpT5VOmu_cf_v6ih07Aems3njxI-JWgLcM94"),
      public: from64(`BCVxsr7N_eNgVRqvHtD0zTZsEc6-VV-JvLexhqUzORcx
                      aOzi6-AYWXvTBHm4bjyPjs7Vd8pZGH6SRpkNtoIAiw4`),
    },
    sender: {
      private: from64("yfWPiYE-n46HLnH0KqZOF1fJJU3MYrct3AELtAQ-oRw"),
      public: from64(`BP4z9KsN6nGRTbVYI_c7VJSPQTBtkgcy27mlmlMoZIIg
                      Dll6e3vCYLocInmYWAmS6TlzAC8wEqKK6PBru3jl7A8`),
    },
    salt: from64("DGv6ra1nlYgDCS1FRnbzlw"),
  };

  let options = {
    senderKeyPair: await importKeyPair(
      fixture.sender.public,
      fixture.sender.private
    ),
    salt: fixture.salt,
  };

  let { ciphertext, encoding } = await PushCrypto.encrypt(
    fixture.plaintext,
    fixture.receiver.public,
    fixture.authSecret,
    options
  );

  Assert.deepEqual(ciphertext, fixture.ciphertext);
  Assert.equal(encoding, "aes128gcm");

  // and for fun, decrypt it and check the plaintext.
  let recvKeyPair = await importKeyPair(
    fixture.receiver.public,
    fixture.receiver.private
  );
  let jwk = await crypto.subtle.exportKey("jwk", recvKeyPair.privateKey);
  let plaintext = await PushCrypto.decrypt(
    jwk,
    fixture.receiver.public,
    fixture.authSecret,
    { encoding: "aes128gcm" },
    ciphertext
  );
  Assert.deepEqual(plaintext, fixture.plaintext);
});

// This is how we expect real code to interact with .encrypt.
add_task(async function aes128gcm_simple() {
  let [recvPublicKey, recvPrivateKey] = await PushCrypto.generateKeys();

  let message = new TextEncoder().encode("Fast for good.");
  let authSecret = crypto.getRandomValues(new Uint8Array(16));
  let { ciphertext, encoding } = await PushCrypto.encrypt(
    message,
    recvPublicKey,
    authSecret
  );
  Assert.equal(encoding, "aes128gcm");
  // and decrypt it.
  let plaintext = await PushCrypto.decrypt(
    recvPrivateKey,
    recvPublicKey,
    authSecret,
    { encoding },
    ciphertext
  );
  deepEqual(message, plaintext);
});

// Variable record size tests
add_task(async function aes128gcm_rs() {
  let [recvPublicKey, recvPrivateKey] = await PushCrypto.generateKeys();

  for (let rs of [-1, 0, 1, 17]) {
    let payload = "x".repeat(1024);
    info(`testing expected failure with rs=${rs}`);
    let message = new TextEncoder().encode(payload);
    let authSecret = crypto.getRandomValues(new Uint8Array(16));
    await Assert.rejects(
      PushCrypto.encrypt(message, recvPublicKey, authSecret, { rs }),
      /recordsize is too small/
    );
  }
  for (let rs of [18, 50, 1024, 4096, 16384]) {
    info(`testing expected success with rs=${rs}`);
    let payload = "x".repeat(rs * 3);
    let message = new TextEncoder().encode(payload);
    let authSecret = crypto.getRandomValues(new Uint8Array(16));
    let { ciphertext, encoding } = await PushCrypto.encrypt(
      message,
      recvPublicKey,
      authSecret,
      { rs }
    );
    Assert.equal(encoding, "aes128gcm");
    // and decrypt it.
    let plaintext = await PushCrypto.decrypt(
      recvPrivateKey,
      recvPublicKey,
      authSecret,
      { encoding },
      ciphertext
    );
    deepEqual(message, plaintext);
  }
});

// And try and hit some edge-cases.
add_task(async function aes128gcm_edgecases() {
  let [recvPublicKey, recvPrivateKey] = await PushCrypto.generateKeys();

  for (let size of [
    0,
    4096 - 16,
    4096 - 16 - 1,
    4096 - 16 + 1,
    4095,
    4096,
    4097,
    10240,
  ]) {
    info(`testing encryption of ${size} byte payload`);
    let message = new TextEncoder().encode("x".repeat(size));
    let authSecret = crypto.getRandomValues(new Uint8Array(16));
    let { ciphertext, encoding } = await PushCrypto.encrypt(
      message,
      recvPublicKey,
      authSecret
    );
    Assert.equal(encoding, "aes128gcm");
    // and decrypt it.
    let plaintext = await PushCrypto.decrypt(
      recvPrivateKey,
      recvPublicKey,
      authSecret,
      { encoding },
      ciphertext
    );
    deepEqual(message, plaintext);
  }
});
