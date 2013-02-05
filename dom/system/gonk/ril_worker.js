/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * This file implements the RIL worker thread. It communicates with
 * the main thread to provide a high-level API to the phone's RIL
 * stack, and with the RIL IPC thread to communicate with the RIL
 * device itself. These communication channels use message events as
 * known from Web Workers:
 *
 * - postMessage()/"message" events for main thread communication
 *
 * - postRILMessage()/"RILMessageEvent" events for RIL IPC thread
 *   communication.
 *
 * The two main objects in this file represent individual parts of this
 * communication chain:
 *
 * - RILMessageEvent -> Buf -> RIL -> postMessage() -> nsIRadioInterfaceLayer
 * - nsIRadioInterfaceLayer -> postMessage() -> RIL -> Buf -> postRILMessage()
 *
 * Note: The code below is purposely lean on abstractions to be as lean in
 * terms of object allocations. As a result, it may look more like C than
 * JavaScript, and that's intended.
 */

"use strict";

importScripts("ril_consts.js", "systemlibs.js");

// set to true in ril_consts.js to see debug messages
let DEBUG = DEBUG_WORKER;

const INT32_MAX   = 2147483647;
const UINT8_SIZE  = 1;
const UINT16_SIZE = 2;
const UINT32_SIZE = 4;
const PARCEL_SIZE_SIZE = UINT32_SIZE;

const PDU_HEX_OCTET_SIZE = 4;

const DEFAULT_EMERGENCY_NUMBERS = ["112", "911"];

// MMI match groups
const MMI_MATCH_GROUP_FULL_MMI = 1;
const MMI_MATCH_GROUP_MMI_PROCEDURE = 2;
const MMI_MATCH_GROUP_SERVICE_CODE = 3;
const MMI_MATCH_GROUP_SIA = 5;
const MMI_MATCH_GROUP_SIB = 7;
const MMI_MATCH_GROUP_SIC = 9;
const MMI_MATCH_GROUP_PWD_CONFIRM = 11;
const MMI_MATCH_GROUP_DIALING_NUMBER = 12;

const MMI_MAX_LENGTH_SHORT_CODE = 2;

const MMI_END_OF_USSD = "#";

let RILQUIRKS_CALLSTATE_EXTRA_UINT32 = libcutils.property_get("ro.moz.ril.callstate_extra_int");
// This may change at runtime since in RIL v6 and later, we get the version
// number via the UNSOLICITED_RIL_CONNECTED parcel.
let RILQUIRKS_V5_LEGACY = libcutils.property_get("ro.moz.ril.v5_legacy");
let RILQUIRKS_REQUEST_USE_DIAL_EMERGENCY_CALL = libcutils.property_get("ro.moz.ril.dial_emergency_call");
let RILQUIRKS_MODEM_DEFAULTS_TO_EMERGENCY_MODE = libcutils.property_get("ro.moz.ril.emergency_by_default");
let RILQUIRKS_SIM_APP_STATE_EXTRA_FIELDS = libcutils.property_get("ro.moz.ril.simstate_extra_field");

// Marker object.
let PENDING_NETWORK_TYPE = {};

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

  INCOMING_BUFFER_LENGTH: 1024,
  OUTGOING_BUFFER_LENGTH: 1024,

  init: function init() {
    this.incomingBuffer = new ArrayBuffer(this.INCOMING_BUFFER_LENGTH);
    this.outgoingBuffer = new ArrayBuffer(this.OUTGOING_BUFFER_LENGTH);

    this.incomingBytes = new Uint8Array(this.incomingBuffer);
    this.outgoingBytes = new Uint8Array(this.outgoingBuffer);

    // Track where incoming data is read from and written to.
    this.incomingWriteIndex = 0;
    this.incomingReadIndex = 0;

    // Leave room for the parcel size for outgoing parcels.
    this.outgoingIndex = PARCEL_SIZE_SIZE;

    // How many bytes we've read for this parcel so far.
    this.readIncoming = 0;

    // How many bytes available as parcel data.
    this.readAvailable = 0;

    // Size of the incoming parcel. If this is zero, we're expecting a new
    // parcel.
    this.currentParcelSize = 0;

    // This gets incremented each time we send out a parcel.
    this.token = 1;

    // Maps tokens we send out with requests to the request type, so that
    // when we get a response parcel back, we know what request it was for.
    this.tokenRequestMap = {};

    // This is the token of last solicited response.
    this.lastSolicitedToken = 0;

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
  startCalOutgoingSize: function startCalOutgoingSize(writeFunction) {
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
  stopCalOutgoingSize: function stopCalOutgoingSize() {
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
  growIncomingBuffer: function growIncomingBuffer(min_size) {
    if (DEBUG) {
      debug("Current buffer of " + this.INCOMING_BUFFER_LENGTH +
            " can't handle incoming " + min_size + " bytes.");
    }
    let oldBytes = this.incomingBytes;
    this.INCOMING_BUFFER_LENGTH =
      2 << Math.floor(Math.log(min_size)/Math.log(2));
    if (DEBUG) debug("New incoming buffer size: " + this.INCOMING_BUFFER_LENGTH);
    this.incomingBuffer = new ArrayBuffer(this.INCOMING_BUFFER_LENGTH);
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
      debug("New incoming buffer size is " + this.INCOMING_BUFFER_LENGTH);
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
      debug("Current buffer of " + this.OUTGOING_BUFFER_LENGTH +
            " is too small.");
    }
    let oldBytes = this.outgoingBytes;
    this.OUTGOING_BUFFER_LENGTH =
      2 << Math.floor(Math.log(min_size)/Math.log(2));
    this.outgoingBuffer = new ArrayBuffer(this.OUTGOING_BUFFER_LENGTH);
    this.outgoingBytes = new Uint8Array(this.outgoingBuffer);
    this.outgoingBytes.set(oldBytes, 0);
    if (DEBUG) {
      debug("New outgoing buffer size is " + this.OUTGOING_BUFFER_LENGTH);
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
   *        this.currentParcelSize.
   */
  ensureIncomingAvailable: function ensureIncomingAvailable(index) {
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
  seekIncoming: function seekIncoming(offset) {
    // Translate to 0..currentParcelSize
    let cur = this.currentParcelSize - this.readAvailable;

    let newIndex = cur + offset;
    this.ensureIncomingAvailable(newIndex);

    // ... incomingReadIndex -->|
    // 0               new     cur           currentParcelSize
    // |================|=======|===================|
    // |<--        cur       -->|<- readAvailable ->|
    // |<-- newIndex -->|<--  new readAvailable  -->|
    this.readAvailable = this.currentParcelSize - newIndex;

    // Translate back:
    if (this.incomingReadIndex < cur) {
      // The incomingReadIndex is wrapped.
      newIndex += this.INCOMING_BUFFER_LENGTH;
    }
    newIndex += (this.incomingReadIndex - cur);
    newIndex %= this.INCOMING_BUFFER_LENGTH;
    this.incomingReadIndex = newIndex;
  },

  readUint8Unchecked: function readUint8Unchecked() {
    let value = this.incomingBytes[this.incomingReadIndex];
    this.incomingReadIndex = (this.incomingReadIndex + 1) %
                             this.INCOMING_BUFFER_LENGTH;
    return value;
  },

  readUint8: function readUint8() {
    // Translate to 0..currentParcelSize
    let cur = this.currentParcelSize - this.readAvailable;
    this.ensureIncomingAvailable(cur);

    this.readAvailable--;
    return this.readUint8Unchecked();
  },

  readUint8Array: function readUint8Array(length) {
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
      if (delimiter != 0) {
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
   *        this.OUTGOING_BUFFER_LENGTH.
   */
  ensureOutgoingAvailable: function ensureOutgoingAvailable(index) {
    if (index >= this.OUTGOING_BUFFER_LENGTH) {
      this.growOutgoingBuffer(index + 1);
    }
  },

  writeUint8: function writeUint8(value) {
    this.ensureOutgoingAvailable(this.outgoingIndex);

    this.outgoingBytes[this.outgoingIndex] = value;
    this.outgoingIndex++;
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
    let currentIndex = this.outgoingIndex;
    this.outgoingIndex = 0;
    this.writeUint8((value >> 24) & 0xff);
    this.writeUint8((value >> 16) & 0xff);
    this.writeUint8((value >> 8) & 0xff);
    this.writeUint8(value & 0xff);
    this.outgoingIndex = currentIndex;
  },

  copyIncomingToOutgoing: function copyIncomingToOutgoing(length) {
    if (!length || (length < 0)) {
      return;
    }

    let translatedReadIndexEnd = this.currentParcelSize - this.readAvailable + length - 1;
    this.ensureIncomingAvailable(translatedReadIndexEnd);

    let translatedWriteIndexEnd = this.outgoingIndex + length - 1
    this.ensureOutgoingAvailable(translatedWriteIndexEnd);

    let newIncomingReadIndex = this.incomingReadIndex + length;
    if (newIncomingReadIndex < this.INCOMING_BUFFER_LENGTH) {
      // Reading won't cause wrapping, go ahead with builtin copy.
      this.outgoingBytes.set(this.incomingBytes.subarray(this.incomingReadIndex, newIncomingReadIndex),
                             this.outgoingIndex);
    } else {
      // Not so lucky.
      newIncomingReadIndex %= this.INCOMING_BUFFER_LENGTH;
      this.outgoingBytes.set(this.incomingBytes.subarray(this.incomingReadIndex, this.INCOMING_BUFFER_LENGTH),
                             this.outgoingIndex);
      if (newIncomingReadIndex) {
        let firstPartLength = this.INCOMING_BUFFER_LENGTH - this.incomingReadIndex;
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
  writeToIncoming: function writeToIncoming(incoming) {
    // We don't have to worry about the head catching the tail since
    // we process any backlog in parcels immediately, before writing
    // new data to the buffer. So the only edge case we need to handle
    // is when the incoming data is larger than the buffer size.
    let minMustAvailableSize = incoming.length + this.readIncoming;
    if (minMustAvailableSize > this.INCOMING_BUFFER_LENGTH) {
      this.growIncomingBuffer(minMustAvailableSize);
    }

    // We can let the typed arrays do the copying if the incoming data won't
    // wrap around the edges of the circular buffer.
    let remaining = this.INCOMING_BUFFER_LENGTH - this.incomingWriteIndex;
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
                              this.INCOMING_BUFFER_LENGTH;
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
      debug("Already read " + this.readIncoming);
    }

    this.writeToIncoming(incoming);
    this.readIncoming += incoming.length;
    while (true) {
      if (!this.currentParcelSize) {
        // We're expecting a new parcel.
        if (this.readIncoming < PARCEL_SIZE_SIZE) {
          // We don't know how big the next parcel is going to be, need more
          // data.
          if (DEBUG) debug("Next parcel size unknown, going to sleep.");
          return;
        }
        this.currentParcelSize = this.readParcelSize();
        if (DEBUG) debug("New incoming parcel of size " +
                         this.currentParcelSize);
        // The size itself is not included in the size.
        this.readIncoming -= PARCEL_SIZE_SIZE;
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
                               % this.INCOMING_BUFFER_LENGTH;

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
   * Process one parcel.
   */
  processParcel: function processParcel() {
    let response_type = this.readUint32();

    let request_type, options;
    if (response_type == RESPONSE_TYPE_SOLICITED) {
      let token = this.readUint32();
      let error = this.readUint32();

      options = this.tokenRequestMap[token];
      if (!options) {
        if (DEBUG) {
          debug("Suspicious uninvited request found: " + token + ". Ignored!");
        }
        return;
      }

      delete this.tokenRequestMap[token];
      request_type = options.rilRequestType;

      options.rilRequestError = error;
      if (DEBUG) {
        debug("Solicited response for request type " + request_type +
              ", token " + token + ", error " + error);
      }
    } else if (response_type == RESPONSE_TYPE_UNSOLICITED) {
      request_type = this.readUint32();
      if (DEBUG) debug("Unsolicited response for request type " + request_type);
    } else {
      if (DEBUG) debug("Unknown response type: " + response_type);
      return;
    }

    RIL.handleParcel(request_type, this.readAvailable, options);
  },

  /**
   * Start a new outgoing parcel.
   *
   * @param type
   *        Integer specifying the request type.
   * @param options [optional]
   *        Object containing information about the request, e.g. the
   *        original main thread message object that led to the RIL request.
   */
  newParcel: function newParcel(type, options) {
    if (DEBUG) debug("New outgoing parcel of type " + type);

    // We're going to leave room for the parcel size at the beginning.
    this.outgoingIndex = PARCEL_SIZE_SIZE;
    this.writeUint32(type);
    let token = this.token;
    this.writeUint32(token);

    if (!options) {
      options = {};
    }
    options.rilRequestType = type;
    options.rilRequestError = null;
    this.tokenRequestMap[token] = options;
    this.token++;
    return token;
  },

  /**
   * Communicate with the RIL IPC thread.
   */
  sendParcel: function sendParcel() {
    // Compute the size of the parcel and write it to the front of the parcel
    // where we left room for it. Note that he parcel size does not include
    // the size itself.
    let parcelSize = this.outgoingIndex - PARCEL_SIZE_SIZE;
    this.writeParcelSize(parcelSize);

    // This assumes that postRILMessage will make a copy of the ArrayBufferView
    // right away!
    let parcel = this.outgoingBytes.subarray(0, this.outgoingIndex);
    if (DEBUG) debug("Outgoing parcel: " + Array.slice(parcel));
    postRILMessage(CLIENT_ID, parcel);
    this.outgoingIndex = PARCEL_SIZE_SIZE;
  },

  simpleRequest: function simpleRequest(type, options) {
    this.newParcel(type, options);
    this.sendParcel();
  }
};


/**
 * The RIL state machine.
 *
 * This object communicates with rild via parcels and with the main thread
 * via post messages. It maintains state about the radio, ICC, calls, etc.
 * and acts upon state changes accordingly.
 */
let RIL = {
  /**
   * Valid calls.
   */
  currentCalls: {},

  /**
   * Existing data calls.
   */
  currentDataCalls: {},

  /**
   * Hash map for received multipart sms fragments. Messages are hashed with
   * its sender address and concatenation reference number. Three additional
   * attributes `segmentMaxSeq`, `receivedSegments`, `segments` are inserted.
   */
  _receivedSmsSegmentsMap: {},

  /**
   * Outgoing messages waiting for SMS-STATUS-REPORT.
   */
  _pendingSentSmsMap: {},

  /**
   * Index of the RIL_PREFERRED_NETWORK_TYPE_TO_GECKO. Its value should be
   * preserved over rild reset.
   */
  preferredNetworkType: null,

  /**
   * Parsed Cell Broadcast search lists.
   * cellBroadcastConfigs.MMI should be preserved over rild reset.
   */
  cellBroadcastConfigs: null,
  mergedCellBroadcastConfig: null,

  _receivedSmsCbPagesMap: {},

  initRILState: function initRILState() {
    /**
     * One of the RADIO_STATE_* constants.
     */
    this.radioState = GECKO_RADIOSTATE_UNAVAILABLE;
    this._isInitialRadioState = true;

    /**
     * True if we are on CDMA network.
     */
    this._isCdma = false;

    /**
     * Set when radio is ready but radio tech is unknown. That is, we are
     * waiting for REQUEST_VOICE_RADIO_TECH
     */
    this._waitingRadioTech = false;

    /**
     * ICC status. Keeps a reference of the data response to the
     * getICCStatus request.
     */
    this.iccStatus = null;

    /**
     * Card state
     */
    this.cardState = null;

    /**
     * Strings
     */
    this.IMEI = null;
    this.IMEISV = null;
    this.ESN = null;
    this.MEID = null;
    this.SMSC = null;

    /**
     * ICC information that is not exposed to Gaia.
     */
    this.iccInfoPrivate = {};

    /**
     * ICC information, such as MSISDN, MCC, MNC, SPN...etc.
     */
    this.iccInfo = {};

    /**
     * Application identification for apps in ICC.
     */
    this.aid = null;

    /**
     * Application type for apps in ICC.
     */
    this.appType = null,

    this.networkSelectionMode = null;

    this.voiceRegistrationState = {};
    this.dataRegistrationState = {};

    /**
     * List of strings identifying the network operator.
     */
    this.operator = null;

    /**
     * String containing the baseband version.
     */
    this.basebandVersion = null;

    // Clean up this.currentCalls: rild might have restarted.
    for each (let currentCall in this.currentCalls) {
      delete this.currentCalls[currentCall.callIndex];
      this._handleDisconnectedCall(currentCall);
    }

    // Deactivate this.currentDataCalls: rild might have restarted.
    for each (let datacall in this.currentDataCalls) {
      this.deactivateDataCall(datacall);
    }

    // Don't clean up this._receivedSmsSegmentsMap or this._pendingSentSmsMap
    // because on rild restart: we may continue with the pending segments.

    /**
     * Whether or not the multiple requests in requestNetworkInfo() are currently
     * being processed
     */
    this._processingNetworkInfo = false;

    /**
     * Pending messages to be send in batch from requestNetworkInfo()
     */
    this._pendingNetworkInfo = {rilMessageType: "networkinfochanged"};

    /**
     * Mute or unmute the radio.
     */
    this._muted = true;

    /**
     * USSD session flag.
     * Only one USSD session may exist at a time, and the session is assumed
     * to exist until:
     *    a) There's a call to cancelUSSD()
     *    b) The implementation sends a UNSOLICITED_ON_USSD with a type code
     *       of "0" (USSD-Notify/no further action) or "2" (session terminated)
     */
    this._ussdSession = null;

   /**
    * Regular expresion to parse MMI codes.
    */
    this._mmiRegExp = null;

    /**
     * Cell Broadcast Search Lists.
     */
    let cbmmi = this.cellBroadcastConfigs && this.cellBroadcastConfigs.MMI;
    this.cellBroadcastConfigs = {
      MMI: cbmmi || null
    };
  },

  get muted() {
    return this._muted;
  },
  set muted(val) {
    val = Boolean(val);
    if (this._muted != val) {
      this.setMute(val);
      this._muted = val;
    }
  },

  /**
   * Parse an integer from a string, falling back to a default value
   * if the the provided value is not a string or does not contain a valid
   * number.
   *
   * @param string
   *        String to be parsed.
   * @param defaultValue [optional]
   *        Default value to be used.
   * @param radix [optional]
   *        A number that represents the numeral system to be used. Default 10.
   */
  parseInt: function RIL_parseInt(string, defaultValue, radix) {
    let number = parseInt(string, radix || 10);
    if (!isNaN(number)) {
      return number;
    }
    if (defaultValue === undefined) {
      defaultValue = null;
    }
    return defaultValue;
  },


  /**
   * Outgoing requests to the RIL. These can be triggered from the
   * main thread via messages that look like this:
   *
   *   {rilMessageType: "methodName",
   *    extra:          "parameters",
   *    go:             "here"}
   *
   * So if one of the following methods takes arguments, it takes only one,
   * an object, which then contains all of the parameters as attributes.
   * The "@param" documentation is to be interpreted accordingly.
   */

  /**
   * Retrieve the ICC's status.
   */
  getICCStatus: function getICCStatus() {
    Buf.simpleRequest(REQUEST_GET_SIM_STATUS);
  },

   /**
   * Helper function for unlocking ICC locks.
   */
  iccUnlockCardLock: function iccUnlockCardLock(options) {
    switch (options.lockType) {
      case "pin":
        this.enterICCPIN(options);
        break;
      case "pin2":
        this.enterICCPIN2(options);
        break;
      case "puk":
        this.enterICCPUK(options);
        break;
      case "puk2":
        this.enterICCPUK2(options);
        break;
      case "nck":
        options.type = CARD_PERSOSUBSTATE_SIM_NETWORK;
        this.enterDepersonalization(options);
        break;
      default:
        options.errorMsg = "Unsupported Card Lock.";
        options.success = false;
        this.sendDOMMessage(options);
    }
  },

  /**
   * Enter a PIN to unlock the ICC.
   *
   * @param pin
   *        String containing the PIN.
   * @param [optional] aid
   *        AID value.
   */
  enterICCPIN: function enterICCPIN(options) {
    Buf.newParcel(REQUEST_ENTER_SIM_PIN, options);
    Buf.writeUint32(RILQUIRKS_V5_LEGACY ? 1 : 2);
    Buf.writeString(options.pin);
    if (!RILQUIRKS_V5_LEGACY) {
      Buf.writeString(options.aid || this.aid);
    }
    Buf.sendParcel();
  },

  /**
   * Enter a PIN2 to unlock the ICC.
   *
   * @param pin
   *        String containing the PIN2.
   * @param [optional] aid
   *        AID value.
   */
  enterICCPIN2: function enterICCPIN2(options) {
    Buf.newParcel(REQUEST_ENTER_SIM_PIN2, options);
    Buf.writeUint32(RILQUIRKS_V5_LEGACY ? 1 : 2);
    Buf.writeString(options.pin);
    if (!RILQUIRKS_V5_LEGACY) {
      Buf.writeString(options.aid || this.aid);
    }
    Buf.sendParcel();
  },

  /**
   * Requests a network personalization be deactivated.
   *
   * @param type
   *        Integer indicating the network personalization be deactivated.
   * @param pin
   *        String containing the pin.
   */
  enterDepersonalization: function enterDepersonalization(options) {
    Buf.newParcel(REQUEST_ENTER_NETWORK_DEPERSONALIZATION_CODE, options);
    Buf.writeUint32(options.type);
    Buf.writeString(options.pin);
    Buf.sendParcel();
  },

   /**
   * Helper function for changing ICC locks.
   */
  iccSetCardLock: function iccSetCardLock(options) {
    if (options.newPin !== undefined) {
      switch (options.lockType) {
        case "pin":
          this.changeICCPIN(options);
          break;
        case "pin2":
          this.changeICCPIN2(options);
          break;
        default:
          options.errorMsg = "Unsupported Card Lock.";
          options.success = false;
          this.sendDOMMessage(options);
      }
    } else { // Enable/Disable pin lock.
      if (options.lockType != "pin") {
        options.errorMsg = "Unsupported Card Lock.";
        options.success = false;
        this.sendDOMMessage(options);
        return;
      }
      this.setICCPinLock(options);
    }
  },

  /**
   * Change the current ICC PIN number.
   *
   * @param pin
   *        String containing the old PIN value
   * @param newPin
   *        String containing the new PIN value
   * @param [optional] aid
   *        AID value.
   */
  changeICCPIN: function changeICCPIN(options) {
    Buf.newParcel(REQUEST_CHANGE_SIM_PIN, options);
    Buf.writeUint32(RILQUIRKS_V5_LEGACY ? 2 : 3);
    Buf.writeString(options.pin);
    Buf.writeString(options.newPin);
    if (!RILQUIRKS_V5_LEGACY) {
      Buf.writeString(options.aid || this.aid);
    }
    Buf.sendParcel();
  },

  /**
   * Change the current ICC PIN2 number.
   *
   * @param pin
   *        String containing the old PIN2 value
   * @param newPin
   *        String containing the new PIN2 value
   * @param [optional] aid
   *        AID value.
   */
  changeICCPIN2: function changeICCPIN2(options) {
    Buf.newParcel(REQUEST_CHANGE_SIM_PIN2, options);
    Buf.writeUint32(RILQUIRKS_V5_LEGACY ? 2 : 3);
    Buf.writeString(options.pin);
    Buf.writeString(options.newPin);
    if (!RILQUIRKS_V5_LEGACY) {
      Buf.writeString(options.aid || this.aid);
    }
    Buf.sendParcel();
  },
  /**
   * Supplies ICC PUK and a new PIN to unlock the ICC.
   *
   * @param puk
   *        String containing the PUK value.
   * @param newPin
   *        String containing the new PIN value.
   * @param [optional] aid
   *        AID value.
   */
   enterICCPUK: function enterICCPUK(options) {
     Buf.newParcel(REQUEST_ENTER_SIM_PUK, options);
     Buf.writeUint32(RILQUIRKS_V5_LEGACY ? 2 : 3);
     Buf.writeString(options.puk);
     Buf.writeString(options.newPin);
     if (!RILQUIRKS_V5_LEGACY) {
       Buf.writeString(options.aid || this.aid);
     }
     Buf.sendParcel();
   },

  /**
   * Supplies ICC PUK2 and a new PIN2 to unlock the ICC.
   *
   * @param puk
   *        String containing the PUK2 value.
   * @param newPin
   *        String containing the new PIN2 value.
   * @param [optional] aid
   *        AID value.
   */
   enterICCPUK2: function enterICCPUK2(options) {
     Buf.newParcel(REQUEST_ENTER_SIM_PUK2, options);
     Buf.writeUint32(RILQUIRKS_V5_LEGACY ? 2 : 3);
     Buf.writeString(options.puk);
     Buf.writeString(options.newPin);
     if (!RILQUIRKS_V5_LEGACY) {
       Buf.writeString(options.aid || this.aid);
     }
     Buf.sendParcel();
   },

  /**
   * Helper function for fetching the state of ICC locks.
   */
  iccGetCardLock: function iccGetCardLock(options) {
    switch (options.lockType) {
      case "pin":
        this.getICCPinLock(options);
        break;
      default:
        options.errorMsg = "Unsupported Card Lock.";
        options.success = false;
        this.sendDOMMessage(options);
    }
  },

  /**
   * Get ICC Pin lock. A wrapper call to queryICCFacilityLock.
   *
   * @param requestId
   *        Request Id from RadioInterfaceLayer.
   */
  getICCPinLock: function getICCPinLock(options) {
    options.facility = ICC_CB_FACILITY_SIM;
    options.password = ""; // For query no need to provide pin.
    options.serviceClass = ICC_SERVICE_CLASS_VOICE |
                           ICC_SERVICE_CLASS_DATA  |
                           ICC_SERVICE_CLASS_FAX;
    this.queryICCFacilityLock(options);
  },

  /**
   * Query ICC facility lock.
   *
   * @param facility
   *        One of ICC_CB_FACILITY_*.
   * @param password
   *        Password for the facility, or "" if not required.
   * @param serviceClass
   *        One of ICC_SERVICE_CLASS_*.
   * @param [optional] aid
   *        AID value.
   */
  queryICCFacilityLock: function queryICCFacilityLock(options) {
    Buf.newParcel(REQUEST_QUERY_FACILITY_LOCK, options);
    Buf.writeUint32(RILQUIRKS_V5_LEGACY ? 3 : 4);
    Buf.writeString(options.facility);
    Buf.writeString(options.password);
    Buf.writeString(options.serviceClass.toString());
    if (!RILQUIRKS_V5_LEGACY) {
      Buf.writeString(options.aid || this.aid);
    }
    Buf.sendParcel();
  },

  /**
   * Set ICC Pin lock. A wrapper call to setICCFacilityLock.
   *
   * @param enabled
   *        true to enable, false to disable.
   * @param pin
   *        Pin code.
   * @param requestId
   *        Request Id from RadioInterfaceLayer.
   */
  setICCPinLock: function setICCPinLock(options) {
    options.facility = ICC_CB_FACILITY_SIM;
    options.enabled = options.enabled;
    options.password = options.pin;
    options.serviceClass = ICC_SERVICE_CLASS_VOICE |
                           ICC_SERVICE_CLASS_DATA  |
                           ICC_SERVICE_CLASS_FAX;
    this.setICCFacilityLock(options);
  },

  /**
   * Set ICC facility lock.
   *
   * @param facility
   *        One of ICC_CB_FACILITY_*.
   * @param enabled
   *        true to enable, false to disable.
   * @param password
   *        Password for the facility, or "" if not required.
   * @param serviceClass
   *        One of ICC_SERVICE_CLASS_*.
   * @param [optional] aid
   *        AID value.
   */
  setICCFacilityLock: function setICCFacilityLock(options) {
    Buf.newParcel(REQUEST_SET_FACILITY_LOCK, options);
    Buf.writeUint32(RILQUIRKS_V5_LEGACY ? 3 : 4);
    Buf.writeString(options.facility);
    Buf.writeString(options.enabled ? "1" : "0");
    Buf.writeString(options.password);
    Buf.writeString(options.serviceClass.toString());
    if (!RILQUIRKS_V5_LEGACY) {
      Buf.writeString(options.aid || this.aid);
    }
    Buf.sendParcel();
  },

  /**
   *  Request an ICC I/O operation.
   *
   *  See TS 27.007 "restricted SIM" operation, "AT Command +CRSM".
   *  The sequence is in the same order as how libril reads this parcel,
   *  see the struct RIL_SIM_IO_v5 or RIL_SIM_IO_v6 defined in ril.h
   *
   *  @param command
   *         The I/O command, one of the ICC_COMMAND_* constants.
   *  @param fileId
   *         The file to operate on, one of the ICC_EF_* constants.
   *  @param pathId
   *         String type, check the 'pathid' parameter from TS 27.007 +CRSM.
   *  @param p1, p2, p3
   *         Arbitrary integer parameters for the command.
   *  @param [optional] dataWriter
   *         The function for writing string parameter for the ICC_COMMAND_UPDATE_RECORD.
   *  @param [optional] pin2
   *         String containing the PIN2.
   *  @param [optional] aid
   *         AID value.
   */
  iccIO: function iccIO(options) {
    let token = Buf.newParcel(REQUEST_SIM_IO, options);
    Buf.writeUint32(options.command);
    Buf.writeUint32(options.fileId);
    Buf.writeString(options.pathId);
    Buf.writeUint32(options.p1);
    Buf.writeUint32(options.p2);
    Buf.writeUint32(options.p3);
    if (options.command == ICC_COMMAND_UPDATE_RECORD &&
        options.dataWriter) {
      options.dataWriter(options.p3);
    } else {
      Buf.writeString(null);
    }
    Buf.writeString(options.pin2 || null);
    if (!RILQUIRKS_V5_LEGACY) {
      Buf.writeString(options.aid || this.aid);
    }
    Buf.sendParcel();
  },

  /**
   * Get IMSI.
   *
   * @param [optional] aid
   *        AID value.
   */
  getIMSI: function getIMSI(aid) {
    if (RILQUIRKS_V5_LEGACY) {
      Buf.simpleRequest(REQUEST_GET_IMSI);
      return;
    }
    let token = Buf.newParcel(REQUEST_GET_IMSI);
    Buf.writeUint32(1);
    Buf.writeString(aid || this.aid);
    Buf.sendParcel();
  },

  /**
   * Choose network names using EF_OPL and EF_PNN
   * See 3GPP TS 31.102 sec. 4.2.58 and sec. 4.2.59 for USIM,
   *     3GPP TS 51.011 sec. 10.3.41 and sec. 10.3.42 for SIM.
   */
  updateNetworkName: function updateNetworkName() {
    let iccInfoPriv = this.iccInfoPrivate;
    let iccInfo = this.iccInfo;

    // We won't update network name if voice registration isn't ready
    // or PNN file haven't been retrieved.
    if (!iccInfoPriv.PNN ||
        !this.voiceRegistrationState.cell ||
        this.voiceRegistrationState.cell.gsmLocationAreaCode == -1) {
      return null;
    }

    let pnnEntry;
    let lac = this.voiceRegistrationState.cell.gsmLocationAreaCode;
    let mcc = this.operator.mcc;
    let mnc = this.operator.mnc;

    // According to 3GPP TS 31.102 Sec. 4.2.59 and 3GPP TS 51.011 Sec. 10.3.42,
    // the ME shall use this EF_OPL in association with the EF_PNN in place
    // of any network name stored within the ME's internal list and any network
    // name received when registered to the PLMN.
    if (iccInfoPriv.OPL) {
      for (let i in iccInfoPriv.OPL) {
        let opl = iccInfoPriv.OPL[i];
        // Try to match the MCC/MNC.
        if (mcc != opl.mcc || mnc != opl.mnc) {
          continue;
        }
        // Try to match the location area code. If current local area code is
        // covered by lac range that specified in the OPL entry, use the PNN
        // that specified in the OPL entry.
        if ((opl.lacTacStart == 0x0 && opl.lacTacEnd == 0xFFFE) ||
            (opl.lacTacStart <= lac && opl.lacTacEnd >= lac)) {
          if (opl.pnnRecordId == 0) {
            // See 3GPP TS 31.102 Sec. 4.2.59 and 3GPP TS 51.011 Sec. 10.3.42,
            // A value of '00' indicates that the name is to be taken from other
            // sources.
            return null;
          }
          pnnEntry = iccInfoPriv.PNN[opl.pnnRecordId - 1]
          break;
        }
      }
    }

    // According to 3GPP TS 31.102 Sec. 4.2.58 and 3GPP TS 51.011 Sec. 10.3.41,
    // the first record in this EF is used for the default network name when
    // registered to the HPLMN.
    // If we haven't get pnnEntry assigned, we should try to assign default
    // value to it.
    if (!pnnEntry && mcc == iccInfo.mcc && mnc == iccInfo.mnc) {
      pnnEntry = iccInfoPriv.PNN[0]
    }

    if (DEBUG) {
      if (pnnEntry) {
        debug("updateNetworkName: Network names will be overriden: longName = " +
              pnnEntry.fullName + ", shortName = " + pnnEntry.shortName);
      } else {
        debug("updateNetworkName: Network names will not be overriden");
      }
    }

    if (pnnEntry) {
      return [pnnEntry.fullName, pnnEntry.shortName];
    }
    return null;
  },

  /**
   * Get UICC Phonebook.
   *
   * @param contactType
   *        "ADN" or "FDN".
   * @param requestId
   *        Request id from RadioInterfaceLayer.
   */
  getICCContacts: function getICCContacts(options) {
    if (!this.appType) {
      options.rilMessageType = "icccontacts";
      options.errorMsg = GECKO_ERROR_REQUEST_NOT_SUPPORTED;
      this.sendDOMMessage(options);
    }

    ICCContactHelper.readICCContacts(
      this.appType,
      options.contactType,
      function onsuccess(contacts) {
        // Reuse 'options' to get 'requestId' and 'contactType'.
        options.rilMessageType = "icccontacts";
        options.contacts = contacts;
        RIL.sendDOMMessage(options);
      }.bind(this),
      function onerror(errorMsg) {
        options.rilMessageType = "icccontacts";
        options.errorMsg = errorMsg;
        RIL.sendDOMMessage(options);
      }.bind(this));
  },

  /**
   * Update UICC Phonebook.
   *
   * @param contactType   "ADN".
   * @param contact       The contact will be updated.
   * @param requestId     Request id from RadioInterfaceLayer.
   */
  updateICCContact: function updateICCContact(options) {
    let onsuccess = function onsuccess() {
      // Reuse 'options' to get 'requestId' and 'contactType'.
      options.rilMessageType = "icccontactupdate";
      RIL.sendDOMMessage(options);
    }.bind(this);

    let onerror = function onerror(errorMsg) {
      options.rilMessageType = "icccontactupdate";
      options.errorMsg = errorMsg;
      RIL.sendDOMMessage(options);
    }.bind(this);

    if (!this.appType || !options.contact) {
      onerror(GECKO_ERROR_REQUEST_NOT_SUPPORTED);
      return;
    }

    // If contact has 'recordId' property, updates corresponding record.
    // If not, inserts the contact into a free record.
    if (options.contact.recordId) {
      ICCContactHelper.updateICCContact(
        this.appType, options.contactType, options.contact, onsuccess, onerror);
    } else {
      ICCContactHelper.addICCContact(
        this.appType, options.contactType, options.contact, onsuccess, onerror);
    }
  },

  /**
   * Request the phone's radio power to be switched on or off.
   *
   * @param on
   *        Boolean indicating the desired power state.
   */
  setRadioPower: function setRadioPower(options) {
    Buf.newParcel(REQUEST_RADIO_POWER);
    Buf.writeUint32(1);
    Buf.writeUint32(options.on ? 1 : 0);
    Buf.sendParcel();
  },

  /**
   * Set call waiting status.
   *
   * @param on
   *        Boolean indicating the desired waiting status.
   */
  setCallWaiting: function setCallWaiting(options) {
    Buf.newParcel(REQUEST_SET_CALL_WAITING, options);
    Buf.writeUint32(2);
    Buf.writeUint32(options.enabled ? 1 : 0);
    Buf.writeUint32(ICC_SERVICE_CLASS_VOICE);
    Buf.sendParcel();
  },

  /**
   * Set screen state.
   *
   * @param on
   *        Boolean indicating whether the screen should be on or off.
   */
  setScreenState: function setScreenState(on) {
    Buf.newParcel(REQUEST_SCREEN_STATE);
    Buf.writeUint32(1);
    Buf.writeUint32(on ? 1 : 0);
    Buf.sendParcel();
  },

  getVoiceRegistrationState: function getVoiceRegistrationState() {
    Buf.simpleRequest(REQUEST_VOICE_REGISTRATION_STATE);
  },

  getVoiceRadioTechnology: function getVoiceRadioTechnology() {
    Buf.simpleRequest(REQUEST_VOICE_RADIO_TECH);
  },

  getDataRegistrationState: function getDataRegistrationState() {
    Buf.simpleRequest(REQUEST_DATA_REGISTRATION_STATE);
  },

  getOperator: function getOperator() {
    Buf.simpleRequest(REQUEST_OPERATOR);
  },

  /**
   * Set the preferred network type.
   *
   * @param options An object contains a valid index of
   *                RIL_PREFERRED_NETWORK_TYPE_TO_GECKO as its `networkType`
   *                attribute, or undefined to set current preferred network
   *                type.
   */
  setPreferredNetworkType: function setPreferredNetworkType(options) {
    if (options) {
      this.preferredNetworkType = options.networkType;
    }
    if (this.preferredNetworkType == null) {
      return;
    }

    Buf.newParcel(REQUEST_SET_PREFERRED_NETWORK_TYPE, options);
    Buf.writeUint32(1);
    Buf.writeUint32(this.preferredNetworkType);
    Buf.sendParcel();
  },

  /**
   * Get the preferred network type.
   */
  getPreferredNetworkType: function getPreferredNetworkType() {
    Buf.simpleRequest(REQUEST_GET_PREFERRED_NETWORK_TYPE);
  },

  /**
   * Request various states about the network.
   */
  requestNetworkInfo: function requestNetworkInfo() {
    if (this._processingNetworkInfo) {
      if (DEBUG) debug("Network info requested, but we're already requesting network info.");
      return;
    }

    if (DEBUG) debug("Requesting network info");

    this._processingNetworkInfo = true;
    this.getVoiceRegistrationState();
    this.getDataRegistrationState(); //TODO only GSM
    this.getOperator();
    this.getNetworkSelectionMode();
  },

  /**
   * Get the available networks
   */
  getAvailableNetworks: function getAvailableNetworks(options) {
    if (DEBUG) debug("Getting available networks");
    Buf.newParcel(REQUEST_QUERY_AVAILABLE_NETWORKS, options);
    Buf.sendParcel();
  },

  /**
   * Request the radio's network selection mode
   */
  getNetworkSelectionMode: function getNetworkSelectionMode(options) {
    if (DEBUG) debug("Getting network selection mode");
    Buf.simpleRequest(REQUEST_QUERY_NETWORK_SELECTION_MODE, options);
  },

  /**
   * Tell the radio to automatically choose a voice/data network
   */
  selectNetworkAuto: function selectNetworkAuto(options) {
    if (DEBUG) debug("Setting automatic network selection");
    Buf.simpleRequest(REQUEST_SET_NETWORK_SELECTION_AUTOMATIC, options);
  },

  /**
   * Tell the radio to choose a specific voice/data network
   */
  selectNetwork: function selectNetwork(options) {
    if (DEBUG) {
      debug("Setting manual network selection: " + options.mcc + options.mnc);
    }

    // TODO: Bug 828307 - B2G RIL: Change to store MNC/MCC values from integer to string
    let mnc = options.mnc.toString();
    if (mnc.length == 1)
      mnc = "0" + mnc;
    let numeric = options.mcc.toString() + mnc;
    Buf.newParcel(REQUEST_SET_NETWORK_SELECTION_MANUAL, options);
    Buf.writeString(numeric);
    Buf.sendParcel();
  },

  /**
   * Get current calls.
   */
  getCurrentCalls: function getCurrentCalls() {
    Buf.simpleRequest(REQUEST_GET_CURRENT_CALLS);
  },

  /**
   * Get the signal strength.
   */
  getSignalStrength: function getSignalStrength() {
    Buf.simpleRequest(REQUEST_SIGNAL_STRENGTH);
  },

  getIMEI: function getIMEI(options) {
    Buf.simpleRequest(REQUEST_GET_IMEI, options);
  },

  getIMEISV: function getIMEISV() {
    Buf.simpleRequest(REQUEST_GET_IMEISV);
  },

  getDeviceIdentity: function getDeviceIdentity() {
    Buf.simpleRequest(REQUEST_DEVICE_IDENTITY);
  },

  getBasebandVersion: function getBasebandVersion() {
    Buf.simpleRequest(REQUEST_BASEBAND_VERSION);
  },

  /**
   * Dial the phone.
   *
   * @param number
   *        String containing the number to dial.
   * @param clirMode
   *        Integer doing something XXX TODO
   * @param uusInfo
   *        Integer doing something XXX TODO
   */
  dial: function dial(options) {
    let dial_request_type = REQUEST_DIAL;
    if (this.voiceRegistrationState.emergencyCallsOnly ||
        options.isDialEmergency) {
      if (!this._isEmergencyNumber(options.number)) {
        // Notify error in establishing the call with an invalid number.
        options.callIndex = -1;
        options.rilMessageType = "callError";
        options.error =
          RIL_CALL_FAILCAUSE_TO_GECKO_CALL_ERROR[CALL_FAIL_UNOBTAINABLE_NUMBER];
        this.sendDOMMessage(options);
        return;
      }

      if (RILQUIRKS_REQUEST_USE_DIAL_EMERGENCY_CALL) {
        dial_request_type = REQUEST_DIAL_EMERGENCY_CALL;
      }
    } else {
      if (this._isEmergencyNumber(options.number) &&
          RILQUIRKS_REQUEST_USE_DIAL_EMERGENCY_CALL) {
        dial_request_type = REQUEST_DIAL_EMERGENCY_CALL;
      }
    }

    let token = Buf.newParcel(dial_request_type);
    Buf.writeString(options.number);
    Buf.writeUint32(options.clirMode || 0);
    Buf.writeUint32(options.uusInfo || 0);
    // TODO Why do we need this extra 0? It was put it in to make this
    // match the format of the binary message.
    Buf.writeUint32(0);
    Buf.sendParcel();
  },

  /**
   * Hang up the phone.
   *
   * @param callIndex
   *        Call index (1-based) as reported by REQUEST_GET_CURRENT_CALLS.
   */
  hangUp: function hangUp(options) {
    let call = this.currentCalls[options.callIndex];
    if (!call) {
      return;
    }

    switch (call.state) {
      case CALL_STATE_ACTIVE:
      case CALL_STATE_DIALING:
      case CALL_STATE_ALERTING:
        Buf.newParcel(REQUEST_HANGUP);
        Buf.writeUint32(1);
        Buf.writeUint32(options.callIndex);
        Buf.sendParcel();
        break;
      case CALL_STATE_HOLDING:
        Buf.simpleRequest(REQUEST_HANGUP_WAITING_OR_BACKGROUND);
        break;
    }
  },

  /**
   * Mute or unmute the radio.
   *
   * @param mute
   *        Boolean to indicate whether to mute or unmute the radio.
   */
  setMute: function setMute(mute) {
    Buf.newParcel(REQUEST_SET_MUTE);
    Buf.writeUint32(1);
    Buf.writeUint32(mute ? 1 : 0);
    Buf.sendParcel();
  },

  /**
   * Answer an incoming/waiting call.
   *
   * @param callIndex
   *        Call index of the call to answer.
   */
  answerCall: function answerCall(options) {
    // Check for races. Since we dispatched the incoming/waiting call
    // notification the incoming/waiting call may have changed. The main
    // thread thinks that it is answering the call with the given index,
    // so only answer if that is still incoming/waiting.
    let call = this.currentCalls[options.callIndex];
    if (!call) {
      return;
    }

    switch (call.state) {
      case CALL_STATE_INCOMING:
        Buf.simpleRequest(REQUEST_ANSWER);
        break;
      case CALL_STATE_WAITING:
        // Answer the waiting (second) call, and hold the first call.
        Buf.simpleRequest(REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE);
        break;
    }
  },

  /**
   * Reject an incoming/waiting call.
   *
   * @param callIndex
   *        Call index of the call to reject.
   */
  rejectCall: function rejectCall(options) {
    // Check for races. Since we dispatched the incoming/waiting call
    // notification the incoming/waiting call may have changed. The main
    // thread thinks that it is rejecting the call with the given index,
    // so only reject if that is still incoming/waiting.
    let call = this.currentCalls[options.callIndex];
    if (!call) {
      return;
    }

    switch (call.state) {
      case CALL_STATE_INCOMING:
        Buf.simpleRequest(REQUEST_UDUB);
        break;
      case CALL_STATE_WAITING:
        // Reject the waiting (second) call, and remain the first call.
        Buf.simpleRequest(REQUEST_HANGUP_WAITING_OR_BACKGROUND);
        break;
    }
  },

  holdCall: function holdCall(options) {
    let call = this.currentCalls[options.callIndex];
    if (call && call.state == CALL_STATE_ACTIVE) {
      Buf.simpleRequest(REQUEST_SWITCH_HOLDING_AND_ACTIVE);
    }
  },

  resumeCall: function resumeCall(options) {
    let call = this.currentCalls[options.callIndex];
    if (call && call.state == CALL_STATE_HOLDING) {
      Buf.simpleRequest(REQUEST_SWITCH_HOLDING_AND_ACTIVE);
    }
  },

  /**
   * Send an SMS.
   *
   * The `options` parameter object should contain the following attributes:
   *
   * @param number
   *        String containing the recipient number.
   * @param body
   *        String containing the message text.
   * @param requestId
   *        String identifying the sms request used by the SmsRequestManager.
   * @param processId
   *        String containing the processId for the SmsRequestManager.
   */
  sendSMS: function sendSMS(options) {
    options.langIndex = options.langIndex || PDU_NL_IDENTIFIER_DEFAULT;
    options.langShiftIndex = options.langShiftIndex || PDU_NL_IDENTIFIER_DEFAULT;

    if (!options.retryCount) {
      options.retryCount = 0;
    }

    if (!options.segmentSeq) {
      // Fist segment to send
      options.segmentSeq = 1;
      options.body = options.segments[0].body;
      options.encodedBodyLength = options.segments[0].encodedBodyLength;
    }

    Buf.newParcel(REQUEST_SEND_SMS, options);
    Buf.writeUint32(2);
    Buf.writeString(options.SMSC);
    GsmPDUHelper.writeMessage(options);
    Buf.sendParcel();
  },

  /**
   * Acknowledge the receipt and handling of an SMS.
   *
   * @param success
   *        Boolean indicating whether the message was successfuly handled.
   * @param cause
   *        SMS_* constant indicating the reason for unsuccessful handling.
   */
  acknowledgeSMS: function acknowledgeSMS(success, cause) {
    let token = Buf.newParcel(REQUEST_SMS_ACKNOWLEDGE);
    Buf.writeUint32(2);
    Buf.writeUint32(success ? 1 : 0);
    Buf.writeUint32(cause);
    Buf.sendParcel();
  },

  /**
   * Acknowledge the receipt and handling of an SMS.
   *
   * @param success
   *        Boolean indicating whether the message was successfuly handled.
   */
  ackSMS: function ackSMS(options) {
    if (options.result == PDU_FCS_RESERVED) {
      return;
    }
    this.acknowledgeSMS(options.result == PDU_FCS_OK, options.result);
  },

  setCellBroadcastSearchList: function setCellBroadcastSearchList(options) {
    try {
      let str = options.searchListStr;
      this.cellBroadcastConfigs.MMI = this._convertCellBroadcastSearchList(str);
    } catch (e) {
      if (DEBUG) {
        debug("Invalid Cell Broadcast search list: " + e);
      }
      options.rilRequestError = ERROR_GENERIC_FAILURE;
      this.sendDOMMessage(options);
      return;
    }

    this._mergeAllCellBroadcastConfigs();
  },

  updateCellBroadcastConfig: function updateCellBroadcastConfig() {
    let activate = (this.mergedCellBroadcastConfig != null)
                   && (this.mergedCellBroadcastConfig.length > 0);
    if (activate) {
      this.setGsmSmsBroadcastConfig(this.mergedCellBroadcastConfig);
    } else {
      // It's unnecessary to set config first if we're deactivating.
      this.setGsmSmsBroadcastActivation(false);
    }
  },

  setGsmSmsBroadcastConfig: function setGsmSmsBroadcastConfig(config) {
    Buf.newParcel(REQUEST_GSM_SET_BROADCAST_SMS_CONFIG);

    let numConfigs = config ? config.length / 2 : 0;
    Buf.writeUint32(numConfigs);
    for (let i = 0; i < config.length;) {
      Buf.writeUint32(config[i++]);
      Buf.writeUint32(config[i++]);
      Buf.writeUint32(0x00);
      Buf.writeUint32(0xFF);
      Buf.writeUint32(1);
    }

    Buf.sendParcel();
  },

  setGsmSmsBroadcastActivation: function setGsmSmsBroadcastActivation(activate) {
    Buf.newParcel(REQUEST_GSM_SMS_BROADCAST_ACTIVATION);
    Buf.writeUint32(1);
    Buf.writeUint32(activate ? 1 : 0);
    Buf.sendParcel();
  },

  /**
   * Start a DTMF Tone.
   *
   * @param dtmfChar
   *        DTMF signal to send, 0-9, *, +
   */
  startTone: function startTone(options) {
    Buf.newParcel(REQUEST_DTMF_START);
    Buf.writeString(options.dtmfChar);
    Buf.sendParcel();
  },

  stopTone: function stopTone() {
    Buf.simpleRequest(REQUEST_DTMF_STOP);
  },

  /**
   * Send a DTMF tone.
   *
   * @param dtmfChar
   *        DTMF signal to send, 0-9, *, +
   */
  sendTone: function sendTone(options) {
    Buf.newParcel(REQUEST_DTMF);
    Buf.writeString(options.dtmfChar);
    Buf.sendParcel();
  },

  /**
   * Get the Short Message Service Center address.
   */
  getSMSCAddress: function getSMSCAddress() {
    Buf.simpleRequest(REQUEST_GET_SMSC_ADDRESS);
  },

  /**
   * Set the Short Message Service Center address.
   *
   * @param SMSC
   *        Short Message Service Center address in PDU format.
   */
  setSMSCAddress: function setSMSCAddress(options) {
    Buf.newParcel(REQUEST_SET_SMSC_ADDRESS);
    Buf.writeString(options.SMSC);
    Buf.sendParcel();
  },

  /**
   * Setup a data call.
   *
   * @param radioTech
   *        Integer to indicate radio technology.
   *        DATACALL_RADIOTECHNOLOGY_CDMA => CDMA.
   *        DATACALL_RADIOTECHNOLOGY_GSM  => GSM.
   * @param apn
   *        String containing the name of the APN to connect to.
   * @param user
   *        String containing the username for the APN.
   * @param passwd
   *        String containing the password for the APN.
   * @param chappap
   *        Integer containing CHAP/PAP auth type.
   *        DATACALL_AUTH_NONE        => PAP and CHAP is never performed.
   *        DATACALL_AUTH_PAP         => PAP may be performed.
   *        DATACALL_AUTH_CHAP        => CHAP may be performed.
   *        DATACALL_AUTH_PAP_OR_CHAP => PAP / CHAP may be performed.
   * @param pdptype
   *        String containing PDP type to request. ("IP", "IPV6", ...)
   */
  setupDataCall: function setupDataCall(options) {
    let token = Buf.newParcel(REQUEST_SETUP_DATA_CALL, options);
    Buf.writeUint32(7);
    Buf.writeString(options.radioTech.toString());
    Buf.writeString(DATACALL_PROFILE_DEFAULT.toString());
    Buf.writeString(options.apn);
    Buf.writeString(options.user);
    Buf.writeString(options.passwd);
    Buf.writeString(options.chappap.toString());
    Buf.writeString(options.pdptype);
    Buf.sendParcel();
    return token;
  },

  /**
   * Deactivate a data call.
   *
   * @param cid
   *        String containing CID.
   * @param reason
   *        One of DATACALL_DEACTIVATE_* constants.
   */
  deactivateDataCall: function deactivateDataCall(options) {
    let datacall = this.currentDataCalls[options.cid];
    if (!datacall) {
      return;
    }

    let token = Buf.newParcel(REQUEST_DEACTIVATE_DATA_CALL, options);
    Buf.writeUint32(2);
    Buf.writeString(options.cid);
    Buf.writeString(options.reason || DATACALL_DEACTIVATE_NO_REASON);
    Buf.sendParcel();

    datacall.state = GECKO_NETWORK_STATE_DISCONNECTING;
    this.sendDOMMessage(datacall);
  },

  /**
   * Get a list of data calls.
   */
  getDataCallList: function getDataCallList() {
    Buf.simpleRequest(REQUEST_DATA_CALL_LIST);
  },

  /**
   * Get failure casue code for the most recently failed PDP context.
   */
  getFailCauseCode: function getFailCauseCode(options) {
    Buf.simpleRequest(REQUEST_LAST_CALL_FAIL_CAUSE, options);
  },

  /**
   * Helper to parse and process a MMI string.
   */
  _parseMMI: function _parseMMI(mmiString) {
    if (!mmiString || !mmiString.length) {
      return null;
    }

    // Regexp to parse and process the MMI code.
    if (this._mmiRegExp == null) {
      // The first group of the regexp takes the whole MMI string.
      // The second group takes the MMI procedure that can be:
      //    - Activation (*SC*SI#).
      //    - Deactivation (#SC*SI#).
      //    - Interrogation (*#SC*SI#).
      //    - Registration (**SC*SI#).
      //    - Erasure (##SC*SI#).
      //  where SC = Service Code (2 or 3 digits) and SI = Supplementary Info
      //  (variable length).
      let pattern = "((\\*[*#]?|##?)";

      // Third group of the regexp looks for the MMI Service code, which is a
      // 2 or 3 digits that uniquely specifies the Supplementary Service
      // associated with the MMI code.
      pattern += "(\\d{2,3})";

      // Groups from 4 to 9 looks for the MMI Supplementary Information SIA,
      // SIB and SIC. SIA may comprise e.g. a PIN code or Directory Number,
      // SIB may be used to specify the tele or bearer service and SIC to
      // specify the value of the "No Reply Condition Timer". Where a particular
      // service request does not require any SI, "*SI" is not entered. The use
      // of SIA, SIB and SIC is optional and shall be entered in any of the
      // following formats:
      //    - *SIA*SIB*SIC#
      //    - *SIA*SIB#
      //    - *SIA**SIC#
      //    - *SIA#
      //    - **SIB*SIC#
      //    - ***SISC#
      pattern += "(\\*([^*#]*)(\\*([^*#]*)(\\*([^*#]*)";

      // The eleventh group takes the password for the case of a password
      // registration procedure.
      pattern += "(\\*([^*#]*))?)?)?)?#)";

      // The last group takes the dial string after the #.
      pattern += "([^#]*)";

      this._mmiRegExp = new RegExp(pattern);
    }
    let matches = this._mmiRegExp.exec(mmiString);

    // If the regex does not apply over the MMI string, it can still be an MMI
    // code. If the MMI String is a #-string (entry of any characters defined
    // in the TS.23.038 Default Alphabet followed by #SEND) it shall be treated
    // as a USSD code.
    if (matches == null) {
      if (mmiString.charAt(mmiString.length - 1) == MMI_END_OF_USSD) {
        return {
          fullMMI: mmiString
        };
      }
      return null;
    }

    // After successfully executing the regular expresion over the MMI string,
    // the following match groups should contain:
    // 1 = full MMI string that might be used as a USSD request.
    // 2 = MMI procedure.
    // 3 = Service code.
    // 5 = SIA.
    // 7 = SIB.
    // 9 = SIC.
    // 11 = Password registration.
    // 12 = Dialing number.
    return {
      fullMMI: matches[MMI_MATCH_GROUP_FULL_MMI],
      procedure: matches[MMI_MATCH_GROUP_MMI_PROCEDURE],
      serviceCode: matches[MMI_MATCH_GROUP_SERVICE_CODE],
      sia: matches[MMI_MATCH_GROUP_SIA],
      sib: matches[MMI_MATCH_GROUP_SIB],
      sic: matches[MMI_MATCH_GROUP_SIC],
      pwd: matches[MMI_MATCH_GROUP_PWD_CONFIRM],
      dialNumber: matches[MMI_MATCH_GROUP_DIALING_NUMBER]
    };
  },

  sendMMI: function sendMMI(options) {
    if (DEBUG) {
      debug("SendMMI " + JSON.stringify(options));
    }
    let mmiString = options.mmi;
    let mmi = this._parseMMI(mmiString);

    let _sendMMIError = (function _sendMMIError(errorMsg) {
      options.rilMessageType = "sendMMI";
      options.errorMsg = errorMsg;
      this.sendDOMMessage(options);
    }).bind(this);

    function _isValidPINPUKRequest() {
      // The only allowed MMI procedure for ICC PIN, PIN2, PUK and PUK2 handling
      // is "Registration" (**).
      if (!mmi.procedure || mmi.procedure != MMI_PROCEDURE_REGISTRATION ) {
        _sendMMIError("WRONG_MMI_PROCEDURE");
        return;
      }

      if (!mmi.sia || !mmi.sia.length || !mmi.sib || !mmi.sib.length ||
          !mmi.sic || !mmi.sic.length) {
        _sendMMIError("MISSING_SUPPLEMENTARY_INFORMATION");
        return;
      }

      if (mmi.sib != mmi.sic) {
        _sendMMIError("NEW_PIN_MISMATCH");
        return;
      }

      return true;
    }

    if (mmi == null) {
      if (this._ussdSession) {
        options.ussd = mmiString;
        this.sendUSSD(options);
        return;
      }
      _sendMMIError("NO_VALID_MMI_STRING");
      return;
    }

    if (DEBUG) {
      debug("MMI " + JSON.stringify(mmi));
    }

    // We check if the MMI service code is supported and in that case we
    // trigger the appropriate RIL request if possible.
    let sc = mmi.serviceCode;

    switch (sc) {
      // Call forwarding
      case MMI_SC_CFU:
      case MMI_SC_CF_BUSY:
      case MMI_SC_CF_NO_REPLY:
      case MMI_SC_CF_NOT_REACHABLE:
      case MMI_SC_CF_ALL:
      case MMI_SC_CF_ALL_CONDITIONAL:
        // Call forwarding requires at least an action, given by the MMI
        // procedure, and a reason, given by the MMI service code, but there
        // is no way that we get this far without a valid procedure or service
        // code.
        options.action = MMI_PROC_TO_CF_ACTION[mmi.procedure];
        options.rilMessageType = "sendMMI";
        options.reason = MMI_SC_TO_CF_REASON[sc];
        options.number = mmi.sia;
        options.serviceClass = this._siToServiceClass(mmi.sib);
        if (options.action == CALL_FORWARD_ACTION_QUERY_STATUS) {
          this.queryCallForwardStatus(options);
          return;
        }

        options.rilMessageType = "setCallForward";
        options.isSendMMI = true;
        options.timeSeconds = mmi.sic;
        this.setCallForward(options);
        return;

      // Change the current ICC PIN number.
      case MMI_SC_PIN:
        // As defined in TS.122.030 6.6.2 to change the ICC PIN we should expect
        // an MMI code of the form **04*OLD_PIN*NEW_PIN*NEW_PIN#, where old PIN
        // should be entered as the SIA parameter and the new PIN as SIB and
        // SIC.
        if (!_isValidPINPUKRequest()) {
          return;
        }

        options.rilRequestType = "sendMMI";
        options.pin = mmi.sia;
        options.newPin = mmi.sib;
        this.changeICCPIN(options);
        return;

      // Change the current ICC PIN2 number.
      case MMI_SC_PIN2:
        // As defined in TS.122.030 6.6.2 to change the ICC PIN2 we should
        // enter and MMI code of the form **042*OLD_PIN2*NEW_PIN2*NEW_PIN2#,
        // where the old PIN2 should be entered as the SIA parameter and the
        // new PIN2 as SIB and SIC.
        if (!_isValidPINPUKRequest()) {
          return;
        }

        options.rilRequestType = "sendMMI";
        options.pin = mmi.sia;
        options.newPin = mmi.sib;
        this.changeICCPIN2(options);
        return;

      // Unblock ICC PIN.
      case MMI_SC_PUK:
        // As defined in TS.122.030 6.6.3 to unblock the ICC PIN we should
        // enter an MMI code of the form **05*PUK*NEW_PIN*NEW_PIN#, where PUK
        // should be entered as the SIA parameter and the new PIN as SIB and
        // SIC.
        if (!_isValidPINPUKRequest()) {
          return;
        }

        options.rilRequestType = "sendMMI";
        options.puk = mmi.sia;
        options.newPin = mmi.sib;
        this.enterICCPUK(options);
        return;

      // Unblock ICC PIN2.
      case MMI_SC_PUK2:
        // As defined in TS.122.030 6.6.3 to unblock the ICC PIN2 we should
        // enter an MMI code of the form **052*PUK2*NEW_PIN2*NEW_PIN2#, where
        // PUK2 should be entered as the SIA parameter and the new PIN2 as SIB
        // and SIC.
        if (!_isValidPINPUKRequest()) {
          return;
        }

        options.rilRequestType = "sendMMI";
        options.puk = mmi.sia;
        options.newPin = mmi.sib;
        this.enterICCPUK2(options);
        return;

      // IMEI
      case MMI_SC_IMEI:
        // A device's IMEI can't change, so we only need to request it once.
        if (this.IMEI == null) {
          this.getIMEI({mmi: true});
          return;
        }
        // If we already had the device's IMEI, we just send it to the DOM.
        options.rilMessageType = "sendMMI";
        options.success = true;
        options.result = this.IMEI;
        this.sendDOMMessage(options);
        return;

      // Call barring
      case MMI_SC_BAOC:
      case MMI_SC_BAOIC:
      case MMI_SC_BAOICxH:
      case MMI_SC_BAIC:
      case MMI_SC_BAICr:
      case MMI_SC_BA_ALL:
      case MMI_SC_BA_MO:
      case MMI_SC_BA_MT:
        _sendMMIError("CALL_BARRING_NOT_SUPPORTED_VIA_MMI");
        return;

      // Call waiting
      case MMI_SC_CALL_WAITING:
        _sendMMIError("CALL_WAITING_NOT_SUPPORTED_VIA_MMI");
        return;
    }

    // If the MMI code is not a known code and is a recognized USSD request or
    // a #-string, it shall still be sent as a USSD request.
    if (mmi.fullMMI &&
        (mmiString.charAt(mmiString.length - 1) == MMI_END_OF_USSD)) {
      options.ussd = mmi.fullMMI;
      this.sendUSSD(options);
      return;
    }

    // At this point, the MMI string is considered as not valid MMI code and
    // not valid USSD code.
    _sendMMIError("NOT_VALID_MMI_STRING");
  },

  /**
   * Send USSD.
   *
   * @param ussd
   *        String containing the USSD code.
   *
   */
   sendUSSD: function sendUSSD(options) {
     Buf.newParcel(REQUEST_SEND_USSD, options);
     Buf.writeString(options.ussd);
     Buf.sendParcel();
   },

  /**
   * Cancel pending USSD.
   */
   cancelUSSD: function cancelUSSD(options) {
     Buf.simpleRequest(REQUEST_CANCEL_USSD, options);
   },

  /**
   * Queries current call forward rules.
   *
   * @param reason
   *        One of nsIDOMMozMobileCFInfo.CALL_FORWARD_REASON_* constants.
   * @param serviceClass
   *        One of ICC_SERVICE_CLASS_* constants.
   * @param number
   *        Phone number of forwarding address.
   */
  queryCallForwardStatus: function queryCallForwardStatus(options) {
    Buf.newParcel(REQUEST_QUERY_CALL_FORWARD_STATUS, options);
    Buf.writeUint32(CALL_FORWARD_ACTION_QUERY_STATUS);
    Buf.writeUint32(options.reason);
    Buf.writeUint32(options.serviceClass);
    Buf.writeUint32(this._toaFromString(options.number));
    Buf.writeString(options.number);
    Buf.writeUint32(0);
    Buf.sendParcel();
  },

  /**
   * Configures call forward rule.
   *
   * @param action
   *        One of nsIDOMMozMobileCFInfo.CALL_FORWARD_ACTION_* constants.
   * @param reason
   *        One of nsIDOMMozMobileCFInfo.CALL_FORWARD_REASON_* constants.
   * @param serviceClass
   *        One of ICC_SERVICE_CLASS_* constants.
   * @param number
   *        Phone number of forwarding address.
   * @param timeSeconds
   *        Time in seconds to wait beforec all is forwarded.
   */
  setCallForward: function setCallForward(options) {
    Buf.newParcel(REQUEST_SET_CALL_FORWARD, options);
    Buf.writeUint32(options.action);
    Buf.writeUint32(options.reason);
    Buf.writeUint32(options.serviceClass);
    Buf.writeUint32(this._toaFromString(options.number));
    Buf.writeString(options.number);
    Buf.writeUint32(options.timeSeconds);
    Buf.sendParcel();
  },

  /**
   * Handle STK CALL_SET_UP request.
   *
   * @param hasConfirmed
   *        Does use have confirmed the call requested from ICC?
   */
  stkHandleCallSetup: function stkHandleCallSetup(options) {
     Buf.newParcel(REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM, options);
     Buf.writeUint32(1);
     Buf.writeUint32(options.hasConfirmed ? 1 : 0);
     Buf.sendParcel();
  },

  /**
   * Send STK Profile Download.
   *
   * @param profile Profile supported by ME.
   */
  sendStkTerminalProfile: function sendStkTerminalProfile(profile) {
    Buf.newParcel(REQUEST_STK_SET_PROFILE);
    Buf.writeUint32(profile.length * 2);
    for (let i = 0; i < profile.length; i++) {
      GsmPDUHelper.writeHexOctet(profile[i]);
    }
    Buf.writeUint32(0);
    Buf.sendParcel();
  },

  /**
   * Send STK terminal response.
   *
   * @param command
   * @param deviceIdentities
   * @param resultCode
   * @param [optional] itemIdentifier
   * @param [optional] input
   * @param [optional] isYesNo
   * @param [optional] hasConfirmed
   * @param [optional] localInfo
   * @param [optional] timer
   */
  sendStkTerminalResponse: function sendStkTerminalResponse(response) {
    if (response.hasConfirmed !== undefined) {
      this.stkHandleCallSetup(response);
      return;
    }

    let token = Buf.newParcel(REQUEST_STK_SEND_TERMINAL_RESPONSE);
    let command = response.command;

    // 1st mark for Parcel size
    Buf.startCalOutgoingSize(function(size) {
      // Parcel size is in string length, which costs 2 uint8 per char.
      Buf.writeUint32(size / 2);
    });

    // Command Details
    GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_COMMAND_DETAILS |
                               COMPREHENSIONTLV_FLAG_CR);
    GsmPDUHelper.writeHexOctet(3);
    if (response.command) {
      GsmPDUHelper.writeHexOctet(command.commandNumber);
      GsmPDUHelper.writeHexOctet(command.typeOfCommand);
      GsmPDUHelper.writeHexOctet(command.commandQualifier);
    } else {
      GsmPDUHelper.writeHexOctet(0x00);
      GsmPDUHelper.writeHexOctet(0x00);
      GsmPDUHelper.writeHexOctet(0x00);
    }

    // Device Identifier
    // According to TS102.223/TS31.111 section 6.8 Structure of
    // TERMINAL RESPONSE, "For all SIMPLE-TLV objects with Min=N,
    // the ME should set the CR(comprehension required) flag to
    // comprehension not required.(CR=0)"
    // Since DEVICE_IDENTITIES and DURATION TLVs have Min=N,
    // the CR flag is not set.
    GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_DEVICE_ID);
    GsmPDUHelper.writeHexOctet(2);
    GsmPDUHelper.writeHexOctet(STK_DEVICE_ID_ME);
    GsmPDUHelper.writeHexOctet(STK_DEVICE_ID_SIM);

    // Result
    GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_RESULT |
                               COMPREHENSIONTLV_FLAG_CR);
    GsmPDUHelper.writeHexOctet(1);
    GsmPDUHelper.writeHexOctet(response.resultCode);

    // Item Identifier
    if (response.itemIdentifier != null) {
      GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_ITEM_ID |
                                 COMPREHENSIONTLV_FLAG_CR);
      GsmPDUHelper.writeHexOctet(1);
      GsmPDUHelper.writeHexOctet(response.itemIdentifier);
    }

    // No need to process Text data if user requests help information.
    if (response.resultCode != STK_RESULT_HELP_INFO_REQUIRED) {
      let text;
      if (response.isYesNo !== undefined) {
        // GET_INKEY
        // When the ME issues a successful TERMINAL RESPONSE for a GET INKEY
        // ("Yes/No") command with command qualifier set to "Yes/No", it shall
        // supply the value '01' when the answer is "positive" and the value
        // '00' when the answer is "negative" in the Text string data object.
        text = response.isYesNo ? 0x01 : 0x00;
      } else {
        text = response.input;
      }

      if (text) {
        GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_TEXT_STRING |
                                   COMPREHENSIONTLV_FLAG_CR);

        // 2nd mark for text length
        Buf.startCalOutgoingSize(function(size) {
          // Text length is in number of hexOctets, which costs 4 uint8 per hexOctet.
          GsmPDUHelper.writeHexOctet(size / 4);
        });

        let coding = command.options.isUCS2 ?
                       STK_TEXT_CODING_UCS2 :
                       (command.options.isPacked ?
                          STK_TEXT_CODING_GSM_7BIT_PACKED :
                          STK_TEXT_CODING_GSM_8BIT);
        GsmPDUHelper.writeHexOctet(coding);

        // Write Text String.
        switch (coding) {
          case STK_TEXT_CODING_UCS2:
            GsmPDUHelper.writeUCS2String(text);
            break;
          case STK_TEXT_CODING_GSM_7BIT_PACKED:
            GsmPDUHelper.writeStringAsSeptets(text, 0, 0, 0);
            break;
          case STK_TEXT_CODING_GSM_8BIT:
            for (let i = 0; i < text.length; i++) {
              GsmPDUHelper.writeHexOctet(text.charCodeAt(i));
            }
            break;
        }

        // Calculate and write text length to 2nd mark
        Buf.stopCalOutgoingSize();
      }
    }

    // Local Information
    if (response.localInfo) {
      let localInfo = response.localInfo;

      // Location Infomation
      if (localInfo.locationInfo) {
        ComprehensionTlvHelper.writeLocationInfoTlv(localInfo.locationInfo);
      }

      // IMEI
      if (localInfo.imei != null) {
        let imei = localInfo.imei;
        if(imei.length == 15) {
          imei = imei + "0";
        }

        GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_IMEI);
        GsmPDUHelper.writeHexOctet(8);
        for (let i = 0; i < imei.length / 2; i++) {
          GsmPDUHelper.writeHexOctet(parseInt(imei.substr(i * 2, 2), 16));
        }
      }

      // Date and Time Zone
      if (localInfo.date != null) {
        ComprehensionTlvHelper.writeDateTimeZoneTlv(localInfo.date);
      }

      // Language
      if (localInfo.language) {
        ComprehensionTlvHelper.writeLanguageTlv(localInfo.language);
      }
    }

    // Timer
    if (response.timer) {
      let timer = response.timer;

      if (timer.timerId) {
        GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_TIMER_IDENTIFIER);
        GsmPDUHelper.writeHexOctet(1);
        GsmPDUHelper.writeHexOctet(timer.timerId);
      }

      if (timer.timerValue) {
        ComprehensionTlvHelper.writeTimerValueTlv(timer.timerValue, false);
      }
    }

    // Calculate and write Parcel size to 1st mark
    Buf.stopCalOutgoingSize();

    Buf.writeUint32(0);
    Buf.sendParcel();
  },

  /**
   * Send STK Envelope(Menu Selection) command.
   *
   * @param itemIdentifier
   * @param helpRequested
   */
  sendStkMenuSelection: function sendStkMenuSelection(command) {
    command.tag = BER_MENU_SELECTION_TAG;
    command.deviceId = {
      sourceId :STK_DEVICE_ID_KEYPAD,
      destinationId: STK_DEVICE_ID_SIM
    };
    this.sendICCEnvelopeCommand(command);
  },

  /**
   * Send STK Envelope(Timer Expiration) command.
   *
   * @param timer
   */
  sendStkTimerExpiration: function sendStkTimerExpiration(command) {
    command.tag = BER_TIMER_EXPIRATION_TAG;
    command.deviceId = {
      sourceId: STK_DEVICE_ID_ME,
      destinationId: STK_DEVICE_ID_SIM
    };
    command.timerId = command.timer.timerId;
    command.timerValue = command.timer.timerValue;
    this.sendICCEnvelopeCommand(command);
  },

  /**
   * Send STK Envelope(Event Download) command.
   * @param event
   */
  sendStkEventDownload: function sendStkEventDownload(command) {
    command.tag = BER_EVENT_DOWNLOAD_TAG;
    command.eventList = command.event.eventType;
    switch (command.eventList) {
      case STK_EVENT_TYPE_LOCATION_STATUS:
        command.deviceId = {
          sourceId :STK_DEVICE_ID_ME,
          destinationId: STK_DEVICE_ID_SIM
        };
        command.locationStatus = command.event.locationStatus;
        // Location info should only be provided when locationStatus is normal.
        if (command.locationStatus == STK_SERVICE_STATE_NORMAL) {
          command.locationInfo = command.event.locationInfo;
        }
        break;
      case STK_EVENT_TYPE_MT_CALL:
        command.deviceId = {
          sourceId: STK_DEVICE_ID_NETWORK,
          destinationId: STK_DEVICE_ID_SIM
        };
        command.transactionId = 0;
        command.address = command.event.number;
        break;
      case STK_EVENT_TYPE_CALL_DISCONNECTED:
        command.cause = command.event.error;
      case STK_EVENT_TYPE_CALL_CONNECTED:  // Fall through
        command.deviceId = {
          sourceId: (command.event.isIssuedByRemote ?
                     STK_DEVICE_ID_NETWORK : STK_DEVICE_ID_ME),
          destinationId: STK_DEVICE_ID_SIM
        };
        command.transactionId = 0;
        break;
      case STK_EVENT_TYPE_IDLE_SCREEN_AVAILABLE:
        command.deviceId = {
          sourceId: STK_DEVICE_ID_DISPLAY,
          destinationId: STK_DEVICE_ID_SIM
        };
        break;
      case STK_EVENT_TYPE_LANGUAGE_SELECTION:
        command.deviceId = {
          sourceId: STK_DEVICE_ID_ME,
          destinationId: STK_DEVICE_ID_SIM
        };
        command.language = command.event.language;
        break;
    }
    this.sendICCEnvelopeCommand(command);
  },

  /**
   * Send REQUEST_STK_SEND_ENVELOPE_COMMAND to ICC.
   *
   * @param tag
   * @patam deviceId
   * @param [optioanl] itemIdentifier
   * @param [optional] helpRequested
   * @param [optional] eventList
   * @param [optional] locationStatus
   * @param [optional] locationInfo
   * @param [optional] address
   * @param [optional] transactionId
   * @param [optional] cause
   * @param [optional] timerId
   * @param [optional] timerValue
   */
  sendICCEnvelopeCommand: function sendICCEnvelopeCommand(options) {
    if (DEBUG) {
      debug("Stk Envelope " + JSON.stringify(options));
    }
    let token = Buf.newParcel(REQUEST_STK_SEND_ENVELOPE_COMMAND);

    // 1st mark for Parcel size
    Buf.startCalOutgoingSize(function(size) {
      // Parcel size is in string length, which costs 2 uint8 per char.
      Buf.writeUint32(size / 2);
    });

    // Write a BER-TLV
    GsmPDUHelper.writeHexOctet(options.tag);
    // 2nd mark for BER length
    Buf.startCalOutgoingSize(function(size) {
      // BER length is in number of hexOctets, which costs 4 uint8 per hexOctet.
      GsmPDUHelper.writeHexOctet(size / 4);
    });

    // Device Identifies
    GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_DEVICE_ID |
                               COMPREHENSIONTLV_FLAG_CR);
    GsmPDUHelper.writeHexOctet(2);
    GsmPDUHelper.writeHexOctet(options.deviceId.sourceId);
    GsmPDUHelper.writeHexOctet(options.deviceId.destinationId);

    // Item Identifier
    if (options.itemIdentifier != null) {
      GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_ITEM_ID |
                                 COMPREHENSIONTLV_FLAG_CR);
      GsmPDUHelper.writeHexOctet(1);
      GsmPDUHelper.writeHexOctet(options.itemIdentifier);
    }

    // Help Request
    if (options.helpRequested) {
      GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_HELP_REQUEST |
                                 COMPREHENSIONTLV_FLAG_CR);
      GsmPDUHelper.writeHexOctet(0);
      // Help Request doesn't have value
    }

    // Event List
    if (options.eventList != null) {
      GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_EVENT_LIST |
                                 COMPREHENSIONTLV_FLAG_CR);
      GsmPDUHelper.writeHexOctet(1);
      GsmPDUHelper.writeHexOctet(options.eventList);
    }

    // Location Status
    if (options.locationStatus != null) {
      let len = options.locationStatus.length;
      GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_LOCATION_STATUS |
                                 COMPREHENSIONTLV_FLAG_CR);
      GsmPDUHelper.writeHexOctet(1);
      GsmPDUHelper.writeHexOctet(options.locationStatus);
    }

    // Location Info
    if (options.locationInfo) {
      ComprehensionTlvHelper.writeLocationInfoTlv(options.locationInfo);
    }

    // Transaction Id
    if (options.transactionId != null) {
      GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_TRANSACTION_ID |
                                 COMPREHENSIONTLV_FLAG_CR);
      GsmPDUHelper.writeHexOctet(1);
      GsmPDUHelper.writeHexOctet(options.transactionId);
    }

    // Address
    if (options.address) {
      GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_ADDRESS |
                                 COMPREHENSIONTLV_FLAG_CR);
      ComprehensionTlvHelper.writeLength(
        Math.ceil(options.address.length/2) + 1 // address BCD + TON
      );
      GsmPDUHelper.writeDiallingNumber(options.address);
    }

    // Cause of disconnection.
    if (options.cause != null) {
      ComprehensionTlvHelper.writeCauseTlv(options.cause);
    }

    // Timer Identifier
    if (options.timerId != null) {
        GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_TIMER_IDENTIFIER |
                                   COMPREHENSIONTLV_FLAG_CR);
        GsmPDUHelper.writeHexOctet(1);
        GsmPDUHelper.writeHexOctet(options.timerId);
    }

    // Timer Value
    if (options.timerValue != null) {
        ComprehensionTlvHelper.writeTimerValueTlv(options.timerValue, true);
    }

    // Language
    if (options.language) {
      ComprehensionTlvHelper.writeLanguageTlv(options.language);
    }

    // Calculate and write BER length to 2nd mark
    Buf.stopCalOutgoingSize();

    // Calculate and write Parcel size to 1st mark
    Buf.stopCalOutgoingSize();

    Buf.writeUint32(0);
    Buf.sendParcel();
  },

  /**
   * Check a given number against the list of emergency numbers provided by the RIL.
   *
   * @param number
   *        The number to look up.
   */
   _isEmergencyNumber: function _isEmergencyNumber(number) {
     // Check read-write ecclist property first.
     let numbers = libcutils.property_get("ril.ecclist");
     if (!numbers) {
       // Then read-only ecclist property since others RIL only uses this.
       numbers = libcutils.property_get("ro.ril.ecclist");
     }

     if (numbers) {
       numbers = numbers.split(",");
     } else {
       // No ecclist system property, so use our own list.
       numbers = DEFAULT_EMERGENCY_NUMBERS;
     }

     return numbers.indexOf(number) != -1;
   },

  /**
   * Report STK Service is running.
   */
  reportStkServiceIsRunning: function reportStkServiceIsRunning() {
    Buf.simpleRequest(REQUEST_REPORT_STK_SERVICE_IS_RUNNING);
  },

  /**
   * Process ICC status.
   */
  _processICCStatus: function _processICCStatus(iccStatus) {
    this.iccStatus = iccStatus;

    if ((!iccStatus) || (iccStatus.cardState == CARD_STATE_ABSENT)) {
      if (DEBUG) debug("ICC absent");
      if (this.cardState == GECKO_CARDSTATE_ABSENT) {
        this.operator = null;
        return;
      }
      this.cardState = GECKO_CARDSTATE_ABSENT;
      this.sendDOMMessage({rilMessageType: "cardstatechange",
                           cardState: this.cardState});
      return;
    }

    // TODO: Bug 726098, change to use cdmaSubscriptionAppIndex when in CDMA.
    let index = iccStatus.gsmUmtsSubscriptionAppIndex;
    let app = iccStatus.apps[index];
    if (!app) {
      if (DEBUG) {
        debug("Subscription application is not present in iccStatus.");
      }
      if (this.cardState == GECKO_CARDSTATE_ABSENT) {
        return;
      }
      this.cardState = GECKO_CARDSTATE_ABSENT;
      this.operator = null;
      this.sendDOMMessage({rilMessageType: "cardstatechange",
                           cardState: this.cardState});
      return;
    }
    // fetchICCRecords will need to read aid, so read aid here.
    this.aid = app.aid;
    this.appType = app.app_type;

    let newCardState;
    switch (app.app_state) {
      case CARD_APPSTATE_PIN:
        newCardState = GECKO_CARDSTATE_PIN_REQUIRED;
        break;
      case CARD_APPSTATE_PUK:
        newCardState = GECKO_CARDSTATE_PUK_REQUIRED;
        break;
      case CARD_APPSTATE_SUBSCRIPTION_PERSO:
        newCardState = GECKO_CARDSTATE_NETWORK_LOCKED;
        break;
      case CARD_APPSTATE_READY:
        newCardState = GECKO_CARDSTATE_READY;
        break;
      case CARD_APPSTATE_UNKNOWN:
        newCardState = GECKO_CARDSTATE_UNKNOWN;
        break;
      case CARD_APPSTATE_DETECTED:
      default:
        newCardState = GECKO_CARDSTATE_NOT_READY;
    }

    if (this.cardState == newCardState) {
      return;
    }

    // This was moved down from CARD_APPSTATE_READY
    this.requestNetworkInfo();
    this.getSignalStrength();
    if (newCardState == GECKO_CARDSTATE_READY) {
      // For type SIM, we need to check EF_phase first.
      // Other types of ICC we can send Terminal_Profile immediately.
      if (this.appType == CARD_APPTYPE_SIM) {
        ICCRecordHelper.getICCPhase();
      } else {
        this.sendStkTerminalProfile(STK_SUPPORTED_TERMINAL_PROFILE);
      }
      ICCRecordHelper.fetchICCRecords();
      this.reportStkServiceIsRunning();
    }

    this.cardState = newCardState;
    this.sendDOMMessage({rilMessageType: "cardstatechange",
                         cardState: this.cardState});
  },

   /**
   * Helper for processing responses of functions such as enterICC* and changeICC*.
   */
  _processEnterAndChangeICCResponses: function _processEnterAndChangeICCResponses(length, options) {
    options.success = options.rilRequestError == 0;
    if (!options.success) {
      options.errorMsg = RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError];
    }
    options.retryCount = length ? Buf.readUint32List()[0] : -1;
    this.sendDOMMessage(options);
  },

  // We combine all of the NETWORK_INFO_MESSAGE_TYPES into one "networkinfochange"
  // message to the RadioInterfaceLayer, so we can avoid sending multiple
  // VoiceInfoChanged events for both operator / voice_data_registration
  //
  // State management here is a little tricky. We need to know both:
  // 1. Whether or not a response was received for each of the
  //    NETWORK_INFO_MESSAGE_TYPES
  // 2. The outbound message that corresponds with that response -- but this
  //    only happens when internal state changes (i.e. it isn't guaranteed)
  //
  // To collect this state, each message response function first calls
  // _receivedNetworkInfo, to mark the response as received. When the
  // final response is received, a call to _sendPendingNetworkInfo is placed
  // on the next tick of the worker thread.
  //
  // Since the original call to _receivedNetworkInfo happens at the top
  // of the response handler, this gives the final handler a chance to
  // queue up it's "changed" message by calling _sendNetworkInfoMessage if/when
  // the internal state has actually changed.
  _sendNetworkInfoMessage: function _sendNetworkInfoMessage(type, message) {
    if (!this._processingNetworkInfo) {
      // We only combine these messages in the case of the combined request
      // in requestNetworkInfo()
      this.sendDOMMessage(message);
      return;
    }

    if (DEBUG) debug("Queuing " + type + " network info message: " + JSON.stringify(message));
    this._pendingNetworkInfo[type] = message;
  },

  _receivedNetworkInfo: function _receivedNetworkInfo(type) {
    if (DEBUG) debug("Received " + type + " network info.");
    if (!this._processingNetworkInfo) {
      return;
    }

    let pending = this._pendingNetworkInfo;

    // We still need to track states for events that aren't fired.
    if (!(type in pending)) {
      pending[type] = PENDING_NETWORK_TYPE;
    }

    // Pending network info is ready to be sent when no more messages
    // are waiting for responses, but the combined payload hasn't been sent.
    for (let i = 0; i < NETWORK_INFO_MESSAGE_TYPES.length; i++) {
      let type = NETWORK_INFO_MESSAGE_TYPES[i];
      if (!(type in pending)) {
        if (DEBUG) debug("Still missing some more network info, not notifying main thread.");
        return;
      }
    }

    // Do a pass to clean up the processed messages that didn't create
    // a response message, so we don't have unused keys in the outbound
    // networkinfochanged message.
    for (let key in pending) {
      if (pending[key] == PENDING_NETWORK_TYPE) {
        delete pending[key];
      }
    }

    if (DEBUG) debug("All pending network info has been received: " + JSON.stringify(pending));

    // Send the message on the next tick of the worker's loop, so we give the
    // last message a chance to call _sendNetworkInfoMessage first.
    setTimeout(this._sendPendingNetworkInfo, 0);
  },

  _sendPendingNetworkInfo: function _sendPendingNetworkInfo() {
    RIL.sendDOMMessage(RIL._pendingNetworkInfo);

    RIL._processingNetworkInfo = false;
    for (let i = 0; i < NETWORK_INFO_MESSAGE_TYPES.length; i++) {
      delete RIL._pendingNetworkInfo[NETWORK_INFO_MESSAGE_TYPES[i]];
    }
  },

  /**
   * Process the network registration flags.
   *
   * @return true if the state changed, false otherwise.
   */
  _processCREG: function _processCREG(curState, newState) {
    let changed = false;

    let regState = RIL.parseInt(newState[0], NETWORK_CREG_STATE_UNKNOWN);
    if (curState.regState != regState) {
      changed = true;
      curState.regState = regState;

      curState.state = NETWORK_CREG_TO_GECKO_MOBILE_CONNECTION_STATE[regState];
      curState.connected = regState == NETWORK_CREG_STATE_REGISTERED_HOME ||
                           regState == NETWORK_CREG_STATE_REGISTERED_ROAMING;
      curState.roaming = regState == NETWORK_CREG_STATE_REGISTERED_ROAMING;
      curState.emergencyCallsOnly =
        (regState >= NETWORK_CREG_STATE_NOT_SEARCHING_EMERGENCY_CALLS) &&
        (regState <= NETWORK_CREG_STATE_UNKNOWN_EMERGENCY_CALLS);
      if (RILQUIRKS_MODEM_DEFAULTS_TO_EMERGENCY_MODE) {
        curState.emergencyCallsOnly = !curState.connected;
      }
    }

    if (!curState.cell) {
      curState.cell = {};
    }

    // From TS 23.003, 0000 and 0xfffe are indicated that no valid LAI exists
    // in MS. So we still need to report the '0000' as well.
    let lac = RIL.parseInt(newState[1], -1, 16);
    if (curState.cell.gsmLocationAreaCode !== lac) {
      curState.cell.gsmLocationAreaCode = lac;
      changed = true;
    }

    let cid = RIL.parseInt(newState[2], -1, 16);
    if (curState.cell.gsmCellId !== cid) {
      curState.cell.gsmCellId = cid;
      changed = true;
    }

    let radioTech = RIL.parseInt(newState[3], NETWORK_CREG_TECH_UNKNOWN);
    if (curState.radioTech != radioTech) {
      changed = true;
      curState.radioTech = radioTech;
      curState.type = GECKO_RADIO_TECH[radioTech] || null;
    }
    return changed;
  },

  _processVoiceRegistrationState: function _processVoiceRegistrationState(state) {
    let rs = this.voiceRegistrationState;
    let stateChanged = this._processCREG(rs, state);
    if (stateChanged && rs.connected) {
      RIL.getSMSCAddress();
    }

    // TODO: This zombie code branch that will be raised from the dead once
    // we add explicit CDMA support everywhere (bug 726098).
    let cdma = false;
    if (cdma) {
      let baseStationId = RIL.parseInt(state[4]);
      let baseStationLatitude = RIL.parseInt(state[5]);
      let baseStationLongitude = RIL.parseInt(state[6]);
      if (!baseStationLatitude && !baseStationLongitude) {
        baseStationLatitude = baseStationLongitude = null;
      }
      let cssIndicator = RIL.parseInt(state[7]);
      let systemId = RIL.parseInt(state[8]);
      let networkId = RIL.parseInt(state[9]);
      let roamingIndicator = RIL.parseInt(state[10]);
      let systemIsInPRL = RIL.parseInt(state[11]);
      let defaultRoamingIndicator = RIL.parseInt(state[12]);
      let reasonForDenial = RIL.parseInt(state[13]);
    }

    if (stateChanged) {
      rs.rilMessageType = "voiceregistrationstatechange";
      this._sendNetworkInfoMessage(NETWORK_INFO_VOICE_REGISTRATION_STATE, rs);
    }
  },

  _processDataRegistrationState: function _processDataRegistrationState(state) {
    let rs = this.dataRegistrationState;
    let stateChanged = this._processCREG(rs, state);
    if (stateChanged) {
      rs.rilMessageType = "dataregistrationstatechange";
      this._sendNetworkInfoMessage(NETWORK_INFO_DATA_REGISTRATION_STATE, rs);
    }
  },

  _processOperator: function _processOperator(operatorData) {
    if (operatorData.length < 3) {
      if (DEBUG) {
        debug("Expected at least 3 strings for operator.");
      }
    }

    if (!this.operator) {
      this.operator = {rilMessageType: "operatorchange"};
    }

    let [longName, shortName, networkTuple] = operatorData;
    let thisTuple = "" + this.operator.mcc + this.operator.mnc;

    if (this.operator.longName !== longName ||
        this.operator.shortName !== shortName ||
        thisTuple !== networkTuple) {

      let networkName = this.updateNetworkName();
      if (networkName) {
        this.operator.longName = networkName[0];
        this.operator.shortName = networkName[1];
      } else {
        this.operator.longName = longName;
        this.operator.shortName = shortName;
      }

      this.operator.mcc = 0;
      this.operator.mnc = 0;

      // According to ril.h, the operator fields will be NULL when the operator
      // is not currently registered. We can avoid trying to parse the numeric
      // tuple in that case.
      if (DEBUG && !longName) {
        debug("Operator is currently unregistered");
      }

      if (networkTuple) {
        try {
          this._processNetworkTuple(networkTuple, this.operator);
        } catch (e) {
          debug("Error processing operator tuple: " + e);
        }
      }
      if (ICCUtilsHelper.updateDisplayCondition()) {
        ICCUtilsHelper.handleICCInfoChange();
      }
      this._sendNetworkInfoMessage(NETWORK_INFO_OPERATOR, this.operator);
    }
  },

  /**
   * Helpers for processing call state and handle the active call.
   */
  _processCalls: function _processCalls(newCalls) {
    // Go through the calls we currently have on file and see if any of them
    // changed state. Remove them from the newCalls map as we deal with them
    // so that only new calls remain in the map after we're done.
    for each (let currentCall in this.currentCalls) {
      let newCall;
      if (newCalls) {
        newCall = newCalls[currentCall.callIndex];
        delete newCalls[currentCall.callIndex];
      }

      if (!newCall) {
        // Call is no longer reported by the radio. Remove from our map and
        // send disconnected state change.
        delete this.currentCalls[currentCall.callIndex];
        this.getFailCauseCode(currentCall);
        continue;
      }

      // Call is still valid.
      if (newCall.state == currentCall.state) {
        continue;
      }

      // State has changed.
      if (newCall.state == CALL_STATE_INCOMING &&
          currentCall.state == CALL_STATE_WAITING) {
        // Update the call internally but we don't notify DOM since these two
        // states are viewed as the same one there.
        currentCall.state = newCall.state;
        continue;
      }

      if (!currentCall.started && newCall.state == CALL_STATE_ACTIVE) {
        currentCall.started = new Date().getTime();
      }
      currentCall.state = newCall.state;
      this._handleChangedCallState(currentCall);
    }

    // Go through any remaining calls that are new to us.
    for each (let newCall in newCalls) {
      if (newCall.isVoice) {
        // Format international numbers appropriately.
        if (newCall.number &&
            newCall.toa == TOA_INTERNATIONAL &&
            newCall.number[0] != "+") {
          newCall.number = "+" + newCall.number;
        }
        if (newCall.state == CALL_STATE_INCOMING) {
          newCall.direction = 'incoming';
        } else if (newCall.state == CALL_STATE_DIALING) {
          newCall.direction = 'outgoing';
        }
        // Add to our map.
        this.currentCalls[newCall.callIndex] = newCall;
        this._handleChangedCallState(newCall);
      }
    }

    // Update our mute status. If there is anything in our currentCalls map then
    // we know it's a voice call and we should leave audio on.
    this.muted = Object.getOwnPropertyNames(this.currentCalls).length == 0;
  },

  _handleChangedCallState: function _handleChangedCallState(changedCall) {
    let message = {rilMessageType: "callStateChange",
                   call: changedCall};
    this.sendDOMMessage(message);
  },

  _handleDisconnectedCall: function _handleDisconnectedCall(disconnectedCall) {
    let message = {rilMessageType: "callDisconnected",
                   call: disconnectedCall};
    this.sendDOMMessage(message);
  },

  _sendDataCallError: function _sendDataCallError(message, errorCode) {
    message.rilMessageType = "datacallerror";
    if (errorCode == ERROR_GENERIC_FAILURE) {
      message.error = RIL_ERROR_TO_GECKO_ERROR[errorCode];
    } else {
      message.error = RIL_DATACALL_FAILCAUSE_TO_GECKO_DATACALL_ERROR[errorCode];
    }
    this.sendDOMMessage(message);
  },

  _processDataCallList: function _processDataCallList(datacalls, newDataCallOptions) {
    // Check for possible PDP errors: We check earlier because the datacall
    // can be removed if is the same as the current one.
    for each (let newDataCall in datacalls) {
      if (newDataCall.status != DATACALL_FAIL_NONE) {
        if (newDataCallOptions) {
          newDataCall.apn = newDataCallOptions.apn;
        }
        this._sendDataCallError(newDataCall, newDataCall.status);
      }
    }

    for each (let currentDataCall in this.currentDataCalls) {
      let updatedDataCall;
      if (datacalls) {
        updatedDataCall = datacalls[currentDataCall.cid];
        delete datacalls[currentDataCall.cid];
      }

      if (!updatedDataCall) {
        // If datacalls list is coming from REQUEST_SETUP_DATA_CALL response,
        // we do not change state for any currentDataCalls not in datacalls list.
        if (!newDataCallOptions) {
          currentDataCall.state = GECKO_NETWORK_STATE_DISCONNECTED;
          currentDataCall.rilMessageType = "datacallstatechange";
          this.sendDOMMessage(currentDataCall);
        }
        continue;
      }

      if (updatedDataCall && !updatedDataCall.ifname) {
        delete this.currentDataCalls[currentDataCall.cid];
        currentDataCall.state = GECKO_NETWORK_STATE_UNKNOWN;
        currentDataCall.rilMessageType = "datacallstatechange";
        this.sendDOMMessage(currentDataCall);
        continue;
      }

      this._setDataCallGeckoState(updatedDataCall);
      if (updatedDataCall.state != currentDataCall.state) {
        currentDataCall.status = updatedDataCall.status;
        currentDataCall.active = updatedDataCall.active;
        currentDataCall.state = updatedDataCall.state;
        currentDataCall.rilMessageType = "datacallstatechange";
        this.sendDOMMessage(currentDataCall);
      }
    }

    for each (let newDataCall in datacalls) {
      if (!newDataCall.ifname) {
        continue;
      }
      this.currentDataCalls[newDataCall.cid] = newDataCall;
      this._setDataCallGeckoState(newDataCall);
      if (newDataCallOptions) {
        newDataCall.radioTech = newDataCallOptions.radioTech;
        newDataCall.apn = newDataCallOptions.apn;
        newDataCall.user = newDataCallOptions.user;
        newDataCall.passwd = newDataCallOptions.passwd;
        newDataCall.chappap = newDataCallOptions.chappap;
        newDataCall.pdptype = newDataCallOptions.pdptype;
        newDataCallOptions = null;
      } else if (DEBUG) {
        debug("Unexpected new data call: " + JSON.stringify(newDataCall));
      }
      newDataCall.rilMessageType = "datacallstatechange";
      this.sendDOMMessage(newDataCall);
    }
  },

  _setDataCallGeckoState: function _setDataCallGeckoState(datacall) {
    switch (datacall.active) {
      case DATACALL_INACTIVE:
        datacall.state = GECKO_NETWORK_STATE_DISCONNECTED;
        break;
      case DATACALL_ACTIVE_DOWN:
      case DATACALL_ACTIVE_UP:
        datacall.state = GECKO_NETWORK_STATE_CONNECTED;
        break;
    }
  },

  _processNetworks: function _processNetworks() {
    let strings = Buf.readStringList();
    let networks = [];

    for (let i = 0; i < strings.length; i += 4) {
      let network = {
        longName: strings[i],
        shortName: strings[i + 1],
        mcc: 0, mnc: 0,
        state: null
      };

      let networkTuple = strings[i + 2];
      try {
        this._processNetworkTuple(networkTuple, network);
      } catch (e) {
        debug("Error processing operator tuple: " + e);
      }

      let state = strings[i + 3];
      if (state === NETWORK_STATE_UNKNOWN) {
        // TODO: looks like this might conflict in style with
        // GECKO_NETWORK_STYLE_UNKNOWN / nsINetworkManager
        state = GECKO_QAN_STATE_UNKNOWN;
      }

      network.state = state;
      networks.push(network);
    }
    return networks;
  },

  /**
   * The "numeric" portion of the operator info is a tuple
   * containing MCC (country code) and MNC (network code).
   * AFAICT, MCC should always be 3 digits, making the remaining
   * portion the MNC.
   */
  _processNetworkTuple: function _processNetworkTuple(networkTuple, network) {
    let tupleLen = networkTuple.length;
    let mcc = 0, mnc = 0;

    if (tupleLen == 5 || tupleLen == 6) {
      mcc = parseInt(networkTuple.substr(0, 3), 10);
      if (isNaN(mcc)) {
        throw new Error("MCC could not be parsed from network tuple: " + networkTuple );
      }

      mnc = parseInt(networkTuple.substr(3), 10);
      if (isNaN(mnc)) {
        throw new Error("MNC could not be parsed from network tuple: " + networkTuple);
      }
    } else {
      throw new Error("Invalid network tuple (should be 5 or 6 digits): " + networkTuple);
    }

    network.mcc = mcc;
    network.mnc = mnc;
  },

  /**
   * Process radio technology change.
   */
  _processRadioTech: function _processRadioTech(radioTech) {
    let isCdma = true;
    this.radioTech = radioTech;

    switch(radioTech) {
    case NETWORK_CREG_TECH_GPRS:
    case NETWORK_CREG_TECH_EDGE:
    case NETWORK_CREG_TECH_UMTS:
    case NETWORK_CREG_TECH_HSDPA:
    case NETWORK_CREG_TECH_HSUPA:
    case NETWORK_CREG_TECH_HSPA:
    case NETWORK_CREG_TECH_LTE:
    case NETWORK_CREG_TECH_HSPAP:
    case NETWORK_CREG_TECH_GSM:
      isCdma = false;
    };

    if (DEBUG) {
      debug("Radio tech is set to: " + GECKO_RADIO_TECH[radioTech] +
            ", it is a " + (isCdma?"cdma":"gsm") + " technology");
    }

    // We should request SIM information when
    //  1. Radio state has been changed, so we are waiting for radioTech or
    //  2. isCdma is different from this._isCdma.
    if (this._waitingRadioTech || isCdma != this._isCdma) {
      this._isCdma = isCdma;
      this._waitingRadioTech = false;
      if (this._isCdma) {
        this.getDeviceIdentity();
      } else {
        this.getIMEI();
        this.getIMEISV();
      }
       this.getICCStatus();
    }
  },

  /**
   * Helper for returning the TOA for the given dial string.
   */
  _toaFromString: function _toaFromString(number) {
    let toa = TOA_UNKNOWN;
    if (number && number.length > 0 && number[0] == '+') {
      toa = TOA_INTERNATIONAL;
    }
    return toa;
  },

  /**
   * Helper for translating basic service group to call forwarding service class
   * parameter.
   */
  _siToServiceClass: function _siToServiceClass(si) {
    if (!si) {
      return ICC_SERVICE_CLASS_NONE;
    }

    let serviceCode = parseInt(si, 10);
    switch (serviceCode) {
      case 10:
        return ICC_SERVICE_CLASS_SMS + ICC_SERVICE_CLASS_FAX  + ICC_SERVICE_CLASS_VOICE;
      case 11:
        return ICC_SERVICE_CLASS_VOICE;
      case 12:
        return ICC_SERVICE_CLASS_SMS + ICC_SERVICE_CLASS_FAX;
      case 13:
        return ICC_SERVICE_CLASS_FAX;
      case 16:
        return ICC_SERVICE_CLASS_SMS;
      case 19:
        return ICC_SERVICE_CLASS_FAX + ICC_SERVICE_CLASS_VOICE;
      case 21:
        return ICC_SERVICE_CLASS_PAD + ICC_SERVICE_CLASS_DATA_ASYNC;
      case 22:
        return ICC_SERVICE_CLASS_PACKET + ICC_SERVICE_CLASS_DATA_SYNC;
      case 25:
        return ICC_SERVICE_CLASS_DATA_ASYNC;
      case 26:
        return ICC_SERVICE_CLASS_DATA_SYNC + SERVICE_CLASS_VOICE;
      case 99:
        return ICC_SERVICE_CLASS_PACKET;
      default:
        return ICC_SERVICE_CLASS_NONE;
    }
  },

  /**
   * @param message A decoded SMS-DELIVER message.
   *
   * @see 3GPP TS 31.111 section 7.1.1
   */
  dataDownloadViaSMSPP: function dataDownloadViaSMSPP(message) {
    let options = {
      pid: message.pid,
      dcs: message.dcs,
      encoding: message.encoding,
    };
    Buf.newParcel(REQUEST_STK_SEND_ENVELOPE_WITH_STATUS, options);

    Buf.seekIncoming(-1 * (Buf.currentParcelSize - Buf.readAvailable
                           - 2 * UINT32_SIZE)); // Skip response_type & request_type.
    let messageStringLength = Buf.readUint32(); // In semi-octets
    let smscLength = GsmPDUHelper.readHexOctet(); // In octets, inclusive of TOA
    let tpduLength = (messageStringLength / 2) - (smscLength + 1); // In octets

    // Device identities: 4 bytes
    // Address: 0 or (2 + smscLength)
    // SMS TPDU: (2 or 3) + tpduLength
    let berLen = 4 +
                 (smscLength ? (2 + smscLength) : 0) +
                 (tpduLength <= 127 ? 2 : 3) + tpduLength; // In octets

    let parcelLength = (berLen <= 127 ? 2 : 3) + berLen; // In octets
    Buf.writeUint32(parcelLength * 2); // In semi-octets

    // Write a BER-TLV
    GsmPDUHelper.writeHexOctet(BER_SMS_PP_DOWNLOAD_TAG);
    if (berLen > 127) {
      GsmPDUHelper.writeHexOctet(0x81);
    }
    GsmPDUHelper.writeHexOctet(berLen);

    // Device Identifies-TLV
    GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_DEVICE_ID |
                               COMPREHENSIONTLV_FLAG_CR);
    GsmPDUHelper.writeHexOctet(0x02);
    GsmPDUHelper.writeHexOctet(STK_DEVICE_ID_NETWORK);
    GsmPDUHelper.writeHexOctet(STK_DEVICE_ID_SIM);

    // Address-TLV
    if (smscLength) {
      GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_ADDRESS);
      GsmPDUHelper.writeHexOctet(smscLength);
      Buf.copyIncomingToOutgoing(PDU_HEX_OCTET_SIZE * smscLength);
    }

    // SMS TPDU-TLV
    GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_SMS_TPDU |
                               COMPREHENSIONTLV_FLAG_CR);
    if (tpduLength > 127) {
      GsmPDUHelper.writeHexOctet(0x81);
    }
    GsmPDUHelper.writeHexOctet(tpduLength);
    Buf.copyIncomingToOutgoing(PDU_HEX_OCTET_SIZE * tpduLength);

    // Write 2 string delimitors for the total string length must be even.
    Buf.writeStringDelimiter(0);

    Buf.sendParcel();
  },

  /**
   * @param success A boolean value indicating the result of previous
   *                SMS-DELIVER message handling.
   * @param responsePduLen ICC IO response PDU length in octets.
   * @param options An object that contains four attributes: `pid`, `dcs`,
   *                `encoding` and `responsePduLen`.
   *
   * @see 3GPP TS 23.040 section 9.2.2.1a
   */
  acknowledgeIncomingGsmSmsWithPDU: function acknowledgeIncomingGsmSmsWithPDU(success, responsePduLen, options) {
    Buf.newParcel(REQUEST_ACKNOWLEDGE_INCOMING_GSM_SMS_WITH_PDU);

    // Two strings.
    Buf.writeUint32(2);

    // String 1: Success
    Buf.writeString(success ? "1" : "0");

    // String 2: RP-ACK/RP-ERROR PDU
    Buf.writeUint32(2 * (responsePduLen + (success ? 5 : 6))); // In semi-octet
    // 1. TP-MTI & TP-UDHI
    GsmPDUHelper.writeHexOctet(PDU_MTI_SMS_DELIVER);
    if (!success) {
      // 2. TP-FCS
      GsmPDUHelper.writeHexOctet(PDU_FCS_USIM_DATA_DOWNLOAD_ERROR);
    }
    // 3. TP-PI
    GsmPDUHelper.writeHexOctet(PDU_PI_USER_DATA_LENGTH |
                               PDU_PI_DATA_CODING_SCHEME |
                               PDU_PI_PROTOCOL_IDENTIFIER);
    // 4. TP-PID
    GsmPDUHelper.writeHexOctet(options.pid);
    // 5. TP-DCS
    GsmPDUHelper.writeHexOctet(options.dcs);
    // 6. TP-UDL
    if (options.encoding == PDU_DCS_MSG_CODING_7BITS_ALPHABET) {
      GsmPDUHelper.writeHexOctet(Math.floor(responsePduLen * 8 / 7));
    } else {
      GsmPDUHelper.writeHexOctet(responsePduLen);
    }
    // TP-UD
    Buf.copyIncomingToOutgoing(PDU_HEX_OCTET_SIZE * responsePduLen);
    // Write 2 string delimitors for the total string length must be even.
    Buf.writeStringDelimiter(0);

    Buf.sendParcel();
  },

  /**
   * @param message A decoded SMS-DELIVER message.
   */
  writeSmsToSIM: function writeSmsToSIM(message) {
    Buf.newParcel(REQUEST_WRITE_SMS_TO_SIM);

    // Write EFsms Status
    Buf.writeUint32(EFSMS_STATUS_FREE);

    Buf.seekIncoming(-1 * (Buf.currentParcelSize - Buf.readAvailable
                           - 2 * UINT32_SIZE)); // Skip response_type & request_type.
    let messageStringLength = Buf.readUint32(); // In semi-octets
    let smscLength = GsmPDUHelper.readHexOctet(); // In octets, inclusive of TOA
    let pduLength = (messageStringLength / 2) - (smscLength + 1); // In octets

    // 1. Write PDU first.
    if (smscLength > 0) {
      Buf.seekIncoming(smscLength * PDU_HEX_OCTET_SIZE);
    }
    // Write EFsms PDU string length
    Buf.writeUint32(2 * pduLength); // In semi-octets
    if (pduLength) {
      Buf.copyIncomingToOutgoing(PDU_HEX_OCTET_SIZE * pduLength);
    }
    // Write 2 string delimitors for the total string length must be even.
    Buf.writeStringDelimiter(0);

    // 2. Write SMSC
    // Write EFsms SMSC string length
    Buf.writeUint32(2 * (smscLength + 1)); // Plus smscLength itself, in semi-octets
    // Write smscLength
    GsmPDUHelper.writeHexOctet(smscLength);
    // Write TOA & SMSC Address
    if (smscLength) {
      Buf.seekIncoming(-1 * (Buf.currentParcelSize - Buf.readAvailable
                             - 2 * UINT32_SIZE // Skip response_type, request_type.
                             - 2 * PDU_HEX_OCTET_SIZE)); // Skip messageStringLength & smscLength.
      Buf.copyIncomingToOutgoing(PDU_HEX_OCTET_SIZE * smscLength);
    }
    // Write 2 string delimitors for the total string length must be even.
    Buf.writeStringDelimiter(0);

    Buf.sendParcel();
  },

  /**
   * Helper for processing received SMS parcel data.
   *
   * @param length
   *        Length of SMS string in the incoming parcel.
   *
   * @return Message parsed or null for invalid message.
   */
  _processReceivedSms: function _processReceivedSms(length) {
    if (!length) {
      if (DEBUG) debug("Received empty SMS!");
      return null;
    }

    // An SMS is a string, but we won't read it as such, so let's read the
    // string length and then defer to PDU parsing helper.
    let messageStringLength = Buf.readUint32();
    if (DEBUG) debug("Got new SMS, length " + messageStringLength);
    let message = GsmPDUHelper.readMessage();
    if (DEBUG) debug("Got new SMS: " + JSON.stringify(message));

    // Read string delimiters. See Buf.readString().
    Buf.readStringDelimiter(length);

    return message;
  },

  /**
   * Helper for processing SMS-DELIVER PDUs.
   *
   * @param length
   *        Length of SMS string in the incoming parcel.
   *
   * @return A failure cause defined in 3GPP 23.040 clause 9.2.3.22.
   */
  _processSmsDeliver: function _processSmsDeliver(length) {
    let message = this._processReceivedSms(length);
    if (!message) {
      return PDU_FCS_UNSPECIFIED;
    }

    if (message.epid == PDU_PID_SHORT_MESSAGE_TYPE_0) {
      // `A short message type 0 indicates that the ME must acknowledge receipt
      // of the short message but shall discard its contents.` ~ 3GPP TS 23.040
      // 9.2.3.9
      return PDU_FCS_OK;
    }

    if (message.messageClass == GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_2]) {
      switch (message.epid) {
        case PDU_PID_ANSI_136_R_DATA:
        case PDU_PID_USIM_DATA_DOWNLOAD:
          if (ICCUtilsHelper.isICCServiceAvailable("DATA_DOWNLOAD_SMS_PP")) {
            // `If the service "data download via SMS Point-to-Point" is
            // allocated and activated in the (U)SIM Service Table, ... then the
            // ME shall pass the message transparently to the UICC using the
            // ENVELOPE (SMS-PP DOWNLOAD).` ~ 3GPP TS 31.111 7.1.1.1
            this.dataDownloadViaSMSPP(message);

            // `the ME shall not display the message, or alert the user of a
            // short message waiting.` ~ 3GPP TS 31.111 7.1.1.1
            return PDU_FCS_RESERVED;
          }
          // Fall through!

          // If the service "data download via SMS-PP" is not available in the
          // (U)SIM Service Table, ..., then the ME shall store the message in
          // EFsms in accordance with TS 31.102` ~ 3GPP TS 31.111 7.1.1.1
        default:
          this.writeSmsToSIM(message);
          break;
      }
    }

    // TODO: Bug 739143: B2G SMS: Support SMS Storage Full event
    if ((message.messageClass != GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_0]) && !true) {
      // `When a mobile terminated message is class 0..., the MS shall display
      // the message immediately and send a ACK to the SC ..., irrespective of
      // whether there is memory available in the (U)SIM or ME.` ~ 3GPP 23.038
      // clause 4.

      if (message.messageClass == GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_2]) {
        // `If all the short message storage at the MS is already in use, the
        // MS shall return "memory capacity exceeded".` ~ 3GPP 23.038 clause 4.
        return PDU_FCS_MEMORY_CAPACITY_EXCEEDED;
      }

      return PDU_FCS_UNSPECIFIED;
    }

    if (message.header && (message.header.segmentMaxSeq > 1)) {
      message = this._processReceivedSmsSegment(message);
    } else {
      if (message.encoding == PDU_DCS_MSG_CODING_8BITS_ALPHABET) {
        message.fullData = message.data;
        delete message.data;
      } else {
        message.fullBody = message.body;
        delete message.body;
      }
    }

    if (message) {
      message.result = PDU_FCS_OK;
      if (message.messageClass == GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_2]) {
        // `MS shall ensure that the message has been to the SMS data field in
        // the (U)SIM before sending an ACK to the SC.`  ~ 3GPP 23.038 clause 4
        message.result = PDU_FCS_RESERVED;
      }
      message.rilMessageType = "sms-received";
      this.sendDOMMessage(message);

      // We will acknowledge receipt of the SMS after we try to store it
      // in the database.
      return MOZ_FCS_WAIT_FOR_EXPLICIT_ACK;
    }

    return PDU_FCS_OK;
  },

  /**
   * Helper for processing SMS-STATUS-REPORT PDUs.
   *
   * @param length
   *        Length of SMS string in the incoming parcel.
   *
   * @return A failure cause defined in 3GPP 23.040 clause 9.2.3.22.
   */
  _processSmsStatusReport: function _processSmsStatusReport(length) {
    let message = this._processReceivedSms(length);
    if (!message) {
      if (DEBUG) debug("invalid SMS-STATUS-REPORT");
      return PDU_FCS_UNSPECIFIED;
    }

    let options = this._pendingSentSmsMap[message.messageRef];
    if (!options) {
      if (DEBUG) debug("no pending SMS-SUBMIT message");
      return PDU_FCS_OK;
    }

    let status = message.status;

    // 3GPP TS 23.040 9.2.3.15 `The MS shall interpret any reserved values as
    // "Service Rejected"(01100011) but shall store them exactly as received.`
    if ((status >= 0x80)
        || ((status >= PDU_ST_0_RESERVED_BEGIN)
            && (status < PDU_ST_0_SC_SPECIFIC_BEGIN))
        || ((status >= PDU_ST_1_RESERVED_BEGIN)
            && (status < PDU_ST_1_SC_SPECIFIC_BEGIN))
        || ((status >= PDU_ST_2_RESERVED_BEGIN)
            && (status < PDU_ST_2_SC_SPECIFIC_BEGIN))
        || ((status >= PDU_ST_3_RESERVED_BEGIN)
            && (status < PDU_ST_3_SC_SPECIFIC_BEGIN))
        ) {
      status = PDU_ST_3_SERVICE_REJECTED;
    }

    // Pending. Waiting for next status report.
    if ((status >>> 5) == 0x01) {
      if (DEBUG) debug("SMS-STATUS-REPORT: delivery still pending");
      return PDU_FCS_OK;
    }

    delete this._pendingSentSmsMap[message.messageRef];

    let deliveryStatus = ((status >>> 5) == 0x00)
                       ? GECKO_SMS_DELIVERY_STATUS_SUCCESS
                       : GECKO_SMS_DELIVERY_STATUS_ERROR;
    this.sendDOMMessage({
      rilMessageType: "sms-delivery",
      envelopeId: options.envelopeId,
      deliveryStatus: deliveryStatus
    });

    return PDU_FCS_OK;
  },

  /**
   * Helper for processing received multipart SMS.
   *
   * @return null for handled segments, and an object containing full message
   *         body/data once all segments are received.
   */
  _processReceivedSmsSegment: function _processReceivedSmsSegment(original) {
    let hash = original.sender + ":" + original.header.segmentRef;
    let seq = original.header.segmentSeq;

    let options = this._receivedSmsSegmentsMap[hash];
    if (!options) {
      options = original;
      this._receivedSmsSegmentsMap[hash] = options;

      options.segmentMaxSeq = original.header.segmentMaxSeq;
      options.receivedSegments = 0;
      options.segments = [];
    } else if (options.segments[seq]) {
      // Duplicated segment?
      if (DEBUG) {
        debug("Got duplicated segment no." + seq + " of a multipart SMS: "
              + JSON.stringify(original));
      }
      return null;
    }

    if (options.encoding == PDU_DCS_MSG_CODING_8BITS_ALPHABET) {
      options.segments[seq] = original.data;
      delete original.data;
    } else {
      options.segments[seq] = original.body;
      delete original.body;
    }
    options.receivedSegments++;
    if (options.receivedSegments < options.segmentMaxSeq) {
      if (DEBUG) {
        debug("Got segment no." + seq + " of a multipart SMS: "
              + JSON.stringify(options));
      }
      return null;
    }

    // Remove from map
    delete this._receivedSmsSegmentsMap[hash];

    // Rebuild full body
    if (options.encoding == PDU_DCS_MSG_CODING_8BITS_ALPHABET) {
      // Uint8Array doesn't have `concat`, so we have to merge all segements
      // by hand.
      let fullDataLen = 0;
      for (let i = 1; i <= options.segmentMaxSeq; i++) {
        fullDataLen += options.segments[i].length;
      }

      options.fullData = new Uint8Array(fullDataLen);
      for (let d= 0, i = 1; i <= options.segmentMaxSeq; i++) {
        let data = options.segments[i];
        for (let j = 0; j < data.length; j++) {
          options.fullData[d++] = data[j];
        }
      }
    } else {
      options.fullBody = options.segments.join("");
    }

    if (DEBUG) {
      debug("Got full multipart SMS: " + JSON.stringify(options));
    }

    return options;
  },

  /**
   * Helper for processing sent multipart SMS.
   */
  _processSentSmsSegment: function _processSentSmsSegment(options) {
    // Setup attributes for sending next segment
    let next = options.segmentSeq;
    options.body = options.segments[next].body;
    options.encodedBodyLength = options.segments[next].encodedBodyLength;
    options.segmentSeq = next + 1;

    this.sendSMS(options);
  },

  _processReceivedSmsCbPage: function _processReceivedSmsCbPage(original) {
    if (original.numPages <= 1) {
      if (original.body) {
        original.fullBody = original.body;
        delete original.body;
      } else if (original.data) {
        original.fullData = original.data;
        delete original.data;
      }
      return original;
    }

    // Hash = <serial>:<mcc>:<mnc>:<lac>:<cid>
    let hash = original.serial + ":" + this.iccInfo.mcc + ":"
               + this.iccInfo.mnc + ":";
    switch (original.geographicalScope) {
      case CB_GSM_GEOGRAPHICAL_SCOPE_CELL_WIDE_IMMEDIATE:
      case CB_GSM_GEOGRAPHICAL_SCOPE_CELL_WIDE:
        hash += this.voiceRegistrationState.cell.gsmLocationAreaCode + ":"
             + this.voiceRegistrationState.cell.gsmCellId;
        break;
      case CB_GSM_GEOGRAPHICAL_SCOPE_LOCATION_AREA_WIDE:
        hash += this.voiceRegistrationState.cell.gsmLocationAreaCode + ":";
        break;
      default:
        hash += ":";
        break;
    }

    let index = original.pageIndex;

    let options = this._receivedSmsCbPagesMap[hash];
    if (!options) {
      options = original;
      this._receivedSmsCbPagesMap[hash] = options;

      options.receivedPages = 0;
      options.pages = [];
    } else if (options.pages[index]) {
      // Duplicated page?
      if (DEBUG) {
        debug("Got duplicated page no." + index + " of a multipage SMSCB: "
              + JSON.stringify(original));
      }
      return null;
    }

    if (options.encoding == PDU_DCS_MSG_CODING_8BITS_ALPHABET) {
      options.pages[index] = original.data;
      delete original.data;
    } else {
      options.pages[index] = original.body;
      delete original.body;
    }
    options.receivedPages++;
    if (options.receivedPages < options.numPages) {
      if (DEBUG) {
        debug("Got page no." + index + " of a multipage SMSCB: "
              + JSON.stringify(options));
      }
      return null;
    }

    // Remove from map
    delete this._receivedSmsCbPagesMap[hash];

    // Rebuild full body
    if (options.encoding == PDU_DCS_MSG_CODING_8BITS_ALPHABET) {
      // Uint8Array doesn't have `concat`, so we have to merge all pages by hand.
      let fullDataLen = 0;
      for (let i = 1; i <= options.numPages; i++) {
        fullDataLen += options.pages[i].length;
      }

      options.fullData = new Uint8Array(fullDataLen);
      for (let d= 0, i = 1; i <= options.numPages; i++) {
        let data = options.pages[i];
        for (let j = 0; j < data.length; j++) {
          options.fullData[d++] = data[j];
        }
      }
    } else {
      options.fullBody = options.pages.join("");
    }

    if (DEBUG) {
      debug("Got full multipage SMSCB: " + JSON.stringify(options));
    }

    return options;
  },

  _mergeCellBroadcastConfigs: function _mergeCellBroadcastConfigs(list, from, to) {
    if (!list) {
      return [from, to];
    }

    for (let i = 0, f1, t1; i < list.length;) {
      f1 = list[i++];
      t1 = list[i++];
      if (to == f1) {
        // ...[from]...[to|f1]...(t1)
        list[i - 2] = from;
        return list;
      }

      if (to < f1) {
        // ...[from]...(to)...[f1] or ...[from]...(to)[f1]
        if (i > 2) {
          // Not the first range pair, merge three arrays.
          return list.slice(0, i - 2).concat([from, to]).concat(list.slice(i - 2));
        } else {
          return [from, to].concat(list);
        }
      }

      if (from > t1) {
        // ...[f1]...(t1)[from] or ...[f1]...(t1)...[from]
        continue;
      }

      // Have overlap or merge-able adjacency with [f1]...(t1). Replace it
      // with [min(from, f1)]...(max(to, t1)).

      let changed = false;
      if (from < f1) {
        // [from]...[f1]...(t1) or [from][f1]...(t1)
        // Save minimum from value.
        list[i - 2] = from;
        changed = true;
      }

      if (to <= t1) {
        // [from]...[to](t1) or [from]...(to|t1)
        // Can't have further merge-able adjacency. Return.
        return list;
      }

      // Try merging possible next adjacent range.
      let j = i;
      for (let f2, t2; j < list.length;) {
        f2 = list[j++];
        t2 = list[j++];
        if (to > t2) {
          // [from]...[f2]...[t2]...(to) or [from]...[f2]...[t2](to)
          // Merge next adjacent range again.
          continue;
        }

        if (to < t2) {
          if (to < f2) {
            // [from]...(to)[f2] or [from]...(to)...[f2]
            // Roll back and give up.
            j -= 2;
          } else if (to < t2) {
            // [from]...[to|f2]...(t2), or [from]...[f2]...[to](t2)
            // Merge to [from]...(t2) and give up.
            to = t2;
          }
        }

        break;
      }

      // Save maximum to value.
      list[i - 1] = to;

      if (j != i) {
        // Remove merged adjacent ranges.
        let ret = list.slice(0, i);
        if (j < list.length) {
          ret = ret.concat(list.slice(j));
        }
        return ret;
      }

      return list;
    }

    // Append to the end.
    list.push(from);
    list.push(to);

    return list;
  },

  /**
   * Merge all members of cellBroadcastConfigs into mergedCellBroadcastConfig.
   */
  _mergeAllCellBroadcastConfigs: function _mergeAllCellBroadcastConfigs() {
    if (!("CBMI" in this.cellBroadcastConfigs)
        || !("CBMIR" in this.cellBroadcastConfigs)
        || !("MMI" in this.cellBroadcastConfigs)) {
      if (DEBUG) {
        debug("cell broadcast configs not ready, waiting ...");
      }
      return;
    }
    if (DEBUG) {
      debug("Cell Broadcast search lists: " + JSON.stringify(this.cellBroadcastConfigs));
    }

    let list = null;
    for each (let ll in this.cellBroadcastConfigs) {
      if (ll == null) {
        continue;
      }

      for (let i = 0; i < ll.length; i += 2) {
        list = this._mergeCellBroadcastConfigs(list, ll[i], ll[i + 1]);
      }
    }

    if (DEBUG) {
      debug("Cell Broadcast search lists(merged): " + JSON.stringify(list));
    }
    this.mergedCellBroadcastConfig = list;
    this.updateCellBroadcastConfig();
  },

  /**
   * Check whether search list from settings is settable by MMI, that is,
   * whether the range is bounded in any entries of CB_NON_MMI_SETTABLE_RANGES.
   */
  _checkCellBroadcastMMISettable: function _checkCellBroadcastMMISettable(from, to) {
    if ((to <= from) || (from >= 65536) || (from < 0)) {
      return false;
    }

    for (let i = 0, f, t; i < CB_NON_MMI_SETTABLE_RANGES.length;) {
      f = CB_NON_MMI_SETTABLE_RANGES[i++];
      t = CB_NON_MMI_SETTABLE_RANGES[i++];
      if ((from < t) && (to > f)) {
        // Have overlap.
        return false;
      }
    }

    return true;
  },

  /**
   * Convert Cell Broadcast settings string into search list.
   */
  _convertCellBroadcastSearchList: function _convertCellBroadcastSearchList(searchListStr) {
    let parts = searchListStr && searchListStr.split(",");
    if (!parts) {
      return null;
    }

    let list = null;
    let result, from, to;
    for (let range of parts) {
      // Match "12" or "12-34". The result will be ["12", "12", null] or
      // ["12-34", "12", "34"].
      result = range.match(/^(\d+)(?:-(\d+))?$/);
      if (!result) {
        throw "Invalid format";
      }

      from = parseInt(result[1]);
      to = (result[2] != null) ? parseInt(result[2]) + 1 : from + 1;
      if (!this._checkCellBroadcastMMISettable(from, to)) {
        throw "Invalid range";
      }

      if (list == null) {
        list = [];
      }
      list.push(from);
      list.push(to);
    }

    return list;
  },

  /**
   * Handle incoming messages from the main UI thread.
   *
   * @param message
   *        Object containing the message. Messages are supposed
   */
  handleDOMMessage: function handleMessage(message) {
    if (DEBUG) debug("Received DOM message " + JSON.stringify(message));
    let method = this[message.rilMessageType];
    if (typeof method != "function") {
      if (DEBUG) {
        debug("Don't know what to do with message " + JSON.stringify(message));
      }
      return;
    }
    method.call(this, message);
  },

  /**
   * Get a list of current voice calls.
   */
  enumerateCalls: function enumerateCalls(options) {
    if (DEBUG) debug("Sending all current calls");
    let calls = [];
    for each (let call in this.currentCalls) {
      calls.push(call);
    }
    options.calls = calls;
    this.sendDOMMessage(options);
  },

  /**
   * Get a list of current data calls.
   */
  enumerateDataCalls: function enumerateDataCalls() {
    let datacall_list = [];
    for each (let datacall in this.currentDataCalls) {
      datacall_list.push(datacall);
    }
    this.sendDOMMessage({rilMessageType: "datacalllist",
                         datacalls: datacall_list});
  },

  /**
   * Process STK Proactive Command.
   */
  processStkProactiveCommand: function processStkProactiveCommand() {
    let length = Buf.readUint32();
    let berTlv = BerTlvHelper.decode(length / 2);
    Buf.readStringDelimiter(length);

    let ctlvs = berTlv.value;
    let ctlv = StkProactiveCmdHelper.searchForTag(
        COMPREHENSIONTLV_TAG_COMMAND_DETAILS, ctlvs);
    if (!ctlv) {
      RIL.sendStkTerminalResponse({
        resultCode: STK_RESULT_CMD_DATA_NOT_UNDERSTOOD});
      throw new Error("Can't find COMMAND_DETAILS ComprehensionTlv");
    }

    let cmdDetails = ctlv.value;
    if (DEBUG) {
      debug("commandNumber = " + cmdDetails.commandNumber +
           " typeOfCommand = " + cmdDetails.typeOfCommand.toString(16) +
           " commandQualifier = " + cmdDetails.commandQualifier);
    }

    // STK_CMD_MORE_TIME need not to propagate event to DOM.
    if (cmdDetails.typeOfCommand == STK_CMD_MORE_TIME) {
      RIL.sendStkTerminalResponse({
        command: cmdDetails,
        resultCode: STK_RESULT_OK});
      return;
    }

    cmdDetails.rilMessageType = "stkcommand";
    cmdDetails.options = StkCommandParamsFactory.createParam(cmdDetails, ctlvs);
    RIL.sendDOMMessage(cmdDetails);
  },

  /**
   * Send messages to the main thread.
   */
  sendDOMMessage: function sendDOMMessage(message) {
    postMessage(message);
  },

  /**
   * Handle incoming requests from the RIL. We find the method that
   * corresponds to the request type. Incidentally, the request type
   * _is_ the method name, so that's easy.
   */

  handleParcel: function handleParcel(request_type, length, options) {
    let method = this[request_type];
    if (typeof method == "function") {
      if (DEBUG) debug("Handling parcel as " + method.name);
      method.call(this, length, options);
    }
  },

  setDebugEnabled: function setDebugEnabled(options) {
    DEBUG = DEBUG_WORKER || options.enabled;
  }
};

RIL.initRILState();

RIL[REQUEST_GET_SIM_STATUS] = function REQUEST_GET_SIM_STATUS(length, options) {
  if (options.rilRequestError) {
    return;
  }

  let iccStatus = {};
  iccStatus.cardState = Buf.readUint32(); // CARD_STATE_*
  iccStatus.universalPINState = Buf.readUint32(); // CARD_PINSTATE_*
  iccStatus.gsmUmtsSubscriptionAppIndex = Buf.readUint32();
  iccStatus.cdmaSubscriptionAppIndex = Buf.readUint32();
  if (!RILQUIRKS_V5_LEGACY) {
    iccStatus.imsSubscriptionAppIndex = Buf.readUint32();
  }

  let apps_length = Buf.readUint32();
  if (apps_length > CARD_MAX_APPS) {
    apps_length = CARD_MAX_APPS;
  }

  iccStatus.apps = [];
  for (let i = 0 ; i < apps_length ; i++) {
    iccStatus.apps.push({
      app_type:       Buf.readUint32(), // CARD_APPTYPE_*
      app_state:      Buf.readUint32(), // CARD_APPSTATE_*
      perso_substate: Buf.readUint32(), // CARD_PERSOSUBSTATE_*
      aid:            Buf.readString(),
      app_label:      Buf.readString(),
      pin1_replaced:  Buf.readUint32(),
      pin1:           Buf.readUint32(),
      pin2:           Buf.readUint32()
    });
    if (RILQUIRKS_SIM_APP_STATE_EXTRA_FIELDS) {
      Buf.readUint32();
      Buf.readUint32();
      Buf.readUint32();
      Buf.readUint32();
    }
  }

  if (DEBUG) debug("iccStatus: " + JSON.stringify(iccStatus));
  this._processICCStatus(iccStatus);
};
RIL[REQUEST_ENTER_SIM_PIN] = function REQUEST_ENTER_SIM_PIN(length, options) {
  this._processEnterAndChangeICCResponses(length, options);
};
RIL[REQUEST_ENTER_SIM_PUK] = function REQUEST_ENTER_SIM_PUK(length, options) {
  this._processEnterAndChangeICCResponses(length, options);
};
RIL[REQUEST_ENTER_SIM_PIN2] = function REQUEST_ENTER_SIM_PIN2(length, options) {
  this._processEnterAndChangeICCResponses(length, options);
};
RIL[REQUEST_ENTER_SIM_PUK2] = function REQUEST_ENTER_SIM_PUK(length, options) {
  this._processEnterAndChangeICCResponses(length, options);
};
RIL[REQUEST_CHANGE_SIM_PIN] = function REQUEST_CHANGE_SIM_PIN(length, options) {
  this._processEnterAndChangeICCResponses(length, options);
};
RIL[REQUEST_CHANGE_SIM_PIN2] = function REQUEST_CHANGE_SIM_PIN2(length, options) {
  this._processEnterAndChangeICCResponses(length, options);
};
RIL[REQUEST_ENTER_NETWORK_DEPERSONALIZATION_CODE] =
  function REQUEST_ENTER_NETWORK_DEPERSONALIZATION_CODE(length, options) {
  this._processEnterAndChangeICCResponses(length, options);
};
RIL[REQUEST_GET_CURRENT_CALLS] = function REQUEST_GET_CURRENT_CALLS(length, options) {
  if (options.rilRequestError) {
    return;
  }

  let calls_length = 0;
  // The RIL won't even send us the length integer if there are no active calls.
  // So only read this integer if the parcel actually has it.
  if (length) {
    calls_length = Buf.readUint32();
  }
  if (!calls_length) {
    this._processCalls(null);
    return;
  }

  let calls = {};
  for (let i = 0; i < calls_length; i++) {
    let call = {};
    call.state          = Buf.readUint32(); // CALL_STATE_*
    call.callIndex      = Buf.readUint32(); // GSM index (1-based)
    call.toa            = Buf.readUint32();
    call.isMpty         = Boolean(Buf.readUint32());
    call.isMT           = Boolean(Buf.readUint32());
    call.als            = Buf.readUint32();
    call.isVoice        = Boolean(Buf.readUint32());
    call.isVoicePrivacy = Boolean(Buf.readUint32());
    if (RILQUIRKS_CALLSTATE_EXTRA_UINT32) {
      Buf.readUint32();
    }
    call.number             = Buf.readString(); //TODO munge with TOA
    call.numberPresentation = Buf.readUint32(); // CALL_PRESENTATION_*
    call.name               = Buf.readString();
    call.namePresentation   = Buf.readUint32();

    call.uusInfo = null;
    let uusInfoPresent = Buf.readUint32();
    if (uusInfoPresent == 1) {
      call.uusInfo = {
        type:     Buf.readUint32(),
        dcs:      Buf.readUint32(),
        userData: null //XXX TODO byte array?!?
      };
    }

    calls[call.callIndex] = call;
  }
  this._processCalls(calls);
};
RIL[REQUEST_DIAL] = function REQUEST_DIAL(length, options) {
  if (options.rilRequestError) {
    // The connection is not established yet.
    options.callIndex = -1;
    this.getFailCauseCode(options);
    return;
  }
};
RIL[REQUEST_GET_IMSI] = function REQUEST_GET_IMSI(length, options) {
  if (options.rilRequestError) {
    return;
  }

  this.iccInfoPrivate.imsi = Buf.readString();
  if (DEBUG) {
    debug("IMSI: " + this.iccInfoPrivate.imsi);
  }

  options.rilMessageType = "iccimsi";
  options.imsi = this.iccInfoPrivate.imsi;
  this.sendDOMMessage(options);
};
RIL[REQUEST_HANGUP] = function REQUEST_HANGUP(length, options) {
  if (options.rilRequestError) {
    return;
  }

  this.getCurrentCalls();
};
RIL[REQUEST_HANGUP_WAITING_OR_BACKGROUND] = function REQUEST_HANGUP_WAITING_OR_BACKGROUND(length, options) {
  if (options.rilRequestError) {
    return;
  }

  this.getCurrentCalls();
};
RIL[REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND] = function REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND(length, options) {
  if (options.rilRequestError) {
    return;
  }

  this.getCurrentCalls();
};
RIL[REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE] = function REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE(length, options) {
  if (options.rilRequestError) {
    return;
  }

  this.getCurrentCalls();
};
RIL[REQUEST_SWITCH_HOLDING_AND_ACTIVE] = function REQUEST_SWITCH_HOLDING_AND_ACTIVE(length, options) {
  if (options.rilRequestError) {
    return;
  }

  // XXX Normally we should get a UNSOLICITED_RESPONSE_CALL_STATE_CHANGED parcel
  // notifying us of call state changes, but sometimes we don't (have no idea why).
  // this.getCurrentCalls() helps update the call state actively.
  this.getCurrentCalls();
};
RIL[REQUEST_CONFERENCE] = null;
RIL[REQUEST_UDUB] = null;
RIL[REQUEST_LAST_CALL_FAIL_CAUSE] = function REQUEST_LAST_CALL_FAIL_CAUSE(length, options) {
  let num = 0;
  if (length) {
    num = Buf.readUint32();
  }
  if (!num) {
    // No response of REQUEST_LAST_CALL_FAIL_CAUSE. Change the call state into
    // 'disconnected' directly.
    this._handleDisconnectedCall(options);
    return;
  }

  let failCause = Buf.readUint32();
  switch (failCause) {
    case CALL_FAIL_NORMAL:
      this._handleDisconnectedCall(options);
      break;
    case CALL_FAIL_BUSY:
      options.state = CALL_STATE_BUSY;
      this._handleChangedCallState(options);
      this._handleDisconnectedCall(options);
      break;
    default:
      options.rilMessageType = "callError";
      options.error = RIL_CALL_FAILCAUSE_TO_GECKO_CALL_ERROR[failCause];
      this.sendDOMMessage(options);
      break;
  }
};
RIL[REQUEST_SIGNAL_STRENGTH] = function REQUEST_SIGNAL_STRENGTH(length, options) {
  if (options.rilRequestError) {
    return;
  }

  let obj = {};

  // GSM
  // Valid values are (0-31, 99) as defined in TS 27.007 8.5.
  let gsmSignalStrength = Buf.readUint32();
  obj.gsmSignalStrength = gsmSignalStrength & 0xff;
  // GSM bit error rate (0-7, 99) as defined in TS 27.007 8.5.
  obj.gsmBitErrorRate = Buf.readUint32();

  obj.gsmDBM = null;
  obj.gsmRelative = null;
  if (obj.gsmSignalStrength >= 0 && obj.gsmSignalStrength <= 31) {
    obj.gsmDBM = -113 + obj.gsmSignalStrength * 2;
    obj.gsmRelative = Math.floor(obj.gsmSignalStrength * 100 / 31);
  }

  // The SGS2 seems to compute the number of bars (0-4) for us and
  // expose those instead of the actual signal strength. Since the RIL
  // needs to be "warmed up" first for the quirk detection to work,
  // we're detecting this ad-hoc and not upfront.
  if (obj.gsmSignalStrength == 99) {
    obj.gsmRelative = (gsmSignalStrength >> 8) * 25;
  }

  // CDMA
  obj.cdmaDBM = Buf.readUint32();
  // The CDMA EC/IO.
  obj.cdmaECIO = Buf.readUint32();
  // The EVDO RSSI value.

  // EVDO
  obj.evdoDBM = Buf.readUint32();
  // The EVDO EC/IO.
  obj.evdoECIO = Buf.readUint32();
  // Signal-to-noise ratio. Valid values are 0 to 8.
  obj.evdoSNR = Buf.readUint32();

  // LTE
  if (!RILQUIRKS_V5_LEGACY) {
    // Valid values are (0-31, 99) as defined in TS 27.007 8.5.
    obj.lteSignalStrength = Buf.readUint32();
    // Reference signal receive power in dBm, multiplied by -1.
    // Valid values are 44 to 140.
    obj.lteRSRP = Buf.readUint32();
    // Reference signal receive quality in dB, multiplied by -1.
    // Valid values are 3 to 20.
    obj.lteRSRQ = Buf.readUint32();
    // Signal-to-noise ratio for the reference signal.
    // Valid values are -200 (20.0 dB) to +300 (30 dB).
    obj.lteRSSNR = Buf.readUint32();
    // Channel Quality Indicator, valid values are 0 to 15.
    obj.lteCQI = Buf.readUint32();
  }

  if (DEBUG) debug("Signal strength " + JSON.stringify(obj));
  obj.rilMessageType = "signalstrengthchange";
  this.sendDOMMessage(obj);
};
RIL[REQUEST_VOICE_REGISTRATION_STATE] = function REQUEST_VOICE_REGISTRATION_STATE(length, options) {
  this._receivedNetworkInfo(NETWORK_INFO_VOICE_REGISTRATION_STATE);

  if (options.rilRequestError) {
    return;
  }

  let state = Buf.readStringList();
  if (DEBUG) debug("voice registration state: " + state);

  this._processVoiceRegistrationState(state);
};
RIL[REQUEST_DATA_REGISTRATION_STATE] = function REQUEST_DATA_REGISTRATION_STATE(length, options) {
  this._receivedNetworkInfo(NETWORK_INFO_DATA_REGISTRATION_STATE);

  if (options.rilRequestError) {
    return;
  }

  let state = Buf.readStringList();
  this._processDataRegistrationState(state);
};
RIL[REQUEST_OPERATOR] = function REQUEST_OPERATOR(length, options) {
  this._receivedNetworkInfo(NETWORK_INFO_OPERATOR);

  if (options.rilRequestError) {
    return;
  }

  let operatorData = Buf.readStringList();
  if (DEBUG) debug("Operator: " + operatorData);
  this._processOperator(operatorData);
};
RIL[REQUEST_RADIO_POWER] = null;
RIL[REQUEST_DTMF] = null;
RIL[REQUEST_SEND_SMS] = function REQUEST_SEND_SMS(length, options) {
  if (options.rilRequestError) {
    if (DEBUG) debug("REQUEST_SEND_SMS: rilRequestError = " + options.rilRequestError);
    switch (options.rilRequestError) {
      case ERROR_SMS_SEND_FAIL_RETRY:
        if (options.retryCount < SMS_RETRY_MAX) {
          options.retryCount++;
          // TODO: bug 736702 TP-MR, retry interval, retry timeout
          this.sendSMS(options);
          break;
        }

        // Fallback to default error handling if it meets max retry count.
      default:
        this.sendDOMMessage({
          rilMessageType: "sms-send-failed",
          envelopeId: options.envelopeId,
          error: options.rilRequestError,
        });
        break;
    }
    return;
  }

  options.messageRef = Buf.readUint32();
  options.ackPDU = Buf.readString();
  options.errorCode = Buf.readUint32();

  if ((options.segmentMaxSeq > 1)
      && (options.segmentSeq < options.segmentMaxSeq)) {
    // Not last segment
    this._processSentSmsSegment(options);
  } else {
    // Last segment sent with success.
    if (options.requestStatusReport) {
      if (DEBUG) debug("waiting SMS-STATUS-REPORT for messageRef " + options.messageRef);
      this._pendingSentSmsMap[options.messageRef] = options;
    }

    this.sendDOMMessage({
      rilMessageType: "sms-sent",
      envelopeId: options.envelopeId,
    });
  }
};
RIL[REQUEST_SEND_SMS_EXPECT_MORE] = null;

RIL.readSetupDataCall_v5 = function readSetupDataCall_v5(options) {
  if (!options) {
    options = {};
  }
  let [cid, ifname, ipaddr, dns, gw] = Buf.readStringList();
  options.cid = cid;
  options.ifname = ifname;
  options.ipaddr = ipaddr;
  options.dns = dns;
  options.gw = gw;
  options.active = DATACALL_ACTIVE_UNKNOWN;
  options.state = GECKO_NETWORK_STATE_CONNECTING;
  return options;
};

RIL[REQUEST_SETUP_DATA_CALL] = function REQUEST_SETUP_DATA_CALL(length, options) {
  if (options.rilRequestError) {
    // On Data Call generic errors, we shall notify caller
    this._sendDataCallError(options, options.rilRequestError);
    return;
  }

  if (RILQUIRKS_V5_LEGACY) {
    // Populate the `options` object with the data call information. That way
    // we retain the APN and other info about how the data call was set up.
    this.readSetupDataCall_v5(options);
    this.currentDataCalls[options.cid] = options;
    options.rilMessageType = "datacallstatechange";
    this.sendDOMMessage(options);
    // Let's get the list of data calls to ensure we know whether it's active
    // or not.
    this.getDataCallList();
    return;
  }
  // Pass `options` along. That way we retain the APN and other info about
  // how the data call was set up.
  this[REQUEST_DATA_CALL_LIST](length, options);
};
RIL[REQUEST_SIM_IO] = function REQUEST_SIM_IO(length, options) {
  if (!length) {
    ICCIOHelper.processICCIOError(options);
    return;
  }

  // Don't need to read rilRequestError since we can know error status from
  // sw1 and sw2.
  options.sw1 = Buf.readUint32();
  options.sw2 = Buf.readUint32();
  if (options.sw1 != ICC_STATUS_NORMAL_ENDING) {
    ICCIOHelper.processICCIOError(options);
    return;
  }
  ICCIOHelper.processICCIO(options);
};
RIL[REQUEST_SEND_USSD] = function REQUEST_SEND_USSD(length, options) {
  if (DEBUG) {
    debug("REQUEST_SEND_USSD " + JSON.stringify(options));
  }
  options.success = this._ussdSession = options.rilRequestError == 0;
  options.errorMsg = RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError];
  this.sendDOMMessage(options);
};
RIL[REQUEST_CANCEL_USSD] = function REQUEST_CANCEL_USSD(length, options) {
  if (DEBUG) {
    debug("REQUEST_CANCEL_USSD" + JSON.stringify(options));
  }
  options.success = options.rilRequestError == 0;
  this._ussdSession = !options.success;
  options.errorMsg = RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError];
  this.sendDOMMessage(options);
};
RIL[REQUEST_GET_CLIR] = null;
RIL[REQUEST_SET_CLIR] = null;
RIL[REQUEST_QUERY_CALL_FORWARD_STATUS] =
  function REQUEST_QUERY_CALL_FORWARD_STATUS(length, options) {
    options.success = options.rilRequestError == 0;
    if (!options.success) {
      options.errorMsg = RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError];
      this.sendDOMMessage(options);
      return;
    }

    let rulesLength = 0;
    if (length) {
      rulesLength = Buf.readUint32();
    }
    if (!rulesLength) {
      options.success = false;
      options.errorMsg =
        "Invalid rule length while querying call forwarding status.";
      this.sendDOMMessage(options);
      return;
    }
    let rules = new Array(rulesLength);
    for (let i = 0; i < rulesLength; i++) {
      let rule = {};
      rule.active       = Buf.readUint32() == 1; // CALL_FORWARD_STATUS_*
      rule.reason       = Buf.readUint32(); // CALL_FORWARD_REASON_*
      rule.serviceClass = Buf.readUint32();
      rule.toa          = Buf.readUint32();
      rule.number       = Buf.readString();
      rule.timeSeconds  = Buf.readUint32();
      rules[i] = rule;
    }
    options.rules = rules;
    this.sendDOMMessage(options);
};
RIL[REQUEST_SET_CALL_FORWARD] =
  function REQUEST_SET_CALL_FORWARD(length, options) {
    options.success = options.rilRequestError == 0;
    if (!options.success) {
      options.errorMsg = RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError];
    }
    this.sendDOMMessage(options);
};
RIL[REQUEST_QUERY_CALL_WAITING] = null;
RIL[REQUEST_SET_CALL_WAITING] = function REQUEST_SET_CALL_WAITING(length, options) {
  options.errorMsg = RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError];
  this.sendDOMMessage(options);
};
RIL[REQUEST_SMS_ACKNOWLEDGE] = null;
RIL[REQUEST_GET_IMEI] = function REQUEST_GET_IMEI(length, options) {
  this.IMEI = Buf.readString();
  // So far we only send the IMEI back to the DOM if it was requested via MMI.
  if (!options.mmi) {
    return;
  }

  options.rilMessageType = "sendMMI";
  options.success = options.rilRequestError == 0;
  options.errorMsg = RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError];
  if ((!options.success || this.IMEI == null) && !options.errorMsg) {
    options.errorMsg = GECKO_ERROR_GENERIC_FAILURE;
  }
  options.result = this.IMEI;
  this.sendDOMMessage(options);
};
RIL[REQUEST_GET_IMEISV] = function REQUEST_GET_IMEISV(length, options) {
  if (options.rilRequestError) {
    return;
  }

  this.IMEISV = Buf.readString();
};
RIL[REQUEST_ANSWER] = null;
RIL[REQUEST_DEACTIVATE_DATA_CALL] = function REQUEST_DEACTIVATE_DATA_CALL(length, options) {
  if (options.rilRequestError) {
    return;
  }

  let datacall = this.currentDataCalls[options.cid];
  delete this.currentDataCalls[options.cid];
  datacall.state = GECKO_NETWORK_STATE_UNKNOWN;
  datacall.rilMessageType = "datacallstatechange";
  this.sendDOMMessage(datacall);
};
RIL[REQUEST_QUERY_FACILITY_LOCK] = function REQUEST_QUERY_FACILITY_LOCK(length, options) {
  options.success = options.rilRequestError == 0;
  if (!options.success) {
    options.errorMsg = RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError];
  }

  if (length) {
    options.enabled = Buf.readUint32List()[0] == 0 ? false : true;
  }
  this.sendDOMMessage(options);
};
RIL[REQUEST_SET_FACILITY_LOCK] = function REQUEST_SET_FACILITY_LOCK(length, options) {
  options.success = options.rilRequestError == 0;
  if (!options.success) {
    options.errorMsg = RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError];
  }
  options.retryCount = length ? Buf.readUint32List()[0] : -1;
  this.sendDOMMessage(options);
};
RIL[REQUEST_CHANGE_BARRING_PASSWORD] = null;
RIL[REQUEST_QUERY_NETWORK_SELECTION_MODE] = function REQUEST_QUERY_NETWORK_SELECTION_MODE(length, options) {
  this._receivedNetworkInfo(NETWORK_INFO_NETWORK_SELECTION_MODE);

  if (options.rilRequestError) {
    options.error = RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError];
    this.sendDOMMessage(options);
    return;
  }

  let mode = Buf.readUint32List();
  let selectionMode;

  switch (mode[0]) {
    case NETWORK_SELECTION_MODE_AUTOMATIC:
      selectionMode = GECKO_NETWORK_SELECTION_AUTOMATIC;
      break;
    case NETWORK_SELECTION_MODE_MANUAL:
      selectionMode = GECKO_NETWORK_SELECTION_MANUAL;
      break;
    default:
      selectionMode = GECKO_NETWORK_SELECTION_UNKNOWN;
      break;
  }

  if (this.networkSelectionMode != selectionMode) {
    this.networkSelectionMode = options.mode = selectionMode;
    options.rilMessageType = "networkselectionmodechange";
    this._sendNetworkInfoMessage(NETWORK_INFO_NETWORK_SELECTION_MODE, options);
  }
};
RIL[REQUEST_SET_NETWORK_SELECTION_AUTOMATIC] = function REQUEST_SET_NETWORK_SELECTION_AUTOMATIC(length, options) {
  if (options.rilRequestError) {
    options.error = RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError];
    this.sendDOMMessage(options);
    return;
  }

  this.sendDOMMessage(options);
};
RIL[REQUEST_SET_NETWORK_SELECTION_MANUAL] = function REQUEST_SET_NETWORK_SELECTION_MANUAL(length, options) {
  if (options.rilRequestError) {
    options.error = RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError];
    this.sendDOMMessage(options);
    return;
  }

  this.sendDOMMessage(options);
};
RIL[REQUEST_QUERY_AVAILABLE_NETWORKS] = function REQUEST_QUERY_AVAILABLE_NETWORKS(length, options) {
  if (options.rilRequestError) {
    options.error = RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError];
    this.sendDOMMessage(options);
    return;
  }

  options.networks = this._processNetworks();
  this.sendDOMMessage(options);
};
RIL[REQUEST_DTMF_START] = null;
RIL[REQUEST_DTMF_STOP] = null;
RIL[REQUEST_BASEBAND_VERSION] = function REQUEST_BASEBAND_VERSION(length, options) {
  if (options.rilRequestError) {
    return;
  }

  this.basebandVersion = Buf.readString();
  if (DEBUG) debug("Baseband version: " + this.basebandVersion);
};
RIL[REQUEST_SEPARATE_CONNECTION] = null;
RIL[REQUEST_SET_MUTE] = null;
RIL[REQUEST_GET_MUTE] = null;
RIL[REQUEST_QUERY_CLIP] = null;
RIL[REQUEST_LAST_DATA_CALL_FAIL_CAUSE] = null;

RIL.readDataCall_v5 = function readDataCall_v5(options) {
  if (!options) {
    options = {};
  }
  options.cid = Buf.readUint32().toString();
  options.active = Buf.readUint32(); // DATACALL_ACTIVE_*
  options.type = Buf.readString();
  options.apn = Buf.readString();
  options.address = Buf.readString();
  return options;
};

RIL.readDataCall_v6 = function readDataCall_v6(options) {
  if (!options) {
    options = {};
  }
  options.status = Buf.readUint32();  // DATACALL_FAIL_*
  options.suggestedRetryTime = Buf.readUint32();
  options.cid = Buf.readUint32().toString();
  options.active = Buf.readUint32();  // DATACALL_ACTIVE_*
  options.type = Buf.readString();
  options.ifname = Buf.readString();
  options.ipaddr = Buf.readString();
  options.dns = Buf.readString();
  options.gw = Buf.readString();
  if (options.dns) {
    options.dns = options.dns.split(" ");
  }
  //TODO for now we only support one address and gateway
  if (options.ipaddr) {
    options.ipaddr = options.ipaddr.split(" ")[0];
  }
  if (options.gw) {
    options.gw = options.gw.split(" ")[0];
  }
  options.ip = null;
  options.netmask = null;
  options.broadcast = null;
  if (options.ipaddr) {
    options.ip = options.ipaddr.split("/")[0];
    let ip_value = netHelpers.stringToIP(options.ip);
    let prefix_len = options.ipaddr.split("/")[1];
    let mask_value = netHelpers.makeMask(prefix_len);
    options.netmask = netHelpers.ipToString(mask_value);
    options.broadcast = netHelpers.ipToString((ip_value & mask_value) + ~mask_value);
  }
  return options;
};

RIL[REQUEST_DATA_CALL_LIST] = function REQUEST_DATA_CALL_LIST(length, options) {
  if (options.rilRequestError) {
    return;
  }

  if (!length) {
    this._processDataCallList(null);
    return;
  }

  let version = 0;
  if (!RILQUIRKS_V5_LEGACY) {
    version = Buf.readUint32();
  }
  let num = num = Buf.readUint32();
  let datacalls = {};
  for (let i = 0; i < num; i++) {
    let datacall;
    if (version < 6) {
      datacall = this.readDataCall_v5();
    } else {
      datacall = this.readDataCall_v6();
    }
    datacalls[datacall.cid] = datacall;
  }

  let newDataCallOptions = null;
  if (options.rilRequestType == REQUEST_SETUP_DATA_CALL) {
    newDataCallOptions = options;
  }
  this._processDataCallList(datacalls, newDataCallOptions);
};
RIL[REQUEST_RESET_RADIO] = null;
RIL[REQUEST_OEM_HOOK_RAW] = null;
RIL[REQUEST_OEM_HOOK_STRINGS] = null;
RIL[REQUEST_SCREEN_STATE] = null;
RIL[REQUEST_SET_SUPP_SVC_NOTIFICATION] = null;
RIL[REQUEST_WRITE_SMS_TO_SIM] = function REQUEST_WRITE_SMS_TO_SIM(length, options) {
  if (options.rilRequestError) {
    // `The MS shall return a "protocol error, unspecified" error message if
    // the short message cannot be stored in the (U)SIM, and there is other
    // message storage available at the MS` ~ 3GPP TS 23.038 section 4. Here
    // we assume we always have indexed db as another storage.
    this.acknowledgeSMS(false, PDU_FCS_PROTOCOL_ERROR);
  } else {
    this.acknowledgeSMS(true, PDU_FCS_OK);
  }
};
RIL[REQUEST_DELETE_SMS_ON_SIM] = null;
RIL[REQUEST_SET_BAND_MODE] = null;
RIL[REQUEST_QUERY_AVAILABLE_BAND_MODE] = null;
RIL[REQUEST_STK_GET_PROFILE] = null;
RIL[REQUEST_STK_SET_PROFILE] = null;
RIL[REQUEST_STK_SEND_ENVELOPE_COMMAND] = null;
RIL[REQUEST_STK_SEND_TERMINAL_RESPONSE] = null;
RIL[REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM] = null;
RIL[REQUEST_EXPLICIT_CALL_TRANSFER] = null;
RIL[REQUEST_SET_PREFERRED_NETWORK_TYPE] = function REQUEST_SET_PREFERRED_NETWORK_TYPE(length, options) {
  if (options.networkType == null) {
    // The request was made by ril_worker itself automatically. Don't report.
    return;
  }

  this.sendDOMMessage({
    rilMessageType: "setPreferredNetworkType",
    networkType: options.networkType,
    success: options.rilRequestError == ERROR_SUCCESS
  });
};
RIL[REQUEST_GET_PREFERRED_NETWORK_TYPE] = function REQUEST_GET_PREFERRED_NETWORK_TYPE(length, options) {
  let networkType;
  if (!options.rilRequestError) {
    networkType = RIL_PREFERRED_NETWORK_TYPE_TO_GECKO.indexOf(GECKO_PREFERRED_NETWORK_TYPE_DEFAULT);
    let responseLen = Buf.readUint32(); // Number of INT32 responsed.
    if (responseLen) {
      this.preferredNetworkType = networkType = Buf.readUint32();
    }
  }

  this.sendDOMMessage({
    rilMessageType: "getPreferredNetworkType",
    networkType: networkType,
    success: options.rilRequestError == ERROR_SUCCESS
  });
};
RIL[REQUEST_GET_NEIGHBORING_CELL_IDS] = null;
RIL[REQUEST_SET_LOCATION_UPDATES] = null;
RIL[REQUEST_CDMA_SET_SUBSCRIPTION_SOURCE] = null;
RIL[REQUEST_CDMA_SET_ROAMING_PREFERENCE] = null;
RIL[REQUEST_CDMA_QUERY_ROAMING_PREFERENCE] = null;
RIL[REQUEST_SET_TTY_MODE] = null;
RIL[REQUEST_QUERY_TTY_MODE] = null;
RIL[REQUEST_CDMA_SET_PREFERRED_VOICE_PRIVACY_MODE] = null;
RIL[REQUEST_CDMA_QUERY_PREFERRED_VOICE_PRIVACY_MODE] = null;
RIL[REQUEST_CDMA_FLASH] = null;
RIL[REQUEST_CDMA_BURST_DTMF] = null;
RIL[REQUEST_CDMA_VALIDATE_AND_WRITE_AKEY] = null;
RIL[REQUEST_CDMA_SEND_SMS] = null;
RIL[REQUEST_CDMA_SMS_ACKNOWLEDGE] = null;
RIL[REQUEST_GSM_GET_BROADCAST_SMS_CONFIG] = null;
RIL[REQUEST_GSM_SET_BROADCAST_SMS_CONFIG] = function REQUEST_GSM_SET_BROADCAST_SMS_CONFIG(length, options) {
  if (options.rilRequestError == ERROR_SUCCESS) {
    this.setGsmSmsBroadcastActivation(true);
  }
};
RIL[REQUEST_GSM_SMS_BROADCAST_ACTIVATION] = null;
RIL[REQUEST_CDMA_GET_BROADCAST_SMS_CONFIG] = null;
RIL[REQUEST_CDMA_SET_BROADCAST_SMS_CONFIG] = null;
RIL[REQUEST_CDMA_SMS_BROADCAST_ACTIVATION] = null;
RIL[REQUEST_CDMA_SUBSCRIPTION] = null;
RIL[REQUEST_CDMA_WRITE_SMS_TO_RUIM] = null;
RIL[REQUEST_CDMA_DELETE_SMS_ON_RUIM] = null;
RIL[REQUEST_DEVICE_IDENTITY] = function REQUEST_DEVICE_IDENTITY(length, options) {
  if (options.rilRequestError) {
    return;
  }

  let result = Buf.readStringList();

  // The result[0] is for IMEI. (Already be handled in REQUEST_GET_IMEI)
  // The result[1] is for IMEISV. (Already be handled in REQUEST_GET_IMEISV)
  // They are both ignored.
  this.ESN = result[2];
  this.MEID = result[3];
};
RIL[REQUEST_EXIT_EMERGENCY_CALLBACK_MODE] = null;
RIL[REQUEST_GET_SMSC_ADDRESS] = function REQUEST_GET_SMSC_ADDRESS(length, options) {
  if (options.rilRequestError) {
    return;
  }

  this.SMSC = Buf.readString();
};
RIL[REQUEST_SET_SMSC_ADDRESS] = null;
RIL[REQUEST_REPORT_SMS_MEMORY_STATUS] = null;
RIL[REQUEST_REPORT_STK_SERVICE_IS_RUNNING] = null;
RIL[REQUEST_ACKNOWLEDGE_INCOMING_GSM_SMS_WITH_PDU] = null;
RIL[REQUEST_STK_SEND_ENVELOPE_WITH_STATUS] = function REQUEST_STK_SEND_ENVELOPE_WITH_STATUS(length, options) {
  if (options.rilRequestError) {
    this.acknowledgeSMS(false, PDU_FCS_UNSPECIFIED);
    return;
  }

  let sw1 = Buf.readUint32();
  let sw2 = Buf.readUint32();
  if ((sw1 == ICC_STATUS_SAT_BUSY) && (sw2 == 0x00)) {
    this.acknowledgeSMS(false, PDU_FCS_USAT_BUSY);
    return;
  }

  let success = ((sw1 == ICC_STATUS_NORMAL_ENDING) && (sw2 == 0x00))
                || (sw1 == ICC_STATUS_NORMAL_ENDING_WITH_EXTRA);

  let messageStringLength = Buf.readUint32(); // In semi-octets
  let responsePduLen = messageStringLength / 2; // In octets
  if (!responsePduLen) {
    this.acknowledgeSMS(success, success ? PDU_FCS_OK
                                         : PDU_FCS_USIM_DATA_DOWNLOAD_ERROR);
    return;
  }

  this.acknowledgeIncomingGsmSmsWithPDU(success, responsePduLen, options);
};
RIL[REQUEST_VOICE_RADIO_TECH] = function REQUEST_VOICE_RADIO_TECH(length, options) {
  if (options.rilRequestError) {
    if (DEBUG) {
      debug("Error when getting voice radio tech: " + options.rilRequestError);
    }
    return;
  }
  let radioTech = Buf.readUint32List();
  this._processRadioTech(radioTech[0]);
};
RIL[UNSOLICITED_RESPONSE_RADIO_STATE_CHANGED] = function UNSOLICITED_RESPONSE_RADIO_STATE_CHANGED() {
  let radioState = Buf.readUint32();

  // Ensure radio state at boot time.
  if (this._isInitialRadioState) {
    this._isInitialRadioState = false;
    if (radioState != RADIO_STATE_OFF) {
      this.setRadioPower({on: false});
      return;
    }
  }

  let newState;
  if (radioState == RADIO_STATE_UNAVAILABLE) {
    newState = GECKO_RADIOSTATE_UNAVAILABLE;
  } else if (radioState == RADIO_STATE_OFF) {
    newState = GECKO_RADIOSTATE_OFF;
  } else {
    newState = GECKO_RADIOSTATE_READY;
  }

  if (DEBUG) {
    debug("Radio state changed from '" + this.radioState +
          "' to '" + newState + "'");
  }
  if (this.radioState == newState) {
    return;
  }

  switch (radioState) {
  case RADIO_STATE_SIM_READY:
  case RADIO_STATE_SIM_NOT_READY:
  case RADIO_STATE_SIM_LOCKED_OR_ABSENT:
    this._isCdma = false;
    this._waitingRadioTech = false;
    break;
  case RADIO_STATE_RUIM_READY:
  case RADIO_STATE_RUIM_NOT_READY:
  case RADIO_STATE_RUIM_LOCKED_OR_ABSENT:
  case RADIO_STATE_NV_READY:
  case RADIO_STATE_NV_NOT_READY:
    this._isCdma = true;
    this._waitingRadioTech = false;
    break;
  case RADIO_STATE_ON: // RIL v7
    // This value is defined in RIL v7, we will retrieve radio tech by another
    // request. We leave _isCdma untouched, and it will be set once we get the
    // radio technology.
    this._waitingRadioTech = true;
    this.getVoiceRadioTechnology();
    break;
  }

  if ((this.radioState == GECKO_RADIOSTATE_UNAVAILABLE ||
       this.radioState == GECKO_RADIOSTATE_OFF) &&
       newState == GECKO_RADIOSTATE_READY) {
    // The radio became available, let's get its info.
    if (!this._waitingRadioTech) {
      if (this._isCdma) {
        this.getDeviceIdentity();
      } else {
        this.getIMEI();
        this.getIMEISV();
      }
    }
    this.getBasebandVersion();
    this.updateCellBroadcastConfig();
  }

  this.radioState = newState;
  this.sendDOMMessage({
    rilMessageType: "radiostatechange",
    radioState: newState
  });

  // If the radio is up and on, so let's query the card state.
  // On older RILs only if the card is actually ready, though.
  // If _waitingRadioTech is set, we don't need to get icc status now.
  if (radioState == RADIO_STATE_UNAVAILABLE ||
      radioState == RADIO_STATE_OFF ||
      this._waitingRadioTech) {
    return;
  }
  this.getICCStatus();
};
RIL[UNSOLICITED_RESPONSE_CALL_STATE_CHANGED] = function UNSOLICITED_RESPONSE_CALL_STATE_CHANGED() {
  this.getCurrentCalls();
};
RIL[UNSOLICITED_RESPONSE_VOICE_NETWORK_STATE_CHANGED] = function UNSOLICITED_RESPONSE_VOICE_NETWORK_STATE_CHANGED() {
  if (DEBUG) debug("Network state changed, re-requesting phone state and ICC status");
  this.getICCStatus();
  this.requestNetworkInfo();
};
RIL[UNSOLICITED_RESPONSE_NEW_SMS] = function UNSOLICITED_RESPONSE_NEW_SMS(length) {
  let result = this._processSmsDeliver(length);
  if (result == PDU_FCS_RESERVED || result == MOZ_FCS_WAIT_FOR_EXPLICIT_ACK) {
    return;
  }
  // Not reserved FCS values, send ACK now.
  this.acknowledgeSMS(result == PDU_FCS_OK, result);
};
RIL[UNSOLICITED_RESPONSE_NEW_SMS_STATUS_REPORT] = function UNSOLICITED_RESPONSE_NEW_SMS_STATUS_REPORT(length) {
  let result = this._processSmsStatusReport(length);
  this.acknowledgeSMS(result == PDU_FCS_OK, result);
};
RIL[UNSOLICITED_RESPONSE_NEW_SMS_ON_SIM] = function UNSOLICITED_RESPONSE_NEW_SMS_ON_SIM(length) {
  let info = Buf.readUint32List();
  //TODO
};
RIL[UNSOLICITED_ON_USSD] = function UNSOLICITED_ON_USSD() {
  let [typeCode, message] = Buf.readStringList();
  if (DEBUG) {
    debug("On USSD. Type Code: " + typeCode + " Message: " + message);
  }

  this._ussdSession = (typeCode != "0" && typeCode != "2");

  this.sendDOMMessage({rilMessageType: "USSDReceived",
                       message: message,
                       sessionEnded: !this._ussdSession});
};
RIL[UNSOLICITED_NITZ_TIME_RECEIVED] = function UNSOLICITED_NITZ_TIME_RECEIVED() {
  let dateString = Buf.readString();

  // The data contained in the NITZ message is
  // in the form "yy/mm/dd,hh:mm:ss(+/-)tz,dt"
  // for example: 12/02/16,03:36:08-20,00,310410

  // Always print the NITZ info so we can collection what different providers
  // send down the pipe (see bug XXX).
  // TODO once data is collected, add in |if (DEBUG)|

  debug("DateTimeZone string " + dateString);

  let now = Date.now();

  let year = parseInt(dateString.substr(0, 2), 10);
  let month = parseInt(dateString.substr(3, 2), 10);
  let day = parseInt(dateString.substr(6, 2), 10);
  let hours = parseInt(dateString.substr(9, 2), 10);
  let minutes = parseInt(dateString.substr(12, 2), 10);
  let seconds = parseInt(dateString.substr(15, 2), 10);
  // Note that |tz| is in 15-min units.
  let tz = parseInt(dateString.substr(17, 3), 10);
  // Note that |dst| is in 1-hour units and is already applied in |tz|.
  let dst = parseInt(dateString.substr(21, 2), 10);

  let timeInMS = Date.UTC(year + PDU_TIMESTAMP_YEAR_OFFSET, month - 1, day,
                          hours, minutes, seconds);

  if (isNaN(timeInMS)) {
    if (DEBUG) debug("NITZ failed to convert date");
    return;
  }

  this.sendDOMMessage({rilMessageType: "nitzTime",
                       networkTimeInMS: timeInMS,
                       networkTimeZoneInMinutes: -(tz * 15),
                       networkDSTInMinutes: -(dst * 60),
                       receiveTimeInMS: now});
};

RIL[UNSOLICITED_SIGNAL_STRENGTH] = function UNSOLICITED_SIGNAL_STRENGTH(length) {
  this[REQUEST_SIGNAL_STRENGTH](length, {rilRequestError: ERROR_SUCCESS});
};
RIL[UNSOLICITED_DATA_CALL_LIST_CHANGED] = function UNSOLICITED_DATA_CALL_LIST_CHANGED(length) {
  if (RILQUIRKS_V5_LEGACY) {
    this.getDataCallList();
    return;
  }
  this[REQUEST_DATA_CALL_LIST](length, {rilRequestError: ERROR_SUCCESS});
};
RIL[UNSOLICITED_SUPP_SVC_NOTIFICATION] = null;

RIL[UNSOLICITED_STK_SESSION_END] = function UNSOLICITED_STK_SESSION_END() {
  this.sendDOMMessage({rilMessageType: "stksessionend"});
};
RIL[UNSOLICITED_STK_PROACTIVE_COMMAND] = function UNSOLICITED_STK_PROACTIVE_COMMAND() {
  this.processStkProactiveCommand();
};
RIL[UNSOLICITED_STK_EVENT_NOTIFY] = function UNSOLICITED_STK_EVENT_NOTIFY() {
  this.processStkProactiveCommand();
};
RIL[UNSOLICITED_STK_CALL_SETUP] = null;
RIL[UNSOLICITED_SIM_SMS_STORAGE_FULL] = null;
RIL[UNSOLICITED_SIM_REFRESH] = null;
RIL[UNSOLICITED_CALL_RING] = function UNSOLICITED_CALL_RING() {
  let info = {rilMessageType: "callRing"};
  let isCDMA = false; //XXX TODO hard-code this for now
  if (isCDMA) {
    info.isPresent = Buf.readUint32();
    info.signalType = Buf.readUint32();
    info.alertPitch = Buf.readUint32();
    info.signal = Buf.readUint32();
  }
  // At this point we don't know much other than the fact there's an incoming
  // call, but that's enough to bring up the Phone app already. We'll know
  // details once we get a call state changed notification and can then
  // dispatch DOM events etc.
  this.sendDOMMessage(info);
};
RIL[UNSOLICITED_RESPONSE_SIM_STATUS_CHANGED] = function UNSOLICITED_RESPONSE_SIM_STATUS_CHANGED() {
  this.getICCStatus();
};
RIL[UNSOLICITED_RESPONSE_CDMA_NEW_SMS] = null;
RIL[UNSOLICITED_RESPONSE_NEW_BROADCAST_SMS] = function UNSOLICITED_RESPONSE_NEW_BROADCAST_SMS(length) {
  let message;
  try {
    message = GsmPDUHelper.readCbMessage(Buf.readUint32());
  } catch (e) {
    if (DEBUG) {
      debug("Failed to parse Cell Broadcast message: " + JSON.stringify(e));
    }
    return;
  }

  message = this._processReceivedSmsCbPage(message);
  if (!message) {
    return;
  }

  message.rilMessageType = "cellbroadcast-received";
  this.sendDOMMessage(message);
};
RIL[UNSOLICITED_CDMA_RUIM_SMS_STORAGE_FULL] = null;
RIL[UNSOLICITED_RESTRICTED_STATE_CHANGED] = null;
RIL[UNSOLICITED_ENTER_EMERGENCY_CALLBACK_MODE] = null;
RIL[UNSOLICITED_CDMA_CALL_WAITING] = null;
RIL[UNSOLICITED_CDMA_OTA_PROVISION_STATUS] = null;
RIL[UNSOLICITED_CDMA_INFO_REC] = null;
RIL[UNSOLICITED_OEM_HOOK_RAW] = null;
RIL[UNSOLICITED_RINGBACK_TONE] = null;
RIL[UNSOLICITED_RESEND_INCALL_MUTE] = null;
RIL[UNSOLICITED_RIL_CONNECTED] = function UNSOLICITED_RIL_CONNECTED(length) {
  // Prevent response id collision between UNSOLICITED_RIL_CONNECTED and
  // UNSOLICITED_VOICE_RADIO_TECH_CHANGED for Akami on gingerbread branch.
  if (!length) {
    return;
  }

  let version = Buf.readUint32List()[0];
  RILQUIRKS_V5_LEGACY = (version < 5);
  if (DEBUG) {
    debug("Detected RIL version " + version);
    debug("RILQUIRKS_V5_LEGACY is " + RILQUIRKS_V5_LEGACY);
  }

  this.initRILState();

  this.setPreferredNetworkType();
};

/**
 * This object exposes the functionality to parse and serialize PDU strings
 *
 * A PDU is a string containing a series of hexadecimally encoded octets
 * or nibble-swapped binary-coded decimals (BCDs). It contains not only the
 * message text but information about the sender, the SMS service center,
 * timestamp, etc.
 */
let GsmPDUHelper = {

  /**
   * Read one character (2 bytes) from a RIL string and decode as hex.
   *
   * @return the nibble as a number.
   */
  readHexNibble: function readHexNibble() {
    let nibble = Buf.readUint16();
    if (nibble >= 48 && nibble <= 57) {
      nibble -= 48; // ASCII '0'..'9'
    } else if (nibble >= 65 && nibble <= 70) {
      nibble -= 55; // ASCII 'A'..'F'
    } else if (nibble >= 97 && nibble <= 102) {
      nibble -= 87; // ASCII 'a'..'f'
    } else {
      throw "Found invalid nibble during PDU parsing: " +
            String.fromCharCode(nibble);
    }
    return nibble;
  },

  /**
   * Encode a nibble as one hex character in a RIL string (2 bytes).
   *
   * @param nibble
   *        The nibble to encode (represented as a number)
   */
  writeHexNibble: function writeHexNibble(nibble) {
    nibble &= 0x0f;
    if (nibble < 10) {
      nibble += 48; // ASCII '0'
    } else {
      nibble += 55; // ASCII 'A'
    }
    Buf.writeUint16(nibble);
  },

  /**
   * Read a hex-encoded octet (two nibbles).
   *
   * @return the octet as a number.
   */
  readHexOctet: function readHexOctet() {
    return (this.readHexNibble() << 4) | this.readHexNibble();
  },

  /**
   * Write an octet as two hex-encoded nibbles.
   *
   * @param octet
   *        The octet (represented as a number) to encode.
   */
  writeHexOctet: function writeHexOctet(octet) {
    this.writeHexNibble(octet >> 4);
    this.writeHexNibble(octet);
  },

  /**
   * Read an array of hex-encoded octets.
   */
  readHexOctetArray: function readHexOctetArray(length) {
    let array = new Uint8Array(length);
    for (let i = 0; i < length; i++) {
      array[i] = this.readHexOctet();
    }
    return array;
  },

  /**
   * Convert an octet (number) to a BCD number.
   *
   * Any nibbles that are not in the BCD range count as 0.
   *
   * @param octet
   *        The octet (a number, as returned by getOctet())
   *
   * @return the corresponding BCD number.
   */
  octetToBCD: function octetToBCD(octet) {
    return ((octet & 0xf0) <= 0x90) * ((octet >> 4) & 0x0f) +
           ((octet & 0x0f) <= 0x09) * (octet & 0x0f) * 10;
  },

  /**
   * Convert a BCD number to an octet (number)
   *
   * Only take two digits with absolute value.
   *
   * @param bcd
   *
   * @return the corresponding octet.
   */
  BCDToOctet: function BCDToOctet(bcd) {
    bcd = Math.abs(bcd);
    return ((bcd % 10) << 4) + (Math.floor(bcd / 10) % 10);
  },

  /**
   * Convert a semi-octet (number) to a GSM BCD char.
   */
  bcdChars: "0123456789*#,;",
  semiOctetToBcdChar: function semiOctetToBcdChar(semiOctet) {
    if (semiOctet >= 14) {
      throw new RangeError();
    }

    return this.bcdChars.charAt(semiOctet);
  },

  /**
   * Read a *swapped nibble* binary coded decimal (BCD)
   *
   * @param pairs
   *        Number of nibble *pairs* to read.
   *
   * @return the decimal as a number.
   */
  readSwappedNibbleBcdNum: function readSwappedNibbleBcdNum(pairs) {
    let number = 0;
    for (let i = 0; i < pairs; i++) {
      let octet = this.readHexOctet();
      // Ignore 'ff' octets as they're often used as filler.
      if (octet == 0xff) {
        continue;
      }
      // If the first nibble is an "F" , only the second nibble is to be taken
      // into account.
      if ((octet & 0xf0) == 0xf0) {
        number *= 10;
        number += octet & 0x0f;
        continue;
      }
      number *= 100;
      number += this.octetToBCD(octet);
    }
    return number;
  },

  /**
   * Read a *swapped nibble* binary coded string (BCD)
   *
   * @param pairs
   *        Number of nibble *pairs* to read.
   *
   * @return The BCD string.
   */
  readSwappedNibbleBcdString: function readSwappedNibbleBcdString(pairs) {
    let str = "";
    for (let i = 0; i < pairs; i++) {
      let nibbleH = this.readHexNibble();
      let nibbleL = this.readHexNibble();
      if (nibbleL == 0x0F) {
        break;
      }

      str += this.semiOctetToBcdChar(nibbleL);
      if (nibbleH != 0x0F) {
        str += this.semiOctetToBcdChar(nibbleH);
      }
    }

    return str;
  },

  /**
   * Write numerical data as swapped nibble BCD.
   *
   * @param data
   *        Data to write (as a string or a number)
   */
  writeSwappedNibbleBCD: function writeSwappedNibbleBCD(data) {
    data = data.toString();
    if (data.length % 2) {
      data += "F";
    }
    for (let i = 0; i < data.length; i += 2) {
      Buf.writeUint16(data.charCodeAt(i + 1));
      Buf.writeUint16(data.charCodeAt(i));
    }
  },

  /**
   * Write numerical data as swapped nibble BCD.
   * If the number of digit of data is even, add '0' at the beginning.
   *
   * @param data
   *        Data to write (as a string or a number)
   */
  writeSwappedNibbleBCDNum: function writeSwappedNibbleBCDNum(data) {
    data = data.toString();
    if (data.length % 2) {
      data = "0" + data;
    }
    for (let i = 0; i < data.length; i += 2) {
      Buf.writeUint16(data.charCodeAt(i + 1));
      Buf.writeUint16(data.charCodeAt(i));
    }
  },

  /**
   * Read user data, convert to septets, look up relevant characters in a
   * 7-bit alphabet, and construct string.
   *
   * @param length
   *        Number of septets to read (*not* octets)
   * @param paddingBits
   *        Number of padding bits in the first byte of user data.
   * @param langIndex
   *        Table index used for normal 7-bit encoded character lookup.
   * @param langShiftIndex
   *        Table index used for escaped 7-bit encoded character lookup.
   *
   * @return a string.
   */
  readSeptetsToString: function readSeptetsToString(length, paddingBits, langIndex, langShiftIndex) {
    let ret = "";
    let byteLength = Math.ceil((length * 7 + paddingBits) / 8);

    /**
     * |<-                    last byte in header                    ->|
     * |<-           incompleteBits          ->|<- last header septet->|
     * +===7===|===6===|===5===|===4===|===3===|===2===|===1===|===0===|
     *
     * |<-                   1st byte in user data                   ->|
     * |<-               data septet 1               ->|<-paddingBits->|
     * +===7===|===6===|===5===|===4===|===3===|===2===|===1===|===0===|
     *
     * |<-                   2nd byte in user data                   ->|
     * |<-                   data spetet 2                   ->|<-ds1->|
     * +===7===|===6===|===5===|===4===|===3===|===2===|===1===|===0===|
     */
    let data = 0;
    let dataBits = 0;
    if (paddingBits) {
      data = this.readHexOctet() >> paddingBits;
      dataBits = 8 - paddingBits;
      --byteLength;
    }

    let escapeFound = false;
    const langTable = PDU_NL_LOCKING_SHIFT_TABLES[langIndex];
    const langShiftTable = PDU_NL_SINGLE_SHIFT_TABLES[langShiftIndex];
    do {
      // Read as much as fits in 32bit word
      let bytesToRead = Math.min(byteLength, dataBits ? 3 : 4);
      for (let i = 0; i < bytesToRead; i++) {
        data |= this.readHexOctet() << dataBits;
        dataBits += 8;
        --byteLength;
      }

      // Consume available full septets
      for (; dataBits >= 7; dataBits -= 7) {
        let septet = data & 0x7F;
        data >>>= 7;

        if (escapeFound) {
          escapeFound = false;
          if (septet == PDU_NL_EXTENDED_ESCAPE) {
            // According to 3GPP TS 23.038, section 6.2.1.1, NOTE 1, "On
            // receipt of this code, a receiving entity shall display a space
            // until another extensiion table is defined."
            ret += " ";
          } else if (septet == PDU_NL_RESERVED_CONTROL) {
            // According to 3GPP TS 23.038 B.2, "This code represents a control
            // character and therefore must not be used for language specific
            // characters."
            ret += " ";
          } else {
            ret += langShiftTable[septet];
          }
        } else if (septet == PDU_NL_EXTENDED_ESCAPE) {
          escapeFound = true;

          // <escape> is not an effective character
          --length;
        } else {
          ret += langTable[septet];
        }
      }
    } while (byteLength);

    if (ret.length != length) {
      /**
       * If num of effective characters does not equal to the length of read
       * string, cut the tail off. This happens when the last octet of user
       * data has following layout:
       *
       * |<-              penultimate octet in user data               ->|
       * |<-               data septet N               ->|<-   dsN-1   ->|
       * +===7===|===6===|===5===|===4===|===3===|===2===|===1===|===0===|
       *
       * |<-                  last octet in user data                  ->|
       * |<-                       fill bits                   ->|<-dsN->|
       * +===7===|===6===|===5===|===4===|===3===|===2===|===1===|===0===|
       *
       * The fill bits in the last octet may happen to form a full septet and
       * be appended at the end of result string.
       */
      ret = ret.slice(0, length);
    }
    return ret;
  },

  writeStringAsSeptets: function writeStringAsSeptets(message, paddingBits, langIndex, langShiftIndex) {
    const langTable = PDU_NL_LOCKING_SHIFT_TABLES[langIndex];
    const langShiftTable = PDU_NL_SINGLE_SHIFT_TABLES[langShiftIndex];

    let dataBits = paddingBits;
    let data = 0;
    for (let i = 0; i < message.length; i++) {
      let c = message.charAt(i);
      let septet = langTable.indexOf(c);
      if (septet == PDU_NL_EXTENDED_ESCAPE) {
        continue;
      }

      if (septet >= 0) {
        data |= septet << dataBits;
        dataBits += 7;
      } else {
        septet = langShiftTable.indexOf(c);
        if (septet == -1) {
          throw new Error("'" + c + "' is not in 7 bit alphabet "
                          + langIndex + ":" + langShiftIndex + "!");
        }

        if (septet == PDU_NL_RESERVED_CONTROL) {
          continue;
        }

        data |= PDU_NL_EXTENDED_ESCAPE << dataBits;
        dataBits += 7;
        data |= septet << dataBits;
        dataBits += 7;
      }

      for (; dataBits >= 8; dataBits -= 8) {
        this.writeHexOctet(data & 0xFF);
        data >>>= 8;
      }
    }

    if (dataBits != 0) {
      this.writeHexOctet(data & 0xFF);
    }
  },

  /**
   * Read GSM 8-bit unpacked octets,
   * which are SMS default 7-bit alphabets with bit 8 set to 0.
   *
   * @param numOctets
   *        Number of octets to be read.
   */
  read8BitUnpackedToString: function read8BitUnpackedToString(numOctets) {
    let ret = "";
    let escapeFound = false;
    let i;
    const langTable = PDU_NL_LOCKING_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];
    const langShiftTable = PDU_NL_SINGLE_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];

    for(i = 0; i < numOctets; i++) {
      let octet = this.readHexOctet();
      if (octet == 0xff) {
        i++;
        break;
      }

      if (escapeFound) {
        escapeFound = false;
        if (octet == PDU_NL_EXTENDED_ESCAPE) {
          // According to 3GPP TS 23.038, section 6.2.1.1, NOTE 1, "On
          // receipt of this code, a receiving entity shall display a space
          // until another extensiion table is defined."
          ret += " ";
        } else if (octet == PDU_NL_RESERVED_CONTROL) {
          // According to 3GPP TS 23.038 B.2, "This code represents a control
          // character and therefore must not be used for language specific
          // characters."
          ret += " ";
        } else {
          ret += langShiftTable[octet];
        }
      } else if (octet == PDU_NL_EXTENDED_ESCAPE) {
        escapeFound = true;
      } else {
        ret += langTable[octet];
      }
    }

    Buf.seekIncoming((numOctets - i) * PDU_HEX_OCTET_SIZE);
    return ret;
  },

  /**
   * Write GSM 8-bit unpacked octets.
   *
   * @param numOctets   Number of total octets to be writen, including trailing
   *                    0xff.
   * @param str         String to be written. Could be null.
   */
  writeStringTo8BitUnpacked: function writeStringTo8BitUnpacked(numOctets, str) {
    const langTable = PDU_NL_LOCKING_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];
    const langShiftTable = PDU_NL_SINGLE_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];

    // If the character is GSM extended alphabet, two octets will be written.
    // So we need to keep track of number of octets to be written.
    let i, j;
    let len = str ? str.length : 0;
    for (i = 0, j = 0; i < len && j < numOctets; i++) {
      let c = str.charAt(i);
      let octet = langTable.indexOf(c);

      if (octet == -1) {
        // Make sure we still have enough space to write two octets.
        if (j + 2 > numOctets) {
          break;
        }

        octet = langShiftTable.indexOf(c);
        if (octet == -1) {
          // Fallback to ASCII space.
          octet = langTable.indexOf(' ');
        }
        this.writeHexOctet(PDU_NL_EXTENDED_ESCAPE);
        j++;
      }
      this.writeHexOctet(octet);
      j++;
    }

    // trailing 0xff
    while (j++ < numOctets) {
      this.writeHexOctet(0xff);
    }
  },

  /**
   * Read user data and decode as a UCS2 string.
   *
   * @param numOctets
   *        Number of octets to be read as UCS2 string.
   *
   * @return a string.
   */
  readUCS2String: function readUCS2String(numOctets) {
    let str = "";
    let length = numOctets / 2;
    for (let i = 0; i < length; ++i) {
      let code = (this.readHexOctet() << 8) | this.readHexOctet();
      str += String.fromCharCode(code);
    }

    if (DEBUG) debug("Read UCS2 string: " + str);

    return str;
  },

  /**
   * Write user data as a UCS2 string.
   *
   * @param message
   *        Message string to encode as UCS2 in hex-encoded octets.
   */
  writeUCS2String: function writeUCS2String(message) {
    for (let i = 0; i < message.length; ++i) {
      let code = message.charCodeAt(i);
      this.writeHexOctet((code >> 8) & 0xFF);
      this.writeHexOctet(code & 0xFF);
    }
  },

  /**
   * Read UCS2 String on UICC.
   *
   * @see TS 101.221, Annex A.
   * @param scheme
   *        Coding scheme for UCS2 on UICC. One of 0x80, 0x81 or 0x82.
   * @param numOctets
   *        Number of octets to be read as UCS2 string.
   */
  readICCUCS2String: function readICCUCS2String(scheme, numOctets) {
    let str = "";
    switch (scheme) {
      /**
       * +------+---------+---------+---------+---------+------+------+
       * | 0x80 | Ch1_msb | Ch1_lsb | Ch2_msb | Ch2_lsb | 0xff | 0xff |
       * +------+---------+---------+---------+---------+------+------+
       */
      case 0x80:
        let isOdd = numOctets % 2;
        let i;
        for (i = 0; i < numOctets - isOdd; i += 2) {
          let code = (this.readHexOctet() << 8) | this.readHexOctet();
          if (code == 0xffff) {
            i += 2;
            break;
          }
          str += String.fromCharCode(code);
        }

        // Skip trailing 0xff
        Buf.seekIncoming((numOctets - i) * PDU_HEX_OCTET_SIZE);
        break;
      case 0x81: // Fall through
      case 0x82:
        /**
         * +------+-----+--------+-----+-----+-----+--------+------+
         * | 0x81 | len | offset | Ch1 | Ch2 | ... | Ch_len | 0xff |
         * +------+-----+--------+-----+-----+-----+--------+------+
         *
         * len : The length of characters.
         * offset : 0hhh hhhh h000 0000
         * Ch_n: bit 8 = 0
         *       GSM default alphabets
         *       bit 8 = 1
         *       UCS2 character whose char code is (Ch_n & 0x7f) + offset
         *
         * +------+-----+------------+------------+-----+-----+-----+--------+
         * | 0x82 | len | offset_msb | offset_lsb | Ch1 | Ch2 | ... | Ch_len |
         * +------+-----+------------+------------+-----+-----+-----+--------+
         *
         * len : The length of characters.
         * offset_msb, offset_lsn: offset
         * Ch_n: bit 8 = 0
         *       GSM default alphabets
         *       bit 8 = 1
         *       UCS2 character whose char code is (Ch_n & 0x7f) + offset
         */
        let len = this.readHexOctet();
        let offset, headerLen;
        if (scheme == 0x81) {
          offset = this.readHexOctet() << 7;
          headerLen = 2;
        } else {
          offset = (this.readHexOctet() << 8) | this.readHexOctet();
          headerLen = 3;
        }

        for (let i = 0; i < len; i++) {
          let ch = this.readHexOctet();
          if (ch & 0x80) {
            // UCS2
            str += String.fromCharCode((ch & 0x7f) + offset);
          } else {
            // GSM 8bit
            let count = 0, gotUCS2 = 0;
            while ((i + count + 1 < len)) {
              count++;
              if (this.readHexOctet() & 0x80) {
                gotUCS2 = 1;
                break;
              };
            }
            // Unread.
            // +1 for the GSM alphabet indexed at i,
            Buf.seekIncoming(-1 * (count + 1) * PDU_HEX_OCTET_SIZE);
            str += this.read8BitUnpackedToString(count + 1 - gotUCS2);
            i += count - gotUCS2;
          }
        }

        // Skipping trailing 0xff
        Buf.seekIncoming((numOctets - len - headerLen) * PDU_HEX_OCTET_SIZE);
        break;
    }
    return str;
  },

  /**
   * Read 1 + UDHL octets and construct user data header.
   *
   * @param msg
   *        message object for output.
   *
   * @see 3GPP TS 23.040 9.2.3.24
   */
  readUserDataHeader: function readUserDataHeader(msg) {
    /**
     * A header object with properties contained in received message.
     * The properties set include:
     *
     * length: totoal length of the header, default 0.
     * langIndex: used locking shift table index, default
     * PDU_NL_IDENTIFIER_DEFAULT.
     * langShiftIndex: used locking shift table index, default
     * PDU_NL_IDENTIFIER_DEFAULT.
     *
     */
    let header = {
      length: 0,
      langIndex: PDU_NL_IDENTIFIER_DEFAULT,
      langShiftIndex: PDU_NL_IDENTIFIER_DEFAULT
    };

    header.length = this.readHexOctet();
    if (DEBUG) debug("Read UDH length: " + header.length);

    let dataAvailable = header.length;
    while (dataAvailable >= 2) {
      let id = this.readHexOctet();
      let length = this.readHexOctet();
      if (DEBUG) debug("Read UDH id: " + id + ", length: " + length);

      dataAvailable -= 2;

      switch (id) {
        case PDU_IEI_CONCATENATED_SHORT_MESSAGES_8BIT: {
          let ref = this.readHexOctet();
          let max = this.readHexOctet();
          let seq = this.readHexOctet();
          dataAvailable -= 3;
          if (max && seq && (seq <= max)) {
            header.segmentRef = ref;
            header.segmentMaxSeq = max;
            header.segmentSeq = seq;
          }
          break;
        }
        case PDU_IEI_APPLICATION_PORT_ADDRESSING_SCHEME_8BIT: {
          let dstp = this.readHexOctet();
          let orip = this.readHexOctet();
          dataAvailable -= 2;
          if ((dstp < PDU_APA_RESERVED_8BIT_PORTS)
              || (orip < PDU_APA_RESERVED_8BIT_PORTS)) {
            // 3GPP TS 23.040 clause 9.2.3.24.3: "A receiving entity shall
            // ignore any information element where the value of the
            // Information-Element-Data is Reserved or not supported"
            break;
          }
          header.destinationPort = dstp;
          header.originatorPort = orip;
          break;
        }
        case PDU_IEI_APPLICATION_PORT_ADDRESSING_SCHEME_16BIT: {
          let dstp = (this.readHexOctet() << 8) | this.readHexOctet();
          let orip = (this.readHexOctet() << 8) | this.readHexOctet();
          dataAvailable -= 4;
          // 3GPP TS 23.040 clause 9.2.3.24.4: "A receiving entity shall
          // ignore any information element where the value of the
          // Information-Element-Data is Reserved or not supported"
          if ((dstp < PDU_APA_VALID_16BIT_PORTS)
              && (orip < PDU_APA_VALID_16BIT_PORTS)) {
            header.destinationPort = dstp;
            header.originatorPort = orip;
          }
          break;
        }
        case PDU_IEI_CONCATENATED_SHORT_MESSAGES_16BIT: {
          let ref = (this.readHexOctet() << 8) | this.readHexOctet();
          let max = this.readHexOctet();
          let seq = this.readHexOctet();
          dataAvailable -= 4;
          if (max && seq && (seq <= max)) {
            header.segmentRef = ref;
            header.segmentMaxSeq = max;
            header.segmentSeq = seq;
          }
          break;
        }
        case PDU_IEI_NATIONAL_LANGUAGE_SINGLE_SHIFT:
          let langShiftIndex = this.readHexOctet();
          --dataAvailable;
          if (langShiftIndex < PDU_NL_SINGLE_SHIFT_TABLES.length) {
            header.langShiftIndex = langShiftIndex;
          }
          break;
        case PDU_IEI_NATIONAL_LANGUAGE_LOCKING_SHIFT:
          let langIndex = this.readHexOctet();
          --dataAvailable;
          if (langIndex < PDU_NL_LOCKING_SHIFT_TABLES.length) {
            header.langIndex = langIndex;
          }
          break;
        case PDU_IEI_SPECIAL_SMS_MESSAGE_INDICATION:
          let msgInd = this.readHexOctet() & 0xFF;
          let msgCount = this.readHexOctet();
          dataAvailable -= 2;


          /*
           * TS 23.040 V6.8.1 Sec 9.2.3.24.2
           * bits 1 0   : basic message indication type
           * bits 4 3 2 : extended message indication type
           * bits 6 5   : Profile id
           * bit  7     : storage type
           */
          let storeType = msgInd & PDU_MWI_STORE_TYPE_BIT;
          let mwi = msg.mwi;
          if (!mwi) {
            mwi = msg.mwi = {};
          }

          if (storeType == PDU_MWI_STORE_TYPE_STORE) {
            // Store message because TP_UDH indicates so, note this may override
            // the setting in DCS, but that is expected
            mwi.discard = false;
          } else if (mwi.discard === undefined) {
            // storeType == PDU_MWI_STORE_TYPE_DISCARD
            // only override mwi.discard here if it hasn't already been set
            mwi.discard = true;
          }

          mwi.msgCount = msgCount & 0xFF;
          mwi.active = mwi.msgCount > 0;

          if (DEBUG) debug("MWI in TP_UDH received: " + JSON.stringify(mwi));

          break;
        default:
          if (DEBUG) {
            debug("readUserDataHeader: unsupported IEI(" + id
                  + "), " + length + " bytes.");
          }

          // Read out unsupported data
          if (length) {
            let octets;
            if (DEBUG) octets = new Uint8Array(length);

            for (let i = 0; i < length; i++) {
              let octet = this.readHexOctet();
              if (DEBUG) octets[i] = octet;
            }
            dataAvailable -= length;

            if (DEBUG) debug("readUserDataHeader: " + Array.slice(octets));
          }
          break;
      }
    }

    if (dataAvailable != 0) {
      throw new Error("Illegal user data header found!");
    }

    msg.header = header;
  },

  /**
   * Write out user data header.
   *
   * @param options
   *        Options containing information for user data header write-out. The
   *        `userDataHeaderLength` property must be correctly pre-calculated.
   */
  writeUserDataHeader: function writeUserDataHeader(options) {
    this.writeHexOctet(options.userDataHeaderLength);

    if (options.segmentMaxSeq > 1) {
      if (options.segmentRef16Bit) {
        this.writeHexOctet(PDU_IEI_CONCATENATED_SHORT_MESSAGES_16BIT);
        this.writeHexOctet(4);
        this.writeHexOctet((options.segmentRef >> 8) & 0xFF);
      } else {
        this.writeHexOctet(PDU_IEI_CONCATENATED_SHORT_MESSAGES_8BIT);
        this.writeHexOctet(3);
      }
      this.writeHexOctet(options.segmentRef & 0xFF);
      this.writeHexOctet(options.segmentMaxSeq & 0xFF);
      this.writeHexOctet(options.segmentSeq & 0xFF);
    }

    if (options.dcs == PDU_DCS_MSG_CODING_7BITS_ALPHABET) {
      if (options.langIndex != PDU_NL_IDENTIFIER_DEFAULT) {
        this.writeHexOctet(PDU_IEI_NATIONAL_LANGUAGE_LOCKING_SHIFT);
        this.writeHexOctet(1);
        this.writeHexOctet(options.langIndex);
      }

      if (options.langShiftIndex != PDU_NL_IDENTIFIER_DEFAULT) {
        this.writeHexOctet(PDU_IEI_NATIONAL_LANGUAGE_SINGLE_SHIFT);
        this.writeHexOctet(1);
        this.writeHexOctet(options.langShiftIndex);
      }
    }
  },

  /**
   * Read SM-TL Address.
   *
   * @param len
   *        Length of useful semi-octets within the Address-Value field. For
   *        example, the lenth of "12345" should be 5, and 4 for "1234".
   *
   * @see 3GPP TS 23.040 9.1.2.5
   */
  readAddress: function readAddress(len) {
    // Address Length
    if (!len || (len < 0)) {
      if (DEBUG) debug("PDU error: invalid sender address length: " + len);
      return null;
    }
    if (len % 2 == 1) {
      len += 1;
    }
    if (DEBUG) debug("PDU: Going to read address: " + len);

    // Type-of-Address
    let toa = this.readHexOctet();
    let addr = "";

    if ((toa & 0xF0) == PDU_TOA_ALPHANUMERIC) {
      addr = this.readSeptetsToString(Math.floor(len * 4 / 7), 0,
          PDU_NL_IDENTIFIER_DEFAULT , PDU_NL_IDENTIFIER_DEFAULT );
      return addr;
    }
    addr = this.readSwappedNibbleBcdString(len / 2);
    if (addr.length <= 0) {
      if (DEBUG) debug("PDU error: no number provided");
      return null;
    }
    if ((toa & 0xF0) == (PDU_TOA_INTERNATIONAL)) {
      addr = '+' + addr;
    }

    return addr;
  },

  /**
   * Read Alpha Id and Dialling number from TS TS 151.011 clause 10.5.1
   *
   * @param recordSize  The size of linear fixed record.
   */
  readAlphaIdDiallingNumber: function readAlphaIdDiallingNumber(recordSize) {
    let length = Buf.readUint32();

    let alphaLen = recordSize - ADN_FOOTER_SIZE_BYTES;
    let alphaId = this.readAlphaIdentifier(alphaLen);

    let number;
    let numLen = this.readHexOctet();
    if (numLen != 0xff) {
      if (numLen > ADN_MAX_BCD_NUMBER_BYTES) {
        throw new Error("invalid length of BCD number/SSC contents - " + numLen);
      }

      number = this.readDiallingNumber(numLen);
      Buf.seekIncoming((ADN_MAX_BCD_NUMBER_BYTES - numLen) * PDU_HEX_OCTET_SIZE);
    } else {
      Buf.seekIncoming(ADN_MAX_BCD_NUMBER_BYTES * PDU_HEX_OCTET_SIZE);
    }

    // Skip 2 unused octets, CCP and EXT1.
    Buf.seekIncoming(2 * PDU_HEX_OCTET_SIZE);
    Buf.readStringDelimiter(length);

    let contact = null;
    if (alphaId || number) {
      contact = {alphaId: alphaId,
                 number: number};
    }
    return contact;
  },

  /**
   * Write Alpha Identifier and Dialling number from TS 151.011 clause 10.5.1
   *
   * @param recordSize  The size of linear fixed record.
   * @param alphaId     Alpha Identifier to be written.
   * @param number      Dialling Number to be written.
   */
  writeAlphaIdDiallingNumber: function writeAlphaIdDiallingNumber(recordSize,
                                                                  alphaId,
                                                                  number) {
    // Write String length
    let length = recordSize * 2;
    Buf.writeUint32(length);

    let alphaLen = recordSize - ADN_FOOTER_SIZE_BYTES;
    this.writeAlphaIdentifier(alphaLen, alphaId);

    if (number) {
      let numStart = number[0] == "+" ? 1 : 0;
      let numDigits = number.length - numStart;
      if (numDigits > ADN_MAX_NUMBER_DIGITS) {
        number = number.substring(0, ADN_MAX_NUMBER_DIGITS + numStart);
        numDigits = number.length - numStart;
      }

      // +1 for TON/NPI
      let numLen = Math.ceil(numDigits / 2) + 1;
      this.writeHexOctet(numLen);
      this.writeDiallingNumber(number);
      // Write trailing 0xff of Dialling Number.
      for (let i = 0; i < ADN_MAX_BCD_NUMBER_BYTES - numLen; i++) {
        this.writeHexOctet(0xff);
      }
    } else {
      // +1 for numLen
      for (let i = 0; i < ADN_MAX_BCD_NUMBER_BYTES + 1; i++) {
        this.writeHexOctet(0xff);
      }
    }

    // Write unused octets 0xff, CCP and EXT1.
    this.writeHexOctet(0xff);
    this.writeHexOctet(0xff);
    Buf.writeStringDelimiter(length);
  },

  /**
   * Read Alpha Identifier.
   *
   * @see TS 131.102
   *
   * @param numOctets
   *        Number of octets to be read.
   *
   * It uses either
   *  1. SMS default 7-bit alphabet with bit 8 set to 0.
   *  2. UCS2 string.
   *
   * Unused bytes should be set to 0xff.
   */
  readAlphaIdentifier: function readAlphaIdentifier(numOctets) {
    if (numOctets === 0) {
      return "";
    }

    let temp;
    // Read the 1st octet to determine the encoding.
    if ((temp = GsmPDUHelper.readHexOctet()) == 0x80 ||
         temp == 0x81 ||
         temp == 0x82) {
      numOctets--;
      return this.readICCUCS2String(temp, numOctets);
    } else {
      Buf.seekIncoming(-1 * PDU_HEX_OCTET_SIZE);
      return this.read8BitUnpackedToString(numOctets);
    }
  },

  /**
   * Write Alpha Identifier.
   *
   * @param numOctets
   *        Total number of octets to be written. This includes the length of
   *        alphaId and the length of trailing unused octets(0xff).
   * @param alphaId
   *        Alpha Identifier to be written.
   *
   * Unused octets will be written as 0xff.
   */
  writeAlphaIdentifier: function writeAlphaIdentifier(numOctets, alphaId) {
    if (numOctets === 0) {
      return;
    }

    // If alphaId is empty or it's of GSM 8 bit.
    if (!alphaId || ICCUtilsHelper.isGsm8BitAlphabet(alphaId)) {
      this.writeStringTo8BitUnpacked(numOctets, alphaId);
    } else {
      // Currently only support UCS2 coding scheme 0x80.
      this.writeHexOctet(0x80);
      numOctets--;
      // Now the alphaId is UCS2 string, each character will take 2 octets.
      if (alphaId.length * 2 > numOctets) {
        alphaId = alphaId.substring(0, Math.floor(numOctets / 2));
      }
      this.writeUCS2String(alphaId);
      for (let i = alphaId.length * 2; i < numOctets; i++) {
        this.writeHexOctet(0xff);
      }
    }
  },

  /**
   * Read Dialling number.
   *
   * @see TS 131.102
   *
   * @param len
   *        The Length of BCD number.
   *
   * From TS 131.102, in EF_ADN, EF_FDN, the field 'Length of BCD number'
   * means the total bytes should be allocated to store the TON/NPI and
   * the dialing number.
   * For example, if the dialing number is 1234567890,
   * and the TON/NPI is 0x81,
   * The field 'Length of BCD number' should be 06, which is
   * 1 byte to store the TON/NPI, 0x81
   * 5 bytes to store the BCD number 2143658709.
   *
   * Here the definition of the length is different from SMS spec,
   * TS 23.040 9.1.2.5, which the length means
   * "number of useful semi-octets within the Address-Value field".
   */
  readDiallingNumber: function readDiallingNumber(len) {
    if (DEBUG) debug("PDU: Going to read Dialling number: " + len);

    // TOA = TON + NPI
    let toa = this.readHexOctet();

    let number = this.readSwappedNibbleBcdString(len - 1).toString();
    if (number.length <= 0) {
      if (DEBUG) debug("PDU error: no number provided");
      return null;
    }
    if ((toa >> 4) == (PDU_TOA_INTERNATIONAL >> 4)) {
      number = '+' + number;
    }
    return number;
  },

  /**
   * Write Dialling Number.
   *
   * @param number  The Dialling number
   */
  writeDiallingNumber: function writeDiallingNumber(number) {
    let toa = PDU_TOA_ISDN; // 81
    if (number[0] == '+') {
      toa = PDU_TOA_INTERNATIONAL | PDU_TOA_ISDN; // 91
      number = number.substring(1);
    }
    this.writeHexOctet(toa);
    this.writeSwappedNibbleBCD(number);
  },

  /**
   * Read TP-Protocol-Indicator(TP-PID).
   *
   * @param msg
   *        message object for output.
   *
   * @see 3GPP TS 23.040 9.2.3.9
   */
  readProtocolIndicator: function readProtocolIndicator(msg) {
    // `The MS shall interpret reserved, obsolete, or unsupported values as the
    // value 00000000 but shall store them exactly as received.`
    msg.pid = this.readHexOctet();

    msg.epid = msg.pid;
    switch (msg.epid & 0xC0) {
      case 0x40:
        // Bit 7..0 = 01xxxxxx
        switch (msg.epid) {
          case PDU_PID_SHORT_MESSAGE_TYPE_0:
          case PDU_PID_ANSI_136_R_DATA:
          case PDU_PID_USIM_DATA_DOWNLOAD:
            return;
          case PDU_PID_RETURN_CALL_MESSAGE:
            // Level 1 of message waiting indication:
            // Only a return call message is provided
            let mwi = msg.mwi = {};

            // TODO: When should we de-activate the level 1 indicator?
            mwi.active = true;
            mwi.discard = false;
            mwi.msgCount = GECKO_VOICEMAIL_MESSAGE_COUNT_UNKNOWN;
            if (DEBUG) debug("TP-PID got return call message: " + msg.sender);
            return;
        }
        break;
    }

    msg.epid = PDU_PID_DEFAULT;
  },

  /**
   * Read TP-Data-Coding-Scheme(TP-DCS)
   *
   * @param msg
   *        message object for output.
   *
   * @see 3GPP TS 23.040 9.2.3.10, 3GPP TS 23.038 4.
   */
  readDataCodingScheme: function readDataCodingScheme(msg) {
    let dcs = this.readHexOctet();
    if (DEBUG) debug("PDU: read SMS dcs: " + dcs);

    // No message class by default.
    let messageClass = PDU_DCS_MSG_CLASS_NORMAL;
    // 7 bit is the default fallback encoding.
    let encoding = PDU_DCS_MSG_CODING_7BITS_ALPHABET;
    switch (dcs & PDU_DCS_CODING_GROUP_BITS) {
      case 0x40: // bits 7..4 = 01xx
      case 0x50:
      case 0x60:
      case 0x70:
        // Bit 5..0 are coded exactly the same as Group 00xx
      case 0x00: // bits 7..4 = 00xx
      case 0x10:
      case 0x20:
      case 0x30:
        if (dcs & 0x10) {
          messageClass = dcs & PDU_DCS_MSG_CLASS_BITS;
        }
        switch (dcs & 0x0C) {
          case 0x4:
            encoding = PDU_DCS_MSG_CODING_8BITS_ALPHABET;
            break;
          case 0x8:
            encoding = PDU_DCS_MSG_CODING_16BITS_ALPHABET;
            break;
        }
        break;

      case 0xE0: // bits 7..4 = 1110
        encoding = PDU_DCS_MSG_CODING_16BITS_ALPHABET;
        // Bit 3..0 are coded exactly the same as Message Waiting Indication
        // Group 1101.
      case 0xC0: // bits 7..4 = 1100
      case 0xD0: // bits 7..4 = 1101
        // Indiciates voicemail indicator set or clear
        let active = (dcs & PDU_DCS_MWI_ACTIVE_BITS) == PDU_DCS_MWI_ACTIVE_VALUE;

        // If TP-UDH is present, these values will be overwritten
        switch (dcs & PDU_DCS_MWI_TYPE_BITS) {
          case PDU_DCS_MWI_TYPE_VOICEMAIL:
            let mwi = msg.mwi;
            if (!mwi) {
              mwi = msg.mwi = {};
            }

            mwi.active = active;
            mwi.discard = (dcs & PDU_DCS_CODING_GROUP_BITS) == 0xC0;
            mwi.msgCount = active ? GECKO_VOICEMAIL_MESSAGE_COUNT_UNKNOWN : 0;

            if (DEBUG) {
              debug("MWI in DCS received for voicemail: " + JSON.stringify(mwi));
            }
            break;
          case PDU_DCS_MWI_TYPE_FAX:
            if (DEBUG) debug("MWI in DCS received for fax");
            break;
          case PDU_DCS_MWI_TYPE_EMAIL:
            if (DEBUG) debug("MWI in DCS received for email");
            break;
          default:
            if (DEBUG) debug("MWI in DCS received for \"other\"");
            break;
        }
        break;

      case 0xF0: // bits 7..4 = 1111
        if (dcs & 0x04) {
          encoding = PDU_DCS_MSG_CODING_8BITS_ALPHABET;
        }
        messageClass = dcs & PDU_DCS_MSG_CLASS_BITS;
        break;

      default:
        // Falling back to default encoding.
        break;
    }

    msg.dcs = dcs;
    msg.encoding = encoding;
    msg.messageClass = GECKO_SMS_MESSAGE_CLASSES[messageClass];

    if (DEBUG) debug("PDU: message encoding is " + encoding + " bit.");
  },

  /**
   * Read GSM TP-Service-Centre-Time-Stamp(TP-SCTS).
   *
   * @see 3GPP TS 23.040 9.2.3.11
   */
  readTimestamp: function readTimestamp() {
    let year   = this.readSwappedNibbleBcdNum(1) + PDU_TIMESTAMP_YEAR_OFFSET;
    let month  = this.readSwappedNibbleBcdNum(1) - 1;
    let day    = this.readSwappedNibbleBcdNum(1);
    let hour   = this.readSwappedNibbleBcdNum(1);
    let minute = this.readSwappedNibbleBcdNum(1);
    let second = this.readSwappedNibbleBcdNum(1);
    let timestamp = Date.UTC(year, month, day, hour, minute, second);

    // If the most significant bit of the least significant nibble is 1,
    // the timezone offset is negative (fourth bit from the right => 0x08):
    //   localtime = UTC + tzOffset
    // therefore
    //   UTC = localtime - tzOffset
    let tzOctet = this.readHexOctet();
    let tzOffset = this.octetToBCD(tzOctet & ~0x08) * 15 * 60 * 1000;
    tzOffset = (tzOctet & 0x08) ? -tzOffset : tzOffset;
    timestamp -= tzOffset;

    return timestamp;
  },

  /**
   * Write GSM TP-Service-Centre-Time-Stamp(TP-SCTS).
   *
   * @see 3GPP TS 23.040 9.2.3.11
   */
  writeTimestamp: function writeTimestamp(date) {
    this.writeSwappedNibbleBCDNum(date.getFullYear() - PDU_TIMESTAMP_YEAR_OFFSET);

    // The value returned by getMonth() is an integer between 0 and 11.
    // 0 is corresponds to January, 1 to February, and so on.
    this.writeSwappedNibbleBCDNum(date.getMonth() + 1);
    this.writeSwappedNibbleBCDNum(date.getDate());
    this.writeSwappedNibbleBCDNum(date.getHours());
    this.writeSwappedNibbleBCDNum(date.getMinutes());
    this.writeSwappedNibbleBCDNum(date.getSeconds());

    // the value returned by getTimezoneOffset() is the difference,
    // in minutes, between UTC and local time.
    // For example, if your time zone is UTC+10 (Australian Eastern Standard Time),
    // -600 will be returned.
    // In TS 23.040 9.2.3.11, the Time Zone field of TP-SCTS indicates
    // the different between the local time and GMT.
    // And expressed in quarters of an hours. (so need to divid by 15)
    let zone = date.getTimezoneOffset() / 15;
    let octet = this.BCDToOctet(zone);

    // the bit3 of the Time Zone field represents the algebraic sign.
    // (0: positive, 1: negative).
    // For example, if the time zone is -0800 GMT,
    // 480 will be returned by getTimezoneOffset().
    // In this case, need to mark sign bit as 1. => 0x08
    if (zone > 0) {
      octet = octet | 0x08;
    }
    this.writeHexOctet(octet);
  },

  /**
   * User data can be 7 bit (default alphabet) data, 8 bit data, or 16 bit
   * (UCS2) data.
   *
   * @param msg
   *        message object for output.
   * @param length
   *        length of user data to read in octets.
   */
  readUserData: function readUserData(msg, length) {
    if (DEBUG) {
      debug("Reading " + length + " bytes of user data.");
    }

    let paddingBits = 0;
    if (msg.udhi) {
      this.readUserDataHeader(msg);

      if (msg.encoding == PDU_DCS_MSG_CODING_7BITS_ALPHABET) {
        let headerBits = (msg.header.length + 1) * 8;
        let headerSeptets = Math.ceil(headerBits / 7);

        length -= headerSeptets;
        paddingBits = headerSeptets * 7 - headerBits;
      } else {
        length -= (msg.header.length + 1);
      }
    }

    if (DEBUG) debug("After header, " + length + " septets left of user data");

    msg.body = null;
    msg.data = null;
    switch (msg.encoding) {
      case PDU_DCS_MSG_CODING_7BITS_ALPHABET:
        // 7 bit encoding allows 140 octets, which means 160 characters
        // ((140x8) / 7 = 160 chars)
        if (length > PDU_MAX_USER_DATA_7BIT) {
          if (DEBUG) debug("PDU error: user data is too long: " + length);
          break;
        }

        let langIndex = msg.udhi ? msg.header.langIndex : PDU_NL_IDENTIFIER_DEFAULT;
        let langShiftIndex = msg.udhi ? msg.header.langShiftIndex : PDU_NL_IDENTIFIER_DEFAULT;
        msg.body = this.readSeptetsToString(length, paddingBits, langIndex,
                                            langShiftIndex);
        break;
      case PDU_DCS_MSG_CODING_8BITS_ALPHABET:
        msg.data = this.readHexOctetArray(length);
        break;
      case PDU_DCS_MSG_CODING_16BITS_ALPHABET:
        msg.body = this.readUCS2String(length);
        break;
    }
  },

  /**
   * Read extra parameters if TP-PI is set.
   *
   * @param msg
   *        message object for output.
   */
  readExtraParams: function readExtraParams(msg) {
    // Because each PDU octet is converted to two UCS2 char2, we should always
    // get even messageStringLength in this#_processReceivedSms(). So, we'll
    // always need two delimitors at the end.
    if (Buf.readAvailable <= 4) {
      return;
    }

    // TP-Parameter-Indicator
    let pi;
    do {
      // `The most significant bit in octet 1 and any other TP-PI octets which
      // may be added later is reserved as an extension bit which when set to a
      // 1 shall indicate that another TP-PI octet follows immediately
      // afterwards.` ~ 3GPP TS 23.040 9.2.3.27
      pi = this.readHexOctet();
    } while (pi & PDU_PI_EXTENSION);

    // `If the TP-UDL bit is set to "1" but the TP-DCS bit is set to "0" then
    // the receiving entity shall for TP-DCS assume a value of 0x00, i.e. the
    // 7bit default alphabet.` ~ 3GPP 23.040 9.2.3.27
    msg.dcs = 0;
    msg.encoding = PDU_DCS_MSG_CODING_7BITS_ALPHABET;

    // TP-Protocol-Identifier
    if (pi & PDU_PI_PROTOCOL_IDENTIFIER) {
      this.readProtocolIndicator(msg);
    }
    // TP-Data-Coding-Scheme
    if (pi & PDU_PI_DATA_CODING_SCHEME) {
      this.readDataCodingScheme(msg);
    }
    // TP-User-Data-Length
    if (pi & PDU_PI_USER_DATA_LENGTH) {
      let userDataLength = this.readHexOctet();
      this.readUserData(msg, userDataLength);
    }
  },

  /**
   * Read and decode a PDU-encoded message from the stream.
   *
   * TODO: add some basic sanity checks like:
   * - do we have the minimum number of chars available
   */
  readMessage: function readMessage() {
    // An empty message object. This gets filled below and then returned.
    let msg = {
      // D:DELIVER, DR:DELIVER-REPORT, S:SUBMIT, SR:SUBMIT-REPORT,
      // ST:STATUS-REPORT, C:COMMAND
      // M:Mandatory, O:Optional, X:Unavailable
      //                  D  DR S  SR ST C
      SMSC:      null, // M  M  M  M  M  M
      mti:       null, // M  M  M  M  M  M
      udhi:      null, // M  M  O  M  M  M
      sender:    null, // M  X  X  X  X  X
      recipient: null, // X  X  M  X  M  M
      pid:       null, // M  O  M  O  O  M
      epid:      null, // M  O  M  O  O  M
      dcs:       null, // M  O  M  O  O  X
      mwi:       null, // O  O  O  O  O  O
      replace:  false, // O  O  O  O  O  O
      header:    null, // M  M  O  M  M  M
      body:      null, // M  O  M  O  O  O
      data:      null, // M  O  M  O  O  O
      timestamp: null, // M  X  X  X  X  X
      status:    null, // X  X  X  X  M  X
      scts:      null, // X  X  X  M  M  X
      dt:        null, // X  X  X  X  M  X
    };

    // SMSC info
    let smscLength = this.readHexOctet();
    if (smscLength > 0) {
      let smscTypeOfAddress = this.readHexOctet();
      // Subtract the type-of-address octet we just read from the length.
      msg.SMSC = this.readSwappedNibbleBcdString(smscLength - 1);
      if ((smscTypeOfAddress >> 4) == (PDU_TOA_INTERNATIONAL >> 4)) {
        msg.SMSC = '+' + msg.SMSC;
      }
    }

    // First octet of this SMS-DELIVER or SMS-SUBMIT message
    let firstOctet = this.readHexOctet();
    // Message Type Indicator
    msg.mti = firstOctet & 0x03;
    // User data header indicator
    msg.udhi = firstOctet & PDU_UDHI;

    switch (msg.mti) {
      case PDU_MTI_SMS_RESERVED:
        // `If an MS receives a TPDU with a "Reserved" value in the TP-MTI it
        // shall process the message as if it were an "SMS-DELIVER" but store
        // the message exactly as received.` ~ 3GPP TS 23.040 9.2.3.1
      case PDU_MTI_SMS_DELIVER:
        return this.readDeliverMessage(msg);
      case PDU_MTI_SMS_STATUS_REPORT:
        return this.readStatusReportMessage(msg);
      default:
        return null;
    }
  },

  /**
   * Read and decode a SMS-DELIVER PDU.
   *
   * @param msg
   *        message object for output.
   */
  readDeliverMessage: function readDeliverMessage(msg) {
    // - Sender Address info -
    let senderAddressLength = this.readHexOctet();
    msg.sender = this.readAddress(senderAddressLength);
    // - TP-Protocolo-Identifier -
    this.readProtocolIndicator(msg);
    // - TP-Data-Coding-Scheme -
    this.readDataCodingScheme(msg);
    // - TP-Service-Center-Time-Stamp -
    msg.timestamp = this.readTimestamp();
    // - TP-User-Data-Length -
    let userDataLength = this.readHexOctet();

    // - TP-User-Data -
    if (userDataLength > 0) {
      this.readUserData(msg, userDataLength);
    }

    return msg;
  },

  /**
   * Read and decode a SMS-STATUS-REPORT PDU.
   *
   * @param msg
   *        message object for output.
   */
  readStatusReportMessage: function readStatusReportMessage(msg) {
    // TP-Message-Reference
    msg.messageRef = this.readHexOctet();
    // TP-Recipient-Address
    let recipientAddressLength = this.readHexOctet();
    msg.recipient = this.readAddress(recipientAddressLength);
    // TP-Service-Centre-Time-Stamp
    msg.scts = this.readTimestamp();
    // TP-Discharge-Time
    msg.dt = this.readTimestamp();
    // TP-Status
    msg.status = this.readHexOctet();

    this.readExtraParams(msg);

    return msg;
  },

  /**
   * Serialize a SMS-SUBMIT PDU message and write it to the output stream.
   *
   * This method expects that a data coding scheme has been chosen already
   * and that the length of the user data payload in that encoding is known,
   * too. Both go hand in hand together anyway.
   *
   * @param address
   *        String containing the address (number) of the SMS receiver
   * @param userData
   *        String containing the message to be sent as user data
   * @param dcs
   *        Data coding scheme. One of the PDU_DCS_MSG_CODING_*BITS_ALPHABET
   *        constants.
   * @param userDataHeaderLength
   *        Length of embedded user data header, in bytes. The whole header
   *        size will be userDataHeaderLength + 1; 0 for no header.
   * @param encodedBodyLength
   *        Length of the user data when encoded with the given DCS. For UCS2,
   *        in bytes; for 7-bit, in septets.
   * @param langIndex
   *        Table index used for normal 7-bit encoded character lookup.
   * @param langShiftIndex
   *        Table index used for escaped 7-bit encoded character lookup.
   * @param requestStatusReport
   *        Request status report.
   */
  writeMessage: function writeMessage(options) {
    if (DEBUG) {
      debug("writeMessage: " + JSON.stringify(options));
    }
    let address = options.number;
    let body = options.body;
    let dcs = options.dcs;
    let userDataHeaderLength = options.userDataHeaderLength;
    let encodedBodyLength = options.encodedBodyLength;
    let langIndex = options.langIndex;
    let langShiftIndex = options.langShiftIndex;

    // SMS-SUBMIT Format:
    //
    // PDU Type - 1 octet
    // Message Reference - 1 octet
    // DA - Destination Address - 2 to 12 octets
    // PID - Protocol Identifier - 1 octet
    // DCS - Data Coding Scheme - 1 octet
    // VP - Validity Period - 0, 1 or 7 octets
    // UDL - User Data Length - 1 octet
    // UD - User Data - 140 octets

    let addressFormat = PDU_TOA_ISDN; // 81
    if (address[0] == '+') {
      addressFormat = PDU_TOA_INTERNATIONAL | PDU_TOA_ISDN; // 91
      address = address.substring(1);
    }
    //TODO validity is unsupported for now
    let validity = 0;

    let headerOctets = (userDataHeaderLength ? userDataHeaderLength + 1 : 0);
    let paddingBits;
    let userDataLengthInSeptets;
    let userDataLengthInOctets;
    if (dcs == PDU_DCS_MSG_CODING_7BITS_ALPHABET) {
      let headerSeptets = Math.ceil(headerOctets * 8 / 7);
      userDataLengthInSeptets = headerSeptets + encodedBodyLength;
      userDataLengthInOctets = Math.ceil(userDataLengthInSeptets * 7 / 8);
      paddingBits = headerSeptets * 7 - headerOctets * 8;
    } else {
      userDataLengthInOctets = headerOctets + encodedBodyLength;
      paddingBits = 0;
    }

    let pduOctetLength = 4 + // PDU Type, Message Ref, address length + format
                         Math.ceil(address.length / 2) +
                         3 + // PID, DCS, UDL
                         userDataLengthInOctets;
    if (validity) {
      //TODO: add more to pduOctetLength
    }

    // Start the string. Since octets are represented in hex, we will need
    // twice as many characters as octets.
    Buf.writeUint32(pduOctetLength * 2);

    // - PDU-TYPE-

    // +--------+----------+---------+---------+--------+---------+
    // | RP (1) | UDHI (1) | SRR (1) | VPF (2) | RD (1) | MTI (2) |
    // +--------+----------+---------+---------+--------+---------+
    // RP:    0   Reply path parameter is not set
    //        1   Reply path parameter is set
    // UDHI:  0   The UD Field contains only the short message
    //        1   The beginning of the UD field contains a header in addition
    //            of the short message
    // SRR:   0   A status report is not requested
    //        1   A status report is requested
    // VPF:   bit4  bit3
    //        0     0     VP field is not present
    //        0     1     Reserved
    //        1     0     VP field present an integer represented (relative)
    //        1     1     VP field present a semi-octet represented (absolute)
    // RD:        Instruct the SMSC to accept(0) or reject(1) an SMS-SUBMIT
    //            for a short message still held in the SMSC which has the same
    //            MR and DA as a previously submitted short message from the
    //            same OA
    // MTI:   bit1  bit0    Message Type
    //        0     0       SMS-DELIVER (SMSC ==> MS)
    //        0     1       SMS-SUBMIT (MS ==> SMSC)

    // PDU type. MTI is set to SMS-SUBMIT
    let firstOctet = PDU_MTI_SMS_SUBMIT;

    // Status-Report-Request
    if (options.requestStatusReport) {
      firstOctet |= PDU_SRI_SRR;
    }

    // Validity period
    if (validity) {
      //TODO: not supported yet, OR with one of PDU_VPF_*
    }
    // User data header indicator
    if (headerOctets) {
      firstOctet |= PDU_UDHI;
    }
    this.writeHexOctet(firstOctet);

    // Message reference 00
    this.writeHexOctet(0x00);

    // - Destination Address -
    this.writeHexOctet(address.length);
    this.writeHexOctet(addressFormat);
    this.writeSwappedNibbleBCD(address);

    // - Protocol Identifier -
    this.writeHexOctet(0x00);

    // - Data coding scheme -
    // For now it assumes bits 7..4 = 1111 except for the 16 bits use case
    this.writeHexOctet(dcs);

    // - Validity Period -
    if (validity) {
      this.writeHexOctet(validity);
    }

    // - User Data -
    if (dcs == PDU_DCS_MSG_CODING_7BITS_ALPHABET) {
      this.writeHexOctet(userDataLengthInSeptets);
    } else {
      this.writeHexOctet(userDataLengthInOctets);
    }

    if (headerOctets) {
      this.writeUserDataHeader(options);
    }

    switch (dcs) {
      case PDU_DCS_MSG_CODING_7BITS_ALPHABET:
        this.writeStringAsSeptets(body, paddingBits, langIndex, langShiftIndex);
        break;
      case PDU_DCS_MSG_CODING_8BITS_ALPHABET:
        // Unsupported.
        break;
      case PDU_DCS_MSG_CODING_16BITS_ALPHABET:
        this.writeUCS2String(body);
        break;
    }

    // End of the string. The string length is always even by definition, so
    // we write two \0 delimiters.
    Buf.writeUint16(0);
    Buf.writeUint16(0);
  },

  /**
   * Read GSM CBS message serial number.
   *
   * @param msg
   *        message object for output.
   *
   * @see 3GPP TS 23.041 section 9.4.1.2.1
   */
  readCbSerialNumber: function readCbSerialNumber(msg) {
    msg.serial = Buf.readUint8() << 8 | Buf.readUint8();
    msg.geographicalScope = (msg.serial >>> 14) & 0x03;
    msg.messageCode = (msg.serial >>> 4) & 0x03FF;
    msg.updateNumber = msg.serial & 0x0F;
  },

  /**
   * Read GSM CBS message message identifier.
   *
   * @param msg
   *        message object for output.
   *
   * @see 3GPP TS 23.041 section 9.4.1.2.2
   */
  readCbMessageIdentifier: function readCbMessageIdentifier(msg) {
    msg.messageId = Buf.readUint8() << 8 | Buf.readUint8();

    if ((msg.format != CB_FORMAT_ETWS)
        && (msg.messageId >= CB_GSM_MESSAGEID_ETWS_BEGIN)
        && (msg.messageId <= CB_GSM_MESSAGEID_ETWS_END)) {
      // `In the case of transmitting CBS message for ETWS, a part of
      // Message Code can be used to command mobile terminals to activate
      // emergency user alert and message popup in order to alert the users.`
      msg.etws = {
        emergencyUserAlert: msg.messageCode & 0x0200 ? true : false,
        popup:              msg.messageCode & 0x0100 ? true : false
      };

      let warningType = msg.messageId - CB_GSM_MESSAGEID_ETWS_BEGIN;
      if (warningType < CB_ETWS_WARNING_TYPE_NAMES.length) {
        msg.etws.warningType = warningType;
      }
    }
  },

  /**
   * Read CBS Data Coding Scheme.
   *
   * @param msg
   *        message object for output.
   *
   * @see 3GPP TS 23.038 section 5.
   */
  readCbDataCodingScheme: function readCbDataCodingScheme(msg) {
    let dcs = Buf.readUint8();
    if (DEBUG) debug("PDU: read CBS dcs: " + dcs);

    let language = null, hasLanguageIndicator = false;
    // `Any reserved codings shall be assumed to be the GSM 7bit default
    // alphabet.`
    let encoding = PDU_DCS_MSG_CODING_7BITS_ALPHABET;
    let messageClass = PDU_DCS_MSG_CLASS_NORMAL;

    switch (dcs & PDU_DCS_CODING_GROUP_BITS) {
      case 0x00: // 0000
        language = CB_DCS_LANG_GROUP_1[dcs & 0x0F];
        break;

      case 0x10: // 0001
        switch (dcs & 0x0F) {
          case 0x00:
            hasLanguageIndicator = true;
            break;
          case 0x01:
            encoding = PDU_DCS_MSG_CODING_16BITS_ALPHABET;
            hasLanguageIndicator = true;
            break;
        }
        break;

      case 0x20: // 0010
        language = CB_DCS_LANG_GROUP_2[dcs & 0x0F];
        break;

      case 0x40: // 01xx
      case 0x50:
      //case 0x60: Text Compression, not supported
      //case 0x70: Text Compression, not supported
      case 0x90: // 1001
        encoding = (dcs & 0x0C);
        if (encoding == 0x0C) {
          encoding = PDU_DCS_MSG_CODING_7BITS_ALPHABET;
        }
        messageClass = (dcs & PDU_DCS_MSG_CLASS_BITS);
        break;

      case 0xF0:
        encoding = (dcs & 0x04) ? PDU_DCS_MSG_CODING_8BITS_ALPHABET
                                : PDU_DCS_MSG_CODING_7BITS_ALPHABET;
        switch(dcs & PDU_DCS_MSG_CLASS_BITS) {
          case 0x01: messageClass = PDU_DCS_MSG_CLASS_USER_1; break;
          case 0x02: messageClass = PDU_DCS_MSG_CLASS_USER_2; break;
          case 0x03: messageClass = PDU_DCS_MSG_CLASS_3; break;
        }
        break;

      case 0x30: // 0011 (Reserved)
      case 0x80: // 1000 (Reserved)
      case 0xA0: // 1010..1100 (Reserved)
      case 0xB0:
      case 0xC0:
        break;

      default:
        throw new Error("Unsupported CBS data coding scheme: " + dcs);
    }

    msg.dcs = dcs;
    msg.encoding = encoding;
    msg.language = language;
    msg.messageClass = GECKO_SMS_MESSAGE_CLASSES[messageClass];
    msg.hasLanguageIndicator = hasLanguageIndicator;
  },

  /**
   * Read GSM CBS message page parameter.
   *
   * @param msg
   *        message object for output.
   *
   * @see 3GPP TS 23.041 section 9.4.1.2.4
   */
  readCbPageParameter: function readCbPageParameter(msg) {
    let octet = Buf.readUint8();
    msg.pageIndex = (octet >>> 4) & 0x0F;
    msg.numPages = octet & 0x0F;
    if (!msg.pageIndex || !msg.numPages) {
      // `If a mobile receives the code 0000 in either the first field or the
      // second field then it shall treat the CBS message exactly the same as a
      // CBS message with page parameter 0001 0001 (i.e. a single page message).`
      msg.pageIndex = msg.numPages = 1;
    }
  },

  /**
   * Read ETWS Primary Notification message warning type.
   *
   * @param msg
   *        message object for output.
   *
   * @see 3GPP TS 23.041 section 9.3.24
   */
  readCbWarningType: function readCbWarningType(msg) {
    let word = Buf.readUint8() << 8 | Buf.readUint8();
    msg.etws = {
      warningType:        (word >>> 9) & 0x7F,
      popup:              word & 0x80 ? true : false,
      emergencyUserAlert: word & 0x100 ? true : false
    };
  },

  /**
   * Read CBS-Message-Information-Page
   *
   * @param msg
   *        message object for output.
   * @param length
   *        length of cell broadcast data to read in octets.
   *
   * @see 3GPP TS 23.041 section 9.3.19
   */
  readGsmCbData: function readGsmCbData(msg, length) {
    let bufAdapter = {
      readHexOctet: function readHexOctet() {
        return Buf.readUint8();
      }
    };

    msg.body = null;
    msg.data = null;
    switch (msg.encoding) {
      case PDU_DCS_MSG_CODING_7BITS_ALPHABET:
        msg.body = this.readSeptetsToString.call(bufAdapter,
                                                 (length * 8 / 7), 0,
                                                 PDU_NL_IDENTIFIER_DEFAULT,
                                                 PDU_NL_IDENTIFIER_DEFAULT);
        if (msg.hasLanguageIndicator) {
          msg.language = msg.body.substring(0, 2);
          msg.body = msg.body.substring(3);
        }
        break;

      case PDU_DCS_MSG_CODING_8BITS_ALPHABET:
        msg.data = Buf.readUint8Array(length);
        break;

      case PDU_DCS_MSG_CODING_16BITS_ALPHABET:
        if (msg.hasLanguageIndicator) {
          msg.language = this.readSeptetsToString.call(bufAdapter, 2, 0,
                                                       PDU_NL_IDENTIFIER_DEFAULT,
                                                       PDU_NL_IDENTIFIER_DEFAULT);
          length -= 2;
        }
        msg.body = this.readUCS2String.call(bufAdapter, length);
        break;
    }
  },

  /**
   * Read Cell GSM/ETWS/UMTS Broadcast Message.
   *
   * @param pduLength
   *        total length of the incoming PDU in octets.
   */
  readCbMessage: function readCbMessage(pduLength) {
    // Validity                                                   GSM ETWS UMTS
    let msg = {
      // Internally used in ril_worker:
      serial:               null,                              //  O   O    O
      updateNumber:         null,                              //  O   O    O
      format:               null,                              //  O   O    O
      dcs:                  0x0F,                              //  O   X    O
      encoding:             PDU_DCS_MSG_CODING_7BITS_ALPHABET, //  O   X    O
      hasLanguageIndicator: false,                             //  O   X    O
      data:                 null,                              //  O   X    O
      body:                 null,                              //  O   X    O
      pageIndex:            1,                                 //  O   X    X
      numPages:             1,                                 //  O   X    X

      // DOM attributes:
      geographicalScope:    null,                              //  O   O    O
      messageCode:          null,                              //  O   O    O
      messageId:            null,                              //  O   O    O
      language:             null,                              //  O   X    O
      fullBody:             null,                              //  O   X    O
      fullData:             null,                              //  O   X    O
      messageClass:         GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL], //  O   x    O
      etws:                 null                               //  ?   O    ?
      /*{
        warningType:        null,                              //  X   O    X
        popup:              false,                             //  X   O    X
        emergencyUserAlert: false,                             //  X   O    X
      }*/
    };

    if (pduLength <= CB_MESSAGE_SIZE_ETWS) {
      msg.format = CB_FORMAT_ETWS;
      return this.readEtwsCbMessage(msg);
    }

    if (pduLength <= CB_MESSAGE_SIZE_GSM) {
      msg.format = CB_FORMAT_GSM;
      return this.readGsmCbMessage(msg, pduLength);
    }

    return null;
  },

  /**
   * Read GSM Cell Broadcast Message.
   *
   * @param msg
   *        message object for output.
   * @param pduLength
   *        total length of the incomint PDU in octets.
   *
   * @see 3GPP TS 23.041 clause 9.4.1.2
   */
  readGsmCbMessage: function readGsmCbMessage(msg, pduLength) {
    this.readCbSerialNumber(msg);
    this.readCbMessageIdentifier(msg);
    this.readCbDataCodingScheme(msg);
    this.readCbPageParameter(msg);

    // GSM CB message header takes 6 octets.
    this.readGsmCbData(msg, pduLength - 6);

    return msg;
  },

  /**
   * Read ETWS Primary Notification Message.
   *
   * @param msg
   *        message object for output.
   *
   * @see 3GPP TS 23.041 clause 9.4.1.3
   */
  readEtwsCbMessage: function readEtwsCbMessage(msg) {
    this.readCbSerialNumber(msg);
    this.readCbMessageIdentifier(msg);
    this.readCbWarningType(msg);

    // Octet 7..56 is Warning Security Information. However, according to
    // section 9.4.1.3.6, `The UE shall ignore this parameter.` So we just skip
    // processing it here.

    return msg;
  },

  /**
   * Read network name.
   *
   * @param len Length of the information element.
   * @return
   *   {
   *     networkName: network name.
   *     shouldIncludeCi: Should Country's initials included in text string.
   *   }
   * @see TS 24.008 clause 10.5.3.5a.
   */
  readNetworkName: function readNetworkName(len) {
    // According to TS 24.008 Sec. 10.5.3.5a, the first octet is:
    // bit 8: must be 1.
    // bit 5-7: Text encoding.
    //          000 - GSM default alphabet.
    //          001 - UCS2 (16 bit).
    //          else - reserved.
    // bit 4: MS should add the letters for Country's Initials and a space
    //        to the text string if this bit is true.
    // bit 1-3: number of spare bits in last octet.

    let codingInfo = GsmPDUHelper.readHexOctet();
    if (!(codingInfo & 0x80)) {
      return null;
    }

    let textEncoding = (codingInfo & 0x70) >> 4,
        shouldIncludeCountryInitials = !!(codingInfo & 0x08),
        spareBits = codingInfo & 0x07;
    let resultString;

    switch (textEncoding) {
    case 0:
      // GSM Default alphabet.
      resultString = GsmPDUHelper.readSeptetsToString(
        ((len - 1) * 8 - spareBits) / 7, 0,
        PDU_NL_IDENTIFIER_DEFAULT,
        PDU_NL_IDENTIFIER_DEFAULT);
      break;
    case 1:
      // UCS2 encoded.
      resultString = this.readUCS2String(len - 1);
      break;
    default:
      // Not an available text coding.
      return null;
    }

    // TODO - Bug 820286: According to shouldIncludeCountryInitials, add
    // country initials to the resulting string.
    return resultString;
  }
};

let StkCommandParamsFactory = {
  createParam: function createParam(cmdDetails, ctlvs) {
    let param;
    switch (cmdDetails.typeOfCommand) {
      case STK_CMD_REFRESH:
        param = this.processRefresh(cmdDetails, ctlvs);
        break;
      case STK_CMD_POLL_INTERVAL:
        param = this.processPollInterval(cmdDetails, ctlvs);
        break;
      case STK_CMD_POLL_OFF:
        param = this.processPollOff(cmdDetails, ctlvs);
        break;
      case STK_CMD_PROVIDE_LOCAL_INFO:
        param = this.processProvideLocalInfo(cmdDetails, ctlvs);
        break;
      case STK_CMD_SET_UP_EVENT_LIST:
        param = this.processSetUpEventList(cmdDetails, ctlvs);
        break;
      case STK_CMD_SET_UP_MENU:
      case STK_CMD_SELECT_ITEM:
        param = this.processSelectItem(cmdDetails, ctlvs);
        break;
      case STK_CMD_DISPLAY_TEXT:
        param = this.processDisplayText(cmdDetails, ctlvs);
        break;
      case STK_CMD_SET_UP_IDLE_MODE_TEXT:
        param = this.processSetUpIdleModeText(cmdDetails, ctlvs);
        break;
      case STK_CMD_GET_INKEY:
        param = this.processGetInkey(cmdDetails, ctlvs);
        break;
      case STK_CMD_GET_INPUT:
        param = this.processGetInput(cmdDetails, ctlvs);
        break;
      case STK_CMD_SEND_SS:
      case STK_CMD_SEND_USSD:
      case STK_CMD_SEND_SMS:
      case STK_CMD_SEND_DTMF:
        param = this.processEventNotify(cmdDetails, ctlvs);
        break;
      case STK_CMD_SET_UP_CALL:
        param = this.processSetupCall(cmdDetails, ctlvs);
        break;
      case STK_CMD_LAUNCH_BROWSER:
        param = this.processLaunchBrowser(cmdDetails, ctlvs);
        break;
      case STK_CMD_PLAY_TONE:
        param = this.processPlayTone(cmdDetails, ctlvs);
        break;
      case STK_CMD_TIMER_MANAGEMENT:
        param = this.processTimerManagement(cmdDetails, ctlvs);
        break;
      default:
        debug("unknown proactive command");
        break;
    }
    return param;
  },

  /**
   * Construct a param for Refresh.
   *
   * @param cmdDetails
   *        The value object of CommandDetails TLV.
   * @param ctlvs
   *        The all TLVs in this proactive command.
   */
  processRefresh: function processRefresh(cmdDetails, ctlvs) {
    let refreshType = cmdDetails.commandQualifier;
    switch (refreshType) {
      case STK_REFRESH_FILE_CHANGE:
      case STK_REFRESH_NAA_INIT_AND_FILE_CHANGE:
        let ctlv = StkProactiveCmdHelper.searchForTag(
          COMPREHENSIONTLV_TAG_FILE_LIST, ctlvs);
        if (ctlv) {
          let list = ctlv.value.fileList;
          if (DEBUG) {
            debug("Refresh, list = " + list);
          }
          ICCRecordHelper.fetchICCRecords();
        }
        break;
    }
    return null;
  },

  /**
   * Construct a param for Poll Interval.
   *
   * @param cmdDetails
   *        The value object of CommandDetails TLV.
   * @param ctlvs
   *        The all TLVs in this proactive command.
   */
  processPollInterval: function processPollInterval(cmdDetails, ctlvs) {
    let ctlv = StkProactiveCmdHelper.searchForTag(
        COMPREHENSIONTLV_TAG_DURATION, ctlvs);
    if (!ctlv) {
      RIL.sendStkTerminalResponse({
        command: cmdDetails,
        resultCode: STK_RESULT_REQUIRED_VALUES_MISSING});
      throw new Error("Stk Poll Interval: Required value missing : Duration");
    }

    return ctlv.value;
  },

  /**
   * Construct a param for Poll Off.
   *
   * @param cmdDetails
   *        The value object of CommandDetails TLV.
   * @param ctlvs
   *        The all TLVs in this proactive command.
   */
  processPollOff: function processPollOff(cmdDetails, ctlvs) {
    return null;
  },

  /**
   * Construct a param for Set Up Event list.
   *
   * @param cmdDetails
   *        The value object of CommandDetails TLV.
   * @param ctlvs
   *        The all TLVs in this proactive command.
   */
  processSetUpEventList: function processSetUpEventList(cmdDetails, ctlvs) {
    let ctlv = StkProactiveCmdHelper.searchForTag(
        COMPREHENSIONTLV_TAG_EVENT_LIST, ctlvs);
    if (!ctlv) {
      RIL.sendStkTerminalResponse({
        command: cmdDetails,
        resultCode: STK_RESULT_REQUIRED_VALUES_MISSING});
      throw new Error("Stk Event List: Required value missing : Event List");
    }

    return ctlv.value || {eventList: null};
  },

  /**
   * Construct a param for Select Item.
   *
   * @param cmdDetails
   *        The value object of CommandDetails TLV.
   * @param ctlvs
   *        The all TLVs in this proactive command.
   */
  processSelectItem: function processSelectItem(cmdDetails, ctlvs) {
    let menu = {};

    let ctlv = StkProactiveCmdHelper.searchForTag(COMPREHENSIONTLV_TAG_ALPHA_ID, ctlvs);
    if (ctlv) {
      menu.title = ctlv.value.identifier;
    }

    menu.items = [];
    for (let i = 0; i < ctlvs.length; i++) {
      let ctlv = ctlvs[i];
      if (ctlv.tag == COMPREHENSIONTLV_TAG_ITEM) {
        menu.items.push(ctlv.value);
      }
    }

    if (menu.items.length == 0) {
      RIL.sendStkTerminalResponse({
        command: cmdDetails,
        resultCode: STK_RESULT_REQUIRED_VALUES_MISSING});
      throw new Error("Stk Menu: Required value missing : items");
    }

    ctlv = StkProactiveCmdHelper.searchForTag(COMPREHENSIONTLV_TAG_ITEM_ID, ctlvs);
    if (ctlv) {
      menu.defaultItem = ctlv.value.identifier - 1;
    }

    // The 1st bit and 2nd bit determines the presentation type.
    menu.presentationType = cmdDetails.commandQualifier & 0x03;

    // Help information available.
    if (cmdDetails.commandQualifier & 0x80) {
      menu.isHelpAvailable = true;
    }

    return menu;
  },

  processDisplayText: function processDisplayText(cmdDetails, ctlvs) {
    let textMsg = {};

    let ctlv = StkProactiveCmdHelper.searchForTag(COMPREHENSIONTLV_TAG_TEXT_STRING, ctlvs);
    if (!ctlv) {
      RIL.sendStkTerminalResponse({
        command: cmdDetails,
        resultCode: STK_RESULT_REQUIRED_VALUES_MISSING});
      throw new Error("Stk Display Text: Required value missing : Text String");
    }
    textMsg.text = ctlv.value.textString;

    ctlv = StkProactiveCmdHelper.searchForTag(COMPREHENSIONTLV_TAG_IMMEDIATE_RESPONSE, ctlvs);
    if (ctlv) {
      textMsg.responseNeeded = true;
    }

    // High priority.
    if (cmdDetails.commandQualifier & 0x01) {
      textMsg.isHighPriority = true;
    }

    // User clear.
    if (cmdDetails.commandQualifier & 0x80) {
      textMsg.userClear = true;
    }

    return textMsg;
  },

  processSetUpIdleModeText: function processSetUpIdleModeText(cmdDetails, ctlvs) {
    let textMsg = {};

    let ctlv = StkProactiveCmdHelper.searchForTag(COMPREHENSIONTLV_TAG_TEXT_STRING, ctlvs);
    if (!ctlv) {
      RIL.sendStkTerminalResponse({
        command: cmdDetails,
        resultCode: STK_RESULT_REQUIRED_VALUES_MISSING});
      throw new Error("Stk Set Up Idle Text: Required value missing : Text String");
    }
    textMsg.text = ctlv.value.textString;

    return textMsg;
  },

  processGetInkey: function processGetInkey(cmdDetails, ctlvs) {
    let input = {};

    let ctlv = StkProactiveCmdHelper.searchForTag(COMPREHENSIONTLV_TAG_TEXT_STRING, ctlvs);
    if (!ctlv) {
      RIL.sendStkTerminalResponse({
        command: cmdDetails,
        resultCode: STK_RESULT_REQUIRED_VALUES_MISSING});
      throw new Error("Stk Get InKey: Required value missing : Text String");
    }
    input.text = ctlv.value.textString;

    // duration
    ctlv = StkProactiveCmdHelper.searchForTag(
        COMPREHENSIONTLV_TAG_DURATION, ctlvs);
    if (ctlv) {
      input.duration = ctlv.value;
    }

    input.minLength = 1;
    input.maxLength = 1;

    // isAlphabet
    if (cmdDetails.commandQualifier & 0x01) {
      input.isAlphabet = true;
    }

    // UCS2
    if (cmdDetails.commandQualifier & 0x02) {
      input.isUCS2 = true;
    }

    // Character sets defined in bit 1 and bit 2 are disable and
    // the YES/NO reponse is required.
    if (cmdDetails.commandQualifier & 0x04) {
      input.isYesNoRequested = true;
    }

    // Help information available.
    if (cmdDetails.commandQualifier & 0x80) {
      input.isHelpAvailable = true;
    }

    return input;
  },

  processGetInput: function processGetInput(cmdDetails, ctlvs) {
    let input = {};

    let ctlv = StkProactiveCmdHelper.searchForTag(COMPREHENSIONTLV_TAG_TEXT_STRING, ctlvs);
    if (!ctlv) {
      RIL.sendStkTerminalResponse({
        command: cmdDetails,
        resultCode: STK_RESULT_REQUIRED_VALUES_MISSING});
      throw new Error("Stk Get Input: Required value missing : Text String");
    }
    input.text = ctlv.value.textString;

    ctlv = StkProactiveCmdHelper.searchForTag(COMPREHENSIONTLV_TAG_RESPONSE_LENGTH, ctlvs);
    if (ctlv) {
      input.minLength = ctlv.value.minLength;
      input.maxLength = ctlv.value.maxLength;
    }

    ctlv = StkProactiveCmdHelper.searchForTag(COMPREHENSIONTLV_TAG_DEFAULT_TEXT, ctlvs);
    if (ctlv) {
      input.defaultText = ctlv.value.textString;
    }

    // Alphabet only
    if (cmdDetails.commandQualifier & 0x01) {
      input.isAlphabet = true;
    }

    // UCS2
    if (cmdDetails.commandQualifier & 0x02) {
      input.isUCS2 = true;
    }

    // User input shall not be revealed
    if (cmdDetails.commandQualifier & 0x04) {
      input.hideInput = true;
    }

    // User input in SMS packed format
    if (cmdDetails.commandQualifier & 0x08) {
      input.isPacked = true;
    }

    // Help information available.
    if (cmdDetails.commandQualifier & 0x80) {
      input.isHelpAvailable = true;
    }

    return input;
  },

  processEventNotify: function processEventNotify(cmdDetails, ctlvs) {
    let textMsg = {};

    let ctlv = StkProactiveCmdHelper.searchForTag(COMPREHENSIONTLV_TAG_ALPHA_ID, ctlvs);
    if (!ctlv) {
      RIL.sendStkTerminalResponse({
        command: cmdDetails,
        resultCode: STK_RESULT_REQUIRED_VALUES_MISSING});
      throw new Error("Stk Event Notfiy: Required value missing : Alpha ID");
    }
    textMsg.text = ctlv.value.identifier;

    return textMsg;
  },

  processSetupCall: function processSetupCall(cmdDetails, ctlvs) {
    let call = {};

    for (let i = 0; i < ctlvs.length; i++) {
      let ctlv = ctlvs[i];
      if (ctlv.tag == COMPREHENSIONTLV_TAG_ALPHA_ID) {
        if (!call.confirmMessage) {
          call.confirmMessage = ctlv.value.identifier;
        } else {
          call.callMessage = ctlv.value.identifier;
          break;
        }
      }
    }

    let ctlv = StkProactiveCmdHelper.searchForTag(COMPREHENSIONTLV_TAG_ADDRESS, ctlvs);
    if (!ctlv) {
      RIL.sendStkTerminalResponse({
        command: cmdDetails,
        resultCode: STK_RESULT_REQUIRED_VALUES_MISSING});
      throw new Error("Stk Set Up Call: Required value missing : Adress");
    }
    call.address = ctlv.value.number;

    return call;
  },

  processLaunchBrowser: function processLaunchBrowser(cmdDetails, ctlvs) {
    let browser = {};

    let ctlv = StkProactiveCmdHelper.searchForTag(COMPREHENSIONTLV_TAG_URL, ctlvs);
    if (!ctlv) {
      RIL.sendStkTerminalResponse({
        command: cmdDetails,
        resultCode: STK_RESULT_REQUIRED_VALUES_MISSING});
      throw new Error("Stk Launch Browser: Required value missing : URL");
    }
    browser.url = ctlv.value.url;

    ctlv = StkProactiveCmdHelper.searchForTag(COMPREHENSIONTLV_TAG_ALPHA_ID, ctlvs)
    if (ctlv) {
      browser.confirmMessage = ctlv.value.identifier;
    }

    browser.mode = cmdDetails.commandQualifier & 0x03;

    return browser;
  },

  processPlayTone: function processPlayTone(cmdDetails, ctlvs) {
    let playTone = {};

    let ctlv = StkProactiveCmdHelper.searchForTag(
        COMPREHENSIONTLV_TAG_ALPHA_ID, ctlvs);
    if (ctlv) {
      playTone.text = ctlv.value.identifier;
    }

    ctlv = StkProactiveCmdHelper.searchForTag(COMPREHENSIONTLV_TAG_TONE, ctlvs);
    if (ctlv) {
      playTone.tone = ctlv.value.tone;
    }

    ctlv = StkProactiveCmdHelper.searchForTag(
        COMPREHENSIONTLV_TAG_DURATION, ctlvs);
    if (ctlv) {
      playTone.duration = ctlv.value;
    }

    // vibrate is only defined in TS 102.223
    playTone.isVibrate = (cmdDetails.commandQualifier & 0x01) != 0x00;

    return playTone;
  },

  /**
   * Construct a param for Provide Local Information
   *
   * @param cmdDetails
   *        The value object of CommandDetails TLV.
   * @param ctlvs
   *        The all TLVs in this proactive command.
   */
  processProvideLocalInfo: function processProvideLocalInfo(cmdDetails, ctlvs) {
    let provideLocalInfo = {
      localInfoType: cmdDetails.commandQualifier
    };
    return provideLocalInfo;
  },

  processTimerManagement: function processTimerManagement(cmdDetails, ctlvs) {
    let timer = {
      timerAction: cmdDetails.commandQualifier
    };

    let ctlv = StkProactiveCmdHelper.searchForTag(
        COMPREHENSIONTLV_TAG_TIMER_IDENTIFIER, ctlvs);
    if (ctlv) {
      timer.timerId = ctlv.value.timerId;
    }

    ctlv = StkProactiveCmdHelper.searchForTag(
        COMPREHENSIONTLV_TAG_TIMER_VALUE, ctlvs);
    if (ctlv) {
      timer.timerValue = ctlv.value.timerValue;
    }

    return timer;
  }
};

let StkProactiveCmdHelper = {
  retrieve: function retrieve(tag, length) {
    switch (tag) {
      case COMPREHENSIONTLV_TAG_COMMAND_DETAILS:
        return this.retrieveCommandDetails(length);
      case COMPREHENSIONTLV_TAG_DEVICE_ID:
        return this.retrieveDeviceId(length);
      case COMPREHENSIONTLV_TAG_ALPHA_ID:
        return this.retrieveAlphaId(length);
      case COMPREHENSIONTLV_TAG_DURATION:
        return this.retrieveDuration(length);
      case COMPREHENSIONTLV_TAG_ADDRESS:
        return this.retrieveAddress(length);
      case COMPREHENSIONTLV_TAG_TEXT_STRING:
        return this.retrieveTextString(length);
      case COMPREHENSIONTLV_TAG_TONE:
        return this.retrieveTone(length);
      case COMPREHENSIONTLV_TAG_ITEM:
        return this.retrieveItem(length);
      case COMPREHENSIONTLV_TAG_ITEM_ID:
        return this.retrieveItemId(length);
      case COMPREHENSIONTLV_TAG_RESPONSE_LENGTH:
        return this.retrieveResponseLength(length);
      case COMPREHENSIONTLV_TAG_FILE_LIST:
        return this.retrieveFileList(length);
      case COMPREHENSIONTLV_TAG_DEFAULT_TEXT:
        return this.retrieveDefaultText(length);
      case COMPREHENSIONTLV_TAG_EVENT_LIST:
        return this.retrieveEventList(length);
      case COMPREHENSIONTLV_TAG_TIMER_IDENTIFIER:
        return this.retrieveTimerId(length);
      case COMPREHENSIONTLV_TAG_TIMER_VALUE:
        return this.retrieveTimerValue(length);
      case COMPREHENSIONTLV_TAG_IMMEDIATE_RESPONSE:
        return this.retrieveImmediaResponse(length);
      case COMPREHENSIONTLV_TAG_URL:
        return this.retrieveUrl(length);
      default:
        debug("StkProactiveCmdHelper: unknown tag " + tag.toString(16));
        Buf.seekIncoming(length * PDU_HEX_OCTET_SIZE);
        return null;
    }
  },

  /**
   * Command Details.
   *
   * | Byte | Description         | Length |
   * |  1   | Command details Tag |   1    |
   * |  2   | Length = 03         |   1    |
   * |  3   | Command number      |   1    |
   * |  4   | Type of Command     |   1    |
   * |  5   | Command Qualifier   |   1    |
   */
  retrieveCommandDetails: function retrieveCommandDetails(length) {
    let cmdDetails = {
      commandNumber: GsmPDUHelper.readHexOctet(),
      typeOfCommand: GsmPDUHelper.readHexOctet(),
      commandQualifier: GsmPDUHelper.readHexOctet()
    };
    return cmdDetails;
  },

  /**
   * Device Identities.
   *
   * | Byte | Description            | Length |
   * |  1   | Device Identity Tag    |   1    |
   * |  2   | Length = 02            |   1    |
   * |  3   | Source device Identity |   1    |
   * |  4   | Destination device Id  |   1    |
   */
  retrieveDeviceId: function retrieveDeviceId(length) {
    let deviceId = {
      sourceId: GsmPDUHelper.readHexOctet(),
      destinationId: GsmPDUHelper.readHexOctet()
    };
    return deviceId;
  },

  /**
   * Alpha Identifier.
   *
   * | Byte         | Description            | Length |
   * |  1           | Alpha Identifier Tag   |   1    |
   * | 2 ~ (Y-1)+2  | Length (X)             |   Y    |
   * | (Y-1)+3 ~    | Alpha identfier        |   X    |
   * | (Y-1)+X+2    |                        |        |
   */
  retrieveAlphaId: function retrieveAlphaId(length) {
    let alphaId = {
      identifier: GsmPDUHelper.readAlphaIdentifier(length)
    };
    return alphaId;
  },

  /**
   * Duration.
   *
   * | Byte | Description           | Length |
   * |  1   | Response Length Tag   |   1    |
   * |  2   | Lenth = 02            |   1    |
   * |  3   | Time unit             |   1    |
   * |  4   | Time interval         |   1    |
   */
  retrieveDuration: function retrieveDuration(length) {
    let duration = {
      timeUnit: GsmPDUHelper.readHexOctet(),
      timeInterval: GsmPDUHelper.readHexOctet(),
    };
    return duration;
  },

  /**
   * Address.
   *
   * | Byte         | Description            | Length |
   * |  1           | Alpha Identifier Tag   |   1    |
   * | 2 ~ (Y-1)+2  | Length (X)             |   Y    |
   * | (Y-1)+3      | TON and NPI            |   1    |
   * | (Y-1)+4 ~    | Dialling number        |   X    |
   * | (Y-1)+X+2    |                        |        |
   */
  retrieveAddress: function retrieveAddress(length) {
    let address = {
      number : GsmPDUHelper.readDiallingNumber(length)
    };
    return address;
  },

  /**
   * Text String.
   *
   * | Byte         | Description        | Length |
   * |  1           | Text String Tag    |   1    |
   * | 2 ~ (Y-1)+2  | Length (X)         |   Y    |
   * | (Y-1)+3      | Data coding scheme |   1    |
   * | (Y-1)+4~     | Text String        |   X    |
   * | (Y-1)+X+2    |                    |        |
   */
  retrieveTextString: function retrieveTextString(length) {
    if (!length) {
      // null string.
      return {textString: null};
    }

    let text = {
      codingScheme: GsmPDUHelper.readHexOctet()
    };

    length--; // -1 for the codingScheme.
    switch (text.codingScheme & 0x0f) {
      case STK_TEXT_CODING_GSM_7BIT_PACKED:
        text.textString = GsmPDUHelper.readSeptetsToString(length * 8 / 7, 0, 0, 0);
        break;
      case STK_TEXT_CODING_GSM_8BIT:
        text.textString = GsmPDUHelper.read8BitUnpackedToString(length);
        break;
      case STK_TEXT_CODING_UCS2:
        text.textString = GsmPDUHelper.readUCS2String(length);
        break;
    }
    return text;
  },

  /**
   * Tone.
   *
   * | Byte | Description     | Length |
   * |  1   | Tone Tag        |   1    |
   * |  2   | Lenth = 01      |   1    |
   * |  3   | Tone            |   1    |
   */
  retrieveTone: function retrieveTone(length) {
    let tone = {
      tone: GsmPDUHelper.readHexOctet(),
    };
    return tone;
  },

  /**
   * Item.
   *
   * | Byte         | Description            | Length |
   * |  1           | Item Tag               |   1    |
   * | 2 ~ (Y-1)+2  | Length (X)             |   Y    |
   * | (Y-1)+3      | Identifier of item     |   1    |
   * | (Y-1)+4 ~    | Text string of item    |   X    |
   * | (Y-1)+X+2    |                        |        |
   */
  retrieveItem: function retrieveItem(length) {
    // TS 102.223 ,clause 6.6.7 SET-UP MENU
    // If the "Item data object for item 1" is a null data object
    // (i.e. length = '00' and no value part), this is an indication to the ME
    // to remove the existing menu from the menu system in the ME.
    if (!length) {
      return null;
    }
    let item = {
      identifier: GsmPDUHelper.readHexOctet(),
      text: GsmPDUHelper.readAlphaIdentifier(length - 1)
    };
    return item;
  },

  /**
   * Item Identifier.
   *
   * | Byte | Description               | Length |
   * |  1   | Item Identifier Tag       |   1    |
   * |  2   | Lenth = 01                |   1    |
   * |  3   | Identifier of Item chosen |   1    |
   */
  retrieveItemId: function retrieveItemId(length) {
    let itemId = {
      identifier: GsmPDUHelper.readHexOctet()
    };
    return itemId;
  },

  /**
   * Response Length.
   *
   * | Byte | Description                | Length |
   * |  1   | Response Length Tag        |   1    |
   * |  2   | Lenth = 02                 |   1    |
   * |  3   | Minimum length of response |   1    |
   * |  4   | Maximum length of response |   1    |
   */
  retrieveResponseLength: function retrieveResponseLength(length) {
    let rspLength = {
      minLength : GsmPDUHelper.readHexOctet(),
      maxLength : GsmPDUHelper.readHexOctet()
    };
    return rspLength;
  },

  /**
   * File List.
   *
   * | Byte         | Description            | Length |
   * |  1           | File List Tag          |   1    |
   * | 2 ~ (Y-1)+2  | Length (X)             |   Y    |
   * | (Y-1)+3      | Number of files        |   1    |
   * | (Y-1)+4 ~    | Files                  |   X    |
   * | (Y-1)+X+2    |                        |        |
   */
  retrieveFileList: function retrieveFileList(length) {
    let num = GsmPDUHelper.readHexOctet();
    let fileList = "";
    length--; // -1 for the num octet.
    for (let i = 0; i < 2 * length; i++) {
      // Didn't use readHexOctet here,
      // otherwise 0x00 will be "0", not "00"
      fileList += String.fromCharCode(Buf.readUint16());
    }
    return {
      fileList: fileList
    };
  },

  /**
   * Default Text.
   *
   * Same as Text String.
   */
  retrieveDefaultText: function retrieveDefaultText(length) {
    return this.retrieveTextString(length);
  },

  /**
   * Event List.
   */
  retrieveEventList: function retrieveEventList(length) {
    if (!length) {
      // null means an indication to ME to remove the existing list of events
      // in ME.
      return null;
    }

    let eventList = [];
    for (let i = 0; i < length; i++) {
      eventList.push(GsmPDUHelper.readHexOctet());
    }
    return {
      eventList: eventList
    };
  },

  /**
   * Timer Identifier.
   *
   * | Byte  | Description          | Length |
   * |  1    | Timer Identifier Tag |   1    |
   * |  2    | Length = 01          |   1    |
   * |  3    | Timer Identifier     |   1    |
   */
  retrieveTimerId: function retrieveTimerId(length) {
    let id = {
      timerId: GsmPDUHelper.readHexOctet()
    };
    return id;
  },

  /**
   * Timer Value.
   *
   * | Byte  | Description          | Length |
   * |  1    | Timer Value Tag      |   1    |
   * |  2    | Length = 03          |   1    |
   * |  3    | Hour                 |   1    |
   * |  4    | Minute               |   1    |
   * |  5    | Second               |   1    |
   */
  retrieveTimerValue: function retrieveTimerValue(length) {
    let value = {
      timerValue: (GsmPDUHelper.readSwappedNibbleBcdNum(1) * 60 * 60) +
                  (GsmPDUHelper.readSwappedNibbleBcdNum(1) * 60) +
                  (GsmPDUHelper.readSwappedNibbleBcdNum(1))
    };
    return value;
  },

  /**
   * Immediate Response.
   *
   * | Byte  | Description            | Length |
   * |  1    | Immediate Response Tag |   1    |
   * |  2    | Length = 00            |   1    |
   */
  retrieveImmediaResponse: function retrieveImmediaResponse(length) {
    return {};
  },

  /**
   * URL
   *
   * | Byte      | Description         | Length |
   * |  1        | URL Tag             |   1    |
   * | 2 ~ (Y+1) | Length(X)           |   Y    |
   * | (Y+2) ~   | URL                 |   X    |
   * | (Y+1+X)   |                     |        |
   */
  retrieveUrl: function retrieveUrl(length) {
    let s = "";
    for (let i = 0; i < length; i++) {
      s += String.fromCharCode(GsmPDUHelper.readHexOctet());
    }
    return {url: s};
  },

  searchForTag: function searchForTag(tag, ctlvs) {
    for (let i = 0; i < ctlvs.length; i++) {
      let ctlv = ctlvs[i];
      if ((ctlv.tag & ~COMPREHENSIONTLV_FLAG_CR) == tag) {
        return ctlv;
      }
    }
    return null;
  },
};

let ComprehensionTlvHelper = {
  /**
   * Decode raw data to a Comprehension-TLV.
   */
  decode: function decode() {
    let hlen = 0; // For header(tag field + length field) length.
    let temp = GsmPDUHelper.readHexOctet();
    hlen++;

    // TS 101.220, clause 7.1.1
    let tag, tagValue, cr;
    switch (temp) {
      // TS 101.220, clause 7.1.1
      case 0x0: // Not used.
      case 0xff: // Not used.
      case 0x80: // Reserved for future use.
        RIL.sendStkTerminalResponse({
          resultCode: STK_RESULT_CMD_DATA_NOT_UNDERSTOOD});
        throw new Error("Invalid octet when parsing Comprehension TLV :" + temp);
        break;
      case 0x7f: // Tag is three byte format.
        // TS 101.220 clause 7.1.1.2.
        // | Byte 1 | Byte 2                        | Byte 3 |
        // |        | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 |        |
        // | 0x7f   |CR | Tag Value                          |
        tag = (GsmPDUHelper.readHexOctet() << 8) | GsmPDUHelper.readHexOctet();
        hlen += 2;
        cr = (tag & 0x8000) != 0;
        tag &= ~0x8000;
        break;
      default: // Tag is single byte format.
        tag = temp;
        // TS 101.220 clause 7.1.1.1.
        // | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 |
        // |CR | Tag Value                 |
        cr = (tag & 0x80) != 0;
        tag &= ~0x80;
    }

    // TS 101.220 clause 7.1.2, Length Encoding.
    // Length   |  Byte 1  | Byte 2 | Byte 3 | Byte 4 |
    // 0 - 127  |  00 - 7f | N/A    | N/A    | N/A    |
    // 128-255  |  81      | 80 - ff| N/A    | N/A    |
    // 256-65535|  82      | 0100 - ffff     | N/A    |
    // 65536-   |  83      |     010000 - ffffff      |
    // 16777215
    //
    // Length errors: TS 11.14, clause 6.10.6

    let length; // Data length.
    temp = GsmPDUHelper.readHexOctet();
    hlen++;
    if (temp < 0x80) {
      length = temp;
    } else if (temp == 0x81) {
      length = GsmPDUHelper.readHexOctet();
      hlen++;
      if (length < 0x80) {
        RIL.sendStkTerminalResponse({
          resultCode: STK_RESULT_CMD_DATA_NOT_UNDERSTOOD});
        throw new Error("Invalid length in Comprehension TLV :" + length);
      }
    } else if (temp == 0x82) {
      length = (GsmPDUHelper.readHexOctet() << 8) | GsmPDUHelper.readHexOctet();
      hlen += 2;
      if (lenth < 0x0100) {
         RIL.sendStkTerminalResponse({
          resultCode: STK_RESULT_CMD_DATA_NOT_UNDERSTOOD});
        throw new Error("Invalid length in 3-byte Comprehension TLV :" + length);
      }
    } else if (temp == 0x83) {
      length = (GsmPDUHelper.readHexOctet() << 16) |
               (GsmPDUHelper.readHexOctet() << 8)  |
                GsmPDUHelper.readHexOctet();
      hlen += 3;
      if (length < 0x010000) {
        RIL.sendStkTerminalResponse({
          resultCode: STK_RESULT_CMD_DATA_NOT_UNDERSTOOD});
        throw new Error("Invalid length in 4-byte Comprehension TLV :" + length);
      }
    } else {
      RIL.sendStkTerminalResponse({
        resultCode: STK_RESULT_CMD_DATA_NOT_UNDERSTOOD});
      throw new Error("Invalid octet in Comprehension TLV :" + length);
    }

    let ctlv = {
      tag: tag,
      length: length,
      value: StkProactiveCmdHelper.retrieve(tag, length),
      cr: cr,
      hlen: hlen
    };
    return ctlv;
  },

  decodeChunks: function decodeChunks(length) {
    let chunks = [];
    let index = 0;
    while (index < length) {
      let tlv = this.decode();
      chunks.push(tlv);
      index += tlv.length;
      index += tlv.hlen;
    }
    return chunks;
  },

  /**
   * Write Location Info Comprehension TLV.
   *
   * @param loc location Information.
   */
  writeLocationInfoTlv: function writeLocationInfoTlv(loc) {
    GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_LOCATION_INFO |
                               COMPREHENSIONTLV_FLAG_CR);
    GsmPDUHelper.writeHexOctet(loc.gsmCellId > 0xffff ? 9 : 7);
    // From TS 11.14, clause 12.19
    // "The mobile country code (MCC), the mobile network code (MNC),
    // the location area code (LAC) and the cell ID are
    // coded as in TS 04.08."
    // And from TS 04.08 and TS 24.008,
    // the format is as follows:
    //
    // MCC = MCC_digit_1 + MCC_digit_2 + MCC_digit_3
    //
    //   8  7  6  5    4  3  2  1
    // +-------------+-------------+
    // | MCC digit 2 | MCC digit 1 | octet 2
    // | MNC digit 3 | MCC digit 3 | octet 3
    // | MNC digit 2 | MNC digit 1 | octet 4
    // +-------------+-------------+
    //
    // Also in TS 24.008
    // "However a network operator may decide to
    // use only two digits in the MNC in the LAI over the
    // radio interface. In this case, bits 5 to 8 of octet 3
    // shall be coded as '1111'".

    // MCC & MNC, 3 octets
    let mcc = loc.mcc.toString();
    let mnc = loc.mnc.toString();
    if (mnc.length == 1) {
      mnc = "F0" + mnc;
    } else if (mnc.length == 2) {
      mnc = "F" + mnc;
    } else {
      mnc = mnc[2] + mnc[0] + mnc[1];
    }
    GsmPDUHelper.writeSwappedNibbleBCD(mcc + mnc);

    // LAC, 2 octets
    GsmPDUHelper.writeHexOctet((loc.gsmLocationAreaCode >> 8) & 0xff);
    GsmPDUHelper.writeHexOctet(loc.gsmLocationAreaCode & 0xff);

    // Cell Id
    if (loc.gsmCellId > 0xffff) {
      // UMTS/WCDMA, gsmCellId is 28 bits.
      GsmPDUHelper.writeHexOctet((loc.gsmCellId >> 24) & 0xff);
      GsmPDUHelper.writeHexOctet((loc.gsmCellId >> 16) & 0xff);
      GsmPDUHelper.writeHexOctet((loc.gsmCellId >> 8) & 0xff);
      GsmPDUHelper.writeHexOctet(loc.gsmCellId & 0xff);
    } else {
      // GSM, gsmCellId is 16 bits.
      GsmPDUHelper.writeHexOctet((loc.gsmCellId >> 8) & 0xff);
      GsmPDUHelper.writeHexOctet(loc.gsmCellId & 0xff);
    }
  },

  /**
   * Given a geckoError string, this function translates it into cause value
   * and write the value into buffer.
   *
   * @param geckoError Error string that is passed to gecko.
   */
  writeCauseTlv: function writeCauseTlv(geckoError) {
    let cause = -1;
    for (let errorNo in RIL_ERROR_TO_GECKO_ERROR) {
      if (geckoError == RIL_ERROR_TO_GECKO_ERROR[errorNo]) {
        cause = errorNo;
        break;
      }
    }
    cause = (cause == -1) ? ERROR_SUCCESS : cause;

    GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_CAUSE |
                               COMPREHENSIONTLV_FLAG_CR);
    GsmPDUHelper.writeHexOctet(2);  // For single cause value.

    // TS 04.08, clause 10.5.4.11: National standard code + user location.
    GsmPDUHelper.writeHexOctet(0x60);

    // TS 04.08, clause 10.5.4.11: ext bit = 1 + 7 bits for cause.
    // +-----------------+----------------------------------+
    // | Ext = 1 (1 bit) |          Cause (7 bits)          |
    // +-----------------+----------------------------------+
    GsmPDUHelper.writeHexOctet(0x80 | cause);
  },

  writeDateTimeZoneTlv: function writeDataTimeZoneTlv(date) {
    GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_DATE_TIME_ZONE);
    GsmPDUHelper.writeHexOctet(7);
    GsmPDUHelper.writeTimestamp(date);
  },

  writeLanguageTlv: function writeLanguageTlv(language) {
    GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_LANGUAGE);
    GsmPDUHelper.writeHexOctet(2);

    // ISO 639-1, Alpha-2 code
    // TS 123.038, clause 6.2.1, GSM 7 bit Default Alphabet
    GsmPDUHelper.writeHexOctet(
      PDU_NL_LOCKING_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT].indexOf(language[0]));
    GsmPDUHelper.writeHexOctet(
      PDU_NL_LOCKING_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT].indexOf(language[1]));
  },

  /**
   * Write Timer Value Comprehension TLV.
   *
   * @param seconds length of time during of the timer.
   * @param cr Comprehension Required or not
   */
  writeTimerValueTlv: function writeTimerValueTlv(seconds, cr) {
    GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_TIMER_VALUE |
                               (cr ? COMPREHENSIONTLV_FLAG_CR : 0));
    GsmPDUHelper.writeHexOctet(3);

    // TS 102.223, clause 8.38
    // +----------------+------------------+-------------------+
    // | hours (1 byte) | minutes (1 btye) | secounds (1 byte) |
    // +----------------+------------------+-------------------+
    GsmPDUHelper.writeSwappedNibbleBCDNum(Math.floor(seconds / 60 / 60));
    GsmPDUHelper.writeSwappedNibbleBCDNum(Math.floor(seconds / 60) % 60);
    GsmPDUHelper.writeSwappedNibbleBCDNum(seconds % 60);
  },

  getSizeOfLengthOctets: function getSizeOfLengthOctets(length) {
    if (length >= 0x10000) {
      return 4; // 0x83, len_1, len_2, len_3
    } else if (length >= 0x100) {
      return 3; // 0x82, len_1, len_2
    } else if (length >= 0x80) {
      return 2; // 0x81, len
    } else {
      return 1; // len
    }
  },

  writeLength: function writeLength(length) {
    // TS 101.220 clause 7.1.2, Length Encoding.
    // Length   |  Byte 1  | Byte 2 | Byte 3 | Byte 4 |
    // 0 - 127  |  00 - 7f | N/A    | N/A    | N/A    |
    // 128-255  |  81      | 80 - ff| N/A    | N/A    |
    // 256-65535|  82      | 0100 - ffff     | N/A    |
    // 65536-   |  83      |     010000 - ffffff      |
    // 16777215
    if (length < 0x80) {
      GsmPDUHelper.writeHexOctet(length);
    } else if (0x80 <= length && length < 0x100) {
      GsmPDUHelper.writeHexOctet(0x81);
      GsmPDUHelper.writeHexOctet(length);
    } else if (0x100 <= length && length < 0x10000) {
      GsmPDUHelper.writeHexOctet(0x82);
      GsmPDUHelper.writeHexOctet((length >> 8) & 0xff);
      GsmPDUHelper.writeHexOctet(length & 0xff);
    } else if (0x10000 <= length && length < 0x1000000) {
      GsmPDUHelper.writeHexOctet(0x83);
      GsmPDUHelper.writeHexOctet((length >> 16) & 0xff);
      GsmPDUHelper.writeHexOctet((length >> 8) & 0xff);
      GsmPDUHelper.writeHexOctet(length & 0xff);
    } else {
      throw new Error("Invalid length value :" + length);
    }
  },
};

let BerTlvHelper = {
  /**
   * Decode Ber TLV.
   *
   * @param dataLen
   *        The length of data in bytes.
   */
  decode: function decode(dataLen) {
    // See TS 11.14, Annex D for BerTlv.
    let hlen = 0;
    let tag = GsmPDUHelper.readHexOctet();
    hlen++;

    // Length    | Byte 1                | Byte 2
    // 0 - 127   | length ('00' to '7f') | N/A
    // 128 - 255 | '81'                  | length ('80' to 'ff')
    let length;
    if (tag == BER_PROACTIVE_COMMAND_TAG) {
      let temp = GsmPDUHelper.readHexOctet();
      hlen++;
      if (temp < 0x80) {
        length = temp;
      } else if(temp == 0x81) {
        length = GsmPDUHelper.readHexOctet();
        if (length < 0x80) {
          RIL.sendStkTerminalResponse({
            resultCode: STK_RESULT_CMD_DATA_NOT_UNDERSTOOD});
          throw new Error("Invalid length " + length);
        }
      } else {
        RIL.sendStkTerminalResponse({
          resultCode: STK_RESULT_CMD_DATA_NOT_UNDERSTOOD});
        throw new Error("Invalid length octet " + temp);
      }
    } else {
      RIL.sendStkTerminalResponse({
        resultCode: STK_RESULT_CMD_DATA_NOT_UNDERSTOOD});
      throw new Error("Unknown BER tag");
    }

    // If the value length of the BerTlv is larger than remaining value on Parcel.
    if (dataLen - hlen < length) {
      RIL.sendStkTerminalResponse({
        resultCode: STK_RESULT_CMD_DATA_NOT_UNDERSTOOD});
      throw new Error("BerTlvHelper value length too long!!");
      return;
    }

    let ctlvs = ComprehensionTlvHelper.decodeChunks(length);
    let berTlv = {
      tag: tag,
      length: length,
      value: ctlvs
    };
    return berTlv;
  }
};

/**
 * ICC Helper for getting EF path.
 */
let ICCFileHelper = {
  /**
   * This function handles only EFs that are common to RUIM, SIM, USIM
   * and other types of ICC cards.
   */
  getCommonEFPath: function getCommonEFPath(fileId) {
    switch (fileId) {
      case ICC_EF_ICCID:
        return EF_PATH_MF_SIM;
      case ICC_EF_ADN:
        return EF_PATH_MF_SIM + EF_PATH_DF_TELECOM;
      case ICC_EF_PBR:
        return EF_PATH_MF_SIM + EF_PATH_DF_TELECOM + EF_PATH_DF_PHONEBOOK;
    }
    return null;
  },

  /**
   * This function handles EFs for SIM.
   */
  getSimEFPath: function getSimEFPath(fileId) {
    switch (fileId) {
      case ICC_EF_FDN:
      case ICC_EF_MSISDN:
        return EF_PATH_MF_SIM + EF_PATH_DF_TELECOM;
      case ICC_EF_AD:
      case ICC_EF_MBDN:
      case ICC_EF_PLMNsel:
      case ICC_EF_SPN:
      case ICC_EF_SPDI:
      case ICC_EF_SST:
      case ICC_EF_PHASE:
      case ICC_EF_CBMI:
      case ICC_EF_CBMIR:
      case ICC_EF_OPL:
      case ICC_EF_PNN:
        return EF_PATH_MF_SIM + EF_PATH_DF_GSM;
      default:
        return null
    }
  },

  /**
   * This function handles EFs for USIM.
   */
  getUSimEFPath: function getUSimEFPath(fileId) {
    switch (fileId) {
      case ICC_EF_AD:
      case ICC_EF_FDN:
      case ICC_EF_MBDN:
      case ICC_EF_UST:
      case ICC_EF_MSISDN:
      case ICC_EF_SPN:
      case ICC_EF_SPDI:
      case ICC_EF_CBMI:
      case ICC_EF_CBMIR:
      case ICC_EF_OPL:
      case ICC_EF_PNN:
        return EF_PATH_MF_SIM + EF_PATH_ADF_USIM;
      default:
        // The file ids in USIM phone book entries are decided by the
        // card manufacturer. So if we don't match any of the cases
        // above and if its a USIM return the phone book path.
        return EF_PATH_MF_SIM + EF_PATH_DF_TELECOM + EF_PATH_DF_PHONEBOOK;
    }
  },

  /**
   * Helper function for getting the pathId for the specific ICC record
   * depeding on which type of ICC card we are using.
   *
   * @param fileId
   *        File id.
   * @return The pathId or null in case of an error or invalid input.
   */
  getEFPath: function getEFPath(fileId) {
    // TODO: Bug 726098, change to use cdmaSubscriptionAppIndex when in CDMA.
    let index = RIL.iccStatus.gsmUmtsSubscriptionAppIndex;
    if (index == -1) {
      return null;
    }
    let app = RIL.iccStatus.apps[index];
    if (!app) {
      return null;
    }

    let path = this.getCommonEFPath(fileId);
    if (path) {
      return path;
    }

    switch (app.app_type) {
      case CARD_APPTYPE_SIM:
        return this.getSimEFPath(fileId);
      case CARD_APPTYPE_USIM:
        return this.getUSimEFPath(fileId);
      default:
        return null;
    }
  },
};

/**
 * Helper for ICC IO functionalities.
 */
let ICCIOHelper = {
  /**
   * Load EF with type 'Linear Fixed'.
   *
   * @param fileId
   *        The file to operate on, one of the ICC_EF_* constants.
   * @param recordNumber [optional]
   *        The number of the record shall be loaded.
   * @param callback [optional]
   *        The callback function shall be called when the record(s) is read.
   * @param this [optional]
   *        The 'this' object when the callback is called.
   * @param onerror [optional]
   *        The callback function shall be called when failure.
   */
  loadLinearFixedEF: function loadLinearFixedEF(options) {
    options.type = EF_TYPE_LINEAR_FIXED;
    let cb = options.callback;
    options.callback = function callback(options) {
      options.callback = cb;
      options.command = ICC_COMMAND_READ_RECORD;
      options.p1 = options.recordNumber || 1; // Record number
      options.p2 = READ_RECORD_ABSOLUTE_MODE;
      options.p3 = options.recordSize;
      RIL.iccIO(options);
    }.bind(this);
    this.getResponse(options);
  },

  /**
   * Load next record from current record Id.
   */
  loadNextRecord: function loadNextRecord(options) {
    options.p1++;
    RIL.iccIO(options);
  },

  /**
   * Update EF with type 'Linear Fixed'.
   *
   * @param fileId
   *        The file to operate on, one of the ICC_EF_* constants.
   * @param recordNumber
   *        The number of the record shall be updated.
   * @param dataWriter [optional]
   *        The function for writing string parameter for the ICC_COMMAND_UPDATE_RECORD.
   * @param callback [optional]
   *        The callback function shall be called when the record is updated.
   * @param onerror [optional]
   *        The callback function shall be called when failure.
   */
  updateLinearFixedEF: function updateLinearFixedEF(options) {
    if (!options.fileId || !options.recordNumber) {
      throw new Error("Unexpected fileId " + options.fileId +
                      " or recordNumber " + options.recordNumber);
    }

    options.type = EF_TYPE_LINEAR_FIXED;
    let cb = options.callback;
    options.callback = function callback(options) {
      options.callback = cb;
      options.command = ICC_COMMAND_UPDATE_RECORD;
      options.p1 = options.recordNumber;
      options.p2 = READ_RECORD_ABSOLUTE_MODE;
      options.p3 = options.recordSize;
      RIL.iccIO(options);
    }.bind(this);
    this.getResponse(options);
  },

  /**
   * Load EF with type 'Transparent'.
   *
   * @param fileId
   *        The file to operate on, one of the ICC_EF_* constants.
   * @param callback [optional]
   *        The callback function shall be called when the record(s) is read.
   * @param onerror [optional]
   *        The callback function shall be called when failure.
   */
  loadTransparentEF: function loadTransparentEF(options) {
    options.type = EF_TYPE_TRANSPARENT;
    let cb = options.callback;
    options.callback = function callback(options) {
      options.callback = cb;
      options.command = ICC_COMMAND_READ_BINARY;
      options.p3 = options.fileSize;
      RIL.iccIO(options);
    }.bind(this);
    this.getResponse(options);
  },

  /**
   * Use ICC_COMMAND_GET_RESPONSE to query the EF.
   *
   * @param fileId
   *        The file to operate on, one of the ICC_EF_* constants.
   */
  getResponse: function getResponse(options) {
    options.command = ICC_COMMAND_GET_RESPONSE;
    options.pathId = ICCFileHelper.getEFPath(options.fileId);
    if (!options.pathId) {
      throw new Error("Unknown pathId for " + options.fileId.toString(16));
    }
    options.p1 = 0; // For GET_RESPONSE, p1 = 0
    options.p2 = 0; // For GET_RESPONSE, p2 = 0
    options.p3 = GET_RESPONSE_EF_SIZE_BYTES;
    RIL.iccIO(options);
  },

  /**
   * Process ICC I/O response.
   */
  processICCIO: function processICCIO(options) {
    let func = this[options.command];
    func.call(this, options);
  },

  /**
   * Process a ICC_COMMAND_GET_RESPONSE type command for REQUEST_SIM_IO.
   */
  processICCIOGetResponse: function processICCIOGetResponse(options) {
    let length = Buf.readUint32();

    // The format is from TS 51.011, clause 9.2.1

    // Skip RFU, data[0] data[1]
    Buf.seekIncoming(2 * PDU_HEX_OCTET_SIZE);

    // File size, data[2], data[3]
    options.fileSize = (GsmPDUHelper.readHexOctet() << 8) |
                        GsmPDUHelper.readHexOctet();

    // 2 bytes File id. data[4], data[5]
    let fileId = (GsmPDUHelper.readHexOctet() << 8) |
                  GsmPDUHelper.readHexOctet();
    if (fileId != options.fileId) {
      throw new Error("Expected file ID " + options.fileId.toString(16) +
                      " but read " + fileId.toString(16));
    }

    // Type of file, data[6]
    let fileType = GsmPDUHelper.readHexOctet();
    if (fileType != TYPE_EF) {
      throw new Error("Unexpected file type " + fileType);
    }

    // Skip 1 byte RFU, data[7],
    //      3 bytes Access conditions, data[8] data[9] data[10],
    //      1 byte File status, data[11],
    //      1 byte Length of the following data, data[12].
    Buf.seekIncoming(((RESPONSE_DATA_STRUCTURE - RESPONSE_DATA_FILE_TYPE - 1) *
        PDU_HEX_OCTET_SIZE));

    // Read Structure of EF, data[13]
    let efType = GsmPDUHelper.readHexOctet();
    if (efType != options.type) {
      throw new Error("Expected EF type " + options.type + " but read " + efType);
    }

    // Length of a record, data[14].
    // Only available for LINEAR_FIXED and CYCLIC.
    if (efType == EF_TYPE_LINEAR_FIXED || efType == EF_TYPE_CYCLIC) {
      options.recordSize = GsmPDUHelper.readHexOctet();
      options.totalRecords = options.fileSize / options.recordSize;
    } else {
      Buf.seekIncoming(1 * PDU_HEX_OCTET_SIZE);
    }

    Buf.readStringDelimiter(length);

    if (options.callback) {
      options.callback(options);
    }
  },

  /**
   * Process a ICC_COMMAND_READ_RECORD type command for REQUEST_SIM_IO.
   */
  processICCIOReadRecord: function processICCIOReadRecord(options) {
    if (options.callback) {
      options.callback(options);
    }
  },

  /**
   * Process a ICC_COMMAND_READ_BINARY type command for REQUEST_SIM_IO.
   */
  processICCIOReadBinary: function processICCIOReadBinary(options) {
    if (options.callback) {
      options.callback(options);
    }
  },

  /**
   * Process a ICC_COMMAND_UPDATE_RECORD type command for REQUEST_SIM_IO.
   */
  processICCIOUpdateRecord: function processICCIOUpdateRecord(options) {
    if (options.callback) {
      options.callback(options);
    }
  },

  /**
   * Process ICC IO error.
   */
  processICCIOError: function processICCIOError(options) {
    let error = options.onerror || debug;

    // See GSM11.11, TS 51.011 clause 9.4, and ISO 7816-4 for the error
    // description.
    let errorMsg = "ICC I/O Error code " +
                   RIL_ERROR_TO_GECKO_ERROR[options.rilRequestError] +
                   " EF id = " + options.fileId.toString(16) +
                   " command = " + options.command.toString(16);
    if (options.sw1 && options.sw2) {
      errorMsg += "(" + options.sw1.toString(16) +
                  "/" + options.sw2.toString(16) + ")";
    }
    error(errorMsg);
  },
};
ICCIOHelper[ICC_COMMAND_SEEK] = null;
ICCIOHelper[ICC_COMMAND_READ_BINARY] = function ICC_COMMAND_READ_BINARY(options) {
  this.processICCIOReadBinary(options);
};
ICCIOHelper[ICC_COMMAND_READ_RECORD] = function ICC_COMMAND_READ_RECORD(options) {
  this.processICCIOReadRecord(options);
};
ICCIOHelper[ICC_COMMAND_GET_RESPONSE] = function ICC_COMMAND_GET_RESPONSE(options) {
  this.processICCIOGetResponse(options);
};
ICCIOHelper[ICC_COMMAND_UPDATE_BINARY] = null;
ICCIOHelper[ICC_COMMAND_UPDATE_RECORD] = function ICC_COMMAND_UPDATE_RECORD(options) {
  this.processICCIOUpdateRecord(options);
};

/**
 * Helper for ICC records.
 */
let ICCRecordHelper = {
  /**
   * Fetch ICC records.
   */
  fetchICCRecords: function fetchICCRecords() {
    this.getICCID();
    RIL.getIMSI();
    this.getMSISDN();
    this.getAD();
    this.getSST();
    this.getMBDN();
  },

  /**
   * Get EF_phase.
   * This EF is only available in SIM.
   */
  getICCPhase: function getICCPhase() {
    function callback() {
      let length = Buf.readUint32();

      let phase = GsmPDUHelper.readHexOctet();
      // If EF_phase is coded '03' or greater, an ME supporting STK shall
      // perform the PROFILE DOWNLOAD procedure.
      if (phase >= ICC_PHASE_2_PROFILE_DOWNLOAD_REQUIRED) {
        RIL.sendStkTerminalProfile(STK_SUPPORTED_TERMINAL_PROFILE);
      }

      Buf.readStringDelimiter(length);
    }

    ICCIOHelper.loadTransparentEF({fileId: ICC_EF_PHASE,
                                   callback: callback.bind(this)});
  },

  /**
   * Read the ICCID.
   */
  getICCID: function getICCID() {
    function callback() {
      let length = Buf.readUint32();
      RIL.iccInfo.iccid = GsmPDUHelper.readSwappedNibbleBcdString(length / 2);
      Buf.readStringDelimiter(length);

      if (DEBUG) debug("ICCID: " + RIL.iccInfo.iccid);
      if (RIL.iccInfo.iccid) {
        ICCUtilsHelper.handleICCInfoChange();
      }
    }

    ICCIOHelper.loadTransparentEF({fileId: ICC_EF_ICCID,
                                   callback: callback.bind(this)});
  },

  /**
   * Read the MSISDN from the ICC.
   */
  getMSISDN: function getMSISDN() {
    function callback(options) {
      let contact = GsmPDUHelper.readAlphaIdDiallingNumber(options.recordSize);
      if (!contact || RIL.iccInfo.msisdn === contact.number) {
        return;
      }
      RIL.iccInfo.msisdn = contact.number;
      if (DEBUG) debug("MSISDN: " + RIL.iccInfo.msisdn);
      ICCUtilsHelper.handleICCInfoChange();
    }

    ICCIOHelper.loadLinearFixedEF({fileId: ICC_EF_MSISDN,
                                   callback: callback.bind(this)});
  },

  /**
   * Read the AD (Administrative Data) from the ICC.
   */
  getAD: function getAD() {
    function callback() {
      let length = Buf.readUint32();
      // Each octet is encoded into two chars.
      let len = length / 2;
      let ad = GsmPDUHelper.readHexOctetArray(len);
      Buf.readStringDelimiter(length);

      if (DEBUG) {
        let str = "";
        for (let i = 0; i < ad.length; i++) {
          str += ad[i] + ", ";
        }
        debug("AD: " + str);
      }

      let imsi = RIL.iccInfoPrivate.imsi;
      if (imsi) {
        // MCC is the first 3 digits of IMSI.
        RIL.iccInfo.mcc = parseInt(imsi.substr(0,3));
        // The 4th byte of the response is the length of MNC.
        RIL.iccInfo.mnc = parseInt(imsi.substr(3, ad[3]));
        if (DEBUG) debug("MCC: " + RIL.iccInfo.mcc + " MNC: " + RIL.iccInfo.mnc);
        ICCUtilsHelper.handleICCInfoChange();
      }
    }

    ICCIOHelper.loadTransparentEF({fileId: ICC_EF_AD,
                                   callback: callback.bind(this)});
  },

  /**
   * Read the SPN (Service Provider Name) from the ICC.
   */
  getSPN: function getSPN() {
    function callback() {
      let length = Buf.readUint32();
      // Each octet is encoded into two chars.
      // Minus 1 because the first octet is used to store display condition.
      let len = (length / 2) - 1;
      let spnDisplayCondition = GsmPDUHelper.readHexOctet();
      let spn = GsmPDUHelper.readAlphaIdentifier(len);
      Buf.readStringDelimiter(length);

      if (DEBUG) {
        debug("SPN: spn = " + spn +
              ", spnDisplayCondition = " + spnDisplayCondition);
      }

      RIL.iccInfoPrivate.SPN = {
        spn : spn,
        spnDisplayCondition : spnDisplayCondition,
      };
      RIL.iccInfo.spn = spn;
      ICCUtilsHelper.updateDisplayCondition();
      ICCUtilsHelper.handleICCInfoChange();
    }

    ICCIOHelper.loadTransparentEF({fileId: ICC_EF_SPN,
                                   callback: callback.bind(this)});
  },

  /**
   * Read the (U)SIM Service Table from the ICC.
   */
  getSST: function getSST() {
    function callback() {
      let length = Buf.readUint32();
      // Each octet is encoded into two chars.
      let len = length / 2;
      let sst = GsmPDUHelper.readHexOctetArray(len);
      Buf.readStringDelimiter(length);
      RIL.iccInfoPrivate.sst = sst;
      if (DEBUG) {
        let str = "";
        for (let i = 0; i < sst.length; i++) {
          str += sst[i] + ", ";
        }
        debug("SST: " + str);
      }

      // Fetch SPN and PLMN list, if some of them are available.
      if (ICCUtilsHelper.isICCServiceAvailable("SPN")) {
        if (DEBUG) debug("SPN: SPN is available");
        this.getSPN();
      } else {
        if (DEBUG) debug("SPN: SPN service is not available");
      }

      if (ICCUtilsHelper.isICCServiceAvailable("SPDI")) {
        if (DEBUG) debug("SPDI: SPDI available.");
        this.getSPDI();
      } else {
        if (DEBUG) debug("SPDI: SPDI service is not available");
      }

      if (ICCUtilsHelper.isICCServiceAvailable("PNN")) {
        if (DEBUG) debug("PNN: PNN is available");
        this.getPNN();
      } else {
        if (DEBUG) debug("PNN: PNN is not available");
      }

      if (ICCUtilsHelper.isICCServiceAvailable("OPL")) {
        if (DEBUG) debug("OPL: OPL is available");
        this.getOPL();
      } else {
        if (DEBUG) debug("OPL: OPL is not available");
      }

      if (ICCUtilsHelper.isICCServiceAvailable("CBMI")) {
        this.getCBMI();
      } else {
        RIL.cellBroadcastConfigs.CBMI = null;
      }
      if (ICCUtilsHelper.isICCServiceAvailable("CBMIR")) {
        this.getCBMIR();
      } else {
        RIL.cellBroadcastConfigs.CBMIR = null;
      }
      RIL._mergeAllCellBroadcastConfigs();
    }

    // ICC_EF_UST has the same value with ICC_EF_SST.
    ICCIOHelper.loadTransparentEF({fileId: ICC_EF_SST,
                                   callback: callback.bind(this)});
  },

  /**
   * Read ICC FDN.
   *
   * @param onsuccess   Callback to be called when success.
   * @param onerror     Callback to be called when error.
   */
  readFDN: function readFDN(onsuccess, onerror) {
    function callback(options) {
      let contact = GsmPDUHelper.readAlphaIdDiallingNumber(options.recordSize);
      if (contact) {
        contacts.push(contact);
      }

      if (options.p1 < options.totalRecords) {
        ICCIOHelper.loadNextRecord(options);
      } else {
        if (onsuccess) {
          onsuccess(contacts);
        }
      }
    }

    let contacts = [];
    ICCIOHelper.loadLinearFixedEF({fileId: ICC_EF_FDN,
                                   callback: callback.bind(this),
                                   onerror: onerror});
  },

  /**
   * Read ICC ADN.
   *
   * @param fileId      EF id of the ADN.
   * @param onsuccess   Callback to be called when success.
   * @param onerror     Callback to be called when error.
   */
  readADN: function readADN(fileId, onsuccess, onerror) {
    function callback(options) {
      let contact = GsmPDUHelper.readAlphaIdDiallingNumber(options.recordSize);
      if (contact) {
        contact.recordId = options.p1;
        contacts.push(contact);
      }

      if (options.p1 < options.totalRecords) {
        ICCIOHelper.loadNextRecord(options);
      } else {
        if (DEBUG) {
          for (let i = 0; i < contacts.length; i++) {
            debug("ADN[" + i + "] " + JSON.stringify(contacts[i]));
          }
        }
        if (onsuccess) {
          onsuccess(contacts);
        }
      }
    }

    let contacts = [];
    ICCIOHelper.loadLinearFixedEF({fileId: fileId,
                                   callback: callback.bind(this),
                                   onerror: onerror});
  },

  /**
   * Update ICC ADN.
   *
   * @param fileId      EF id of the ADN.
   * @param contact     The contact will be updated. (Shall have recordId property)
   * @param onsuccess   Callback to be called when success.
   * @param onerror     Callback to be called when error.
   */
  updateADN: function updateADN(fileId, contact, onsuccess, onerror) {
    function dataWriter(recordSize) {
      GsmPDUHelper.writeAlphaIdDiallingNumber(recordSize,
                                              contact.alphaId,
                                              contact.number);
    }

    function callback(options) {
      if (onsuccess) {
        onsuccess();
      }
    }

    if (!contact || !contact.recordId) {
      if (onerror) {
        onerror("Invalid parameter.");
      }
      return;
    }

    ICCIOHelper.updateLinearFixedEF({fileId: fileId,
                                     recordNumber: contact.recordId,
                                     dataWriter: dataWriter.bind(this),
                                     callback: callback.bind(this),
                                     onerror: onerror});
  },

  /**
   * Get ICC MBDN. (Mailbox Dialling Number)
   *
   * @see TS 131.102, clause 4.2.60
   */
  getMBDN: function getMBDN() {
    function callback(options) {
      let contact = GsmPDUHelper.readAlphaIdDiallingNumber(options.recordSize);
      if (!contact || RIL.iccInfo.mbdn === contact.number){
        return;
      }
      RIL.iccInfo.mbdn = contact.number;
      if (DEBUG) {
        debug("MBDN, alphaId="+contact.alphaId+" number="+contact.number);
      }
      contact.rilMessageType = "iccmbdn";
      RIL.sendDOMMessage(contact);
    }

    ICCIOHelper.loadLinearFixedEF({fileId: ICC_EF_MBDN,
                                   callback: callback.bind(this)});
  },

  /**
   * Read USIM Phonebook.
   *
   * @param onsuccess   Callback to be called when success.
   * @param onerror     Callback to be called when error.
   */
  readPBR: function readPBR(onsuccess, onerror) {
    function callback(options) {
      let strLen = Buf.readUint32();
      let octetLen = strLen / 2, readLen = 0;

      let pbrTlvs = [];
      while (readLen < octetLen) {
        let tag = GsmPDUHelper.readHexOctet();
        if (tag == 0xff) {
          readLen++;
          Buf.seekIncoming((octetLen - readLen) * PDU_HEX_OCTET_SIZE);
          break;
        }

        let tlvLen = GsmPDUHelper.readHexOctet();
        let tlvs = ICCUtilsHelper.decodeSimTlvs(tlvLen);
        pbrTlvs.push({tag: tag,
                      length: tlvLen,
                      value: tlvs});

        readLen += tlvLen + 2; // +2 for tag and tlvLen
      }
      Buf.readStringDelimiter(strLen);

      if (pbrTlvs.length == 0) {
        let error = onerror || debug;
        error("Cannot access Phonebook.");
        return;
      }

      let pbr = ICCUtilsHelper.parsePbrTlvs(pbrTlvs);
      if (onsuccess) {
        onsuccess(pbr);
      }
    }

    ICCIOHelper.loadLinearFixedEF({fileId : ICC_EF_PBR,
                                   callback: callback.bind(this),
                                   onerror: onerror});
  },

  /**
   * Read the PLMNsel (Public Land Mobile Network) from the ICC.
   *
   * See ETSI TS 100.977 section 10.3.4 EF_PLMNsel
   */
  getPLMNSelector: function getPLMNSelector() {
    function callback() {
      if (DEBUG) debug("PLMN Selector: Process PLMN Selector");

      let length = Buf.readUint32();
      RIL.iccInfoPrivate.PLMN = this.readPLMNEntries(length/3);
      Buf.readStringDelimiter(length);

      if (DEBUG) debug("PLMN Selector: " + JSON.stringify(RIL.iccInfoPrivate.PLMN));

      if (RIL.updateDisplayCondition()) {
        this.handleICCInfoChange();
      }
    }

    // PLMN List is Service 7 in SIM, EF_PLMNsel
    ICCIOHelper.loadTransparentEF({fileId: ICC_EF_PLMNsel,
                                   callback: callback.bind(this)});
  },

  /**
   * Read the SPDI (Service Provider Display Information) from the ICC.
   *
   * See TS 131.102 section 4.2.66 for USIM and TS 51.011 section 10.3.50
   * for SIM.
   */
  getSPDI: function getSPDI() {
    function callback() {
      let length = Buf.readUint32();
      let readLen = 0;
      let endLoop = false;
      RIL.iccInfoPrivate.SPDI = null;
      while ((readLen < length / 2) && !endLoop) {
        let tlvTag = GsmPDUHelper.readHexOctet();
        let tlvLen = GsmPDUHelper.readHexOctet();
        readLen += 2; // For tag and length fields.
        switch (tlvTag) {
        case SPDI_TAG_SPDI:
          // The value part itself is a TLV.
          continue;
        case SPDI_TAG_PLMN_LIST:
          // This PLMN list is what we want.
          RIL.iccInfoPrivate.SPDI = this.readPLMNEntries(tlvLen / 3);
          readLen += tlvLen;
          endLoop = true;
          break;
        default:
          // We don't care about its content if its tag is not SPDI nor
          // PLMN_LIST.
          endLoop = true;
          break;
        }
      }

      // Consume unread octets.
      Buf.seekIncoming((length / 2 - readLen) * PDU_HEX_OCTET_SIZE);
      Buf.readStringDelimiter(length);

      if (DEBUG) debug("SPDI: " + JSON.stringify(RIL.iccInfoPrivate.SPDI));
      if (ICCUtilsHelper.updateDisplayCondition()) {
        ICCUtilsHelper.handleICCInfoChange();
      }
    }

    // PLMN List is Servive 51 in USIM, EF_SPDI
    ICCIOHelper.loadTransparentEF({fileId: ICC_EF_SPDI,
                                   callback: callback.bind(this)});
  },

  /**
   * Read EFcbmi (Cell Broadcast Message Identifier selection)
   *
   * @see 3GPP TS 31.102 v110.02.0 section 4.2.14 EFcbmi
   */
  getCBMI: function getCBMI() {
    function callback() {
      let strLength = Buf.readUint32();

      // Each Message Identifier takes two octets and each octet is encoded
      // into two chars.
      let numIds = strLength / 4, list = null;
      if (numIds) {
        list = [];
        for (let i = 0, id; i < numIds; i++) {
          id = GsmPDUHelper.readHexOctet() << 8 | GsmPDUHelper.readHexOctet();
          // `Unused entries shall be set to 'FF FF'.`
          if (id != 0xFFFF) {
            list.push(id);
            list.push(id + 1);
          }
        }
      }
      if (DEBUG) {
        debug("CBMI: " + JSON.stringify(list));
      }

      Buf.readStringDelimiter(strLength);

      RIL.cellBroadcastConfigs.CBMI = list;
      RIL._mergeAllCellBroadcastConfigs();
    }

    function onerror() {
      RIL.cellBroadcastConfigs.CBMI = null;
      RIL._mergeAllCellBroadcastConfigs();
    }

    ICCIOHelper.loadTransparentEF({fileId: ICC_EF_CBMI,
                                   callback: callback.bind(this),
                                   onerror: onerror.bind(this)});
  },

  /**
   * Read EFcbmir (Cell Broadcast Message Identifier Range selection)
   *
   * @see 3GPP TS 31.102 v110.02.0 section 4.2.22 EFcbmir
   */
  getCBMIR: function getCBMIR() {
    function callback() {
      let strLength = Buf.readUint32();

      // Each Message Identifier range takes four octets and each octet is
      // encoded into two chars.
      let numIds = strLength / 8, list = null;
      if (numIds) {
        list = [];
        for (let i = 0, from, to; i < numIds; i++) {
          // `Bytes one and two of each range identifier equal the lower value
          // of a cell broadcast range, bytes three and four equal the upper
          // value of a cell broadcast range.`
          from = GsmPDUHelper.readHexOctet() << 8 | GsmPDUHelper.readHexOctet();
          to = GsmPDUHelper.readHexOctet() << 8 | GsmPDUHelper.readHexOctet();
          // `Unused entries shall be set to 'FF FF'.`
          if ((from != 0xFFFF) && (to != 0xFFFF)) {
            list.push(from);
            list.push(to + 1);
          }
        }
      }
      if (DEBUG) {
        debug("CBMIR: " + JSON.stringify(list));
      }

      Buf.readStringDelimiter(strLength);

      RIL.cellBroadcastConfigs.CBMIR = list;
      RIL._mergeAllCellBroadcastConfigs();
    }

    function onerror() {
      RIL.cellBroadcastConfigs.CBMIR = null;
      RIL._mergeAllCellBroadcastConfigs();
    }

    ICCIOHelper.loadTransparentEF({fileId: ICC_EF_CBMIR,
                                   callback: callback.bind(this),
                                   onerror: onerror.bind(this)});
  },

  /**
   * Read OPL (Operator PLMN List) from USIM.
   *
   * See 3GPP TS 31.102 Sec. 4.2.59 for USIM
   *     3GPP TS 51.011 Sec. 10.3.42 for SIM.
   */
  getOPL: function getOPL() {
    let opl = [];
    function callback(options) {
      let len = Buf.readUint32();
      // The first 7 bytes are LAI (for UMTS) and the format of LAI is defined
      // in 3GPP TS 23.003, Sec 4.1
      //    +-------------+---------+
      //    | Octet 1 - 3 | MCC/MNC |
      //    +-------------+---------+
      //    | Octet 4 - 7 |   LAC   |
      //    +-------------+---------+
      let mccMnc = [GsmPDUHelper.readHexOctet(),
                    GsmPDUHelper.readHexOctet(),
                    GsmPDUHelper.readHexOctet()];
      if (mccMnc[0] != 0xFF || mccMnc[1] != 0xFF || mccMnc[2] != 0xFF) {
        let oplElement = {};
        let semiOctets = [];
        for (let i = 0; i < mccMnc.length; i++) {
          semiOctets.push((mccMnc[i] & 0xf0) >> 4);
          semiOctets.push(mccMnc[i] & 0x0f);
        }
        let reformat = [semiOctets[1], semiOctets[0], semiOctets[3],
                        semiOctets[5], semiOctets[4], semiOctets[2]];
        let buf = "";
        for (let i = 0; i < reformat.length; i++) {
          if (reformat[i] != 0xF) {
            buf += GsmPDUHelper.semiOctetToBcdChar(reformat[i]);
          }
          if (i === 2) {
            // 0-2: MCC
            oplElement.mcc = parseInt(buf);
            buf = "";
          } else if (i === 5) {
            // 3-5: MNC
            oplElement.mnc = parseInt(buf);
          }
        }
        // LAC/TAC
        oplElement.lacTacStart =
          (GsmPDUHelper.readHexOctet() << 8) | GsmPDUHelper.readHexOctet();
        oplElement.lacTacEnd =
          (GsmPDUHelper.readHexOctet() << 8) | GsmPDUHelper.readHexOctet();
        // PLMN Network Name Record Identifier
        oplElement.pnnRecordId = GsmPDUHelper.readHexOctet();
        if (DEBUG) {
          debug("OPL: [" + (opl.length + 1) + "]: " + JSON.stringify(oplElement));
        }
        opl.push(oplElement);
      } else {
        Buf.seekIncoming(5 * PDU_HEX_OCTET_SIZE);
      }
      Buf.readStringDelimiter(len);

      if (options.p1 < options.totalRecords) {
        ICCIOHelper.loadNextRecord(options);
      } else {
        RIL.iccInfoPrivate.OPL = opl;
      }
    }

    ICCIOHelper.loadLinearFixedEF({fileId: ICC_EF_OPL,
                                   callback: callback.bind(this)});
  },

  /**
   * Read PNN (PLMN Network Name) from USIM.
   *
   * See 3GPP TS 31.102 Sec. 4.2.58 for USIM
   *     3GPP TS 51.011 Sec. 10.3.41 for SIM.
   */
  getPNN: function getPNN() {
    let pnn = [];
    function callback(options) {
      let pnnElement = {
        fullName: "",
        shortName: ""
      };
      let len = Buf.readUint32();
      let readLen = 0;
      while (len > readLen) {
        let tlvTag = GsmPDUHelper.readHexOctet();
        readLen = readLen + 2; // 1 Hex octet
        if (tlvTag == 0xFF) {
          // Unused byte
          continue;
        }
        let tlvLen = GsmPDUHelper.readHexOctet();
        let name;
        switch (tlvTag) {
        case PNN_IEI_FULL_NETWORK_NAME:
          pnnElement.fullName = GsmPDUHelper.readNetworkName(tlvLen);
          break;
        case PNN_IEI_SHORT_NETWORK_NAME:
          pnnElement.shortName = GsmPDUHelper.readNetworkName(tlvLen);
          break;
        default:
          Buf.seekIncoming(PDU_HEX_OCTET_SIZE * tlvLen);
        }
        readLen += (tlvLen * 2 + 2);
      }
      if (DEBUG) {
        debug("PNN: [" + (pnn.length + 1) + "]: " + JSON.stringify(pnnElement));
      }
      Buf.readStringDelimiter(len);
      pnn.push(pnnElement);

      if (options.p1 < options.totalRecords) {
        ICCIOHelper.loadNextRecord(options);
      } else {
        RIL.iccInfoPrivate.PNN = pnn;
      }
    }

    ICCIOHelper.loadLinearFixedEF({fileId: ICC_EF_PNN,
                                   callback: callback.bind(this)});
  },

  /**
   * Get free ICC ADN record id.
   *
   * @param fileId      EF id of the ADN.
   * @param onsuccess   Callback to be called when success.
   * @param onerror     Callback to be called when error.
   */
  getADNFreeRecordId: function getADNFreeRecordId(fileId, onsuccess, onerror) {
    function callback(options) {
      let contact = GsmPDUHelper.readAlphaIdDiallingNumber(options.recordSize);
      if (!contact) {
        // Find free record.
        if (onsuccess) {
          onsuccess(options.p1);
        }
        return;
      }

      if (options.p1 < options.totalRecords) {
        ICCIOHelper.loadNextRecord(options);
      } else {
        // No free record found.
        if (onerror) {
          onerror("No free record found.");
        }
      }
    }

    ICCIOHelper.loadLinearFixedEF({fileId: fileId,
                                   callback: callback.bind(this),
                                   onerror: onerror});
  },

  /**
   *  Read the list of PLMN (Public Land Mobile Network) entries
   *  We cannot directly rely on readSwappedNibbleBcdToString(),
   *  since it will no correctly handle some corner-cases that are
   *  not a problem in our case (0xFF 0xFF 0xFF).
   *
   *  @param length The number of PLMN records.
   *  @return An array of string corresponding to the PLMNs.
   */
  readPLMNEntries: function readPLMNEntries(length) {
    let plmnList = [];
    // Each PLMN entry has 3 bytes.
    debug("readPLMNEntries: PLMN entries length = " + length);
    let index = 0;
    while (index < length) {
      // Unused entries will be 0xFFFFFF, according to EF_SPDI
      // specs (TS 131 102, section 4.2.66)
      try {
        let plmn = [GsmPDUHelper.readHexOctet(),
                    GsmPDUHelper.readHexOctet(),
                    GsmPDUHelper.readHexOctet()];
        if (DEBUG) debug("readPLMNEntries: Reading PLMN entry: [" + index +
                         "]: '" + plmn + "'");
        if (plmn[0] != 0xFF &&
            plmn[1] != 0xFF &&
            plmn[2] != 0xFF) {
          let semiOctets = [];
          for (let idx = 0; idx < plmn.length; idx++) {
            semiOctets.push((plmn[idx] & 0xF0) >> 4);
            semiOctets.push(plmn[idx] & 0x0F);
          }

          // According to TS 24.301, 9.9.3.12, the semi octets is arranged
          // in format:
          // Byte 1: MCC[2] | MCC[1]
          // Byte 2: MNC[3] | MCC[3]
          // Byte 3: MNC[2] | MNC[1]
          // Therefore, we need to rearrange them.
          let reformat = [semiOctets[1], semiOctets[0], semiOctets[3],
                          semiOctets[5], semiOctets[4], semiOctets[2]];
          let buf = "";
          let plmnEntry = {};
          for (let i = 0; i < reformat.length; i++) {
            if (reformat[i] != 0xF) {
              buf += GsmPDUHelper.semiOctetToBcdChar(reformat[i]);
            }
            if (i === 2) {
              // 0-2: MCC
              plmnEntry.mcc = parseInt(buf);
              buf = "";
            } else if (i === 5) {
              // 3-5: MNC
              plmnEntry.mnc = parseInt(buf);
            }
          }
          if (DEBUG) debug("readPLMNEntries: PLMN = " + plmnEntry.mcc + ", " + plmnEntry.mnc);
          plmnList.push(plmnEntry);
        }
      } catch (e) {
        if (DEBUG) debug("readPLMNEntries: PLMN entry " + index + " is invalid.");
        break;
      }
      index ++;
    }
    return plmnList;
  },
};

/**
 * Helper functions for ICC utilities.
 */
let ICCUtilsHelper = {
  /**
   * This will compute the spnDisplay field of the network.
   * See TS 22.101 Annex A and TS 51.011 10.3.11 for details.
   *
   * @return True if some of iccInfo is changed in by this function.
   */
  updateDisplayCondition: function updateDisplayCondition() {
    // If EFspn isn't existed in SIM or it haven't been read yet, we should
    // just set isDisplayNetworkNameRequired = true and
    // isDisplaySpnRequired = false
    let iccInfo = RIL.iccInfo;
    let iccInfoPriv = RIL.iccInfoPrivate;
    let iccSpn = iccInfoPriv.SPN;
    let origIsDisplayNetworkNameRequired = iccInfo.isDisplayNetworkNameRequired;
    let origIsDisplaySPNRequired = iccInfo.isDisplaySpnRequired;

    if (!iccSpn) {
      iccInfo.isDisplayNetworkNameRequired = true;
      iccInfo.isDisplaySpnRequired = false;
    } else {
      let operatorMnc = RIL.operator.mnc;
      let operatorMcc = RIL.operator.mcc;

      // First detect if we are on HPLMN or one of the PLMN
      // specified by the SIM card.
      let isOnMatchingPlmn = false;

      // If the current network is the one defined as mcc/mnc
      // in SIM card, it's okay.
      if (iccInfo.mcc == operatorMcc && iccInfo.mnc == operatorMnc) {
        isOnMatchingPlmn = true;
      }

      // Test to see if operator's mcc/mnc match mcc/mnc of PLMN.
      if (!isOnMatchingPlmn && iccInfoPriv.SPDI) {
        let iccSpdi = iccInfoPriv.SPDI; // PLMN list
        for (let plmn in iccSpdi) {
          let plmnMcc = iccSpdi[plmn].mcc;
          let plmnMnc = iccSpdi[plmn].mnc;
          isOnMatchingPlmn = (plmnMcc == operatorMcc) && (plmnMnc == operatorMnc);
          if (isOnMatchingPlmn) {
            break;
          }
        }
      }

      if (isOnMatchingPlmn) {
        // The first bit of display condition tells us if we should display
        // registered PLMN.
        if (DEBUG) debug("updateDisplayCondition: PLMN is HPLMN or PLMN is in PLMN list");

        // TS 31.102 Sec. 4.2.66 and TS 51.011 Sec. 10.3.50
        // EF_SPDI contains a list of PLMNs in which the Service Provider Name
        // shall be displayed.
        iccInfo.isDisplaySpnRequired = true;
        if (iccSpn.spnDisplayCondition & 0x01) {
          iccInfo.isDisplayNetworkNameRequired = true;
        } else {
          iccInfo.isDisplayNetworkNameRequired = false;
        }
      } else {
        // The second bit of display condition tells us if we should display
        // registered PLMN.
        if (DEBUG) debug("updateICCDisplayName: PLMN isn't HPLMN and PLMN isn't in PLMN list");

        // We didn't found the requirement of displaying network name if
        // current PLMN isn't HPLMN nor one of PLMN in SPDI. So we keep
        // isDisplayNetworkNameRequired false.
        if (iccSpn.spnDisplayCondition & 0x02) {
          iccInfo.isDisplayNetworkNameRequired = false;
          iccInfo.isDisplaySpnRequired = false;
        } else {
          iccInfo.isDisplayNetworkNameRequired = false;
          iccInfo.isDisplaySpnRequired = true;
        }
      }
    }

    if (DEBUG) {
      debug("updateDisplayCondition: isDisplayNetworkNameRequired = " + iccInfo.isDisplayNetworkNameRequired);
      debug("updateDisplayCondition: isDisplaySpnRequired = " + iccInfo.isDisplaySpnRequired);
    }

    return ((origIsDisplayNetworkNameRequired !== iccInfo.isDisplayNetworkNameRequired) ||
            (origIsDisplaySPNRequired !== iccInfo.isDisplaySpnRequired));
  },

  decodeSimTlvs: function decodeSimTlvs(tlvsLen) {
    let index = 0;
    let tlvs = [];
    while (index < tlvsLen) {
      let simTlv = {
        tag : GsmPDUHelper.readHexOctet(),
        length : GsmPDUHelper.readHexOctet(),
      };
      simTlv.value = GsmPDUHelper.readHexOctetArray(simTlv.length)
      tlvs.push(simTlv);
      index += simTlv.length + 2 /* The length of 'tag' and 'length' field */;
    }
    return tlvs;
  },

  /**
   * Parse those TLVs and convert it to an object.
   */
  parsePbrTlvs: function parsePbrTlvs(pbrTlvs) {
    let pbr = {};
    for (let i = 0; i < pbrTlvs.length; i++) {
      let pbrTlv = pbrTlvs[i];
      for (let j = 0; j < pbrTlv.value.length; j++) {
        let tlv = pbrTlv.value[j];
        let tagName = USIM_TAG_NAME[tlv.tag];

        // ANR could have multiple files.
        if (tlv.tag == ICC_USIM_EFANR_TAG) {
          if (!pbr.anr) {
            pbr.anr = [];
          }
          pbr.anr.push(tlv);
        } else {
          pbr[tagName] = tlv;
        }

        pbr[tagName].fileType = pbrTlv.tag;
        pbr[tagName].fileId = (tlv.value[0] << 8) | tlv.value[1];

        // For Type 2, the order of files is in the same order in IAP.
        if (pbrTlv.tag == ICC_USIM_TYPE2_TAG) {
          pbr[tagName].indexInIAP = j;
        }
      }
    }

    return pbr;
  },

  /**
   * Update the ICC information to RadioInterfaceLayer.
   */
  handleICCInfoChange: function handleICCInfoChange() {
    RIL.iccInfo.rilMessageType = "iccinfochange";
    RIL.sendDOMMessage(RIL.iccInfo);
  },

  /**
   * Get whether specificed (U)SIM service is available.
   *
   * @param geckoService
   *        Service name like "ADN", "BDN", etc.
   *
   * @return true if the service is enabled, false otherwise.
   */
  isICCServiceAvailable: function isICCServiceAvailable(geckoService) {
    let serviceTable = RIL.iccInfoPrivate.sst;
    let index, bitmask;
    if (RIL.appType == CARD_APPTYPE_SIM) {
      /**
       * Service id is valid in 1..N, and 2 bits are used to code each service.
       *
       * +----+--  --+----+----+
       * | b8 | ...  | b2 | b1 |
       * +----+--  --+----+----+
       *
       * b1 = 0, service not allocated.
       *      1, service allocated.
       * b2 = 0, service not activatd.
       *      1, service allocated.
       *
       * @see 3GPP TS 51.011 10.3.7.
       */
      let simService = GECKO_ICC_SERVICES.sim[geckoService];
      if (!simService) {
        return false;
      }
      simService -= 1;
      index = Math.floor(simService / 4);
      bitmask = 2 << ((simService % 4) << 1);
    } else {
      /**
       * Service id is valid in 1..N, and 1 bit is used to code each service.
       *
       * +----+--  --+----+----+
       * | b8 | ...  | b2 | b1 |
       * +----+--  --+----+----+
       *
       * b1 = 0, service not avaiable.
       *      1, service available.
       * b2 = 0, service not avaiable.
       *      1, service available.
       *
       * @see 3GPP TS 31.102 4.2.8.
       */
      let usimService = GECKO_ICC_SERVICES.usim[geckoService];
      if (!usimService) {
        return false;
      }
      usimService -= 1;
      index = Math.floor(usimService / 8);
      bitmask = 1 << ((usimService % 8) << 0);
    }

    return (serviceTable &&
           (index < serviceTable.length) &&
           (serviceTable[index] & bitmask)) != 0;
  },

  /**
   * Check if the string is of GSM default 7-bit coded alphabets with bit 8
   * set to 0.
   *
   * @param str  String to be checked.
   */
  isGsm8BitAlphabet: function isGsm8BitAlphabet(str) {
    if (!str) {
      return false;
    }

    const langTable = PDU_NL_LOCKING_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];
    const langShiftTable = PDU_NL_SINGLE_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];

    for (let i = 0; i < str.length; i++) {
      let c = str.charAt(i);
      let octet = langTable.indexOf(c);
      if (octet == -1) {
        octet = langShiftTable.indexOf(c);
        if (octet == -1) {
          return false;
        }
      }
    }

    return true;
  },
};

/**
 * Helper for ICC Contacts.
 */
let ICCContactHelper = {
  /**
   * Helper function to read ICC contacts.
   *
   * @param appType       CARD_APPTYPE_SIM or CARD_APPTYPE_USIM.
   * @param contactType   "ADN" or "FDN"
   * @param onsuccess     Callback to be called when success.
   * @param onerror       Callback to be called when error.
   */
  readICCContacts: function readICCContacts(appType, contactType, onsuccess, onerror) {
    switch (contactType) {
      case "ADN":
        switch (appType) {
          case CARD_APPTYPE_SIM:
            this.readSimContacts(onsuccess, onerror);
            break;
          case CARD_APPTYPE_USIM:
            this.readUSimContacts(onsuccess, onerror);
            break;
        }
        break;
      case "FDN":
        ICCRecordHelper.readFDN(onsuccess, onerror);
        break;
    }
  },

  /**
   * Helper function to find free contact record.
   *
   * @param appType       CARD_APPTYPE_SIM or CARD_APPTYPE_USIM.
   * @param contactType   "ADN".
   * @param onsuccess     Callback to be called when success.
   * @param onerror       Callback to be called when error.
   */
  findFreeICCContact: function findFreeICCContact(appType, contactType, onsuccess, onerror) {
    switch (contactType) {
      case "ADN":
        switch (appType) {
          case CARD_APPTYPE_SIM:
            ICCRecordHelper.getADNFreeRecordId(ICC_EF_ADN, onsuccess, onerror);
            break;
          case CARD_APPTYPE_USIM:
            let gotPbrCb = function gotPbrCb(pbr) {
              if (pbr.adn) {
                ICCRecordHelper.getADNFreeRecordId(pbr.adn.fileId, onsuccess, onerror);
              }
            }.bind(this);

            ICCRecordHelper.readPBR(gotPbrCb, onerror);
            break;
        }
        break;
      default:
        if (onerror) {
          onerror(GECKO_ERROR_REQUEST_NOT_SUPPORTED);
        }
        break;
    }
  },

  /**
   * Helper function to add a new ICC contact.
   *
   * @param appType       CARD_APPTYPE_SIM or CARD_APPTYPE_USIM.
   * @param contactType   "ADN"
   * @param contact       The contact will be added.
   * @param onsuccess     Callback to be called when success.
   * @param onerror       Callback to be called when error.
   */
  addICCContact: function addICCContact(appType, contactType, contact, onsuccess, onerror) {
    let foundFreeCb = function foundFreeCb(recordId) {
      contact.recordId = recordId;
      ICCContactHelper.updateICCContact(appType, contactType, contact, onsuccess, onerror);
    }.bind(this);

    // Find free record first.
    ICCContactHelper.findFreeICCContact(appType, contactType, foundFreeCb, onerror);
  },

  /**
   * Helper function to update ICC contact.
   *
   * @param appType       CARD_APPTYPE_SIM or CARD_APPTYPE_USIM.
   * @param contactType   "ADN".
   * @param contact       The contact will be updated.
   * @param onsuccess     Callback to be called when success.
   * @param onerror       Callback to be called when error.
   */
  updateICCContact: function updateICCContact(appType, contactType, contact, onsuccess, onerror) {
    switch (contactType) {
      case "ADN":
        switch (appType) {
          case CARD_APPTYPE_SIM:
            this.updateSimContact(contact, onsuccess, onerror);
            break;
          case CARD_APPTYPE_USIM:
            this.updateUSimContact(contact, onsuccess, onerror);
            break;
        }
        break;
      default:
        if (onerror) {
          onerror(GECKO_ERROR_REQUEST_NOT_SUPPORTED);
        }
        break;
    }
  },

  /**
   * Read contacts from USIM.
   *
   * @param onsuccess     Callback to be called when success.
   * @param onerror       Callback to be called when error.
   */
  readUSimContacts: function readUSimContacts(onsuccess, onerror) {
    let gotPbrCb = function gotPbrCb(pbr) {
      if (pbr.adn) {
        let fileId = pbr.adn.fileId;
        ICCRecordHelper.readADN(fileId, onsuccess, onerror);
      } else {
        let error = onerror || debug;
        error("Cannot access ADN.");
      }
    }.bind(this);

    ICCRecordHelper.readPBR(gotPbrCb, onerror);
  },

  /**
   * Read contacts from SIM.
   *
   * @param onsuccess     Callback to be called when success.
   * @param onerror       Callback to be called when error.
   */
  readSimContacts: function readSimContacts(onsuccess, onerror) {
    ICCRecordHelper.readADN(ICC_EF_ADN, onsuccess, onerror);
  },

  /**
   * Update USIM contact.
   *
   * @param contact       The contact will be updated.
   * @param onsuccess     Callback to be called when success.
   * @param onerror       Callback to be called when error.
   */
  updateUSimContact: function updateUSimContact(contact, onsuccess, onerror) {
    let gotPbrCb = function gotPbrCb(pbr) {
      if (pbr.adn) {
        ICCRecordHelper.updateADN(pbr.adn.fileId, contact, onsuccess, onerror);
      } else {
        if (onerror) {
          onerror("Cannot access ADN.");
        }
      }
    }.bind(this);

    ICCRecordHelper.readPBR(gotPbrCb, onerror);
  },

  /**
   * Update SIM contact.
   *
   * @param contact       The contact will be updated.
   * @param onsuccess     Callback to be called when success.
   * @param onerror       Callback to be called when error.
   */
  updateSimContact: function updateSimContact(contact, onsuccess, onerror) {
    ICCRecordHelper.updateADN(ICC_EF_ADN, contact, onsuccess, onerror);
  },
};

/**
 * Global stuff.
 */

if (!this.debug) {
  // Debugging stub that goes nowhere.
  this.debug = function debug(message) {
    dump("RIL Worker[" + CLIENT_ID + "]: " + message + "\n");
  };
}

// Initialize buffers. This is a separate function so that unit tests can
// re-initialize the buffers at will.
Buf.init();

function onRILMessage(data) {
  Buf.processIncoming(data);
};

onmessage = function onmessage(event) {
  RIL.handleDOMMessage(event.data);
};

onerror = function onerror(event) {
  debug("RIL Worker error" + event.message + "\n");
};
