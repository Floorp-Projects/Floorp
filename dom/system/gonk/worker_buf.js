/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const INT32_MAX   = 2147483647;
const UINT8_SIZE  = 1;
const UINT16_SIZE = 2;
const UINT32_SIZE = 4;
const PARCEL_SIZE_SIZE = UINT32_SIZE;

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

let mIncomingBufferLength = 1024;
let mOutgoingBufferLength = 1024;
let mIncomingBuffer, mOutgoingBuffer, mIncomingBytes, mOutgoingBytes,
    mIncomingWriteIndex, mIncomingReadIndex, mOutgoingIndex, mReadIncoming,
    mReadAvailable, mCurrentParcelSize, mToken, mTokenRequestMap,
    mLasSolicitedToken, mOutgoingBufferCalSizeQueue, mOutputStream;

function init() {
  mIncomingBuffer = new ArrayBuffer(mIncomingBufferLength);
  mOutgoingBuffer = new ArrayBuffer(mOutgoingBufferLength);

  mIncomingBytes = new Uint8Array(mIncomingBuffer);
  mOutgoingBytes = new Uint8Array(mOutgoingBuffer);

  // Track where incoming data is read from and written to.
  mIncomingWriteIndex = 0;
  mIncomingReadIndex = 0;

  // Leave room for the parcel size for outgoing parcels.
  mOutgoingIndex = PARCEL_SIZE_SIZE;

  // How many bytes we've read for this parcel so far.
  mReadIncoming = 0;

  // How many bytes available as parcel data.
  mReadAvailable = 0;

  // Size of the incoming parcel. If this is zero, we're expecting a new
  // parcel.
  mCurrentParcelSize = 0;

  // This gets incremented each time we send out a parcel.
  mToken = 1;

  // Maps tokens we send out with requests to the request type, so that
  // when we get a response parcel back, we know what request it was for.
  mTokenRequestMap = {};

  // This is the token of last solicited response.
  mLasSolicitedToken = 0;

  // Queue for storing outgoing override points
  mOutgoingBufferCalSizeQueue = [];
}

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
function startCalOutgoingSize(writeFunction) {
  let sizeInfo = {index: mOutgoingIndex,
                  write: writeFunction};

  // Allocate buffer for data lemgtj.
  writeFunction.call(0);

  // Get size of data length buffer for it is not counted into data size.
  sizeInfo.size = mOutgoingIndex - sizeInfo.index;

  // Enqueue size calculation information.
  mOutgoingBufferCalSizeQueue.push(sizeInfo);
}

/**
 * Calculate data length since last mark, and write it into mark position.
 **/
function stopCalOutgoingSize() {
  let sizeInfo = mOutgoingBufferCalSizeQueue.pop();

  // Remember current mOutgoingIndex.
  let currentOutgoingIndex = mOutgoingIndex;
  // Calculate data length, in uint8.
  let writeSize = mOutgoingIndex - sizeInfo.index - sizeInfo.size;

  // Write data length to mark, use same function for allocating buffer to make
  // sure there is no buffer overloading.
  mOutgoingIndex = sizeInfo.index;
  sizeInfo.write(writeSize);

  // Restore mOutgoingIndex.
  mOutgoingIndex = currentOutgoingIndex;
}

/**
 * Grow the incoming buffer.
 *
 * @param min_size
 *        Minimum new size. The actual new size will be the the smallest
 *        power of 2 that's larger than this number.
 */
