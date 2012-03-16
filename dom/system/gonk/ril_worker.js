/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is RIL JS Worker.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Kyle Machulis <kyle@nonpolynomial.com>
 *   Philipp von Weitershausen <philipp@weitershausen.de>
 *   Fernando Jimenez <ferjmoreno@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
 * The three objects in this file represent individual parts of this
 * communication chain:
 *
 * - RILMessageEvent -> Buf -> RIL -> Phone -> postMessage()
 * - "message" event -> Phone -> RIL -> Buf -> postRILMessage()
 *
 * Note: The code below is purposely lean on abstractions to be as lean in
 * terms of object allocations. As a result, it may look more like C than
 * JavaScript, and that's intended.
 */

"use strict";

importScripts("ril_consts.js", "systemlibs.js");

// We leave this as 'undefined' instead of setting it to 'false'. That
// way an outer scope can define it to 'true' (e.g. for testing purposes)
// without us overriding that here.
let DEBUG;

const INT32_MAX   = 2147483647;
const UINT8_SIZE  = 1;
const UINT16_SIZE = 2;
const UINT32_SIZE = 4;
const PARCEL_SIZE_SIZE = UINT32_SIZE;

let RILQUIRKS_CALLSTATE_EXTRA_UINT32 = false;
let RILQUIRKS_DATACALLSTATE_DOWN_IS_UP = false;

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

  readUint8Unchecked: function readUint8Unchecked() {
    let value = this.incomingBytes[this.incomingReadIndex];
    this.incomingReadIndex = (this.incomingReadIndex + 1) %
                             this.INCOMING_BUFFER_LENGTH;
    return value;
  },

  readUint8: function readUint8() {
    if (!this.readAvailable) {
      throw new Error("Trying to read data beyond the parcel end!");
    }

    this.readAvailable--;
    return this.readUint8Unchecked();
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
    let delimiter = this.readUint16();
    if (!(string_len & 1)) {
      delimiter |= this.readUint16();
    }
    if (DEBUG) {
      if (delimiter != 0) {
        debug("Something's wrong, found string delimiter: " + delimiter);
      }
    }
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

  readParcelSize: function readParcelSize() {
    return this.readUint8Unchecked() << 24 |
           this.readUint8Unchecked() << 16 |
           this.readUint8Unchecked() <<  8 |
           this.readUint8Unchecked();
  },

  /**
   * Functions for writing data to the outgoing buffer.
   */

  writeUint8: function writeUint8(value) {
    if (this.outgoingIndex >= this.OUTGOING_BUFFER_LENGTH) {
      this.growOutgoingBuffer(this.outgoingIndex + 1);
    }
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
    this.writeUint16(0);
    if (!(value.length & 1)) {
      this.writeUint16(0);
    }
  },

  writeStringList: function writeStringList(strings) {
    this.writeUint32(strings.length);
    for (let i = 0; i < strings.length; i++) {
      this.writeString(strings[i]);
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
    if (incoming.length > this.INCOMING_BUFFER_LENGTH) {
      this.growIncomingBuffer(incoming.length);
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
      delete this.tokenRequestMap[token];
      request_type = options.rilRequestType;

      options.rilRequestError = error;
      if (error) {   	  
        if (DEBUG) {
          debug("Received error " + error + " for solicited parcel type " +
                request_type);
        }
        RIL.handleRequestError(options);
        return;
      }
      if (DEBUG) {
        debug("Solicited response for request type " + request_type +
              ", token " + token);
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
    postRILMessage(parcel);
    this.outgoingIndex = PARCEL_SIZE_SIZE;
  },

  simpleRequest: function simpleRequest(type, options) {
    this.newParcel(type, options);
    this.sendParcel();
  }
};


/**
 * Provide a high-level API representing the RIL's capabilities. This is
 * where parcels are sent and received from and translated into API calls.
 * For the most part, this object is pretty boring as it simply translates
 * between method calls and RIL parcels. Somebody's gotta do the job...
 */
let RIL = {

  /**
   * One of the RADIO_STATE_* constants.
   */
  radioState: RADIO_STATE_UNAVAILABLE,

  /**
   * ICC status. Keeps a reference of the data response to the
   * getICCStatus request.
   */
  iccStatus: null,

  /**
   * Card state
   */
  cardState: null,

  /**
   * Strings
   */
  IMEI: null,
  IMEISV: null,
  IMSI: null,
  SMSC: null,
  MSISDN: null,

  registrationState: {},
  gprsRegistrationState: {},

  /**
   * List of strings identifying the network operator.
   */
  operator: null,

  /**
   * String containing the baseband version.
   */
  basebandVersion: null,

  /**
   * Network selection mode. 0 for automatic, 1 for manual selection.
   */
  networkSelectionMode: null,

  /**
   * Active calls
   */
  currentCalls: {},

  /**
   * Existing data calls.
   */
  currentDataCalls: {},

  /**
   * Mute or unmute the radio.
   */
  _muted: true,
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
   * Set quirk flags based on the RIL model detected. Note that this
   * requires the RIL being "warmed up" first, which happens when on
   * an incoming or outgoing voice call or data call.
   */
  rilQuirksInitialized: false,
  initRILQuirks: function initRILQuirks() {
    if (this.rilQuirksInitialized) {
      return;
    }

    // The Samsung Galaxy S2 I-9100 radio sends an extra Uint32 in the
    // call state.
    let model_id = libcutils.property_get("ril.model_id");
    if (DEBUG) debug("Detected RIL model " + model_id);
    if (model_id == "I9100") {
      if (DEBUG) {
        debug("Detected I9100, enabling " +
              "RILQUIRKS_CALLSTATE_EXTRA_UINT32, " +
              "RILQUIRKS_DATACALLSTATE_DOWN_IS_UP.");
      }
      RILQUIRKS_CALLSTATE_EXTRA_UINT32 = true;
      RILQUIRKS_DATACALLSTATE_DOWN_IS_UP = true;
    }

    this.rilQuirksInitialized = true;
  },

  /**
   * Parse an integer from a string, falling back to a default value
   * if the the provided value is not a string or does not contain a valid
   * number.
   *
   * @param string
   *        String to be parsed.
   * @param defaultValue
   *        Default value to be used.
   */
  parseInt: function RIL_parseInt(string, defaultValue) {
    let number = parseInt(string, 10);
    if (!isNaN(number)) {
      return number;
    }
    if (defaultValue === undefined) {
      defaultValue = null;
    }
    return defaultValue;
  },

  /**
   * Retrieve the ICC's status.
   */
  getICCStatus: function getICCStatus() {
    Buf.simpleRequest(REQUEST_GET_SIM_STATUS);
  },

  /**
   * Enter a PIN to unlock the ICC.
   *
   * @param pin
   *        String containing the PIN.
   */
  enterICCPIN: function enterICCPIN(options) {
    Buf.newParcel(REQUEST_ENTER_SIM_PIN);
    Buf.writeUint32(1);
    Buf.writeString(options.pin);
    Buf.sendParcel();
  },

  /**
   * Change the current ICC PIN number
   *
   * @param oldPin
   *        String containing the old PIN value
   * @param newPin
   *        String containing the new PIN value
   */
  changeICCPIN: function changeICCPIN(options) {
    Buf.newParcel(REQUEST_CHANGE_SIM_PIN);
    Buf.writeUint32(2);
    Buf.writeString(options.oldPin);
    Buf.writeString(options.newPin);
    Buf.sendParcel();
  },

  /**
   * Supplies SIM PUK and a new PIN to unlock the ICC
   *
   * @param puk
   *        String containing the PUK value.
   * @param newPin
   *        String containing the new PIN value.
   *
   * Response will call Phone.onEnterICCPUK().
   */
   enterICCPUK: function enterICCPUK(options) {
     Buf.newParcel(REQUEST_ENTER_SIM_PUK);
     Buf.writeUint32(2);
     Buf.writeString(options.puk);
     Buf.writeString(options.newPin);
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
   *  @param fileid
   *         The file to operate on, one of the ICC_EF_* constants.
   *  @param pathid
   *         String type, check pathid from TS 27.007 +CRSM  
   *  @param p1, p2, p3
   *         Arbitrary integer parameters for the command.
   *  @param data
   *         String parameter for the command.
   *  @param pin2 [optional]
   *         String containing the PIN2.
   */
  iccIO: function iccIO(options) {
    let token = Buf.newParcel(REQUEST_SIM_IO, options);
    Buf.writeUint32(options.command);
    Buf.writeUint32(options.fileid);
    Buf.writeString(options.path);
    Buf.writeUint32(options.p1);
    Buf.writeUint32(options.p2);
    Buf.writeUint32(options.p3);
    Buf.writeString(options.data);
    if (options.pin2 != null) {
      Buf.writeString(options.pin2);
    }
    Buf.sendParcel();
  },
  
  /**
   * Read the MSISDN from the ICC.
   */
  getMSISDN: function getMSISDN() {
    this.iccIO({
      command: ICC_COMMAND_GET_RESPONSE,
      fileid:  ICC_EF_MSISDN,
      pathid:  EF_PATH_MF_SIM + EF_PATH_DF_TELECOM,
      p1:      0, // For GET_RESPONSE, p1 = 0
      p2:      0, // For GET_RESPONSE, p2 = 0
      p3:      GET_RESPONSE_EF_SIZE_BYTES,
      data:    null,
      pin2:    null,
    });
  },

  /**
   * Request the phone's radio power to be switched on or off.
   *
   * @param on
   *        Boolean indicating the desired power state.
   */
  setRadioPower: function setRadioPower(on) {
    Buf.newParcel(REQUEST_RADIO_POWER);
    Buf.writeUint32(1);
    Buf.writeUint32(on ? 1 : 0);
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

  getRegistrationState: function getRegistrationState() {
    Buf.simpleRequest(REQUEST_REGISTRATION_STATE);
  },

  getGPRSRegistrationState: function getGPRSRegistrationState() {
    Buf.simpleRequest(REQUEST_GPRS_REGISTRATION_STATE);
  },

  getOperator: function getOperator() {
    Buf.simpleRequest(REQUEST_OPERATOR);
  },

  getNetworkSelectionMode: function getNetworkSelectionMode() {
    Buf.simpleRequest(REQUEST_QUERY_NETWORK_SELECTION_MODE);
  },

  /**
   * Request various states about the network.
   */
  requestNetworkInfo: function requestNetworkInfo() {
    if (DEBUG) debug("Requesting phone state");
    this.getRegistrationState();
    this.getGPRSRegistrationState(); //TODO only GSM
    this.getOperator();
    this.getNetworkSelectionMode();
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

  getIMEI: function getIMEI() {
    Buf.simpleRequest(REQUEST_GET_IMEI);
  },

  getIMEISV: function getIMEISV() {
    Buf.simpleRequest(REQUEST_GET_IMEISV);
  },

  getDeviceIdentity: function getDeviceIdentity() {
    Buf.simpleRequest(REQUEST_GET_DEVICE_IDENTITY);
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
    let token = Buf.newParcel(REQUEST_DIAL);
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
    //TODO need to check whether call is holding/waiting/background
    // and then use REQUEST_HANGUP_WAITING_OR_BACKGROUND
    Buf.newParcel(REQUEST_HANGUP);
    Buf.writeUint32(1);
    Buf.writeUint32(options.callIndex);
    Buf.sendParcel();
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
   * Answer an incoming call.
   *
   * @param callIndex
   *        Call index of the call to answer.
   */
  answerCall: function answerCall() {
    // Check for races. Since we dispatched the incoming call notification the
    // incoming call may have changed. The main thread thinks that it is
    // answering the call with the given index, so only answer if that is still
    // incoming.
    let call = this.currentCalls[options.callIndex];
    if (call && call.state == CALL_STATE_INCOMING) {
      Buf.simpleRequest(REQUEST_ANSWER);
    }
  },

  /**
   * Reject an incoming call.
   *
   * @param callIndex
   *        Call index of the call to reject.
   */
  rejectCall: function rejectCall() {
    // Check for races. Since we dispatched the incoming call notification the
    // incoming call may have changed. The main thread thinks that it is
    // rejecting the call with the given index, so only reject if that is still
    // incoming.
    let call = this.currentCalls[options.callIndex];
    if (call && call.state == CALL_STATE_INCOMING) {
      Buf.simpleRequest(REQUEST_UDUB);
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
    // Get the SMS Center address
    if (!this.SMSC) {
      // We request the SMS center address again, passing it the SMS options
      // in order to try to send it again after retrieving the SMSC number.
      this.getSMSCAddress(options);
      return;
    }
    // We explicitly save this information on the options object so that we
    // can refer to it later, in particular on the main thread (where this
    // object may get sent eventually.)
    options.SMSC = this.SMSC;

    //TODO: verify values on 'options'
    //TODO: the data encoding and length in octets should eventually be
    // computed on the mainthread and passed down to us.
    GsmPDUHelper.calculateUserDataLength(options);

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
   *
   * @param pendingSMS
   *        Object containing the parameters of an SMS waiting to be sent.
   */
  getSMSCAddress: function getSMSCAddress(pendingSMS) {
    Buf.simpleRequest(REQUEST_GET_SMSC_ADDRESS, pendingSMS);
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
    let token = Buf.newParcel(REQUEST_SETUP_DATA_CALL);
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

    let token = Buf.newParcel(REQUEST_DEACTIVATE_DATA_CALL);
    Buf.writeUint32(2);
    Buf.writeString(options.cid);
    Buf.writeString(options.reason || DATACALL_DEACTIVATE_NO_REASON);
    Buf.sendParcel();


    datacall.state = GECKO_NETWORK_STATE_DISCONNECTING;
    Phone.sendDOMMessage({type: "datacallstatechange",
                          datacall: datacall});
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
  getFailCauseCode: function getFailCauseCode() {
    Buf.simpleRequest(REQUEST_LAST_CALL_FAIL_CAUSE);
  },


  /**
   * Process ICC status.
   */
  _processICCStatus: function _processICCStatus(iccStatus) {
    if (DEBUG) {
      debug("iccStatus: " + JSON.stringify(iccStatus));
    }
    this.iccStatus = iccStatus;

    if ((!iccStatus) || (iccStatus.cardState == CARD_STATE_ABSENT)) {
      if (DEBUG) debug("ICC absent");
      if (this.cardState == GECKO_CARDSTATE_ABSENT) {
        this.operator = null;
        return;
      }
      this.cardState = GECKO_CARDSTATE_ABSENT;
      Phone.sendDOMMessage({type: "cardstatechange",
                            cardState: this.cardState});
      return;
    }

    if ((this.radioState == RADIO_STATE_OFF) ||
        (this.radioState == RADIO_STATE_UNAVAILABLE) ||
        (this.radioState == RADIO_STATE_SIM_NOT_READY) ||
        (this.radioState == RADIO_STATE_RUIM_NOT_READY) ||
        (this.radioState == RADIO_STATE_NV_NOT_READY) ||
        (this.radioState == RADIO_STATE_NV_READY)) {
      if (DEBUG) debug("ICC not ready");
      if (this.cardState == GECKO_CARDSTATE_NOT_READY) {
        return;
      }
      this.cardState = GECKO_CARDSTATE_NOT_READY;
      Phone.sendDOMMessage({type: "cardstatechange",
                            cardState: this.cardState});
      return;
    }

    if ((this.radioState == RADIO_STATE_SIM_LOCKED_OR_ABSENT) ||
        (this.radioState == RADIO_STATE_SIM_READY) ||
        (this.radioState == RADIO_STATE_RUIM_LOCKED_OR_ABSENT) ||
        (this.radioState == RADIO_STATE_RUIM_READY)) {
      let app = iccStatus.apps[iccStatus.gsmUmtsSubscriptionAppIndex];
      if (!app) {
        if (DEBUG) {
          debug("Subscription application is not present in iccStatus.");
        }
        if (this.cardState == GECKO_CARDSTATE_ABSENT) {
          return;
        }
        this.cardState = GECKO_CARDSTATE_ABSENT;
        this.operator = null;
        Phone.sendDOMMessage({type: "cardstatechange",
                              cardState: this.cardState});
        return;
      }

      let newCardState;
      switch (app.app_state) {
        case CARD_APP_STATE_PIN:
          newCardState = GECKO_CARDSTATE_PIN_REQUIRED;
          break;
        case CARD_APP_STATE_PUK:
          newCardState = GECKO_CARDSTATE_PUK_REQUIRED;
          break;
        case CARD_APP_STATE_SUBSCRIPTION_PERSO:
          newCardState = GECKO_CARDSTATE_NETWORK_LOCKED;
          break;
        case CARD_APP_STATE_READY:
          newCardState = GECKO_CARDSTATE_READY;
          break;
        case CARD_APP_STATE_UNKNOWN:
        case CARD_APP_STATE_DETECTED:
        default:
          newCardState = GECKO_CARDSTATE_NOT_READY;
      }

      if (this.cardState == newCardState) {
        return;
      }
      this.cardState = newCardState;
      Phone.sendDOMMessage({type: "cardstatechange",
                            cardState: this.cardState});
    }
  },

  _processMSISDNResponse: function _processMSISDNResponse(options) {
    let sw1 = Buf.readUint32();
    let sw2 = Buf.readUint32();
    // See GSM11.11 section 9.4 for sw1 and sw2
    if (sw1 != STATUS_NORMAL_ENDING) {
      // TODO: error 
      // Wait for fix for Bug 733990 to report error.
      debug("Error in iccIO");
    }
    if (DEBUG) debug("ICC I/O (" + sw1 + "/" + sw2 + ")");

    switch (options.command) {
      case ICC_COMMAND_GET_RESPONSE:
        let response = Buf.readString();
        let recordSize = parseInt(
            response.substr(RESPONSE_DATA_RECORD_LENGTH * 2, 2), 16) & 0xff;
        let options = {
          command: ICC_COMMAND_READ_RECORD,
          fileid:  ICC_EF_MSISDN,
          pathid:  EF_PATH_MF_SIM + EF_PATH_DF_TELECOM,
          p1:      1, // Record number, MSISDN is always in the 1st record
          p2:      READ_RECORD_ABSOLUTE_MODE,
          p3:      recordSize,
          data:    null,
          pin2:    null,
        };
        RIL.iccIO(options);
        break;

      case ICC_COMMAND_READ_RECORD:
        // Ignore 2 bytes prefix, which is 4 chars
        let number = GsmPDUHelper.readStringAsBCD().toString().substr(4); 
        if (DEBUG) debug("MSISDN: " + number);
        this.MSISDN = number;
        break;
    } 
  },

  _processRegistrationState: function _processRegistrationState(state) {
    let rs = this.registrationState;
    let stateChanged = false;

    let regState = RIL.parseInt(state[0], NETWORK_CREG_STATE_UNKNOWN);
    if (rs.regState != regState) {
      rs.regState = regState;
      stateChanged = true;
    }

    let radioTech = RIL.parseInt(state[3], NETWORK_CREG_TECH_UNKNOWN);
    if (rs.radioTech != radioTech) {
      rs.radioTech = radioTech;
      stateChanged = true;
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
      Phone.sendDOMMessage({type: "registrationstatechange",
                            registrationState: rs});
    }
  },

  _processGPRSRegistrationState: function _processGPRSRegistrationState(state) {
    let rs = this.gprsRegistrationState;
    let stateChanged = false;

    let regState = RIL.parseInt(state[0], NETWORK_CREG_STATE_UNKNOWN);
    if (rs.regState != regState) {
      rs.regState = regState;
      stateChanged = true;
    }

    let radioTech = RIL.parseInt(state[3], NETWORK_CREG_TECH_UNKNOWN);
    if (rs.radioTech != radioTech) {
      rs.radioTech = radioTech;
      stateChanged = true;
    }

    if (stateChanged) {
      Phone.sendDOMMessage({type: "gprsregistrationstatechange",
                            gprsRegistrationState: rs});
    }
  },

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

      if (newCall) {
        // Call is still valid.
        if (newCall.state != currentCall.state) {
          // State has changed.
          currentCall.state = newCall.state;
          this._handleChangedCallState(currentCall);
        }
      } else {
        // Call is no longer reported by the radio. Remove from our map and
        // send disconnected state change.
        delete this.currentCalls[currentCall.callIndex];
        this._handleDisconnectedCall(currentCall);
      }
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
    let message = {type: "callStateChange",
                   call: {callIndex: changedCall.callIndex,
                          state: changedCall.state,
                          number: changedCall.number,
                          name: changedCall.name}};
    Phone.sendDOMMessage(message);
  },

  _handleDisconnectedCall: function _handleDisconnectedCall(disconnectedCall) {
    let message = {type: "callDisconnected",
                   call: {callIndex: disconnectedCall.callIndex}};
    Phone.sendDOMMessage(message);
  },

  _processDataCallList: function _processDataCallList(datacalls) {
    for each (let currentDataCall in this.currentDataCalls) {
      let newDataCall;
      if (datacalls) {
        newDataCall = datacalls[currentDataCall.cid];
        delete datacalls[currentDataCall.cid];
      }

      if (newDataCall) {
        switch (newDataCall.active) {
          case DATACALL_INACTIVE:
            newDataCall.state = GECKO_NETWORK_STATE_DISCONNECTED;
            break;
          case DATACALL_ACTIVE_DOWN:
            newDataCall.state = GECKO_NETWORK_STATE_SUSPENDED;
            if (RILQUIRKS_DATACALLSTATE_DOWN_IS_UP) {
              newDataCall.state = GECKO_NETWORK_STATE_CONNECTED;
            }
            break;
          case DATACALL_ACTIVE_UP:
            newDataCall.state = GECKO_NETWORK_STATE_CONNECTED;
            break;
        }
        if (newDataCall.state != currentDataCall.state) {
          currentDataCall.active = newDataCall.active;
          currentDataCall.state = newDataCall.state;
          this.sendDOMMessage({type: "datacallstatechange",
                               datacall: currentDataCall});
        }
      } else {
        delete this.currentDataCalls[currentDataCall.callIndex];
        currentDataCall.state = GECKO_NETWORK_STATE_DISCONNECTED;
        this.sendDOMMessage({type: "datacallstatechange",
                             datacall: currentDataCall});
      }
    }

    for each (let datacall in datacalls) {
      if (DEBUG) debug("Unexpected data call: " + JSON.stringify(datacall));
    }
  },


  /**
   * Handle the RIL request errors
   */ 
  handleRequestError: function handleRequestError(options) {	  
	options.type = "error";
	Phone.sendDOMMessage(options);
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
  }
};

RIL[REQUEST_GET_SIM_STATUS] = function REQUEST_GET_SIM_STATUS() {
  let iccStatus = {
    cardState:                   Buf.readUint32(), // CARD_STATE_*
    universalPINState:           Buf.readUint32(), // PINSTATE_*
    gsmUmtsSubscriptionAppIndex: Buf.readUint32(),
    setCdmaSubscriptionAppIndex: Buf.readUint32(),
    apps:                        []
  };

  let apps_length = Buf.readUint32();
  if (apps_length > CARD_MAX_APPS) {
    apps_length = CARD_MAX_APPS;
  }

  for (let i = 0 ; i < apps_length ; i++) {
    iccStatus.apps.push({
      app_type:       Buf.readUint32(), // APPTYPE_*
      app_state:      Buf.readUint32(), // CARD_APP_STATE_*
      perso_substate: Buf.readUint32(), // PERSOSUBSTATE_*
      aid:            Buf.readString(),
      app_label:      Buf.readString(),
      pin1_replaced:  Buf.readUint32(),
      pin1:           Buf.readUint32(),
      pin2:           Buf.readUint32()
    });
  }
  if (DEBUG) debug("iccStatus: " + JSON.stringify(iccStatus));
  this._processICCStatus(iccStatus);
};
RIL[REQUEST_ENTER_SIM_PIN] = function REQUEST_ENTER_SIM_PIN() {
  let response = Buf.readUint32List();
  if (DEBUG) debug("REQUEST_ENTER_SIM_PIN returned " + response);
};
RIL[REQUEST_ENTER_SIM_PUK] = function REQUEST_ENTER_SIM_PUK() {
  let response = Buf.readUint32List();
  if (DEBUG) debug("REQUEST_ENTER_SIM_PUK returned " + response);
};
RIL[REQUEST_ENTER_SIM_PIN2] = null;
RIL[REQUEST_ENTER_SIM_PUK2] = null;
RIL[REQUEST_CHANGE_SIM_PIN] = null;
RIL[REQUEST_CHANGE_SIM_PIN2] = null;
RIL[REQUEST_ENTER_NETWORK_DEPERSONALIZATION] = null;
RIL[REQUEST_GET_CURRENT_CALLS] = function REQUEST_GET_CURRENT_CALLS(length) {
  this.initRILQuirks();

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
RIL[REQUEST_DIAL] = null;
RIL[REQUEST_GET_IMSI] = function REQUEST_GET_IMSI(length) {
  this.IMSI = Buf.readString();
};
RIL[REQUEST_HANGUP] = null;
RIL[REQUEST_HANGUP_WAITING_OR_BACKGROUND] = null;
RIL[REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND] = null;
RIL[REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE] = null;
RIL[REQUEST_SWITCH_HOLDING_AND_ACTIVE] = null;
RIL[REQUEST_CONFERENCE] = null;
RIL[REQUEST_UDUB] = null;
RIL[REQUEST_LAST_CALL_FAIL_CAUSE] = null;
RIL[REQUEST_SIGNAL_STRENGTH] = function REQUEST_SIGNAL_STRENGTH() {
  let signalStrength = Buf.readUint32();
  // The SGS2 seems to compute the number of bars for us and expose those
  // instead of the actual signal strength.
  let bars = signalStrength >> 8;
  signalStrength = signalStrength & 0xff;
  let strength = {
    // Valid values are (0-31, 99) as defined in TS 27.007 8.5.
    gsmSignalStrength: signalStrength,
    // Non-standard extension by the SGS2.
    bars:              bars,
    // GSM bit error rate (0-7, 99) as defined in TS 27.007 8.5.
    gsmBitErrorRate:   Buf.readUint32(),
    // The CDMA RSSI value.
    cdmaDBM:           Buf.readUint32(),
    // The CDMA EC/IO.
    cdmaECIO:          Buf.readUint32(),
    // The EVDO RSSI value.
    evdoDBM:           Buf.readUint32(),
    // The EVDO EC/IO.
    evdoECIO:          Buf.readUint32(),
    // Valid values are 0-8.  8 is the highest signal to noise ratio
    evdoSNR:           Buf.readUint32()
  };
  if (DEBUG) debug("Signal strength " + JSON.stringify(strength));
  Phone.sendDOMMessage({type: "signalstrengthchange",
                        signalStrength: strength});
};
RIL[REQUEST_REGISTRATION_STATE] = function REQUEST_REGISTRATION_STATE(length) {
  let state = Buf.readStringList();
  this._processRegistrationState(state);
};
RIL[REQUEST_GPRS_REGISTRATION_STATE] = function REQUEST_GPRS_REGISTRATION_STATE(length) {
  let state = Buf.readStringList();
  this._processGPRSRegistrationState(state);
};
RIL[REQUEST_OPERATOR] = function REQUEST_OPERATOR(length) {
  let operator = Buf.readStringList();
  if (operator.length < 3) {
    if (DEBUG) debug("Expected at least 3 strings for operator.");
  }
  if (!this.operator ||
      this.operator.alphaLong  != operator[0] ||
      this.operator.alphaShort != operator[1] ||
      this.operator.numeric    != operator[2]) {
    this.operator = {alphaLong:  operator[0],
                     alphaShort: operator[1],
                     numeric:    operator[2]};
    Phone.sendDOMMessage({type: "operatorchange",
                          operator: this.operator});
  }
};
RIL[REQUEST_RADIO_POWER] = null;
RIL[REQUEST_DTMF] = null;
RIL[REQUEST_SEND_SMS] = function REQUEST_SEND_SMS(length, options) {
  options.messageRef = Buf.readUint32();
  options.ackPDU = Buf.readString();
  options.errorCode = Buf.readUint32();
  options.type = "sms-sent";
  Phone.sendDOMMessage(options);
};
RIL[REQUEST_SEND_SMS_EXPECT_MORE] = null;
RIL[REQUEST_SETUP_DATA_CALL] = function REQUEST_SETUP_DATA_CALL(length, options) {
  let [cid, ifname, ipaddr, dns, gw] = Buf.readStringList();
  options.cid = cid;
  options.ifname = ifname;
  options.ipaddr = ipaddr;
  options.dns = dns;
  options.gw = gw;
  options.active = DATACALL_ACTIVE_UNKNOWN;
  options.state = GECKO_NETWORK_STATE_CONNECTING;
  this.currentDataCalls[options.cid] = options;
  Phone.sendDOMMessage({type: "datacallstatechange",
                        datacall: options});

  // Let's get the list of data calls to ensure we know whether it's active
  // or not.
  this.getDataCallList();
};
RIL[REQUEST_SIM_IO] = function REQUEST_SIM_IO(length, options) {
  switch (options.fileid) {
    case ICC_EF_MSISDN:
      this._processMSISDNResponse(options);
      break;
  }
};
RIL[REQUEST_SEND_USSD] = null;
RIL[REQUEST_CANCEL_USSD] = null;
RIL[REQUEST_GET_CLIR] = null;
RIL[REQUEST_SET_CLIR] = null;
RIL[REQUEST_QUERY_CALL_FORWARD_STATUS] = null;
RIL[REQUEST_SET_CALL_FORWARD] = null;
RIL[REQUEST_QUERY_CALL_WAITING] = null;
RIL[REQUEST_SET_CALL_WAITING] = null;
RIL[REQUEST_SMS_ACKNOWLEDGE] = null;
RIL[REQUEST_GET_IMEI] = function REQUEST_GET_IMEI() {
  this.IMEI = Buf.readString();
};
RIL[REQUEST_GET_IMEISV] = function REQUEST_GET_IMEISV() {
  this.IMEISV = Buf.readString();
};
RIL[REQUEST_ANSWER] = null;
RIL[REQUEST_DEACTIVATE_DATA_CALL] = function REQUEST_DEACTIVATE_DATA_CALL(length, options) {
  let datacall = this.currentDataCalls[options.cid];
  delete this.currentDataCalls[options.cid];
  datacall.state = GECKO_NETWORK_STATE_DISCONNECTED;
  Phone.sendDOMMessage({type: "datacallstatechange",
                       datacall: datacall});
};
RIL[REQUEST_QUERY_FACILITY_LOCK] = null;
RIL[REQUEST_SET_FACILITY_LOCK] = null;
RIL[REQUEST_CHANGE_BARRING_PASSWORD] = null;
RIL[REQUEST_QUERY_NETWORK_SELECTION_MODE] = function REQUEST_QUERY_NETWORK_SELECTION_MODE() {
  let mode = Buf.readUint32List();
  this.networkSelectionMode = mode[0];
};
RIL[REQUEST_SET_NETWORK_SELECTION_AUTOMATIC] = null;
RIL[REQUEST_SET_NETWORK_SELECTION_MANUAL] = null;
RIL[REQUEST_QUERY_AVAILABLE_NETWORKS] = null;
RIL[REQUEST_DTMF_START] = null;
RIL[REQUEST_DTMF_STOP] = null;
RIL[REQUEST_BASEBAND_VERSION] = function REQUEST_BASEBAND_VERSION() {
  this.basebandVersion = Buf.readString();
};
RIL[REQUEST_SEPARATE_CONNECTION] = null;
RIL[REQUEST_SET_MUTE] = null;
RIL[REQUEST_GET_MUTE] = null;
RIL[REQUEST_QUERY_CLIP] = null;
RIL[REQUEST_LAST_DATA_CALL_FAIL_CAUSE] = null;
RIL[REQUEST_DATA_CALL_LIST] = function REQUEST_DATA_CALL_LIST(length) {
  this.initRILQuirks();

  let num = 0;
  if (length) {
    num = Buf.readUint32();
  }
  if (!num) {
    this._processDataCallList(null);
    return;
  }

  let datacalls = {};
  for (let i = 0; i < num; i++) {
    let datacall = {
      cid: Buf.readUint32().toString(),
      active: Buf.readUint32(),
      type: Buf.readString(),
      apn: Buf.readString(),
      address: Buf.readString()
    };
    datacalls[datacall.cid] = datacall;
  }

  this._processDataCallList(datacalls);
};
RIL[REQUEST_RESET_RADIO] = null;
RIL[REQUEST_OEM_HOOK_RAW] = null;
RIL[REQUEST_OEM_HOOK_STRINGS] = null;
RIL[REQUEST_SCREEN_STATE] = null;
RIL[REQUEST_SET_SUPP_SVC_NOTIFICATION] = null;
RIL[REQUEST_WRITE_SMS_TO_SIM] = null;
RIL[REQUEST_DELETE_SMS_ON_SIM] = null;
RIL[REQUEST_SET_BAND_MODE] = null;
RIL[REQUEST_QUERY_AVAILABLE_BAND_MODE] = null;
RIL[REQUEST_STK_GET_PROFILE] = null;
RIL[REQUEST_STK_SET_PROFILE] = null;
RIL[REQUEST_STK_SEND_ENVELOPE_COMMAND] = null;
RIL[REQUEST_STK_SEND_TERMINAL_RESPONSE] = null;
RIL[REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM] = null;
RIL[REQUEST_EXPLICIT_CALL_TRANSFER] = null;
RIL[REQUEST_SET_PREFERRED_NETWORK_TYPE] = null;
RIL[REQUEST_GET_PREFERRED_NETWORK_TYPE] = null;
RIL[REQUEST_GET_NEIGHBORING_CELL_IDS] = null;
RIL[REQUEST_SET_LOCATION_UPDATES] = null;
RIL[REQUEST_CDMA_SET_SUBSCRIPTION] = null;
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
RIL[REQUEST_GSM_SET_BROADCAST_SMS_CONFIG] = null;
RIL[REQUEST_GSM_SMS_BROADCAST_ACTIVATION] = null;
RIL[REQUEST_CDMA_GET_BROADCAST_SMS_CONFIG] = null;
RIL[REQUEST_CDMA_SET_BROADCAST_SMS_CONFIG] = null;
RIL[REQUEST_CDMA_SMS_BROADCAST_ACTIVATION] = null;
RIL[REQUEST_CDMA_SUBSCRIPTION] = null;
RIL[REQUEST_CDMA_WRITE_SMS_TO_RUIM] = null;
RIL[REQUEST_CDMA_DELETE_SMS_ON_RUIM] = null;
RIL[REQUEST_DEVICE_IDENTITY] = null;
RIL[REQUEST_EXIT_EMERGENCY_CALLBACK_MODE] = null;
RIL[REQUEST_GET_SMSC_ADDRESS] = function REQUEST_GET_SMSC_ADDRESS(length, options) {
  //TODO: notify main thread if we fail retrieving the SMSC, especially
  // if there was a pending SMS (bug 727319).
  this.SMSC = Buf.readString();
  // If the SMSC was not retrieved on RIL initialization, an attempt to
  // get it is triggered from this.sendSMS followed by the 'options'
  // parameter of the SMS, so that we can send it after successfully
  // retrieving the SMSC.
  if (this.SMSC && options.body) {
    this.sendSMS(options);
  }
};
RIL[REQUEST_SET_SMSC_ADDRESS] = null;
RIL[REQUEST_REPORT_SMS_MEMORY_STATUS] = null;
RIL[REQUEST_REPORT_STK_SERVICE_IS_RUNNING] = null;
RIL[UNSOLICITED_RESPONSE_RADIO_STATE_CHANGED] = function UNSOLICITED_RESPONSE_RADIO_STATE_CHANGED() {
  let newState = Buf.readUint32();
  if (DEBUG) {
    debug("Radio state changed from " + this.radioState + " to " + newState);
  }
  if (this.radioState == newState) {
    // No change in state, return.
    return;
  }

  let gsm = newState == RADIO_STATE_SIM_NOT_READY        ||
            newState == RADIO_STATE_SIM_LOCKED_OR_ABSENT ||
            newState == RADIO_STATE_SIM_READY;
  let cdma = newState == RADIO_STATE_RUIM_NOT_READY       ||
             newState == RADIO_STATE_RUIM_READY            ||
             newState == RADIO_STATE_RUIM_LOCKED_OR_ABSENT ||
             newState == RADIO_STATE_NV_NOT_READY          ||
             newState == RADIO_STATE_NV_READY;

  // Figure out state transitions and send out more RIL requests as necessary
  // as well as events to the main thread.

  if (this.radioState == RADIO_STATE_UNAVAILABLE &&
      newState != RADIO_STATE_UNAVAILABLE) {
    // The radio became available, let's get its info.
    if (gsm) {
      this.getIMEI();
      this.getIMEISV();
    }
    if (cdma) {
      this.getDeviceIdentity();
    }
    this.getBasebandVersion();
    this.setScreenState(true);
    Phone.sendDOMMessage({
      type: "radiostatechange",
      radioState: (newState == RADIO_STATE_OFF) ?
                   GECKO_RADIOSTATE_OFF : GECKO_RADIOSTATE_READY
    });

    //XXX TODO For now, just turn the radio on if it's off. for the real
    // deal we probably want to do the opposite: start with a known state
    // when we boot up and let the UI layer control the radio power.
    if (newState == RADIO_STATE_OFF) {
      this.setRadioPower(true);
    }
  }

  if (newState == RADIO_STATE_UNAVAILABLE) {
    // The radio is no longer available, we need to deal with any
    // remaining pending requests.
    //TODO do that
    Phone.sendDOMMessage({type: "radiostatechange",
                         radioState: GECKO_RADIOSTATE_UNAVAILABLE});
  }

  if (newState == RADIO_STATE_SIM_READY  ||
      newState == RADIO_STATE_RUIM_READY ||
      newState == RADIO_STATE_NV_READY) {
    // The ICC has become available. Get all the things.
    this.getICCStatus();
    this.requestNetworkInfo();
    this.getSignalStrength();
    this.getSMSCAddress();
    this.getMSISDN();
    Phone.sendDOMMessage({type: "cardstatechange",
                         cardState: GECKO_CARDSTATE_READY});
  }
  if (newState == RADIO_STATE_SIM_LOCKED_OR_ABSENT  ||
      newState == RADIO_STATE_RUIM_LOCKED_OR_ABSENT) {
    this.getICCStatus();
    Phone.sendDOMMessage({type: "cardstatechange",
                          cardState: GECKO_CARDSTATE_UNAVAILABLE});
  }

  let wasOn = this.radioState != RADIO_STATE_OFF &&
              this.radioState != RADIO_STATE_UNAVAILABLE;
  let isOn = newState != RADIO_STATE_OFF &&
             newState != RADIO_STATE_UNAVAILABLE;
  if (!wasOn && isOn) {
    //TODO
  }
  if (wasOn && !isOn) {
    //TODO
  }

  this.radioState = newState;
};
RIL[UNSOLICITED_RESPONSE_CALL_STATE_CHANGED] = function UNSOLICITED_RESPONSE_CALL_STATE_CHANGED() {
  this.getCurrentCalls();
};
RIL[UNSOLICITED_RESPONSE_NETWORK_STATE_CHANGED] = function UNSOLICITED_RESPONSE_NETWORK_STATE_CHANGED() {
  if (DEBUG) debug("Network state changed, re-requesting phone state.");
  this.requestNetworkInfo();
};
RIL[UNSOLICITED_RESPONSE_NEW_SMS] = function UNSOLICITED_RESPONSE_NEW_SMS(length) {
  if (!length) {
    if (DEBUG) debug("Received empty SMS!");
    //TODO: should we acknowledge the SMS here? maybe only after multiple
    //failures.
    return;
  }
  // An SMS is a string, but we won't read it as such, so let's read the
  // string length and then defer to PDU parsing helper.
  let messageStringLength = Buf.readUint32();
  if (DEBUG) debug("Got new SMS, length " + messageStringLength);
  let message = GsmPDUHelper.readMessage();
  if (DEBUG) debug(message);

  // Read string delimiters. See Buf.readString().
  let delimiter = Buf.readUint16();
  if (!(messageStringLength & 1)) {
    delimiter |= Buf.readUint16();
  }
  if (DEBUG) {
    if (delimiter != 0) {
      debug("Something's wrong, found string delimiter: " + delimiter);
    }
  }

  message.type = "sms-received";
  this.sendDOMMessage(message);

  //TODO: this might be a lie? do we want to wait for the mainthread to
  // report back?
  this.acknowledgeSMS(true, SMS_HANDLED);
};
RIL[UNSOLICITED_RESPONSE_NEW_SMS_STATUS_REPORT] = function UNSOLICITED_RESPONSE_NEW_SMS_STATUS_REPORT(length) {
  let info = Buf.readStringList();
  //TODO
};
RIL[UNSOLICITED_RESPONSE_NEW_SMS_ON_SIM] = function UNSOLICITED_RESPONSE_NEW_SMS_ON_SIM(length) {
  let info = Buf.readUint32List();
  //TODO
};
RIL[UNSOLICITED_ON_USSD] = null;
RIL[UNSOLICITED_ON_USSD_REQUEST] = null;

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
  let tz = parseInt(dateString.substr(17, 3), 10); // TZ is in 15 min. units
  let dst = parseInt(dateString.substr(21, 2), 10); // DST already is in local time

  let timeInSeconds = Date.UTC(year + PDU_TIMESTAMP_YEAR_OFFSET, month - 1, day,
                               hours, minutes, seconds) / 1000;

  if (isNaN(timeInSeconds)) {
    debug("NITZ failed to convert date");
  } else {
    Phone.onNITZ(timeInSeconds, tz*15, dst, now);
  }
};

RIL[UNSOLICITED_SIGNAL_STRENGTH] = function UNSOLICITED_SIGNAL_STRENGTH() {
  this[REQUEST_SIGNAL_STRENGTH]();
};
RIL[UNSOLICITED_DATA_CALL_LIST_CHANGED] = function UNSOLICITED_DATA_CALL_LIST_CHANGED(length) {
  this.getDataCallList();
};
RIL[UNSOLICITED_SUPP_SVC_NOTIFICATION] = null;
RIL[UNSOLICITED_STK_SESSION_END] = null;
RIL[UNSOLICITED_STK_PROACTIVE_COMMAND] = null;
RIL[UNSOLICITED_STK_EVENT_NOTIFY] = null;
RIL[UNSOLICITED_STK_CALL_SETUP] = null;
RIL[UNSOLICITED_SIM_SMS_STORAGE_FULL] = null;
RIL[UNSOLICITED_SIM_REFRESH] = null;
RIL[UNSOLICITED_CALL_RING] = function UNSOLICITED_CALL_RING() {
  let info;
  let isCDMA = false; //XXX TODO hard-code this for now
  if (isCDMA) {
    info = {
      isPresent:  Buf.readUint32(),
      signalType: Buf.readUint32(),
      alertPitch: Buf.readUint32(),
      signal:     Buf.readUint32()
    };
  }
  // For now we don't need to do anything here because we'll also get a
  // call state changed notification.
};
RIL[UNSOLICITED_RESPONSE_SIM_STATUS_CHANGED] = function UNSOLICITED_RESPONSE_SIM_STATUS_CHANGED() {
  this.getICCStatus();
};
RIL[UNSOLICITED_RESPONSE_CDMA_NEW_SMS] = null;
RIL[UNSOLICITED_RESPONSE_NEW_BROADCAST_SMS] = null;
RIL[UNSOLICITED_CDMA_RUIM_SMS_STORAGE_FULL] = null;
RIL[UNSOLICITED_RESTRICTED_STATE_CHANGED] = null;
RIL[UNSOLICITED_ENTER_EMERGENCY_CALLBACK_MODE] = null;
RIL[UNSOLICITED_CDMA_CALL_WAITING] = null;
RIL[UNSOLICITED_CDMA_OTA_PROVISION_STATUS] = null;
RIL[UNSOLICITED_CDMA_INFO_REC] = null;
RIL[UNSOLICITED_OEM_HOOK_RAW] = null;
RIL[UNSOLICITED_RINGBACK_TONE] = null;
RIL[UNSOLICITED_RESEND_INCALL_MUTE] = null;


/**
 * This object represents the phone's state and functionality. It is
 * essentially a state machine that's being acted upon from RIL and the
 * mainthread via postMessage communication.
 */
let Phone = {

  /**
   * Handlers for messages from the RIL. They all begin with on* and are called
   * from RIL object.
   */

  onNITZ: function onNITZ(timeInSeconds, timeZoneInMinutes, dstFlag, timeStampInMS) {
    let message = {type: "nitzTime",
                   networkTimeInSeconds: timeInSeconds,
                   networkTimeZoneInMinutes: timeZoneInMinutes,
                   dstFlag: dstFlag,
                   localTimeStampInMS: timeStampInMS};
    this.sendDOMMessage(message);
  },

  /**
   * Outgoing requests to the RIL. These can be triggered from the
   * main thread via messages that look like this:
   *
   *   {type:  "methodName",
   *    extra: "parameters",
   *    go:    "here"}
   *
   * So if one of the following methods takes arguments, it takes only one,
   * an object, which then contains all of the parameters as attributes.
   * The "@param" documentation is to be interpreted accordingly.
   */

  /**
   * Get a list of current voice calls.
   */
  enumerateCalls: function enumerateCalls() {
    if (DEBUG) debug("Sending all current calls");
    let calls = [];
    for each (let call in RIL.currentCalls) {
      calls.push(call);
    }
    this.sendDOMMessage({type: "enumerateCalls", calls: calls});
  },

  enumerateDataCalls: function enumerateDataCalls() {
    let datacall_list = [];
    for each (let datacall in RIL.currentDataCalls) {
      datacall_list.push(datacall);
    }
    this.sendDOMMessage({type: "datacalllist",
                         datacalls: datacall_list});
  },

  /**
   * Dial the phone.
   *
   * @param number
   *        String containing the number to dial.
   */
  dial: function dial(options) {
    RIL.dial(options);
  },

  /**
   * Send DTMF Tone
   *
   * @param dtmfChar
   *        String containing the DTMF signal to send.
   */
  sendTone: function sendTone(options) {
    RIL.sendTone(options);
  },

  /**
   * Start DTMF Tone
   *
   * @param dtmfChar
   *        String containing the DTMF signal to send.
   */
  startTone: function startTone(options) {
    RIL.startTone(options);
  },

  /**
   * Stop DTMF Tone
   */
  stopTone: function stopTone() {
    RIL.stopTone();
  },

  /**
   * Hang up a call.
   *
   * @param callIndex
   *        Call index of the call to hang up.
   */
  hangUp: function hangUp(options) {
    RIL.hangUp(options);
  },

  /**
   * Answer an incoming call.
   *
   * @param callIndex
   *        Call index of the call to answer.
   */
  answerCall: function answerCall(options) {
    RIL.answerCall(options);
  },

  /**
   * Reject an incoming call.
   *
   * @param callIndex
   *        Call index of the call to reject.
   */
  rejectCall: function rejectCall(options) {
    RIL.rejectCall(options);
  },

  /**
   * Send an SMS.
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
    RIL.sendSms(options);
  },

  /**
   * Setup a data call (PDP).
   */
  setupDataCall: function setupDataCall(options) {
    if (DEBUG) debug("setupDataCall: " + JSON.stringify(options));
    RIL.setupDataCall(options);
  },

  /**
   * Deactivate a data call (PDP).
   */
  deactivateDataCall: function deactivateDataCall(options) {
    RIL.deactivateDataCall(options);
  },

  /**
   * Get the list of data calls.
   */
  getDataCallList: function getDataCallList(options) {
    RIL.getDataCallList();
  },

  /**
   * Get failure cause code for the last failed PDP context.
   */
  getFailCauseCode: function getFailCauseCode(options) {
    RIL.getFailCauseCode();
  },

  /**
   * Handle incoming messages from the main UI thread.
   *
   * @param message
   *        Object containing the message. Messages are supposed
   */
  handleDOMMessage: function handleMessage(message) {
    if (DEBUG) debug("Received DOM message " + JSON.stringify(message));
    let method = this[message.type];
    if (typeof method != "function") {
      if (DEBUG) {
        debug("Don't know what to do with message " + JSON.stringify(message));
      }
      return;
    }
    method.call(this, message);
  },

  /**
   * Send messages to the main UI thread.
   */
  sendDOMMessage: function sendDOMMessage(message) {
    postMessage(message, "*");
  }

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
   * List of tuples of national language identifier pairs.
   */
  enabledGsmTableTuples: [
    [PDU_NL_IDENTIFIER_DEFAULT, PDU_NL_IDENTIFIER_DEFAULT],
  ],

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
   * Read a *swapped nibble* binary coded decimal (BCD)
   *
   * @param length
   *        Number of nibble *pairs* to read.
   *
   * @return the decimal as a number.
   */
  readSwappedNibbleBCD: function readSwappedNibbleBCD(length) {
    let number = 0;
    for (let i = 0; i < length; i++) {
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
   *  Read a string from Buf and convert it to BCD
   * 
   *  @return the decimal as a number.
   */ 
  readStringAsBCD: function readStringAsBCD() {
    let length = Buf.readUint32();
    let bcd = this.readSwappedNibbleBCD(length / 2);
    let delimiter = Buf.readUint16();
    if (!(length & 1)) {
      delimiter |= Buf.readUint16();
    }
    if (DEBUG) {
      if (delimiter != 0) {
        debug("Something's wrong, found string delimiter: " + delimiter);
      }
    }
    return bcd;
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
      let septet = langTable.indexOf(message[i]);
      if (septet == PDU_NL_EXTENDED_ESCAPE) {
        continue;
      }

      if (septet >= 0) {
        data |= septet << dataBits;
        dataBits += 7;
      } else {
        septet = langShiftTable.indexOf(message[i]);
        if (septet == -1) {
          throw new Error(message[i] + " not in 7 bit alphabet "
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
   * Read user data and decode as a UCS2 string.
   *
   * @param numOctets
   *        num of octets to read as UCS2 string.
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
   * Calculate encoded length using specified locking/single shift table
   *
   * @param message
   *        message string to be encoded.
   * @param langTable
   *        locking shift table string.
   * @param langShiftTable
   *        single shift table string.
   *
   * @return encoded length in septets.
   *
   * @note that the algorithm used in this function must match exactly with
   * #writeStringAsSeptets.
   */
  _calculateLangEncodedSeptets: function _calculateLangEncodedSeptets(message, langTable, langShiftTable) {
    let length = 0;
    for (let msgIndex = 0; msgIndex < message.length; msgIndex++) {
      let septet = langTable.indexOf(message.charAt(msgIndex));

      // According to 3GPP TS 23.038, section 6.1.1 General notes, "The
      // characters marked '1)' are not used but are displayed as a space."
      if (septet == PDU_NL_EXTENDED_ESCAPE) {
        continue;
      }

      if (septet >= 0) {
        length++;
        continue;
      }

      septet = langShiftTable.indexOf(message.charAt(msgIndex));
      if (septet == -1) {
        return -1;
      }

      // According to 3GPP TS 23.038 B.2, "This code represents a control
      // character and therefore must not be used for language specific
      // characters."
      if (septet == PDU_NL_RESERVED_CONTROL) {
        continue;
      }

      // The character is not found in locking shfit table, but could be
      // encoded as <escape><char> with single shift table. Note that it's
      // still possible for septet to has the value of PDU_NL_EXTENDED_ESCAPE,
      // but we can display it as a space in this case as said in previous
      // comment.
      length += 2;
    }

    return length;
  },

  /**
   * Calculate user data length and its encoding.
   *
   * The `options` parameter object should contain the `body` attribute, and
   * the `dcs`, `userDataHeaderLength`, `encodedBodyLength`, `langIndex`,
   * `langShiftIndex` attributes will be set as return:
   *
   * @param body
   *        String containing the message body.
   * @param dcs
   *        Data coding scheme. One of the PDU_DCS_MSG_CODING_*BITS_ALPHABET
   *        constants.
   * @param userDataHeaderLength
   *        Length of embedded user data header, in bytes. The whole header
   *        size will be userDataHeaderLength + 1; 0 for no header.
   * @param encodedBodyLength
   *        Length of the message body when encoded with the given DCS. For
   *        UCS2, in bytes; for 7-bit, in septets.
   * @param langIndex
   *        Table index used for normal 7-bit encoded character lookup.
   * @param langShiftIndex
   *        Table index used for escaped 7-bit encoded character lookup.
   */
  calculateUserDataLength: function calculateUserDataLength(options) {
    //TODO: support multipart SMS, see bug 712933
    options.dcs = PDU_DCS_MSG_CODING_7BITS_ALPHABET;
    options.langIndex = PDU_NL_IDENTIFIER_DEFAULT;
    options.langShiftIndex = PDU_NL_IDENTIFIER_DEFAULT;
    options.encodedBodyLength = 0;
    options.userDataHeaderLength = 0;

    let needUCS2 = true;
    let minUserDataSeptets = Number.MAX_VALUE;
    for (let i = 0; i < this.enabledGsmTableTuples.length; i++) {
      let [langIndex, langShiftIndex] = this.enabledGsmTableTuples[i];

      const langTable = PDU_NL_LOCKING_SHIFT_TABLES[langIndex];
      const langShiftTable = PDU_NL_SINGLE_SHIFT_TABLES[langShiftIndex];

      let bodySeptets = this._calculateLangEncodedSeptets(options.body,
                                                          langTable,
                                                          langShiftTable);
      if (bodySeptets < 0) {
        continue;
      }

      let headerLen = 0;
      if (langIndex != PDU_NL_IDENTIFIER_DEFAULT) {
        headerLen += 3; // IEI + len + langIndex
      }
      if (langShiftIndex != PDU_NL_IDENTIFIER_DEFAULT) {
        headerLen += 3; // IEI + len + langShiftIndex
      }

      // Calculate full user data length, note the extra byte is for header len
      let headerSeptets = Math.ceil((headerLen ? headerLen + 1 : 0) * 8 / 7);
      let userDataSeptets = bodySeptets + headerSeptets;
      if (userDataSeptets >= minUserDataSeptets) {
        continue;
      }

      needUCS2 = false;
      minUserDataSeptets = userDataSeptets;

      options.encodedBodyLength = bodySeptets;
      options.userDataHeaderLength = headerLen;
      options.langIndex = langIndex;
      options.langShiftIndex = langShiftIndex;
    }

    if (needUCS2) {
      options.dcs = PDU_DCS_MSG_CODING_16BITS_ALPHABET;
      options.encodedBodyLength = options.body.length * 2;
      options.userDataHeaderLength = 0;
    }
  },

  /**
   * Read 1 + UDHL octets and construct user data header at return.
   *
   * @return A header object with properties contained in received message.
   * The properties set include:
   * <ul>
   * <li>length: totoal length of the header, default 0.
   * <li>langIndex: used locking shift table index, default
   *     PDU_NL_IDENTIFIER_DEFAULT.
   * <li>langShiftIndex: used locking shift table index, default
   *     PDU_NL_IDENTIFIER_DEFAULT.
   * </ul>
   */
  readUserDataHeader: function readUserDataHeader() {
    let header = {
      length: 0,
      langIndex: PDU_NL_IDENTIFIER_DEFAULT,
      langShiftIndex: PDU_NL_IDENTIFIER_DEFAULT
    };

    header.length = this.readHexOctet();
    let dataAvailable = header.length;
    while (dataAvailable >= 2) {
      let id = this.readHexOctet();
      let length = this.readHexOctet();
      dataAvailable -= 2;

      switch (id) {
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

    return header;
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
  },

  /**
   * User data can be 7 bit (default alphabet) data, 8 bit data, or 16 bit
   * (UCS2) data.
   */
  readUserData: function readUserData(length, codingScheme, hasHeader) {
    if (DEBUG) {
      debug("Reading " + length + " bytes of user data.");
      debug("Coding scheme: " + codingScheme);
    }
    // 7 bit is the default fallback encoding.
    let encoding = PDU_DCS_MSG_CODING_7BITS_ALPHABET;
    switch (codingScheme & 0xC0) {
      case 0x0:
        // bits 7..4 = 00xx
        switch (codingScheme & 0x0C) {
          case 0x4:
            encoding = PDU_DCS_MSG_CODING_8BITS_ALPHABET;
            break;
          case 0x8:
            encoding = PDU_DCS_MSG_CODING_16BITS_ALPHABET;
            break;
        }
        break;
      case 0xC0:
        // bits 7..4 = 11xx
        switch (codingScheme & 0x30) {
          case 0x20:
            encoding = PDU_DCS_MSG_CODING_16BITS_ALPHABET;
            break;
          case 0x30:
            if (!codingScheme & 0x04) {
              encoding = PDU_DCS_MSG_CODING_8BITS_ALPHABET;
            }
            break;
        }
        break;
      default:
        // Falling back to default encoding.
        break;
    }

    let header;
    let paddingBits = 0;
    if (hasHeader) {
      header = this.readUserDataHeader();

      if (encoding == PDU_DCS_MSG_CODING_7BITS_ALPHABET) {
        let headerBits = (header.length + 1) * 8;
        let headerSeptets = Math.ceil(headerBits / 7);

        length -= headerSeptets;
        paddingBits = headerSeptets * 7 - headerBits;
      } else {
        length -= (header.length + 1);
      }
    }

    if (DEBUG) debug("PDU: message encoding is " + encoding + " bit.");
    switch (encoding) {
      case PDU_DCS_MSG_CODING_7BITS_ALPHABET:
        // 7 bit encoding allows 140 octets, which means 160 characters
        // ((140x8) / 7 = 160 chars)
        if (length > PDU_MAX_USER_DATA_7BIT) {
          if (DEBUG) debug("PDU error: user data is too long: " + length);
          return null;
        }

        return this.readSeptetsToString(length,
                                        paddingBits,
                                        hasHeader ? header.langIndex : PDU_NL_IDENTIFIER_DEFAULT,
                                        hasHeader ? header.langShiftIndex : PDU_NL_IDENTIFIER_DEFAULT);
      case PDU_DCS_MSG_CODING_8BITS_ALPHABET:
        // Unsupported.
        return null;
      case PDU_DCS_MSG_CODING_16BITS_ALPHABET:
        return this.readUCS2String(length);
    }
    return null;
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
      SMSC:      null,
      reference: null,
      sender:    null,
      body:      null,
      validity:  null,
      timestamp: null
    };

    // SMSC info
    let smscLength = this.readHexOctet();
    if (smscLength > 0) {
      let smscTypeOfAddress = this.readHexOctet();
      // Subtract the type-of-address octet we just read from the length.
      msg.SMSC = this.readSwappedNibbleBCD(smscLength - 1).toString();
      if ((smscTypeOfAddress >> 4) == (PDU_TOA_INTERNATIONAL >> 4)) {
        msg.SMSC = '+' + msg.SMSC;
      }
    }

    // First octet of this SMS-DELIVER or SMS-SUBMIT message
    let firstOctet = this.readHexOctet();

    // User data header indicator
    let hasUserDataHeader = firstOctet & PDU_UDHI;

    // if the sms is of SMS-SUBMIT type it would contain a TP-MR
    let isSmsSubmit = firstOctet & PDU_MTI_SMS_SUBMIT;
    if (isSmsSubmit) {
      msg.reference = this.readHexOctet(); // TP-Message-Reference
    }

    // - Sender Address info -
    // Address length
    let senderAddressLength = this.readHexOctet();
    if (senderAddressLength <= 0) {
      if (DEBUG) debug("PDU error: invalid sender address length: " + senderAddressLength);
      return null;
    }
    // Type-of-Address
    let senderTypeOfAddress = this.readHexOctet();
    if (senderAddressLength % 2 == 1) {
      senderAddressLength += 1;
    }
    if (DEBUG) debug("PDU: Going to read sender address: " + senderAddressLength);
    msg.sender = this.readSwappedNibbleBCD(senderAddressLength / 2).toString();
    if (msg.sender.length <= 0) {
      if (DEBUG) debug("PDU error: no sender number provided");
      return null;
    }
    if ((senderTypeOfAddress >> 4) == (PDU_TOA_INTERNATIONAL >> 4)) {
      msg.sender = '+' + msg.sender;
    }

    // - TP-Protocolo-Identifier -
    let protocolIdentifier = this.readHexOctet();

    // - TP-Data-Coding-Scheme -
    let dataCodingScheme = this.readHexOctet();

    // SMS of SMS-SUBMIT type contains a TP-Service-Center-Time-Stamp field
    // SMS of SMS-DELIVER type contains a TP-Validity-Period octet
    if (isSmsSubmit) {
      //  - TP-Validity-Period -
      //  The Validity Period octet is optional. Depends on the SMS-SUBMIT
      //  first octet
      //  Validity Period Format. Bit4 and Bit3 specify the TP-VP field
      //  according to this table:
      //  bit4 bit3
      //    0   0 : TP-VP field not present
      //    1   0 : TP-VP field present. Relative format (one octet)
      //    0   1 : TP-VP field present. Enhanced format (7 octets)
      //    1   1 : TP-VP field present. Absolute format (7 octets)
      if (firstOctet & (PDU_VPF_ABSOLUTE | PDU_VPF_RELATIVE | PDU_VPF_ENHANCED)) {
        msg.validity = this.readHexOctet();
      }
      //TODO: check validity period
    } else {
      // - TP-Service-Center-Time-Stamp -
      let year   = this.readSwappedNibbleBCD(1) + PDU_TIMESTAMP_YEAR_OFFSET;
      let month  = this.readSwappedNibbleBCD(1) - 1;
      let day    = this.readSwappedNibbleBCD(1);
      let hour   = this.readSwappedNibbleBCD(1);
      let minute = this.readSwappedNibbleBCD(1);
      let second = this.readSwappedNibbleBCD(1);
      msg.timestamp = Date.UTC(year, month, day, hour, minute, second);

      // If the most significant bit of the least significant nibble is 1,
      // the timezone offset is negative (fourth bit from the right => 0x08).
      let tzOctet = this.readHexOctet();
      let tzOffset = this.octetToBCD(tzOctet & ~0x08) * 15 * 60 * 1000;
      if (tzOctet & 0x08) {
        msg.timestamp -= tzOffset;
      } else {
        msg.timestamp += tzOffset;
      }
    }

    // - TP-User-Data-Length -
    let userDataLength = this.readHexOctet();

    // - TP-User-Data -
    if (userDataLength > 0) {
      msg.body = this.readUserData(userDataLength,
                                   dataCodingScheme,
                                   hasUserDataHeader);
    }

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
  }
};

/**
 * Global stuff.
 */

if (!this.debug) {
  // Debugging stub that goes nowhere.
  this.debug = function debug(message) {
    dump("RIL Worker: " + message + "\n");
  };
}

// Initialize buffers. This is a separate function so that unit tests can
// re-initialize the buffers at will.
Buf.init();

function onRILMessage(data) {
  Buf.processIncoming(data);
};

onmessage = function onmessage(event) {
  Phone.handleDOMMessage(event.data);
};

onerror = function onerror(event) {
  debug("RIL Worker error" + event.message + "\n");
};
