/* global require */
/* global DEBUG, DEBUG_WORKER */
/* global RESPONSE_TYPE_SOLICITED */
/* global RESPONSE_TYPE_UNSOLICITED */

"use strict";

/**
 * This is a specialized worker buffer for the Parcel protocol.
 *
 * NOTE: To prevent including/importing twice, this file should be included
 * in a file which already includes 'ril_consts.js' and 'require.js'.
 */
(function(exports) {

  // Set to true in ril_consts.js to see debug messages
  let DEBUG = DEBUG_WORKER;
  // Need to inherit it.
  let Buf = require("resource://gre/modules/workers/worker_buf.js").Buf;

  let BufObject = function(aContext) {
    this.context = aContext;
    // This gets incremented each time we send out a parcel.
    this.mToken = 1;
    // Maps tokens we send out with requests to the request type, so that
    // when we get a response parcel back, we know what request it was for.
    this.mTokenRequestMap = new Map();
    // This is because the underlying 'Buf' is still using the 'init' pattern, so
    // this derived one needs to invoke it.
    // Using 'apply' style to mark it's a parent method calling explicitly.
    Buf._init.apply(this);

    // Remapping the request type to different values based on RIL version.
    // We only have to do this for SUBSCRIPTION right now, so I just make it
    // simple. A generic logic or structure could be discussed if we have more
    // use cases, especially the cases from different partners.
    this._requestMap = {};
    // RIL version 8.
    // For the CAF's proprietary parcels. Please see
    // https://www.codeaurora.org/cgit/quic/la/platform/hardware/ril/tree/include/telephony/ril.h?h=b2g_jb_3.2
    let map = {};
    map[REQUEST_SET_UICC_SUBSCRIPTION] = 114;
    map[REQUEST_SET_DATA_SUBSCRIPTION] = 115;
    this._requestMap[8] = map;
    // RIL version 9.
    // For the CAF's proprietary parcels. Please see
    // https://www.codeaurora.org/cgit/quic/la/platform/hardware/ril/tree/include/telephony/ril.h?h=b2g_kk_3.5
    map = {};
    map[REQUEST_SET_UICC_SUBSCRIPTION] = 115;
    map[REQUEST_SET_DATA_SUBSCRIPTION] = 116;
    this._requestMap[9] = map;
  };

  /**
   * "inherit" the basic worker buffer.
   */
  BufObject.prototype = Object.create(Buf);

  /**
   * Process one parcel.
   */
  BufObject.prototype.processParcel = function() {
    let responseType = this.readInt32();

    let requestType, options;
    if (responseType == RESPONSE_TYPE_SOLICITED) {
      let token = this.readInt32();
      let error = this.readInt32();

      options = this.mTokenRequestMap.get(token);
      if (!options) {
        if (DEBUG) {
          this.context.debug("Suspicious uninvited request found: " +
                             token + ". Ignored!");
        }
        return;
      }

      this.mTokenRequestMap.delete(token);
      requestType = options.rilRequestType;

      options.rilRequestError = error;
      if (DEBUG) {
        this.context.debug("Solicited response for request type " + requestType +
                           ", token " + token + ", error " + error);
      }
    } else if (responseType == RESPONSE_TYPE_UNSOLICITED) {
      requestType = this.readInt32();
      if (DEBUG) {
        this.context.debug("Unsolicited response for request type " + requestType);
      }
    } else {
      if (DEBUG) {
        this.context.debug("Unknown response type: " + responseType);
      }
      return;
    }

    this.context.RIL.handleParcel(requestType, this.readAvailable, options);
  };

  /**
   * Start a new outgoing parcel.
   *
   * @param type
   *        Integer specifying the request type.
   * @param options [optional]
   *        Object containing information about the request, e.g. the
   *        original main thread message object that led to the RIL request.
   */
  BufObject.prototype.newParcel = function(type, options) {
    if (DEBUG) {
      this.context.debug("New outgoing parcel of type " + type);
    }

    // We're going to leave room for the parcel size at the beginning.
    this.outgoingIndex = this.PARCEL_SIZE_SIZE;
    this.writeInt32(this._reMapRequestType(type));
    this.writeInt32(this.mToken);

    if (!options) {
      options = {};
    }
    options.rilRequestType = type;
    options.rilRequestError = null;
    this.mTokenRequestMap.set(this.mToken, options);
    this.mToken++;
    return this.mToken;
  };

  BufObject.prototype.simpleRequest = function(type, options) {
    this.newParcel(type, options);
    this.sendParcel();
  };

  BufObject.prototype.onSendParcel = function(parcel) {
    self.postRILMessage(this.context.clientId, parcel);
  };

  /**
   * Remapping the request type to different values based on RIL version.
   * We only have to do this for SUBSCRIPTION right now, so I just make it
   * simple. A generic logic or structure could be discussed if we have more
   * use cases, especially the cases from different partners.
   */
  BufObject.prototype._reMapRequestType = function(type) {
    for (let version in this._requestMap) {
      if (this.context.RIL.version <= version) {
        let newType = this._requestMap[version][type];
        if (newType) {
          if (DEBUG) {
            this.context.debug("Remap request type to " + newType);
          }
          return newType;
        }
      }
    }
    return type;
  };

  // Before we make sure to form it as a module would not add extra
  // overhead of module loading, we need to define it in this way
  // rather than 'module.exports' it as a module component.
  exports.BufObject = BufObject;
})(self); // in worker self is the global