function growIncomingBuffer(min_size) {
  if (DEBUG) {
    debug("Current buffer of " + mIncomingBufferLength +
          " can't handle incoming " + min_size + " bytes.");
  }
  let oldBytes = mIncomingBytes;
  mIncomingBufferLength =
    2 << Math.floor(Math.log(min_size)/Math.log(2));
  if (DEBUG) debug("New incoming buffer size: " + mIncomingBufferLength);
  mIncomingBuffer = new ArrayBuffer(mIncomingBufferLength);
  mIncomingBytes = new Uint8Array(mIncomingBuffer);
  if (mIncomingReadIndex <= mIncomingWriteIndex) {
    // Read and write index are in natural order, so we can just copy
    // the old buffer over to the bigger one without having to worry
    // about the indexes.
    mIncomingBytes.set(oldBytes, 0);
  } else {
    // The write index has wrapped around but the read index hasn't yet.
    // Write whatever the read index has left to read until it would
    // circle around to the beginning of the new buffer, and the rest
    // behind that.
    let head = oldBytes.subarray(mIncomingReadIndex);
    let tail = oldBytes.subarray(0, mIncomingReadIndex);
    mIncomingBytes.set(head, 0);
    mIncomingBytes.set(tail, head.length);
    mIncomingReadIndex = 0;
    mIncomingWriteIndex += head.length;
  }
  if (DEBUG) {
    debug("New incoming buffer size is " + mIncomingBufferLength);
  }
}

/**
 * Grow the outgoing buffer.
 *
 * @param min_size
 *        Minimum new size. The actual new size will be the the smallest
 *        power of 2 that's larger than this number.
 */
function growOutgoingBuffer(min_size) {
  if (DEBUG) {
    debug("Current buffer of " + mOutgoingBufferLength +
          " is too small.");
  }
  let oldBytes = mOutgoingBytes;
  mOutgoingBufferLength =
    2 << Math.floor(Math.log(min_size)/Math.log(2));
  mOutgoingBuffer = new ArrayBuffer(mOutgoingBufferLength);
  mOutgoingBytes = new Uint8Array(mOutgoingBuffer);
  mOutgoingBytes.set(oldBytes, 0);
  if (DEBUG) {
    debug("New outgoing buffer size is " + mOutgoingBufferLength);
  }
}

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
function ensureIncomingAvailable(index) {
  if (index >= mCurrentParcelSize) {
    throw new Error("Trying to read data beyond the parcel end!");
  } else if (index < 0) {
    throw new Error("Trying to read data before the parcel begin!");
  }
}

/**
 * Seek in current incoming parcel.
 *
 * @param offset
 *        Seek offset in relative to current position.
 */
function seekIncoming(offset) {
  // Translate to 0..mCurrentParcelSize
  let cur = mCurrentParcelSize - mReadAvailable;

  let newIndex = cur + offset;
  ensureIncomingAvailable(newIndex);

  // ... mIncomingReadIndex -->|
  // 0               new     cur           mCurrentParcelSize
  // |================|=======|===================|
  // |<--        cur       -->|<- mReadAvailable ->|
  // |<-- newIndex -->|<--  new mReadAvailable  -->|
  mReadAvailable = mCurrentParcelSize - newIndex;

  // Translate back:
  if (mIncomingReadIndex < cur) {
    // The mIncomingReadIndex is wrapped.
    newIndex += mIncomingBufferLength;
  }
  newIndex += (mIncomingReadIndex - cur);
  newIndex %= mIncomingBufferLength;
  mIncomingReadIndex = newIndex;
}

function readUint8Unchecked() {
  let value = mIncomingBytes[mIncomingReadIndex];
  mIncomingReadIndex = (mIncomingReadIndex + 1) %
                           mIncomingBufferLength;
  return value;
}

function readUint8() {
  // Translate to 0..mCurrentParcelSize
  let cur = mCurrentParcelSize - mReadAvailable;
  ensureIncomingAvailable(cur);

  mReadAvailable--;
  return readUint8Unchecked();
}

function readUint8Array(length) {
  // Translate to 0..mCurrentParcelSize
  let last = mCurrentParcelSize - mReadAvailable;
  last += (length - 1);
  ensureIncomingAvailable(last);

  let array = new Uint8Array(length);
  for (let i = 0; i < length; i++) {
    array[i] = readUint8Unchecked();
  }

  mReadAvailable -= length;
  return array;
}

function readUint16() {
  return readUint8() | readUint8() << 8;
}

function readUint32() {
  return readUint8()       | readUint8() <<  8 |
         readUint8() << 16 | readUint8() << 24;
}

