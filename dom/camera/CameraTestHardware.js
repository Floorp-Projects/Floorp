/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/DOMRequestHelper.jsm");

const MOZ_CAMERATESTHW_CONTRACTID = "@mozilla.org/cameratesthardware;1";
const MOZ_CAMERATESTHW_CID        = Components.ID("{fcb7b4cd-689e-453c-8a2c-611a45fa09ac}");
const DEBUG = false;

function debug(msg) {
  if (DEBUG) {
    dump('-*- MozCameraTestHardware: ' + msg + '\n');
  }
}

function MozCameraTestHardware() {
  this._params = {};
}

MozCameraTestHardware.prototype = {
  classID:        MOZ_CAMERATESTHW_CID,
  contractID:     MOZ_CAMERATESTHW_CONTRACTID,

  classInfo:      XPCOMUtils.generateCI({classID: MOZ_CAMERATESTHW_CID,
                                         contractID: MOZ_CAMERATESTHW_CONTRACTID,
                                         flags: Ci.nsIClassInfo.SINGLETON,
                                         interfaces: [Ci.nsICameraTestHardware]}),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsICameraTestHardware]),

  _params: null,
  _window: null,
  _mock: null,
  _handler: null,

  attach: function(mock) {
    /* Waive xrays permits us to call functions provided to us
       in the mock */
    this._mock = Components.utils.waiveXrays(mock);
  },

  detach: function() {
    this._mock = null;
  },

  /* Trigger a delegate handler attached to the test hardware
     if given via attach. If there is no delegate attached, or
     it does not provide a handler for this specific operation,
     or the handler returns true, it will execute the default
     behaviour. The handler may throw an exception in order to
     return an error code from the driver call. */
  _delegate: function(prop) {
    return (this._mock && this._mock[prop] && !this._mock[prop]());
  },

  get params() {
    return this._params;
  },

  set params(aParams) {
    this._params = aParams;
  },

  setHandler: function(handler) {
    this._handler = handler;
  },

  dispatchEvent: function(evt) {
    var self = this;
    /* We should not dispatch the event in the current thread
       context because it may be directly from a driver call
       and we could hit a deadlock situation. */
    this._window.setTimeout(function() {
      if (self._handler) {
        self._handler.handleEvent(evt);
      }
    }, 0);
  },

  reset: function(aWindow) {
    this._window = aWindow;
    this._mock = null;
    this._params = {};
  },

  initCamera: function() {
    this._delegate('init');
  },

  pushParameters: function(params) {
    let oldParams = this._params;
    this._params = {};
    let s = params.split(';');
    for(let i = 0; i < s.length; ++i) {
      let parts = s[i].split('=');
      if (parts.length == 2) {
        this._params[parts[0]] = parts[1];
      }
    }
    try {
      this._delegate('pushParameters');
    } catch(e) {
      this._params = oldParams;
      throw e;
    }
  },

  pullParameters: function() {
    this._delegate('pullParameters');
    let ret = "";
    for(let p in this._params) {
      ret += p + "=" + this._params[p] + ";";
    }
    return ret;
  },

  autoFocus: function() {
    if (!this._delegate('autoFocus')) {
      this.fireAutoFocusComplete(true);
    }
  },

  fireAutoFocusMoving: function(moving) {
    let evt = new this._window.CameraStateChangeEvent('focus', { 'newState': moving ? 'focusing' : 'not_focusing' } );
    this.dispatchEvent(evt);
  },

  fireAutoFocusComplete: function(state) {
    let evt = new this._window.CameraStateChangeEvent('focus', { 'newState': state ? 'focused' : 'unfocused' } );
    this.dispatchEvent(evt);
  },

  cancelAutoFocus: function() {
    this._delegate('cancelAutoFocus');
  },

  fireShutter: function() {
    let evt = new this._window.Event('shutter');
    this.dispatchEvent(evt);
  },

  takePicture: function() {
    if (!this._delegate('takePicture')) {
      this.fireTakePictureComplete(new this._window.Blob(['foobar'], {'type': 'jpeg'}));
    }
  },

  fireTakePictureComplete: function(blob) {
    let evt = new this._window.BlobEvent('picture', {'data': blob});
    this.dispatchEvent(evt);
  },

  fireTakePictureError: function() {
    let evt = new this._window.ErrorEvent('error', {'message': 'picture'});
    this.dispatchEvent(evt);
  },

  cancelTakePicture: function() {
    this._delegate('cancelTakePicture');
  },

  startPreview: function() {
    this._delegate('startPreview');
  },

  stopPreview: function() {
    this._delegate('stopPreview');
  },

  startFaceDetection: function() {
    this._delegate('startFaceDetection');
  },

  stopFaceDetection: function() {
    this._delegate('stopFaceDetection');
  },

  fireFacesDetected: function(faces) {
    /* This works around the fact that we can't have references to
       dictionaries in a dictionary in WebIDL; we provide a boolean
       to indicate whether or not the values for those features are
       actually valid. */
    let facesIf = [];
    if (typeof(faces) === 'object' && typeof(faces.faces) === 'object') {
      let self = this;
      faces.faces.forEach(function(face) {
        face.hasLeftEye = face.hasOwnProperty('leftEye') && face.leftEye != null;
        face.hasRightEye = face.hasOwnProperty('rightEye') && face.rightEye != null;
        face.hasMouth = face.hasOwnProperty('mouth') && face.mouth != null;
        facesIf.push(new self._window.CameraDetectedFace(face));
      });
    }

    let evt = new this._window.CameraFacesDetectedEvent('facesdetected', {'faces': facesIf});
    this.dispatchEvent(evt);
  },

  startRecording: function() {
    this._delegate('startRecording');
  },

  stopRecording: function() {
    this._delegate('stopRecording');
  },

  fireSystemError: function() {
    let evt = new this._window.ErrorEvent('error', {'message': 'system'});
    this.dispatchEvent(evt);
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([MozCameraTestHardware]);
