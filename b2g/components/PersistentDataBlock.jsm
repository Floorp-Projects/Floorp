/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * The Persistent Partition has this layout:
 *
 * Bytes:     32       4        4          <DATA_LENGTH>               1
 * Fields: [[DIGEST][MAGIC][DATA_LENGTH][        DATA        ][OEM_UNLOCK_ENABLED]]
 *
 */

"use strict";

const DEBUG = false;

this.EXPORTED_SYMBOLS = [ "PersistentDataBlock" ];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;
// This is a marker that will be written after digest in the partition.
const PARTITION_MAGIC = 0x19901873;
// This is the limit in Android because of issues with Binder if blocks are > 100k
// We dont really have this issues because we don't use Binder, but let's stick
// to Android implementation.
const MAX_DATA_BLOCK_SIZE = 1024 * 100;
const DIGEST_SIZE_BYTES = 32;
const HEADER_SIZE_BYTES = 8;
const PARTITION_MAGIC_SIZE_BYTES = 4;
const DATA_SIZE_BYTES = 4;
const OEM_UNLOCK_ENABLED_BYTES = 1;
// The position of the Digest
const DIGEST_OFFSET = 0;
const XPCOM_SHUTDOWN_OBSERVER_TOPIC = "xpcom-shutdown";
// This property will have the path to the persistent partition
const PERSISTENT_DATA_BLOCK_PROPERTY = "ro.frp.pst";
const OEM_UNLOCK_PROPERTY = "sys.oem_unlock_allowed";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise", "resource://gre/modules/Promise.jsm");

XPCOMUtils.defineLazyGetter(this, "libcutils", function () {
  Cu.import("resource://gre/modules/systemlibs.js");
  return libcutils;
});

var inParent = Cc["@mozilla.org/xre/app-info;1"]
                 .getService(Ci.nsIXULRuntime)
                 .processType === Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;

function log(str) {
  dump("PersistentDataBlock.jsm: " + str + "\n");
}