function readUint32List() {
  let length = readUint32();
  let ints = [];
  for (let i = 0; i < length; i++) {
    ints.push(readUint32());
  }
  return ints;
}

function readString() {
  let string_len = readUint32();
  if (string_len < 0 || string_len >= INT32_MAX) {
    return null;
  }
  let s = "";
  for (let i = 0; i < string_len; i++) {
    s += String.fromCharCode(readUint16());
  }
  // Strings are \0\0 delimited, but that isn't part of the length. And
  // if the string length is even, the delimiter is two characters wide.
  // It's insane, I know.
  readStringDelimiter(string_len);
  return s;
}

function readStringList() {
  let num_strings = readUint32();
  let strings = [];
  for (let i = 0; i < num_strings; i++) {
    strings.push(readString());
  }
  return strings;
}

function readStringDelimiter(length) {
  let delimiter = readUint16();
  if (!(length & 1)) {
    delimiter |= readUint16();
  }
  if (DEBUG) {
    if (delimiter !== 0) {
      debug("Something's wrong, found string delimiter: " + delimiter);
    }
  }
}

function readParcelSize() {
  return readUint8Unchecked() << 24 |
         readUint8Unchecked() << 16 |
         readUint8Unchecked() <<  8 |
         readUint8Unchecked();
}

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
function ensureOutgoingAvailable(index) {
  if (index >= mOutgoingBufferLength) {
    growOutgoingBuffer(index + 1);
  }
}

function writeUint8(value) {
  ensureOutgoingAvailable(mOutgoingIndex);

  mOutgoingBytes[mOutgoingIndex] = value;
  mOutgoingIndex++;
}

function writeUint16(value) {
  writeUint8(value & 0xff);
  writeUint8((value >> 8) & 0xff);
}

function writeUint32(value) {
  writeUint8(value & 0xff);
  writeUint8((value >> 8) & 0xff);
  writeUint8((value >> 16) & 0xff);
  writeUint8((value >> 24) & 0xff);
}

function writeString(value) {
  if (value == null) {
    writeUint32(-1);
    return;
  }
  writeUint32(value.length);
  for (let i = 0; i < value.length; i++) {
    writeUint16(value.charCodeAt(i));
  }
  // Strings are \0\0 delimited, but that isn't part of the length. And
  // if the string length is even, the delimiter is two characters wide.
  // It's insane, I know.
  writeStringDelimiter(value.length);
}

function writeStringList(strings) {
  writeUint32(strings.length);
  for (let i = 0; i < strings.length; i++) {
    writeString(strings[i]);
  }
}

function writeStringDelimiter(length) {
  writeUint16(0);
  if (!(length & 1)) {
    writeUint16(0);
  }
}

function writeParcelSize(value) {
  /**
   *  Parcel size will always be the first thing in the parcel byte
   *  array, but the last thing written. Store the current index off
   *  to a temporary to be reset after we write the size.
   */
  let currentIndex = mOutgoingIndex;
  mOutgoingIndex = 0;
  writeUint8((value >> 24) & 0xff);
  writeUint8((value >> 16) & 0xff);
  writeUint8((value >> 8) & 0xff);
  writeUint8(value & 0xff);
  mOutgoingIndex = currentIndex;
}

function copyIncomingToOutgoing(length) {
  if (!length || (length < 0)) {
    return;
  }

  let translatedReadIndexEnd = mCurrentParcelSize - mReadAvailable + length - 1;
  ensureIncomingAvailable(translatedReadIndexEnd);

  let translatedWriteIndexEnd = mOutgoingIndex + length - 1;
  ensureOutgoingAvailable(translatedWriteIndexEnd);

  let newIncomingReadIndex = mIncomingReadIndex + length;
  if (newIncomingReadIndex < mIncomingBufferLength) {
    // Reading won't cause wrapping, go ahead with builtin copy.
    mOutgoingBytes.set(mIncomingBytes.subarray(mIncomingReadIndex, newIncomingReadIndex),
                           mOutgoingIndex);
  } else {
    // Not so lucky.
    newIncomingReadIndex %= mIncomingBufferLength;
    mOutgoingBytes.set(mIncomingBytes.subarray(mIncomingReadIndex, mIncomingBufferLength),
                           mOutgoingIndex);
    if (newIncomingReadIndex) {
      let firstPartLength = mIncomingBufferLength - mIncomingReadIndex;
      mOutgoingBytes.set(mIncomingBytes.subarray(0, newIncomingReadIndex),
                             mOutgoingIndex + firstPartLength);
    }
  }

  mIncomingReadIndex = newIncomingReadIndex;
  mReadAvailable -= length;
  mOutgoingIndex += length;
}

