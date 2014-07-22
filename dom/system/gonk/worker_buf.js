/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


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
  INT32_MAX: 2147483647,
  UINT8_SIZE: 1,
  UINT16_SIZE: 2,
  UINT32_SIZE: 4,
  PARCEL_SIZE_SIZE: 4,
  PDU_HEX_OCTET_SIZE: 4,

  incomingBufferLength: 1024,
  incomingBuffer: null,
  incomingBytes: null,
  incomingWriteIndex: 0,
  incomingReadIndex: 0,
  readIncoming: 0,
  readAvailable: 0,
  currentParcelSize: 0,

  outgoingBufferLength: 1024,
  outgoingBuffer: null,
  outgoingBytes: null,
  outgoingIndex: 0,
  outgoingBufferCalSizeQueue: null,

  _init: function() {
    this.incomingBuffer = new ArrayBuffer(this.incomingBufferLength);
    this.outgoingBuffer = new ArrayBuffer(this.outgoingBufferLength);

    this.incomingBytes = new Uint8Array(this.incomingBuffer);
    this.outgoingBytes = new Uint8Array(this.outgoingBuffer);

    // Track where incoming data is read from and written to.
    this.incomingWriteIndex = 0;
    this.incomingReadIndex = 0;

    // Leave room for the parcel size for outgoing parcels.
    this.outgoingIndex = this.PARCEL_SIZE_SIZE;

    // How many bytes we've read for this parcel so far.
    this.readIncoming = 0;

    // How many bytes available as parcel data.
    this.readAvailable = 0;

    // Size of the incoming parcel. If this is zero, we're expecting a new
    // parcel.
    this.currentParcelSize = 0;

    // Queue for storing outgoing override points
    this.outgoingBufferCalSizeQueue = [];
  },

  /**
   * Mark current outgoingIndex as start point for calculation length of data
   * written to outgoingBuffer.
   * Mark can be nested for here uses queue to remember marks.
   *
   * @param writeFunction
   *        Function to write data length into outgoingBuffer, this function is
   *        also used to allocate buffer for data length.
   *        Raw data size(in Uint8) is provided as parameter calling writeFunction.
   *        If raw data size is not in proper unit for writing, user can adjust
   *        the length value in writeFunction before writing.
   **/
  startCalOutgoingSize: function(writeFunction) {
    let sizeInfo = {index: this.outgoingIndex,
                    write: writeFunction};

    // Allocate buffer for data lemgtj.
    writeFunction.call(0);

    // Get size of data length buffer for it is not counted into data size.
    sizeInfo.size = this.outgoingIndex - sizeInfo.index;

    // Enqueue size calculation information.
    this.outgoingBufferCalSizeQueue.push(sizeInfo);
  },

  /**
   * Calculate data length since last mark, and write it into mark position.
   **/
  stopCalOutgoingSize: function() {
    let sizeInfo = this.outgoingBufferCalSizeQueue.pop();

    // Remember current outgoingIndex.
    let currentOutgoingIndex = this.outgoingIndex;
    // Calculate data length, in uint8.
    let writeSize = this.outgoingIndex - sizeInfo.index - sizeInfo.size;

    // Write data length to mark, use same function for allocating buffer to make
    // sure there is no buffer overloading.
    this.outgoingIndex = sizeInfo.index;
    sizeInfo.write(writeSize);

    // Restore outgoingIndex.
    this.outgoingIndex = currentOutgoingIndex;
  },

  /**
   * Grow the incoming buffer.
   *
   * @param min_size
   *        Minimum new size. The actual new size will be the the smallest
   *        power of 2 that's larger than this number.
   */
  growIncomingBuffer: function(min_size) {
    if (DEBUG) {
      debug("Current buffer of " + this.incomingBufferLength +
            " can't handle incoming " + min_size + " bytes.");
    }
    let oldBytes = this.incomingBytes;
    this.incomingBufferLength =
      2 << Math.floor(Math.log(min_size)/Math.log(2));
    if (DEBUG) debug("New incoming buffer size: " + this.incomingBufferLength);
    this.incomingBuffer = new ArrayBuffer(this.incomingBufferLength);
    this.incomingBytes = new Uint8Array(this.incomingBuffer);
    if (this.incomingReadIndex <= this.incomingWriteIndex) {
      // Read and write index are in natural order, so we can just copy
      // the old buffer over to the bigger one without having to worry
      // about the indexes.
      this.incomingBytes.set(oldBytes, 0);
    } else {
      // The write index has wrapped around but the read index hasn't yet.
      // Write whatever the read index has left to read until it would
      // circle around to the beginning of the new buffer, and the rest
      // behind that.
      let head = oldBytes.subarray(this.incomingReadIndex);
      let tail = oldBytes.subarray(0, this.incomingReadIndex);
      this.incomingBytes.set(head, 0);
      this.incomingBytes.set(tail, head.length);
      this.incomingReadIndex = 0;
      this.incomingWriteIndex += head.length;
    }
    if (DEBUG) {
      debug("New incoming buffer size is " + this.incomingBufferLength);
    }
  },

  /**
   * Grow the outgoing buffer.
   *
   * @param min_size
   *        Minimum new size. The actual new size will be the the smallest
   *        power of 2 that's larger than this number.
   */
  growOutgoingBuffer: function(min_size) {
    if (DEBUG) {
      debug("Current buffer of " + this.outgoingBufferLength +
            " is too small.");
    }
    let oldBytes = this.outgoingBytes;
    this.outgoingBufferLength =
      2 << Math.floor(Math.log(min_size)/Math.log(2));
    this.outgoingBuffer = new ArrayBuffer(this.outgoingBufferLength);
    this.outgoingBytes = new Uint8Array(this.outgoingBuffer);
    this.outgoingBytes.set(oldBytes, 0);
    if (DEBUG) {
      debug("New outgoing buffer size is " + this.outgoingBufferLength);
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
   *        currentParcelSize.
   */
  ensureIncomingAvailable: function(index) {
    if (index >= this.currentParcelSize) {
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
  seekIncoming: function(offset) {
    // Translate to 0..currentParcelSize
    let cur = this.currentParcelSize - this.readAvailable;

    let newIndex = cur + offset;
    this.ensureIncomingAvailable(newIndex);

    // ... incomingReadIndex -->|
    // 0               new     cur           currentParcelSize
    // |================|=======|====================|
    // |<--        cur       -->|<- readAvailable ->|
    // |<-- newIndex -->|<--  new readAvailable  -->|
    this.readAvailable = this.currentParcelSize - newIndex;

    // Translate back:
    if (this.incomingReadIndex < cur) {
      // The incomingReadIndex is wrapped.
      newIndex += this.incomingBufferLength;
    }
    newIndex += (this.incomingReadIndex - cur);
    newIndex %= this.incomingBufferLength;
    this.incomingReadIndex = newIndex;
  },

  readUint8Unchecked: function() {
    let value = this.incomingBytes[this.incomingReadIndex];
    this.incomingReadIndex = (this.incomingReadIndex + 1) %
                             this.incomingBufferLength;
    return value;
  },

  readUint8: function() {
    // Translate to 0..currentParcelSize
    let cur = this.currentParcelSize - this.readAvailable;
    this.ensureIncomingAvailable(cur);

    this.readAvailable--;
    return this.readUint8Unchecked();
  },

  readUint8Array: function(length) {
    // Translate to 0..currentParcelSize
    let last = this.currentParcelSize - this.readAvailable;
    last += (length - 1);
    this.ensureIncomingAvailable(last);

    let array = new Uint8Array(length);
    for (let i = 0; i < length; i++) {
      array[i] = this.readUint8Unchecked();
    }

    this.readAvailable -= length;
    return array;
  },

  readUint16: function() {
    return this.readUint8() | this.readUint8() << 8;
  },

  readInt32: function() {
    return this.readUint8()       | this.readUint8() <<  8 |
           this.readUint8() << 16 | this.readUint8() << 24;
  },

  readInt64: function() {
    // Avoid using bitwise operators as the operands of all bitwise operators
    // are converted to signed 32-bit integers.
    return this.readUint8()                   +
           this.readUint8() * Math.pow(2, 8)  +
           this.readUint8() * Math.pow(2, 16) +
           this.readUint8() * Math.pow(2, 24) +
           this.readUint8() * Math.pow(2, 32) +
           this.readUint8() * Math.pow(2, 40) +
           this.readUint8() * Math.pow(2, 48) +
           this.readUint8() * Math.pow(2, 56);
  },

  readInt32List: function() {
    let length = this.readInt32();
    let ints = [];
    for (let i = 0; i < length; i++) {
      ints.push(this.readInt32());
    }
    return ints;
  },

  readString: function() {
    let string_len = this.readInt32();
    if (string_len < 0 || string_len >= this.INT32_MAX) {
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

  readStringList: function() {
    let num_strings = this.readInt32();
    let strings = [];
    for (let i = 0; i < num_strings; i++) {
      strings.push(this.readString());
    }
    return strings;
  },

  readStringDelimiter: function(length) {
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

  readParcelSize: function() {
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
   *        outgoingBufferLength.
   */
  ensureOutgoingAvailable: function(index) {
    if (index >= this.outgoingBufferLength) {
      this.growOutgoingBuffer(index + 1);
    }
  },

  writeUint8: function(value) {
    this.ensureOutgoingAvailable(this.outgoingIndex);

    this.outgoingBytes[this.outgoingIndex] = value;
    this.outgoingIndex++;
  },

  writeUint16: function(value) {
    this.writeUint8(value & 0xff);
    this.writeUint8((value >> 8) & 0xff);
  },

  writeInt32: function(value) {
    this.writeUint8(value & 0xff);
    this.writeUint8((value >> 8) & 0xff);
    this.writeUint8((value >> 16) & 0xff);
    this.writeUint8((value >> 24) & 0xff);
  },

  writeString: function(value) {
    if (value == null) {
      this.writeInt32(-1);
      return;
    }
    this.writeInt32(value.length);
    for (let i = 0; i < value.length; i++) {
      this.writeUint16(value.charCodeAt(i));
    }
    // Strings are \0\0 delimited, but that isn't part of the length. And
    // if the string length is even, the delimiter is two characters wide.
    // It's insane, I know.
    this.writeStringDelimiter(value.length);
  },

  writeStringList: function(strings) {
    this.writeInt32(strings.length);
    for (let i = 0; i < strings.length; i++) {
      this.writeString(strings[i]);
    }
  },

  writeStringDelimiter: function(length) {
    this.writeUint16(0);
    if (!(length & 1)) {
      this.writeUint16(0);
    }
  },

  writeParcelSize: function(value) {
    /**
     *  Parcel size will always be the first thing in the parcel byte
     *  array, but the last thing written. Store the current index off
     *  to a temporary to be reset after we write the size.
     */
    let currentIndex = this.outgoingIndex;
    this.outgoingIndex = 0;
    this.writeUint8((value >> 24) & 0xff);
    this.writeUint8((value >> 16) & 0xff);
    this.writeUint8((value >> 8) & 0xff);
    this.writeUint8(value & 0xff);
    this.outgoingIndex = currentIndex;
  },

  copyIncomingToOutgoing: function(length) {
    if (!length || (length < 0)) {
      return;
    }

    let translatedReadIndexEnd =
      this.currentParcelSize - this.readAvailable + length - 1;
    this.ensureIncomingAvailable(translatedReadIndexEnd);

    let translatedWriteIndexEnd = this.outgoingIndex + length - 1;
    this.ensureOutgoingAvailable(translatedWriteIndexEnd);

    let newIncomingReadIndex = this.incomingReadIndex + length;
    if (newIncomingReadIndex < this.incomingBufferLength) {
      // Reading won't cause wrapping, go ahead with builtin copy.
      this.outgoingBytes
          .set(this.incomingBytes.subarray(this.incomingReadIndex,
                                           newIncomingReadIndex),
               this.outgoingIndex);
    } else {
      // Not so lucky.
      newIncomingReadIndex %= this.incomingBufferLength;
      this.outgoingBytes
          .set(this.incomingBytes.subarray(this.incomingReadIndex,
                                           this.incomingBufferLength),
               this.outgoingIndex);
      if (newIncomingReadIndex) {
        let firstPartLength = this.incomingBufferLength - this.incomingReadIndex;
        this.outgoingBytes.set(this.incomingBytes.subarray(0, newIncomingReadIndex),
                               this.outgoingIndex + firstPartLength);
      }
    }

    this.incomingReadIndex = newIncomingReadIndex;
    this.readAvailable -= length;
    this.outgoingIndex += length;
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
  writeToIncoming: function(incoming) {
    // We don't have to worry about the head catching the tail since
    // we process any backlog in parcels immediately, before writing
    // new data to the buffer. So the only edge case we need to handle
    // is when the incoming data is larger than the buffer size.
    let minMustAvailableSize = incoming.length + this.readIncoming;
    if (minMustAvailableSize > this.incomingBufferLength) {
      this.growIncomingBuffer(minMustAvailableSize);
    }

    // We can let the typed arrays do the copying if the incoming data won't
    // wrap around the edges of the circular buffer.
    let remaining = this.incomingBufferLength - this.incomingWriteIndex;
    if (remaining >= incoming.length) {
      this.incomingBytes.set(incoming, this.incomingWriteIndex);
    } else {
      // The incoming data would wrap around it.
      let head = incoming.subarray(0, remaining);
      let tail = incoming.subarray(remaining);
      this.incomingBytes.set(head, this.incomingWriteIndex);
      this.incomingBytes.set(tail, 0);
    }
    this.incomingWriteIndex = (this.incomingWriteIndex + incoming.length) %
                               this.incomingBufferLength;
  },

  /**
   * Process incoming data.
   *
   * @param incoming
   *        Uint8Array containing the incoming data.
   */
  processIncoming: function(incoming) {
    if (DEBUG) {
      debug("Received " + incoming.length + " bytes.");
      debug("Already read " + this.readIncoming);
    }

    this.writeToIncoming(incoming);
    this.readIncoming += incoming.length;
    while (true) {
      if (!this.currentParcelSize) {
        // We're expecting a new parcel.
        if (this.readIncoming < this.PARCEL_SIZE_SIZE) {
          // We don't know how big the next parcel is going to be, need more
          // data.
          if (DEBUG) debug("Next parcel size unknown, going to sleep.");
          return;
        }
        this.currentParcelSize = this.readParcelSize();
        if (DEBUG) {
          debug("New incoming parcel of size " + this.currentParcelSize);
        }
        // The size itself is not included in the size.
        this.readIncoming -= this.PARCEL_SIZE_SIZE;
      }

      if (this.readIncoming < this.currentParcelSize) {
        // We haven't read enough yet in order to be able to process a parcel.
        if (DEBUG) debug("Read " + this.readIncoming + ", but parcel size is "
                         + this.currentParcelSize + ". Going to sleep.");
        return;
      }

      // Alright, we have enough data to process at least one whole parcel.
      // Let's do that.
      let expectedAfterIndex = (this.incomingReadIndex + this.currentParcelSize)
                               % this.incomingBufferLength;

      if (DEBUG) {
        let parcel;
        if (expectedAfterIndex < this.incomingReadIndex) {
          let head = this.incomingBytes.subarray(this.incomingReadIndex);
          let tail = this.incomingBytes.subarray(0, expectedAfterIndex);
          parcel = Array.slice(head).concat(Array.slice(tail));
        } else {
          parcel = Array.slice(this.incomingBytes.subarray(
            this.incomingReadIndex, expectedAfterIndex));
        }
        debug("Parcel (size " + this.currentParcelSize + "): " + parcel);
      }

      if (DEBUG) debug("We have at least one complete parcel.");
      try {
        this.readAvailable = this.currentParcelSize;
        this.processParcel();
      } catch (ex) {
        if (DEBUG) debug("Parcel handling threw " + ex + "\n" + ex.stack);
      }

      // Ensure that the whole parcel was consumed.
      if (this.incomingReadIndex != expectedAfterIndex) {
        if (DEBUG) {
          debug("Parcel handler didn't consume whole parcel, " +
                Math.abs(expectedAfterIndex - this.incomingReadIndex) +
                " bytes left over");
        }
        this.incomingReadIndex = expectedAfterIndex;
      }
      this.readIncoming -= this.currentParcelSize;
      this.readAvailable = 0;
      this.currentParcelSize = 0;
    }
  },

  /**
   * Communicate with the IPC thread.
   */
  sendParcel: function() {
    // Compute the size of the parcel and write it to the front of the parcel
    // where we left room for it. Note that he parcel size does not include
    // the size itself.
    let parcelSize = this.outgoingIndex - this.PARCEL_SIZE_SIZE;
    this.writeParcelSize(parcelSize);

    // This assumes that postRILMessage will make a copy of the ArrayBufferView
    // right away!
    let parcel = this.outgoingBytes.subarray(0, this.outgoingIndex);
    if (DEBUG) debug("Outgoing parcel: " + Array.slice(parcel));
    this.onSendParcel(parcel);
    this.outgoingIndex = this.PARCEL_SIZE_SIZE;
  },

  getCurrentParcelSize: function() {
    return this.currentParcelSize;
  },

  getReadAvailable: function() {
    return this.readAvailable;
  }

  /**
   * Process one parcel.
   *
   * |processParcel| is an implementation provided incoming parcel processing
   * function invoked when we have received a complete parcel.  Implementation
   * may call multiple read functions to extract data from the incoming buffer.
   */
  //processParcel: function() {
  //  let something = this.readInt32();
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
  //onSendParcel: function(parcel) {
  //  ...
  //}
};

module.exports = { Buf: Buf };
