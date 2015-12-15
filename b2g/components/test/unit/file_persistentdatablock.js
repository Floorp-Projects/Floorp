/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");
// This constants must be synced with the ones in PersistentDataBlock.jsm
const PARTITION_MAGIC = 0x19901873;
const DIGEST_SIZE_BYTES = 32;
const PARTITION_MAGIC_SIZE_BYTES = 4;
const DATA_SIZE_BYTES = 4;
const OEM_UNLOCK_ENABLED_BYTES = 1;

const CACHE_PARTITION = "/dev/block/mtdblock2";
const PARTITION_FAKE_FILE = "/data/local/tmp/frp.test";
const CACHE_PARTITION_SIZE = 69206016;

function log(str) {
  do_print("head_persistentdatablock: " + str + "\n");
}

function toHexString(data) {
  function toHexChar(charCode) {
    return ("0" + charCode.toString(16).slice(-2));
  }
  let hexString = "";
  if (typeof data === "string") {
    hexString = [toHexChar(data.charCodeAt(i)) for (i in data)].join("");
  } else if (typeof data === "array") {
    hexString = [toHexChar(data[i]) for (i in data)].join("");
  }
  return hexString;
}

function _prepareConfig(_args) {
  let args = _args || {};
  // This digest has been previously calculated given the data to be written later, and setting the OEM Unlocked Enabled byte
  // to 1. If we need different values, some tests will fail because this precalculated digest won't be valid then.
  args.digest = args.digest || new Uint8Array([0x00, 0x41, 0x7e, 0x5f, 0xe2, 0xdd, 0xaa, 0xed, 0x11, 0x90, 0x0e, 0x1d, 0x26,
                                               0x10, 0x30, 0xbd, 0x44, 0x9e, 0xcc, 0x4b, 0x65, 0xbe, 0x2e, 0x99, 0x9f, 0x86,
                                               0xf0, 0xfc, 0x5b, 0x33, 0x00, 0xd0]);
  args.dataLength = args.dataLength || 6;
  args.data = args.data || new Uint8Array(["P", "A", "S", "S", "W", "D"]);
  args.oem = args.oem === undefined ? true : args.oem;
  args.oemUnlockAllowed = args.oemUnlockAllowed === undefined ? true : args.oemUnlockAllowed;

  log("_prepareConfig: args.digest = " + args.digest);
  log("_prepareConfig: args.dataLength = " + args.dataLength);
  log("_prepareConfig: args.data = " + args.data);
  log("_prepareConfig: args.oem = " + args.oem);
  log("_prepareConfig: args.oemUnlockAllowed = " + args.oemUnlockAllowed);

  /* This function will be called after passing all native stuff tests, so we will write into a file instead of a real
   * partition. Obviously, there are some native operations like getting the device block size or wipping, that will not
   * work in a regular file, so we need to fake them. */
  PersistentDataBlock._libcutils.property_set("sys.oem_unlock_allowed", args.oemUnlockAllowed === true ? "true" : "false");
  PersistentDataBlock.setTestingMode(true);
  PersistentDataBlock._dataBlockFile = PARTITION_FAKE_FILE;
  // Create the test file with the same structure as the partition will be
  let tempFile;
  return OS.File.open(PersistentDataBlock._dataBlockFile, {write:true, append:false, truncate: true}).then(_tempFile => {
    log("_prepareConfig: Writing DIGEST...");
    tempFile = _tempFile;
    return tempFile.write(args.digest);
  }).then(bytes => {
    log("_prepareConfig: Writing the magic: " + PARTITION_MAGIC);
    return tempFile.write(new Uint32Array([PARTITION_MAGIC]));
  }).then(bytes => {
    log("_prepareConfig: Writing the length of data field");
    return tempFile.write(new Uint32Array([args.dataLength]));
  }).then(bytes => {
    log("_prepareConfig: Writing the data field");
    let data = new Uint8Array(PersistentDataBlock._getBlockDeviceSize() -
               (DIGEST_SIZE_BYTES + PARTITION_MAGIC_SIZE_BYTES + DATA_SIZE_BYTES + OEM_UNLOCK_ENABLED_BYTES));
    data.set(args.data);
    return tempFile.write(data);
  }).then(bytes => {
    return tempFile.write(new Uint8Array([ args.oem === true ? 1 : 0 ]));
  }).then(bytes => {
    return tempFile.close();
  }).then(() =>{
    return Promise.resolve(true);
  }).catch(ex => {
    log("_prepareConfig: ERROR: ex = " + ex);
    return Promise.reject(ex);
  });
}

