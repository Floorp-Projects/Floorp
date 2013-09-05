/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const INT32_MAX   = 2147483647;
const UINT8_SIZE  = 1;
const UINT16_SIZE = 2;
const UINT32_SIZE = 4;

/**
 * This object contains helpers buffering incoming data & deconstructing it
 * into parcels as well as buffering outgoing data & constructing parcels.
 * For that it maintains two buffers and corresponding uint8 views, indexes.
 *
 * The incoming buffer is a circular buffer where we store incoming data.
 * As soon as a complete parcel is received, it is processed right away, so
 * the buffer only needs to be large enough to hold one parcel.
 *
 * The outgoing buffer is to prepare outgoing parcels. The index is reset
 * every time a parcel is sent.
 */

let Buf = {
  PARCEL_SIZE_SIZE: UINT32_SIZE,

  mIncomingBufferLength: 1024,
  mIncomingBuffer: null,
  mIncomingBytes: null,
  mIncomingWriteIndex: 0,
  mIncomingReadIndex: 0,
  mReadIncoming: 0,
  mReadAvailable: 0,
  mCurrentParcelSize: 0,

  mOutgoingBufferLength: 1024,
  mOutgoingBuffer: null,
  mOutgoingBytes: null,
  mOutgoingIndex: 0,
  mOutgoingBufferCalSizeQueue: null,

  _init: function _init() {
    this.mIncomingBuffer = new ArrayBuffer(this.mIncomingBufferLength);
    this.mOutgoingBuffer = new ArrayBuffer(this.mOutgoingBufferLength);

    this.mIncomingBytes = new Uint8Array(this.mIncomingBuffer);
    this.mOutgoingBytes = new Uint8Array(this.mOutgoingBuffer);

    // Track where incoming data is read from and written to.
    this.mIncomingWriteIndex = 0;
    this.mIncomingReadIndex = 0;

    // Leave room for the parcel size for outgoing parcels.
    this.mOutgoingIndex = this.PARCEL_SIZE_SIZE;

    // How many bytes we've read for this parcel so far.
    this.mReadIncoming = 0;

    // How many bytes available as parcel data.
    this.mReadAvailable = 0;

    // Size of the incoming parcel. If this is zero, we're expecting a new
    // parcel.
    this.mCurrentParcelSize = 0;

    // Queue for storing outgoing override points
    this.mOutgoingBufferCalSizeQueue = [];
  },

  /**
   * Mark current mOutgoingIndex as start point for calculation length of data
   * written to mOutgoingBuffer.
   * Mark can be nested for here uses queue to remember marks.
   *
   * @param writeFunction
   *        Function to write data length into mOutgoingBuffer, this function is
   *        also used to allocate buffer for data length.
   *        Raw data size(in Uint8) is provided as parameter calling writeFunction.
   *        If raw data size is not in proper unit for writing, user can adjust
   *        the length value in writeFunction before writing.
   **/
  startCalOutgoingSize: function startCalOutgoingSize(writeFunction) {
    let sizeInfo = {index: this.mOutgoingIndex,
                    write: writeFunction};

    // Allocate buffer for data lemgtj.
    writeFunction.call(0);

    // Get size of data length buffer for it is not counted into data size.
    sizeInfo.size = this.mOutgoingIndex - sizeInfo.index;

    // Enqueue size calculation information.
    this.mOutgoingBufferCalSizeQueue.push(sizeInfo);
  },

  /**
   * Calculate data length since last mark, and write it into mark position.
   **/
  stopCalOutgoingSize: function stopCalOutgoingSize() {
    let sizeInfo = this.mOutgoingBufferCalSizeQueue.pop();

    // Remember current mOutgoingIndex.
    let currentOutgoingIndex = this.mOutgoingIndex;
    // Calculate data length, in uint8.
    let writeSize = this.mOutgoingIndex - sizeInfo.index - sizeInfo.size;

    // Write data length to mark, use same function for allocating buffer to make
    // sure there is no buffer overloading.
    this.mOutgoingIndex = sizeInfo.index;
    sizeInfo.write(writeSize);

    // Restore mOutgoingIndex.
    this.mOutgoingIndex = currentOutgoingIndex;
  },

  /**
   * Grow the incoming buffer.
   *
   * @param min_size
   *        Minimum new size. The actual new size will be the the smallest
   *        power of 2 that's larger than this number.
   */
  growIncomingBuffer: function growIncomingBuffer(min_size) {
    if (DEBUG) {
      debug("Current buffer of " + this.mIncomingBufferLength +
            " can't handle incoming " + min_size + " bytes.");
    }
    let oldBytes = this.mIncomingBytes;
    this.mIncomingBufferLength =
      2 << Math.floor(Math.log(min_size)/Math.log(2));
    if (DEBUG) debug("New incoming buffer size: " + this.mIncomingBufferLength);
    this.mIncomingBuffer = new ArrayBuffer(this.mIncomingBufferLength);
    this.mIncomingBytes = new Uint8Array(this.mIncomingBuffer);
    if (this.mIncomingReadIndex <= this.mIncomingWriteIndex) {
      // Read and write index are in natural order, so we can just copy
      // the old buffer over to the bigger one without having to worry
      // about the indexes.
      this.mIncomingBytes.set(oldBytes, 0);
    } else {
      // The write index has wrapped around but the read index hasn't yet.
      // Write whatever the read index has left to read until it would
      // circle around to the beginning of the new buffer, and the rest
      // behind that.
      let head = oldBytes.subarray(this.mIncomingReadIndex);
      let tail = oldBytes.subarray(0, this.mIncomingReadIndex);
      this.mIncomingBytes.set(head, 0);
      this.mIncomingBytes.set(tail, head.length);
      this.mIncomingReadIndex = 0;
      this.mIncomingWriteIndex += head.length;
    }
    if (DEBUG) {
      debug("New incoming buffer size is " + this.mIncomingBufferLength);
    }
  },

  /**
   * Grow the outgoing buffer.
   *
   * @param min_size
   *        Minimum new size. The actual new size will be the the smallest
   *        power of 2 that's larger than this number.
   */
  growOutgoingBuffer: function growOutgoingBuffer(min_size) {
    if (DEBUG) {
      debug("Current buffer of " + this.mOutgoingBufferLength +
            " is too small.");
    }
    let oldBytes = this.mOutgoingBytes;
    this.mOutgoingBufferLength =
      2 << Math.floor(Math.log(min_size)/Math.log(2));
    this.mOutgoingBuffer = new ArrayBuffer(this.mOutgoingBufferLength);
    this.mOutgoingBytes = new Uint8Array(this.mOutgoingBuffer);
    this.mOutgoingBytes.set(oldBytes, 0);
    if (DEBUG) {
      debug("New outgoing buffer size is " + this.mOutgoingBufferLength);
    }
  },

  /**
   * Functions for reading data from the incoming buffer.
   *
   * These are all little endian, apart from readParcelSize();
   */

  /**
   * Ensure position specified is readable.
   *
   * @param index
   *        Data position in incoming parcel, valid from 0 to
   *        mCurrentParcelSize.
   */
  ensureIncomingAvailable: function ensureIncomingAvailable(index) {
    if (index >= this.mCurrentParcelSize) {
      throw new Error("Trying to read data beyond the parcel end!");
    } else if (index < 0) {
      throw new Error("Trying to read data before the parcel begin!");
    }
  },

  /**
   * Seek in current incoming parcel.
   *
   * @param offset
   *        Seek offset in relative to current position.
   */
  seekIncoming: function seekIncoming(offset) {
    // Translate to 0..mCurrentParcelSize
    let cur = this.mCurrentParcelSize - this.mReadAvailable;

    let newIndex = cur + offset;
    this.ensureIncomingAvailable(newIndex);

    // ... mIncomingReadIndex -->|
    // 0               new     cur           mCurrentParcelSize
    // |================|=======|====================|
    // |<--        cur       -->|<- mReadAvailable ->|
    // |<-- newIndex -->|<--  new mReadAvailable  -->|
    this.mReadAvailable = this.mCurrentParcelSize - newIndex;

    // Translate back:
    if (this.mIncomingReadIndex < cur) {
      // The mIncomingReadIndex is wrapped.
      newIndex += this.mIncomingBufferLength;
    }
    newIndex += (this.mIncomingReadIndex - cur);
    newIndex %= this.mIncomingBufferLength;
    this.mIncomingReadIndex = newIndex;
  },

  readUint8Unchecked: function readUint8Unchecked() {
    let value = this.mIncomingBytes[this.mIncomingReadIndex];
    this.mIncomingReadIndex = (this.mIncomingReadIndex + 1) %
                             this.mIncomingBufferLength;
    return value;
  },

  readUint8: function readUint8() {
    // Translate to 0..mCurrentParcelSize
    let cur = this.mCurrentParcelSize - this.mReadAvailable;
    this.ensureIncomingAvailable(cur);

    this.mReadAvailable--;
    return this.readUint8Unchecked();
  },

  readUint8Array: function readUint8Array(length) {
    // Translate to 0..mCurrentParcelSize
    let last = this.mCurrentParcelSize - this.mReadAvailable;
    last += (length - 1);
    this.ensureIncomingAvailable(last);

    let array = new Uint8Array(length);
    for (let i = 0; i < length; i++) {
      array[i] = this.readUint8Unchecked();
    }

    this.mReadAvailable -= length;
    return array;
  },

  readUint16: function readUint16() {
    return this.readUint8() | this.readUint8() << 8;
  },

  readUint32: function readUint32() {
    return this.readUint8()       | this.readUint8() <<  8 |
           this.readUint8() << 16 | this.readUint8() << 24;
  },

  readUint32List: function readUint32List() {
    let length = this.readUint32();
    let ints = [];
    for (let i = 0; i < length; i++) {
      ints.push(this.readUint32());
    }
    return ints;
  },

  readString: function readString() {
    let string_len = this.readUint32();
    if (string_len < 0 || string_len >= INT32_MAX) {
      return null;
    }
    let s = "";
    for (let i = 0; i < string_len; i++) {
      s += String.fromCharCode(this.readUint16());
    }
    // Strings are \0\0 delimited, but that isn't part of the length. And
    // if the string length is even, the delimiter is two characters wide.
    // It's insane, I know.
    this.readStringDelimiter(string_len);
    return s;
  },

  readStringList: function readStringList() {
    let num_strings = this.readUint32();
    let strings = [];
    for (let i = 0; i < num_strings; i++) {
      strings.push(this.readString());
    }
    return strings;
  },

  readStringDelimiter: function readStringDelimiter(length) {
    let delimiter = this.readUint16();
    if (!(length & 1)) {
      delimiter |= this.readUint16();
    }
    if (DEBUG) {
      if (delimiter !== 0) {
        debug("Something's wrong, found string delimiter: " + delimiter);
      }
    }
  },

  readParcelSize: function readParcelSize() {
    return this.readUint8Unchecked() << 24 |
           this.readUint8Unchecked() << 16 |
           this.readUint8Unchecked() <<  8 |
           this.readUint8Unchecked();
  },

  /**
   * Functions for writing data to the outgoing buffer.
   */

  /**
   * Ensure position specified is writable.
   *
   * @param index
   *        Data position in outgoing parcel, valid from 0 to
   *        mOutgoingBufferLength.
   */
  ensureOutgoingAvailable: function ensureOutgoingAvailable(index) {
    if (index >= this.mOutgoingBufferLength) {
      this.growOutgoingBuffer(index + 1);
    }
  },

  writeUint8: function writeUint8(value) {
    this.ensureOutgoingAvailable(this.mOutgoingIndex);

    this.mOutgoingBytes[this.mOutgoingIndex] = value;
    this.mOutgoingIndex++;
  },

  writeUint16: function writeUint16(value) {
    this.writeUint8(value & 0xff);
    this.writeUint8((value >> 8) & 0xff);
  },

  writeUint32: function writeUint32(value) {
    this.writeUint8(value & 0xff);
    this.writeUint8((value >> 8) & 0xff);
    this.writeUint8((value >> 16) & 0xff);
    this.writeUint8((value >> 24) & 0xff);
  },

  writeString: function writeString(value) {
    if (value == null) {
      this.writeUint32(-1);
      return;
    }
    this.writeUint32(value.length);
    for (let i = 0; i < value.length; i++) {
      this.writeUint16(value.charCodeAt(i));
    }
    // Strings are \0\0 delimited, but that isn't part of the length. And
    // if the string length is even, the delimiter is two characters wide.
    // It's insane, I know.
    this.writeStringDelimiter(value.length);
  },

  writeStringList: function writeStringList(strings) {
    this.writeUint32(strings.length);
    for (let i = 0; i < strings.length; i++) {
      this.writeString(strings[i]);
    }
  },

  writeStringDelimiter: function writeStringDelimiter(length) {
    this.writeUint16(0);
    if (!(length & 1)) {
      this.writeUint16(0);
    }
  },

  writeParcelSize: function writeParcelSize(value) {
    /**
     *  Parcel size will always be the first thing in the parcel byte
     *  array, but the last thing written. Store the current index off
     *  to a temporary to be reset after we write the size.
     */
    let currentIndex = this.mOutgoingIndex;
    this.mOutgoingIndex = 0;
    this.writeUint8((value >> 24) & 0xff);
    this.writeUint8((value >> 16) & 0xff);
    this.writeUint8((value >> 8) & 0xff);
    this.writeUint8(value & 0xff);
    this.mOutgoingIndex = currentIndex;
  },

  copyIncomingToOutgoing: function copyIncomingToOutgoing(length) {
    if (!length || (length < 0)) {
      return;
    }

    let translatedReadIndexEnd =
      this.mCurrentParcelSize - this.mReadAvailable + length - 1;
    this.ensureIncomingAvailable(translatedReadIndexEnd);

    let translatedWriteIndexEnd = this.mOutgoingIndex + length - 1;
    this.ensureOutgoingAvailable(translatedWriteIndexEnd);

    let newIncomingReadIndex = this.mIncomingReadIndex + length;
    if (newIncomingReadIndex < this.mIncomingBufferLength) {
      // Reading won't cause wrapping, go ahead with builtin copy.
      this.mOutgoingBytes
          .set(this.mIncomingBytes.subarray(this.mIncomingReadIndex,
                                            newIncomingReadIndex),
               this.mOutgoingIndex);
    } else {
      // Not so lucky.
      newIncomingReadIndex %= this.mIncomingBufferLength;
      this.mOutgoingBytes
          .set(this.mIncomingBytes.subarray(this.mIncomingReadIndex,
                                            this.mIncomingBufferLength),
               this.mOutgoingIndex);
      if (newIncomingReadIndex) {
        let firstPartLength = this.mIncomingBufferLength - this.mIncomingReadIndex;
        this.mOutgoingBytes.set(this.mIncomingBytes.subarray(0, newIncomingReadIndex),
                               this.mOutgoingIndex + firstPartLength);
      }
    }

    this.mIncomingReadIndex = newIncomingReadIndex;
    this.mReadAvailable -= length;
    this.mOutgoingIndex += length;
  },

  /**
   * Parcel management
   */

  /**
   * Write incoming data to the circular buffer.
   *
   * @param incoming
   *        Uint8Array containing the incoming data.
   */
  writeToIncoming: function writeToIncoming(incoming) {
    // We don't have to worry about the head catching the tail since
    // we process any backlog in parcels immediately, before writing
    // new data to the buffer. So the only edge case we need to handle
    // is when the incoming data is larger than the buffer size.
    let minMustAvailableSize = incoming.length + this.mReadIncoming;
    if (minMustAvailableSize > this.mIncomingBufferLength) {
      this.growIncomingBuffer(minMustAvailableSize);
    }

    // We can let the typed arrays do the copying if the incoming data won't
    // wrap around the edges of the circular buffer.
    let remaining = this.mIncomingBufferLength - this.mIncomingWriteIndex;
    if (remaining >= incoming.length) {
      this.mIncomingBytes.set(incoming, this.mIncomingWriteIndex);
    } else {
      // The incoming data would wrap around it.
      let head = incoming.subarray(0, remaining);
      let tail = incoming.subarray(remaining);
      this.mIncomingBytes.set(head, this.mIncomingWriteIndex);
      this.mIncomingBytes.set(tail, 0);
    }
    this.mIncomingWriteIndex = (this.mIncomingWriteIndex + incoming.length) %
                               this.mIncomingBufferLength;
  },

  /**
   * Process incoming data.
   *
   * @param incoming
   *        Uint8Array containing the incoming data.
   */
  processIncoming: function processIncoming(incoming) {
    if (DEBUG) {
      debug("Received " + incoming.length + " bytes.");
      debug("Already read " + this.mReadIncoming);
    }

    this.writeToIncoming(incoming);
    this.mReadIncoming += incoming.length;
    while (true) {
      if (!this.mCurrentParcelSize) {
        // We're expecting a new parcel.
        if (this.mReadIncoming < this.PARCEL_SIZE_SIZE) {
          // We don't know how big the next parcel is going to be, need more
          // data.
          if (DEBUG) debug("Next parcel size unknown, going to sleep.");
          return;
        }
        this.mCurrentParcelSize = this.readParcelSize();
        if (DEBUG) {
          debug("New incoming parcel of size " + this.mCurrentParcelSize);
        }
        // The size itself is not included in the size.
        this.mReadIncoming -= this.PARCEL_SIZE_SIZE;
      }

      if (this.mReadIncoming < this.mCurrentParcelSize) {
        // We haven't read enough yet in order to be able to process a parcel.
        if (DEBUG) debug("Read " + this.mReadIncoming + ", but parcel size is "
                         + this.mCurrentParcelSize + ". Going to sleep.");
        return;
      }

      // Alright, we have enough data to process at least one whole parcel.
      // Let's do that.
      let expectedAfterIndex = (this.mIncomingReadIndex + this.mCurrentParcelSize)
                               % this.mIncomingBufferLength;

      if (DEBUG) {
        let parcel;
        if (expectedAfterIndex < this.mIncomingReadIndex) {
          let head = this.mIncomingBytes.subarray(this.mIncomingReadIndex);
          let tail = this.mIncomingBytes.subarray(0, expectedAfterIndex);
          parcel = Array.slice(head).concat(Array.slice(tail));
        } else {
          parcel = Array.slice(this.mIncomingBytes.subarray(
            this.mIncomingReadIndex, expectedAfterIndex));
        }
        debug("Parcel (size " + this.mCurrentParcelSize + "): " + parcel);
      }

      if (DEBUG) debug("We have at least one complete parcel.");
      try {
        this.mReadAvailable = this.mCurrentParcelSize;
        this.processParcel();
      } catch (ex) {
        if (DEBUG) debug("Parcel handling threw " + ex + "\n" + ex.stack);
      }

      // Ensure that the whole parcel was consumed.
      if (this.mIncomingReadIndex != expectedAfterIndex) {
        if (DEBUG) {
          debug("Parcel handler didn't consume whole parcel, " +
                Math.abs(expectedAfterIndex - this.mIncomingReadIndex) +
                " bytes left over");
        }
        this.mIncomingReadIndex = expectedAfterIndex;
      }
      this.mReadIncoming -= this.mCurrentParcelSize;
      this.mReadAvailable = 0;
      this.mCurrentParcelSize = 0;
    }
  },

  /**
   * Communicate with the IPC thread.
   */
  sendParcel: function sendParcel() {
    // Compute the size of the parcel and write it to the front of the parcel
    // where we left room for it. Note that he parcel size does not include
    // the size itself.
    let parcelSize = this.mOutgoingIndex - this.PARCEL_SIZE_SIZE;
    this.writeParcelSize(parcelSize);

    // This assumes that postRILMessage will make a copy of the ArrayBufferView
    // right away!
    let parcel = this.mOutgoingBytes.subarray(0, this.mOutgoingIndex);
    if (DEBUG) debug("Outgoing parcel: " + Array.slice(parcel));
    this.onSendParcel(parcel);
    this.mOutgoingIndex = this.PARCEL_SIZE_SIZE;
  },

  getCurrentParcelSize: function getCurrentParcelSize() {
    return this.mCurrentParcelSize;
  },

  getReadAvailable: function getReadAvailable() {
    return this.mReadAvailable;
  }

  /**
   * Process one parcel.
   *
   * |processParcel| is an implementation provided incoming parcel processing
   * function invoked when we have received a complete parcel.  Implementation
   * may call multiple read functions to extract data from the incoming buffer.
   */
  //processParcel: function processParcel() {
  //  let something = this.readUint32();
  //  ...
  //},

  /**
   * Write raw data out to underlying channel.
   *
   * |onSendParcel| is an implementation provided stream output function
   * invoked when we're really going to write something out.  We assume the
   * data are completely copied to some output buffer in this call and may
   * be destroyed when it's done.
   *
   * @param parcel
   *        An array of numeric octet data.
   */
  //onSendParcel: function onSendParcel(parcel) {
  //  ...
  //}
};

module.exports = { Buf: Buf };
