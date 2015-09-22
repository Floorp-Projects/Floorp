/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {Cc, Ci, Cu, CC} = require("chrome");
const Services = require("Services");
const protocol = require("devtools/server/protocol");
const {method, RetVal} = protocol;
const promise = require("promise");
const {LongStringActor} = require("devtools/server/actors/string");
const {DebuggerServer} = require("devtools/server/main");
const {getSystemInfo, getSetting} = require("devtools/shared/shared/system");

Cu.import("resource://gre/modules/PermissionsTable.jsm")

var DeviceActor = exports.DeviceActor = protocol.ActorClass({
  typeName: "device",

  _desc: null,

  getDescription: method(function() {
    return getSystemInfo();
  }, {request: {},response: { value: RetVal("json")}}),

  getWallpaper: method(function() {
    let deferred = promise.defer();
    getSetting("wallpaper.image").then((blob) => {
      let FileReader = CC("@mozilla.org/files/filereader;1");
      let reader = new FileReader();
      let conn = this.conn;
      reader.addEventListener("load", function() {
        let str = new LongStringActor(conn, reader.result);
        deferred.resolve(str);
      });
      reader.addEventListener("error", function() {
        deferred.reject(reader.error);
      });
      reader.readAsDataURL(blob);
    });
    return deferred.promise;
  }, {request: {},response: { value: RetVal("longstring")}}),

  screenshotToDataURL: method(function() {
    let window = Services.wm.getMostRecentWindow(DebuggerServer.chromeWindowType);
    var devicePixelRatio = window.devicePixelRatio;
    let canvas = window.document.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
    let width = window.innerWidth;
    let height = window.innerHeight;
    canvas.setAttribute('width', Math.round(width * devicePixelRatio));
    canvas.setAttribute('height', Math.round(height * devicePixelRatio));
    let context = canvas.getContext('2d');
    let flags =
          context.DRAWWINDOW_DRAW_CARET |
          context.DRAWWINDOW_DRAW_VIEW |
          context.DRAWWINDOW_USE_WIDGET_LAYERS;
    context.scale(devicePixelRatio, devicePixelRatio);
    context.drawWindow(window, 0, 0, width, height, 'rgb(255,255,255)', flags);
    let dataURL = canvas.toDataURL('image/png')
    return new LongStringActor(this.conn, dataURL);
  }, {request: {},response: { value: RetVal("longstring")}}),

  getRawPermissionsTable: method(function() {
    return {
      rawPermissionsTable: PermissionsTable,
      UNKNOWN_ACTION: Ci.nsIPermissionManager.UNKNOWN_ACTION,
      ALLOW_ACTION: Ci.nsIPermissionManager.ALLOW_ACTION,
      DENY_ACTION: Ci.nsIPermissionManager.DENY_ACTION,
      PROMPT_ACTION: Ci.nsIPermissionManager.PROMPT_ACTION
    };
  }, {request: {},response: { value: RetVal("json")}})
});

var DeviceFront = protocol.FrontClass(DeviceActor, {
  initialize: function(client, form) {
    protocol.Front.prototype.initialize.call(this, client);
    this.actorID = form.deviceActor;
    this.manage(this);
  },

  screenshotToBlob: function() {
    return this.screenshotToDataURL().then(longstr => {
      return longstr.string().then(dataURL => {
        let deferred = promise.defer();
        longstr.release().then(null, Cu.reportError);
        let req = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].createInstance(Ci.nsIXMLHttpRequest);
        req.open("GET", dataURL, true);
        req.responseType = "blob";
        req.onload = () => {
          deferred.resolve(req.response);
        };
        req.onerror = () => {
          deferred.reject(req.status);
        }
        req.send();
        return deferred.promise;
      });
    });
  },
});

const _knownDeviceFronts = new WeakMap();

exports.getDeviceFront = function(client, form) {
  if (!form.deviceActor) {
    return null;
  }

  if (_knownDeviceFronts.has(client)) {
    return _knownDeviceFronts.get(client);
  }

  let front = new DeviceFront(client, form);
  _knownDeviceFronts.set(client, front);
  return front;
};