function utils_getByteAt(pos) {
  let file;
  let byte;
  return OS.File.open(PersistentDataBlock._dataBlockFile, {read:true, existing:true, append:false}).then(_file => {
    file = _file;
    return file.setPosition(pos, OS.File.POS_START);
  }).then(() => {
    return file.read(1);
  }).then(_byte => {
    byte = _byte;
    return file.close();
  }).then(() => {
    return Promise.resolve(byte[0]);
  }).catch(ex => {
    return Promise.reject(ex);
  });
}

function utils_getHeader() {
  let file;
  let header = {};
  return OS.File.open(PersistentDataBlock._dataBlockFile, {read:true, existing:true, append:false}).then(_file => {
    file = _file;
    return file.read(DIGEST_SIZE_BYTES);
  }).then(digest => {
    header.digest = digest;
    return file.read(PARTITION_MAGIC_SIZE_BYTES);
  }).then(magic => {
    header.magic = magic;
    return file.read(DATA_SIZE_BYTES);
  }).then(dataLength => {
    header.dataLength = dataLength;
    return file.close();
  }).then(() => {
    return Promise.resolve(header);
  }).catch(ex => {
    return Promise.reject(ex);
  });
}

function utils_getData() {
  let file;
  let data;
  return OS.File.open(PersistentDataBlock._dataBlockFile, {read:true, existing:true, append:false}).then(_file => {
    file = _file;
    return file.setPosition(DIGEST_SIZE_BYTES + PARTITION_MAGIC_SIZE_BYTES, OS.File.POS_START);
  }).then(() => {
    return file.read(4);
  }).then(_dataLength => {
    let dataLength = new Uint32Array(_dataLength.buffer);
    log("utils_getData: dataLength = " + dataLength[0]);
    return file.read(dataLength[0]);
  }).then(_data => {
    data = _data;
    return file.close();
  }).then(() => {
    return Promise.resolve(data);
  }).catch(ex => {
    return Promise.reject(ex);
  });
}