/**
 * Parcel management
 */

/**
 * Write incoming data to the circular buffer.
 *
 * @param incoming
 *        Uint8Array containing the incoming data.
 */
function writeToIncoming(incoming) {
  // We don't have to worry about the head catching the tail since
  // we process any backlog in parcels immediately, before writing
  // new data to the buffer. So the only edge case we need to handle
  // is when the incoming data is larger than the buffer size.
  let minMustAvailableSize = incoming.length + mReadIncoming;
  if (minMustAvailableSize > mIncomingBufferLength) {
    growIncomingBuffer(minMustAvailableSize);
  }

  // We can let the typed arrays do the copying if the incoming data won't
  // wrap around the edges of the circular buffer.
  let remaining = mIncomingBufferLength - mIncomingWriteIndex;
  if (remaining >= incoming.length) {
    mIncomingBytes.set(incoming, mIncomingWriteIndex);
  } else {
    // The incoming data would wrap around it.
    let head = incoming.subarray(0, remaining);
    let tail = incoming.subarray(remaining);
    mIncomingBytes.set(head, mIncomingWriteIndex);
    mIncomingBytes.set(tail, 0);
  }
  mIncomingWriteIndex = (mIncomingWriteIndex + incoming.length) %
                            mIncomingBufferLength;
}

/**
 * Process incoming data.
 *
 * @param incoming
 *        Uint8Array containing the incoming data.
 */
function processIncoming(incoming) {
  if (DEBUG) {
    debug("Received " + incoming.length + " bytes.");
    debug("Already read " + mReadIncoming);
  }

  writeToIncoming(incoming);
  mReadIncoming += incoming.length;
  while (true) {
    if (!mCurrentParcelSize) {
      // We're expecting a new parcel.
      if (mReadIncoming < PARCEL_SIZE_SIZE) {
        // We don't know how big the next parcel is going to be, need more
        // data.
        if (DEBUG) debug("Next parcel size unknown, going to sleep.");
        return;
      }
      mCurrentParcelSize = readParcelSize();
      if (DEBUG) debug("New incoming parcel of size " +
                       mCurrentParcelSize);
      // The size itself is not included in the size.
      mReadIncoming -= PARCEL_SIZE_SIZE;
    }

    if (mReadIncoming < mCurrentParcelSize) {
      // We haven't read enough yet in order to be able to process a parcel.
      if (DEBUG) debug("Read " + mReadIncoming + ", but parcel size is "
                       + mCurrentParcelSize + ". Going to sleep.");
      return;
    }

    // Alright, we have enough data to process at least one whole parcel.
    // Let's do that.
    let expectedAfterIndex = (mIncomingReadIndex + mCurrentParcelSize)
                             % mIncomingBufferLength;

    if (DEBUG) {
      let parcel;
      if (expectedAfterIndex < mIncomingReadIndex) {
        let head = mIncomingBytes.subarray(mIncomingReadIndex);
        let tail = mIncomingBytes.subarray(0, expectedAfterIndex);
        parcel = Array.slice(head).concat(Array.slice(tail));
      } else {
        parcel = Array.slice(mIncomingBytes.subarray(
          mIncomingReadIndex, expectedAfterIndex));
      }
      debug("Parcel (size " + mCurrentParcelSize + "): " + parcel);
    }

    if (DEBUG) debug("We have at least one complete parcel.");
    try {
      mReadAvailable = mCurrentParcelSize;
      processParcel();
    } catch (ex) {
      if (DEBUG) debug("Parcel handling threw " + ex + "\n" + ex.stack);
    }

    // Ensure that the whole parcel was consumed.
    if (mIncomingReadIndex != expectedAfterIndex) {
      if (DEBUG) {
        debug("Parcel handler didn't consume whole parcel, " +
              Math.abs(expectedAfterIndex - mIncomingReadIndex) +
              " bytes left over");
      }
      mIncomingReadIndex = expectedAfterIndex;
    }
    mReadIncoming -= mCurrentParcelSize;
    mReadAvailable = 0;
    mCurrentParcelSize = 0;
  }
}