function debug(str) {
  DEBUG && log(str);
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

function arr2bstr(arr) {
  let bstr = "";
  for (let i = 0; i < arr.length; i++) {
    bstr += String.fromCharCode(arr[i]);
  }
  return bstr;
}

this.PersistentDataBlock = {

  /**
   * libc funcionality. Accessed via ctypes
   */
   _libc: {
      handler: null,
      open: function() {},
      close: function() {},
      ioctl: function() {}
   },

  /**
   * Component to access property_get/set functions
   */
  _libcutils: null,

  /**
   * The size of a device block. This is assigned by querying the kernel.
   */
  _blockDeviceSize: -1,

  /**
   * Data block file
   */
  _dataBlockFile: "",

  /**
  * Change the behavior of the class for some methods to testing mode. This will fake the return value of some
  * methods realted to native operations with block devices.
  */
  _testing: false,

  /*
  * *** USE ONLY FOR TESTING ***
  * This component will interface between Gecko and a special secure partition with no formatting, a raw partition.
  * This interaction requires a specific partition layout structure which emulators don't have so far. So for
  * our unit tests to pass, we need a way for some methods to behave differently. This method will change this
  * behavior at runtime so some low-level platform-specific operations will be faked:
  *  - Getting the size of a partition: We can use any partition to get the size, is up to the test to choose
  *      which partition to use. But, in testing mode we use files instead of partitions, so we need to fake the
  *      return value of this method in this case.
  *  - Wipping a partition: This will fully remove the partition as well as it filesystem type, so we cannot
  *      test it on any existing emulator partition. Testing mode will skip this operation.
  *
  * @param enabled {Bool} Set testing mode. See _testing property.
  */
  setTestingMode: function(enabled) {
     this._testing = enabled || false;
  },

  /**
  * Initialize the class.
  *
  */
  init: function(mode) {
    debug("init()");

    if (libcutils) {
      this._libcutils = libcutils;
    }

    if (!this.ctypes) {
      Cu.import("resource://gre/modules/ctypes.jsm", this);
    }

    if (this._libc.handler === null) {
#ifdef MOZ_WIDGET_GONK
      try {
        this._libc.handler = this.ctypes.open(this.ctypes.libraryName("c"));
        this._libc.close = this._libc.handler.declare("close",
                                                      this.ctypes.default_abi,
                                                      this.ctypes.int,
                                                      this.ctypes.int
                                                     );
        this._libc.open = this._libc.handler.declare("open",
                                                     this.ctypes.default_abi,
                                                     this.ctypes.int,
                                                     this.ctypes.char.ptr,
                                                     this.ctypes.int
                                                    );
        this._libc.ioctl = this._libc.handler.declare("ioctl",
                                                      this.ctypes.default_abi,
                                                      this.ctypes.int,
                                                      this.ctypes.int,
                                                      this.ctypes.unsigned_long,
                                                      this.ctypes.unsigned_long.ptr);

      } catch(ex) {
        log("Unable to open libc.so: ex = " + ex);
        throw Cr.NS_ERROR_FAILURE;
      }
#else
      log("This component requires Gonk!");
      throw Cr.NS_ERROR_ABORT;
#endif
    }

    this._dataBlockFile = this._libcutils.property_get(PERSISTENT_DATA_BLOCK_PROPERTY);
    if (this._dataBlockFile === null) {
      log("init: ERROR: property " +  PERSISTENT_DATA_BLOCK_PROPERTY + " doesn't exist!");
      throw Cr.NS_ERROR_FAILURE;
    }

    Services.obs.addObserver(this, XPCOM_SHUTDOWN_OBSERVER_TOPIC, false);
  },

  uninit: function() {
    debug("uninit()");
    this._libc.handler.close();
    Services.obs.removeObserver(this, XPCOM_SHUTDOWN_OBSERVER_TOPIC);
  },

  _checkLibcUtils: function() {
    debug("_checkLibcUtils");
    if (!this._libcutils) {
      log("No proper libcutils binding, aborting.");
      throw Cr.NS_ERROR_NO_INTERFACE;
    }

    return true;
  },

  /**
   * Callback mehtod for addObserver
   */
  observe: function(aSubject, aTopic, aData) {
    debug("observe()");
    switch (aTopic) {
      case XPCOM_SHUTDOWN_OBSERVER_TOPIC:
        this.uninit();
        break;

      default:
        log("Wrong observer topic: " + aTopic);
        break;
    }
  },

  /**
  * This method will format the persistent partition if it detects manipulation (digest calculation will fail)
  * or if the OEM Unlock Enabled byte is set to true.
  * We need to call this method on every boot.
  */
  start: function() {
    debug("start()");
    return this._enforceChecksumValidity().then(() => {
      return this._formatIfOemUnlockEnabled().then(() => {
        return Promise.resolve(true);
      })
    }).catch(ex => {
      return Promise.reject(ex);
    });
  },

  /**
   * Computes the digest of the entire data block.
   * The digest is saved in the first 32 bytes of the block.
   *
   * @param isStoredDigestReturned {Bool} Tells the function to return the stored digest as well as the calculated.
   *                                      True means to return stored digest and the calculated
   *                                      False means to return just the calculated one
   *
   * @return Promise<digest> {Object} The calculated digest into the "calculated" property, and the stored
   *                                  digest into the "stored" property.
   */
  _computeDigest: function (isStoredDigestReturned) {
    debug("_computeDigest()");
    let digest = {calculated: "", stored: ""};
    let partition;
    debug("_computeDigest: _dataBlockFile = " + this._dataBlockFile);
    return OS.File.open(this._dataBlockFile, {existing:true, append:false, read:true}).then(_partition => {
      partition = _partition;
      return partition.read(DIGEST_SIZE_BYTES);
    }).then(digestDataRead => {
      // If storedDigest is passed as a parameter, the caller will likely compare the
      // one is already stored in the partition with the one we are going to compute later.
      if (isStoredDigestReturned === true) {
        debug("_computeDigest: get stored digest from the partition");
        digest.stored = arr2bstr(digestDataRead);
      }
      return partition.read();
    }).then(data => {
      // Calculate Digest with the data retrieved after the digest
      let hasher = Cc["@mozilla.org/security/hash;1"].createInstance(Ci.nsICryptoHash);
      hasher.init(hasher.SHA256);
      hasher.update(data, data.byteLength);
      digest.calculated = hasher.finish(false);
      debug("_computeDigest(): Digest = "  + toHexString(digest.calculated) +
            "(" + digest.calculated.length + ")");
      return partition.close();
    }).then(() => {
      return Promise.resolve(digest);
    }).catch(ex => {
      log("_computeDigest(): Failed to read partition: ex = " + ex);
      return Promise.reject(ex);
    });
  },

  /**
   * Returns the size of a block from the undelaying filesystem
   *
   * @return {Number} The size of the block
   */
  _getBlockDeviceSize: function() {
    debug("_getBlockDeviceSize()");

    // See _testing property
    if (this._testing === true) {
      debug("_getBlockDeviceSize: No real block device size in testing mode!. Returning 1024.");
      return 1024;
    }

#ifdef MOZ_WIDGET_GONK
    const O_READONLY = 0;
    const O_NONBLOCK = 1 << 11;
    /* Getting the correct values for ioctl() operations by reading the headers is not a trivial task, so
     * the better way to get the values below is by writting a simple test aplication in C that will
     * print the values to the output.
     * 32bits and 64bits value for ioctl() BLKGETSIZE64 operation is different. So we will fallback in
     * case ioctl() returns ENOTTY (22). */
    const BLKGETSIZE64_32_BITS = 0x80041272;
    const BLKGETSIZE64_64_BITS = 0x80081272;
    const ENOTTY = 25;

    debug("_getBlockDeviceSize: _dataBlockFile = " + this._dataBlockFile);
    let fd = this._libc.open(this._dataBlockFile, O_READONLY | O_NONBLOCK);
    if (fd < 0) {
      log("_getBlockDeviceSize: couldn't open partition!: errno = " + this.ctypes.errno);
      throw Cr.NS_ERROR_FAILURE;
    }

    let size = new this.ctypes.unsigned_long();
    let sizeAddress = size.address();
    let ret = this._libc.ioctl(fd, BLKGETSIZE64_32_BITS, sizeAddress);
    if (ret < 0) {
      if (this.ctypes.errno === ENOTTY) {
        log("_getBlockDeviceSize: errno is ENOTTY, falling back to 64 bit version of BLKGETSIZE64...");
        ret = this._libc.ioctl(fd, BLKGETSIZE64_64_BITS, sizeAddress);
        if (ret < 0) {
          this._libc.close(fd);
          log("_getBlockDeviceSize: BLKGETSIZE64 failed again!. errno = " + this.ctypes.errno);
          throw Cr.NS_ERROR_FAILURE;
        }
      } else {
        this._libc.close(fd);
        log("_getBlockDeviceSize: couldn't get block device size!: errno = " + this.ctypes.errno);
        throw Cr.NS_ERROR_FAILURE;
      }
    }
    this._libc.close(fd);
    debug("_getBlockDeviceSize: size =" + size.value);
    return size.value;
#else
    log("_getBlockDeviceSize: ERROR: This feature is only supported in Gonk!");
    return -1;
#endif
  },

  /**
   * Sets the byte into the partition which represents the OEM Unlock Enabled feature.
   * A value of "1" means that the user doesn't want to enable KillSwitch.
   * The byte is the last one byte into the device block.
   *
   * @param isSetOemUnlockEnabled {bool} If true, sets the OEM Unlock Enabled byte to 1.
   *                                     Otherwise, sets it to 0.
   */
  _doSetOemUnlockEnabled: function(isSetOemUnlockEnabled) {
    debug("_doSetOemUnlockEnabled()");
    let partition;
    return OS.File.open(this._dataBlockFile, {existing:true, append:false, write:true}).then(_partition => {
      partition = _partition;
      return partition.setPosition(this._getBlockDeviceSize() - OEM_UNLOCK_ENABLED_BYTES, OS.File.POS_START);
    }).then(() => {
      return partition.write(new Uint8Array([ isSetOemUnlockEnabled === true ? 1 : 0 ]));
    }).then(bytesWrittenLength => {
      if (bytesWrittenLength != 1) {
        log("_doSetOemUnlockEnabled: Error writting OEM Unlock Enabled byte!");
        return Promise.reject();
      }
      return partition.close();
    }).then(() => {
      let oemUnlockByte = (isSetOemUnlockEnabled === true ? "1" : "0");
      debug("_doSetOemUnlockEnabled: OEM unlock enabled written to " + oemUnlockByte);
      this._libcutils.property_set(OEM_UNLOCK_PROPERTY, oemUnlockByte);
      return Promise.resolve();
    }).catch(ex => {
      return Promise.reject(ex);
    });
  },

  /**
   * Computes the digest by reading the entire block of data and write it to the digest field
   *
   * @return true Promise<bool> Operation succeed
   * @return false Promise<bool> Operation failed
   */
  _computeAndWriteDigest: function() {
    debug("_computeAndWriteDigest()");
    let digest;
    let partition;
    return this._computeDigest().then(_digest => {
      digest = _digest;
      return OS.File.open(this._dataBlockFile, {write:true, existing:true, append:false});
    }).then(_partition => {
      partition = _partition;
      return partition.setPosition(DIGEST_OFFSET, OS.File.POS_START);
    }).then(() => {
      return partition.write(new Uint8Array([digest.calculated.charCodeAt(i) for (i in digest.calculated)]));
    }).then(bytesWrittenLength => {
      if (bytesWrittenLength != DIGEST_SIZE_BYTES) {
        log("_computeAndWriteDigest: Error writting digest to partition!. Expected: " + DIGEST_SIZE_BYTES + " Written: " + bytesWrittenLength);
        return Promise.reject();
      }
      return partition.close();
    }).then(() => {
      debug("_computeAndWriteDigest: digest written to partition");
      return Promise.resolve(true);
    }).catch(ex => {
      log("_computeAndWriteDigest: Couldn't write digest in the persistent partion. ex = " + ex );
      return Promise.reject(ex);
    });
  },

  /**
   * Formats the persistent partition if the OEM Unlock Enabled field is set to true, and
   * write the Unlock Property accordingly.
   *
   * @return true Promise<bool> OEM Unlock was enabled, so the partition has been formated
   * @return false Promise<bool> OEM Unlock was disabled, so the partition hasn't been formated
   */
  _formatIfOemUnlockEnabled: function () {
    debug("_formatIfOemUnlockEnabled()");
    return this.getOemUnlockEnabled().then(enabled => {
      this._libcutils.property_set(OEM_UNLOCK_PROPERTY,(enabled === true ? "1" : "0"));
      if (enabled === true) {
        return this._formatPartition(true);
      }
      return Promise.resolve(false);
    }).then(result => {
      if (result === false) {
        return Promise.resolve(false);
      } else {
        return Promise.resolve(true);
      }
    }).catch(ex => {
      log("_formatIfOemUnlockEnabled: An error ocurred!. ex = " + ex);
      return Promise.reject(ex);
    });
  },

  /**
   * Formats the persistent data partition with the proper structure.
   *
   * @param isSetOemUnlockEnabled {bool} If true, writes a "1" in the OEM Unlock Enabled field (last
   *                                     byte of the block). If false, writes a "0".
   *
   * @return Promise
   */
  _formatPartition: function(isSetOemUnlockEnabled) {
    debug("_formatPartition()");
    let partition;
    return OS.File.open(this._dataBlockFile, {write:true, existing:true, append:false}).then(_partition => {
      partition = _partition;
      return partition.write(new Uint8Array(DIGEST_SIZE_BYTES));
    }).then(bytesWrittenLength => {
      if (bytesWrittenLength != DIGEST_SIZE_BYTES) {
        log("_formatPartition Error writting zero-digest!. Expected: " + DIGEST_SIZE_BYTES + " Written: " + bytesWrittenLength);
        return Promise.reject();
      }
      return partition.write(new Uint32Array([PARTITION_MAGIC]));
    }).then(bytesWrittenLength => {
       if (bytesWrittenLength != PARTITION_MAGIC_SIZE_BYTES) {
        log("_formatPartition Error writting magic number!. Expected: " + PARTITION_MAGIC_SIZE_BYTES + " Written: " + bytesWrittenLength);
        return Promise.reject();
      }
      return partition.write(new Uint8Array(DATA_SIZE_BYTES));
    }).then(bytesWrittenLength => {
      if (bytesWrittenLength != DATA_SIZE_BYTES) {
        log("_formatPartition Error writting data size!. Expected: " + DATA_SIZE_BYTES + " Written: " + bytesWrittenLength);
        return Promise.reject();
      }
      return partition.close();
    }).then(() => {
      return this._doSetOemUnlockEnabled(isSetOemUnlockEnabled);
    }).then(() => {
      return this._computeAndWriteDigest();
    }).then(() => {
      return Promise.resolve();
    }).catch(ex => {
      log("_formatPartition: Failed to format block device!: ex = " + ex);
      return Promise.reject(ex);
    });
  },

  /**
   * Check digest validity. If it's not valid, formats the persistent partition
   *
   * @return true Promise<bool> The checksum is valid so the promise is resolved to true
   * @return false Promise<bool> The checksum is not valid, so the partition is going to be
   *                             formatted and the OEM Unlock Enabled field written to 0 (false).
   */
  _enforceChecksumValidity: function() {
    debug("_enforceChecksumValidity");
    return this._computeDigest(true).then(digest => {
      if (digest.stored != digest.calculated) {
        log("_enforceChecksumValidity: Validation failed! Stored digest: " + toHexString(digest.stored) +
            " is not the same as the calculated one: " + toHexString(digest.calculated));
        return Promise.reject();
      }
      debug("_enforceChecksumValidity: Digest computation succeed.");
      return Promise.resolve(true);
    }).catch(ex => {
      log("_enforceChecksumValidity: Digest computation failed: ex = " + ex);
      log("_enforceChecksumValidity: Formatting FRP partition...");
      return this._formatPartition(false).then(() => {
        return Promise.resolve(false);
      }).catch(ex => {
        log("_enforceChecksumValidity: Error ocurred while formating the partition!: ex = " + ex);
        return Promise.reject(ex);
      });
    });
  },

  /**
   * Reads the entire data field
   *
   * @return bytes Promise<Uint8Array> A promise resolved with the bytes read
   */
  read: function() {
    debug("read()");
    let partition;
    let bytes;
    let dataSize;
    return this.getDataFieldSize().then(_dataSize => {
      dataSize = _dataSize;
      return OS.File.open(this._dataBlockFile, {read:true, existing:true, append:false});
    }).then(_partition => {
      partition = _partition;
      return partition.setPosition(DIGEST_SIZE_BYTES + HEADER_SIZE_BYTES, OS.File.POS_START);
    }).then(() => {
      return partition.read(dataSize);
    }).then(_bytes => {
      bytes = _bytes;
      if (bytes.byteLength < dataSize) {
        log("read: Failed to read entire data block. Bytes read: " + bytes.byteLength + "/" + dataSize);
        return Promise.reject();
      }
      return partition.close();
    }).then(() => {
      return Promise.resolve(bytes);
    }).catch(ex => {
      log("read: Failed to read entire data block. Exception: " + ex);
      return Promise.reject(ex);
    });
  },

  /**
   * Writes an entire block to the persistent partition
   *
   * @param data {Uint8Array}
   *
   * @return Promise<Number> Promise resolved to the number of bytes written.
   */
  write: function(data) {
    debug("write()");
    // Ensure that we don't overwrite digest/magic/data-length and the last byte
    let maxBlockSize = this._getBlockDeviceSize() - (DIGEST_SIZE_BYTES + HEADER_SIZE_BYTES + 1);
    if (data.byteLength > maxBlockSize) {
      log("write: Couldn't write more than " + maxBlockSize + " bytes to the partition. " +
           maxBlockSize + " bytes given.");
      return Promise.reject();
    }

    let partition;
    return OS.File.open(this._dataBlockFile, {write:true, existing:true, append:false}).then(_partition => {
      let digest = new Uint8Array(DIGEST_SIZE_BYTES);
      let magic = new Uint8Array((new Uint32Array([PARTITION_MAGIC])).buffer);
      let dataLength = new Uint8Array((new Uint32Array([data.byteLength])).buffer);
      let bufferToWrite = new Uint8Array(digest.byteLength + magic.byteLength + dataLength.byteLength + data.byteLength );
      let offset = 0;
      bufferToWrite.set(digest, offset);
      offset += digest.byteLength;
      bufferToWrite.set(magic, offset);
      offset += magic.byteLength;
      bufferToWrite.set(dataLength, offset);
      offset += dataLength.byteLength;
      bufferToWrite.set(data, offset);
      partition = _partition;
      return partition.write(bufferToWrite);
    }).then(bytesWrittenLength => {
      let expectedWrittenLength = DIGEST_SIZE_BYTES + HEADER_SIZE_BYTES + data.byteLength;
      if (bytesWrittenLength != expectedWrittenLength) {
        log("write: Error writting data to partition!: Expected: " + expectedWrittenLength + " Written: " + bytesWrittenLength);
        return Promise.reject();
      }
      return partition.close();
    }).then(() => {
      return this._computeAndWriteDigest();
    }).then(couldComputeAndWriteDigest => {
      if (couldComputeAndWriteDigest === true) {
        return Promise.resolve(data.byteLength);
      } else {
        log("write: Failed to compute and write the digest");
        return Promise.reject();
      }
    }).catch(ex => {
      log("write: Failed to write to the persistent partition: ex = " + ex);
      return Promise.reject(ex);
    });
  },

  /**
   * Wipes the persistent partition.
   *
   * @return Promise If no errors, the promise is resolved
   */
  wipe: function() {
    debug("wipe()");

    if (this._testing === true) {
      log("wipe: No wipe() funcionality in testing mode");
      return Promise.resolve();
    }

#ifdef MOZ_WIDGET_GONK
    const O_READONLY = 0;
    const O_RDWR = 2;
    const O_NONBLOCK = 1 << 11;
    // This constant value is the same under 32 and 64 bits arch.
    const BLKSECDISCARD = 0x127D;
    // This constant value is the same under 32 and 64 bits arch.
    const BLKDISCARD = 0x1277;

    return new Promise((resolve, reject) => {
      let range = new this.ctypes.unsigned_long();
      let rangeAddress = range.address();
      let blockDeviceLength = this._getBlockDeviceSize();
      range[0] = 0;
      range[1] = blockDeviceLength;
      if (range[1] === 0) {
        log("wipe: Block device size is 0!");
        return reject();
      }
      let fd = this._libc.open(this._dataBlockFile, O_RDWR);
      if (fd < 0) {
        log("wipe: ERROR couldn't open partition!: error = " + this.ctypes.errno);
        return reject();
      }
      let ret = this._libc.ioctl(fd, BLKSECDISCARD, rangeAddress);
      if (ret < 0) {
        log("wipe: Something went wrong secure discarding block: errno: " + this.ctypes.errno + ": Falling back to non-secure discarding...");
        ret = this._libc.ioctl(fd, BLKDISCARD, rangeAddress);
        if (ret < 0) {
          this._libc.close(fd);
          log("wipe: CRITICAL: non-secure discarding failed too!!: errno: " + this.ctypes.errno);
          return reject();
        } else {
          this._libc.close(fd);
          log("wipe: non-secure discard used and succeed");
          return resolve();
        }
      }
      this._libc.close(fd);
      log("wipe: secure discard succeed");
      return resolve();
    });
#else
    log("wipe: ERROR: This feature is only supported in Gonk!");
    return Promise.reject();
#endif
  },

  /**
   * Set the OEM Unlock Enabled field (one byte at the end of the partition), to 1 or 0 depending on
   * the input parameter.
   *
   * @param enabled {bool} If enabled, we write a 1 in the last byte of the partition.
   *
   * @return Promise
   *
   */
  setOemUnlockEnabled: function(enabled) {
    debug("setOemUnlockEnabled()");
    return this._doSetOemUnlockEnabled(enabled).then(() => {
      return this._computeAndWriteDigest();
    }).then(() => {
      return Promise.resolve();
    }).catch(ex => {
      return Promise.reject(ex);
    });
  },

  /**
   * Gets the byte from the partition which represents the OEM Unlock Enabled state.
   *
   * @return true Promise<Bool> The user didn't activate KillSwitch.
   * @return false Promise<Bool> The user did activate KillSwitch.
   */
  getOemUnlockEnabled: function() {
    log("getOemUnlockEnabled()");
    let ret = false;
    let partition;
    return OS.File.open(this._dataBlockFile, {existing:true, append:false, read:true}).then(_partition => {
      partition = _partition;
      return partition.setPosition(this._getBlockDeviceSize() - OEM_UNLOCK_ENABLED_BYTES, OS.File.POS_START);
    }).then(() => {
      return partition.read(OEM_UNLOCK_ENABLED_BYTES);
    }).then(data => {
      debug("getOemUnlockEnabled: OEM unlock enabled byte = '" + data[0] + "'");
      ret = (data[0] === 1 ? true : false);
      return partition.close();
    }).then(() => {
      return Promise.resolve(ret);
    }).catch(ex => {
      log("getOemUnlockEnabled: Error reading OEM unlock enabled byte from partition: ex = " + ex);
      return Promise.reject(ex);
    });
  },

  /**
   * Gets the size of the data block by reading the data-length field
   *
   * @return Promise<Number> A promise resolved to the number of bytes os the data field.
   */
  getDataFieldSize: function() {
    debug("getDataFieldSize()");
    let partition
    let dataLength = 0;
    return OS.File.open(this._dataBlockFile, {read:true, existing:true, append:false}).then(_partition => {
      partition = _partition;
      // Skip the digest field
      return partition.setPosition(DIGEST_SIZE_BYTES, OS.File.POS_START);
    }).then(() => {
      // Read the Magic field
      return partition.read(PARTITION_MAGIC_SIZE_BYTES);
    }).then(_magic => {
      let magic = new Uint32Array(_magic.buffer)[0];
      if (magic === PARTITION_MAGIC) {
        return partition.read(PARTITION_MAGIC_SIZE_BYTES);
      } else {
        log("getDataFieldSize: ERROR: Invalid Magic number!");
        return Promise.reject();
      }
    }).then(_dataLength => {
      if (_dataLength) {
        dataLength = new Uint32Array(_dataLength.buffer)[0];
      }
      return partition.close();
    }).then(() => {
      if (dataLength && dataLength != 0) {
        return Promise.resolve(dataLength);
      } else {
        return Promise.reject();
      }
    }).catch(ex => {
      log("getDataFieldSize: Couldn't get data field size: ex = " + ex);
      return Promise.reject(ex);
    });
  },

  /**
   * Gets the maximum possible size of a data field
   *
   * @return Promise<Number> A Promise resolved to the maximum number of bytes allowed for the data field
   *
   */
  getMaximumDataBlockSize: function() {
    debug("getMaximumDataBlockSize()");
    return new Promise((resolve, reject) => {
      let actualSize = this._getBlockDeviceSize() - HEADER_SIZE_BYTES - OEM_UNLOCK_ENABLED_BYTES;
      resolve(actualSize <= MAX_DATA_BLOCK_SIZE ? actualSize : MAX_DATA_BLOCK_SIZE);
    });
  }

};

// This code should ALWAYS be living only on the parent side.
if (!inParent) {
  log("PersistentDataBlock should only be living on parent side.");
  throw Cr.NS_ERROR_ABORT;
} else {
  this.PersistentDataBlock.init();
}