function _installTests() {
  // <NATIVE_TESTS> Native operation tests go first
  add_test(function test_getBlockDeviceSize() {
    // We will use emulator /cache partition to get it's size.
    PersistentDataBlock._dataBlockFile = CACHE_PARTITION;
    // Disable testing mode for this specific test because we can get the size of a real block device,
    // but we need to flip to testing mode after this test because we use files instead of partitions
    // and we cannot run this operation on files.
    PersistentDataBlock.setTestingMode(false);
    let blockSize = PersistentDataBlock._getBlockDeviceSize();
    ok(blockSize !== CACHE_PARTITION_SIZE, "test_getBlockDeviceSize: Block device size should be greater than 0");
    run_next_test();
  });

  add_test(function test_wipe() {
    // Turning into testing mode again.
    PersistentDataBlock.setTestingMode(true);
    PersistentDataBlock.wipe().then(() => {
      // We don't evaluate anything because in testing mode we always return ok!
      run_next_test();
    }).catch(ex => {
      // ... something went really really bad if this happens.
      ok(false, "test_wipe failed!: ex: " + ex);
    });
  });
  // </NATIVE_TESTS>

  add_test(function test_computeDigest() {
    _prepareConfig().then(() => {
      PersistentDataBlock._computeDigest().then(digest => {
        // So in order to update this value in a future (should only happens if the partition data is changed), you just need
        // to launch this test manually, see the result in the logs and update this constant with that value.
        const _EXPECTED_VALUE = "0004107e05f0e20dd0aa0ed0110900e01d0260100300bd04409e0cc04b0650be02e09909f0860f00fc05b033000d0";
        let calculatedValue = toHexString(digest.calculated);
        strictEqual(calculatedValue, _EXPECTED_VALUE);
        run_next_test();
      }).catch(ex => {
        ok(false, "test_computeDigest failed!: ex: " + ex);
      });
    });
  });

  add_test(function test_getDataFieldSize() {
    PersistentDataBlock.getDataFieldSize().then(dataFieldLength => {
      log("test_getDataFieldSize: dataFieldLength is " + dataFieldLength);
      strictEqual(dataFieldLength, 6);
      run_next_test();
    }).catch(ex => {
      ok(false, "test_getOemUnlockedEnabled failed: ex:" + ex);
    });
  });

  add_test(function test_setOemUnlockedEnabledToTrue() {
    PersistentDataBlock.setOemUnlockEnabled(true).then(() => {
      return utils_getByteAt(PersistentDataBlock._getBlockDeviceSize() - 1);
    }).then(byte => {
      log("test_setOemUnlockedEnabledToTrue: byte = " + byte );
      strictEqual(byte, 1);
      run_next_test();
    }).catch(ex => {
      ok(false, "test_setOemUnlockedEnabledToTrue failed!: ex: " + ex);
    });
  });

  add_test(function test_setOemUnlockedEnabledToFalse() {
    PersistentDataBlock.setOemUnlockEnabled(false).then(() => {
      return utils_getByteAt(PersistentDataBlock._getBlockDeviceSize() - 1);
    }).then(byte => {
      log("test_setOemUnlockedEnabledToFalse: byte = " + byte );
      strictEqual(byte, 0);
      run_next_test();
    }).catch(ex => {
      ok(false, "test_setOemUnlockedEnabledToFalse failed!: ex: " + ex);
    });
  });

  add_test(function test_getOemUnlockedEnabledWithTrue() {
    // We first need to set the OEM Unlock Enabled byte to true so we can test
    // the getter properly
    PersistentDataBlock.setOemUnlockEnabled(true).then(() => {
      return PersistentDataBlock.getOemUnlockEnabled().then(enabled => {
        log("test_getOemUnlockedEnabledWithTrue: enabled is " + enabled);
        ok(enabled === true, "test_getOemUnlockedEnabledWithTrue: enabled value should be true");
        run_next_test();
      }).catch(ex => {
        ok(false, "test_getOemUnlockedEnabledWithTrue failed: ex:" + ex);
      });
    }).catch(ex => {
      ok(false, "test_getOemUnlockedEnabledWithTrue failed: An error ocurred while setting the OEM Unlock Enabled byte to true: ex:" + ex);
    });
  });

  add_test(function test_getOemUnlockedEnabledWithFalse() {
    // We first need to set the OEM Unlock Enabled byte to false so we can test
    // the getter properly
    PersistentDataBlock.setOemUnlockEnabled(false).then(() => {
      return PersistentDataBlock.getOemUnlockEnabled().then(enabled => {
        log("test_getOemUnlockedEnabledWithFalse: enabled is " + enabled);
        ok(enabled === false, "test_getOemUnlockedEnabledWithFalse: enabled value should be false");
        run_next_test();
      }).catch(ex => {
        ok(false, "test_getOemUnlockedEnabledWithFalse failed: ex:" + ex);
      });
    }).catch(ex => {
      ok(false, "test_getOemUnlockedEnabledWithFalse failed: An error ocurred while setting the OEM Unlock Enabled byte to false: ex:" + ex);
    });
  });

  add_test(function test_computeAndWriteDigest() {
    PersistentDataBlock._computeAndWriteDigest().then(() => {
      return utils_getHeader();
    }).then(header => {
      log("test_computeAndWriteDigest: header = " + header);
      let magicRead = new Uint32Array(header.magic.buffer);
      let magicSupposed = new Uint32Array([PARTITION_MAGIC]);
      strictEqual(magicRead[0], magicSupposed[0]);
      let dataLength = new Uint32Array([header.dataLength]);
      strictEqual(header.dataLength[0], 6);
      run_next_test();
    }).catch(ex => {
      ok(false, "test_computeAndWriteDigest failed!: ex: " + ex);
    });
  });

  add_test(function test_formatIfOemUnlockEnabledWithTrue() {
    _prepareConfig({oem:true}).then(() => {
      return PersistentDataBlock._formatIfOemUnlockEnabled();
    }).then(result => {
      ok(result === true, "test_formatIfOemUnlockEnabledWithTrue: result should be true");
      return utils_getByteAt(PersistentDataBlock._getBlockDeviceSize() - 1);
    }).then(byte => {
      // Check if the OEM Unlock Enabled byte is 1
      strictEqual(byte, 1);
      run_next_test();
    }).catch(ex => {
      ok(false, "test_formatIfOemUnlockEnabledWithTrue failed!: ex: " + ex);
    });
  });

  add_test(function test_formatIfOemUnlockEnabledWithFalse() {
    _prepareConfig({oem:false}).then(() => {
      return PersistentDataBlock._formatIfOemUnlockEnabled();
    }).then(result => {
      log("test_formatIfOemUnlockEnabledWithFalse: result = " + result);
      ok(result === false, "test_formatIfOemUnlockEnabledWithFalse: result should be false");
      return utils_getByteAt(PersistentDataBlock._getBlockDeviceSize() - 1);
    }).then(byte => {
      // Check if the OEM Unlock Enabled byte is 0
      strictEqual(byte, 0);
      run_next_test();
    }).catch(ex => {
      ok(false, "test_formatIfOemUnlockEnabledWithFalse failed!: ex: " + ex);
    });
  });

  add_test(function test_formatPartition() {
    // Restore a fullfilled partition so we can check if formatting works...
    _prepareConfig({oem:true}).then(() => {
      return PersistentDataBlock._formatPartition(true);
    }).then(() => {
      return utils_getByteAt(PersistentDataBlock._getBlockDeviceSize() - 1);
    }).then(byte => {
      // Check if the last byte is 1
      strictEqual(byte, 1);
      return utils_getHeader();
    }).then(header => {
      // The Magic number should exists in a formatted partition
      let magicRead = new Uint32Array(header.magic.buffer);
      let magicSupposed = new Uint32Array([PARTITION_MAGIC]);
      strictEqual(magicRead[0], magicSupposed[0]);
      // In a formatted partition, the digest field is always 32 bytes of zeros.
      let digestSupposed = new Uint8Array(DIGEST_SIZE_BYTES);
      strictEqual(header.digest.join(""), "94227253995810864198417798821014713171138121254110134189198178208133167236184116199");
      return PersistentDataBlock._formatPartition(false);
    }).then(() => {
      return utils_getByteAt(PersistentDataBlock._getBlockDeviceSize() - 1);
    }).then(byte => {
      // In this case OEM Unlock enabled byte should be set to 0 because we passed false to the _formatPartition method before.
      strictEqual(byte, 0);
      run_next_test();
    }).catch(ex => {
      ok(false, "test_formatPartition failed!: ex: " + ex);
    });
  });

  add_test(function test_enforceChecksumValidityWithValidChecksum() {
    // We need a valid partition layout to pass this test
    _prepareConfig().then(() => {
      PersistentDataBlock._enforceChecksumValidity().then(() => {
        ok(true, "test_enforceChecksumValidityWithValidChecksum passed");
        run_next_test();
      }).catch(ex => {
        ok(false, "test_enforceChecksumValidityWithValidChecksum failed!: ex: " + ex);
      });
    });
  });

  add_test(function test_enforceChecksumValidityWithInvalidChecksum() {
    var badDigest = new Uint8Array([0x01, 0x02, 0x03, 0x04, 0x05, 0x05, 0x06, 0x07,
                                    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
                                    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
                                    0x18, 0x19, 0x1A, 0x1C, 0x1D, 0x1E, 0x1F, 0x20]);
    // We need a valid partition layout to pass this test
    _prepareConfig({digest: badDigest}).then(() => {
      PersistentDataBlock._enforceChecksumValidity().then(() => {
        return utils_getHeader();
      }).then(header => {
        // Check that we have a valid magic after formatting
        let magicRead = new Uint32Array(header.magic.buffer)[0];
        let magicSupposed = new Uint32Array([PARTITION_MAGIC])[0];
        strictEqual(magicRead, magicSupposed);
        // Data length field should be 0, because we formatted the partition
        let dataLengthRead = new Uint32Array(header.dataLength.buffer)[0];
        strictEqual(dataLengthRead, 0);
        run_next_test();
      }).catch(ex => {
        ok(false, "test_enforceChecksumValidityWithValidChecksum failed!: ex: " + ex);
      });
    });
  });

  add_test(function test_read() {
    // Before reading, let's write some bytes of data first.
    PersistentDataBlock.write(new Uint8Array([1,2,3,4])).then(() => {
      PersistentDataBlock.read().then(bytes => {
        log("test_read: bytes (in hex): " + toHexString(bytes));
        strictEqual(bytes[0], 1);
        strictEqual(bytes[1], 2);
        strictEqual(bytes[2], 3);
        strictEqual(bytes[3], 4);
        run_next_test();
      }).catch(ex => {
        ok(false, "test_read failed!: ex: " + ex);
      });
    });

  });

  add_test(function test_write() {
    let data = new Uint8Array(['1','2','3','4','5']);
    PersistentDataBlock.write(data).then(bytesWrittenLength => {
      log("test_write: bytesWrittenLength = " + bytesWrittenLength);
      return utils_getData();
    }).then(data => {
      strictEqual(data[0], 1);
      strictEqual(data[1], 2);
      strictEqual(data[2], 3);
      strictEqual(data[3], 4);
      strictEqual(data[4], 5);
      run_next_test();
    }).catch(ex => {
      ok(false, "test_write failed!: ex: " + ex);
    });
  });
}