/**
 * Process one parcel.
 */
function processParcel() {
  let response_type = readUint32();

  let request_type, options;
  if (response_type == RESPONSE_TYPE_SOLICITED) {
    let token = readUint32();
    let error = readUint32();

    options = mTokenRequestMap[token];
    if (!options) {
      if (DEBUG) {
        debug("Suspicious uninvited request found: " + token + ". Ignored!");
      }
      return;
    }

    delete mTokenRequestMap[token];
    request_type = options.rilRequestType;

    options.rilRequestError = error;
    if (DEBUG) {
      debug("Solicited response for request type " + request_type +
            ", token " + token + ", error " + error);
    }
  } else if (response_type == RESPONSE_TYPE_UNSOLICITED) {
    request_type = readUint32();
    if (DEBUG) debug("Unsolicited response for request type " + request_type);
  } else {
    if (DEBUG) debug("Unknown response type: " + response_type);
    return;
  }

  RIL.handleParcel(request_type, mReadAvailable, options);
}

/**
 * Start a new outgoing parcel.
 *
 * @param type
 *        Integer specifying the request type.
 * @param options [optional]
 *        Object containing information about the request, e.g. the
 *        original main thread message object that led to the RIL request.
 */
function newParcel(type, options) {
  if (DEBUG) debug("New outgoing parcel of type " + type);

  // We're going to leave room for the parcel size at the beginning.
  mOutgoingIndex = PARCEL_SIZE_SIZE;
  writeUint32(type);
  writeUint32(mToken);

  if (!options) {
    options = {};
  }
  options.rilRequestType = type;
  options.rilRequestError = null;
  mTokenRequestMap[mToken] = options;
  mToken++;
  return mToken;
}

/**
 * Communicate with the RIL IPC thread.
 */
function sendParcel() {
  // Compute the size of the parcel and write it to the front of the parcel
  // where we left room for it. Note that he parcel size does not include
  // the size itself.
  let parcelSize = mOutgoingIndex - PARCEL_SIZE_SIZE;
  writeParcelSize(parcelSize);

  // This assumes that postRILMessage will make a copy of the ArrayBufferView
  // right away!
  let parcel = mOutgoingBytes.subarray(0, mOutgoingIndex);
  if (DEBUG) debug("Outgoing parcel: " + Array.slice(parcel));
  mOutputStream(parcel);
  mOutgoingIndex = PARCEL_SIZE_SIZE;
}

function setOutputStream(func) {
  mOutputStream = func;
}

function simpleRequest(type, options) {
  newParcel(type, options);
  sendParcel();
}

module.exports = {
  init: init,
  startCalOutgoingSize: startCalOutgoingSize,
  stopCalOutgoingSize: stopCalOutgoingSize,
  seekIncoming: seekIncoming,
  readUint8: readUint8,
  readUint8Array: readUint8Array,
  readUint16: readUint16,
  readUint32: readUint32,
  readUint32List: readUint32List,
  readString: readString,
  readStringList: readStringList,
  readStringDelimiter: readStringDelimiter,
  writeUint8: writeUint8,
  writeUint16: writeUint16,
  writeUint32: writeUint32,
  writeString: writeString,
  writeStringList: writeStringList,
  writeStringDelimiter: writeStringDelimiter,
  copyIncomingToOutgoing: copyIncomingToOutgoing,
  processIncoming: processIncoming,
  newParcel: newParcel,
  sendParcel: sendParcel,
  simpleRequest: simpleRequest,
  setOutputStream: setOutputStream
};
