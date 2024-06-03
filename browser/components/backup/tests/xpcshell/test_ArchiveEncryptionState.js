/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { OSKeyStoreTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/OSKeyStoreTestUtils.sys.mjs"
);
const { ArchiveEncryptionState } = ChromeUtils.importESModule(
  "resource:///modules/backup/ArchiveEncryptionState.sys.mjs"
);

const TEST_RECOVERY_CODE = "This is my recovery code.";

add_setup(async () => {
  OSKeyStoreTestUtils.setup();
  registerCleanupFunction(async () => {
    await OSKeyStoreTestUtils.cleanup();
  });
});

/**
 * Tests that we can initialize an ArchiveEncryptionState from scratch, and
 * generate all of the necessary information for encrypting a backup.
 */
add_task(async function test_ArchiveEncryptionState_enable() {
  let { instance: encState, recoveryCode } =
    await ArchiveEncryptionState.initialize(TEST_RECOVERY_CODE);

  Assert.equal(recoveryCode, TEST_RECOVERY_CODE, "Got back recovery code.");
  Assert.ok(encState.publicKey, "A public key was computed.");
  Assert.ok(encState.authKey, "An auth key was computed.");
  Assert.ok(encState.salt, "A salt was computed.");
  Assert.ok(encState.nonce, "A nonce was computed.");
  Assert.ok(encState.wrappedSecrets, "Wrapped secrets were computed.");
});

/**
 * Tests that an ArchiveEncryptionState can be serialized to an object that
 * can be written to disk, and then deserialized back out again.
 */
add_task(
  async function test_ArchiveEncryptionState_serialization_deserialization() {
    let { instance: encState } = await ArchiveEncryptionState.initialize(
      TEST_RECOVERY_CODE
    );
    let serialization = await encState.serialize();

    // We'll pretend to write this serialization to disk by stringifying it,
    // and then parsing it again.
    serialization = JSON.parse(JSON.stringify(serialization));

    Assert.equal(
      serialization.version,
      ArchiveEncryptionState.VERSION,
      "The ArchiveEncryptionState version was included in the serialization."
    );

    let { instance: recoveredState } = await ArchiveEncryptionState.initialize(
      serialization
    );

    Assert.deepEqual(
      encState.publicKey,
      recoveredState.publicKey,
      "Public keys match"
    );
    Assert.deepEqual(
      encState.authKey,
      recoveredState.authKey,
      "Auth keys match"
    );
    Assert.deepEqual(encState.salt, recoveredState.salt, "Salts match");
    Assert.deepEqual(encState.nonce, recoveredState.nonce, "Nonces match");
    Assert.deepEqual(
      encState.wrappedSecrets,
      recoveredState.wrappedSecrets,
      "Wrapped secrets match"
    );
  }
);

/**
 * Tests that ArchiveEncryptionState will throw if it attempts to deserialize
 * a serialized state from a newer version of ArchiveEncryptionState.
 */
add_task(async function test_ArchiveEncryptionState_deserialize_newer() {
  let { instance: encState } = await ArchiveEncryptionState.initialize(
    TEST_RECOVERY_CODE
  );
  let serialization = await encState.serialize();

  // We'll pretend to write this serialization to disk by stringifying it,
  // and then parsing it again.
  serialization = JSON.parse(JSON.stringify(serialization));

  // Poke in a version that's greater than the one that ArchiveEncryptionState
  // currently knows how to deal with.
  serialization.version = ArchiveEncryptionState.VERSION + 1;

  await Assert.rejects(
    ArchiveEncryptionState.initialize(serialization),
    /newer version/,
    "Should have thrown when deserializing from a newer version"
  );
});

/**
 * Tests that if no recovery code is passed to enable(), that one is generated
 * and returned instead.
 */
add_task(async function test_ArchiveEncryptionState_generate_recoveryCode() {
  let { recoveryCode } = await ArchiveEncryptionState.initialize();

  Assert.equal(
    recoveryCode.length,
    ArchiveEncryptionState.GENERATED_RECOVERY_CODE_LENGTH,
    "The generated recovery code has the right length."
  );
});
