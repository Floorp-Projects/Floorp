/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {Ci, Cu} = require("chrome");
const Services = require("Services");
const protocol = require("devtools/shared/protocol");
const promise = require("promise");
const {LongStringActor} = require("devtools/server/actors/string");
const {DebuggerServer} = require("devtools/server/main");
const {getSystemInfo, getSetting} = require("devtools/shared/system");
const {deviceSpec} = require("devtools/shared/specs/device");

Cu.importGlobalProperties(["FileReader"]);
Cu.import("resource://gre/modules/PermissionsTable.jsm");

var DeviceActor = exports.DeviceActor = protocol.ActorClassWithSpec(deviceSpec, {
  _desc: null,

  getDescription: function () {
    return getSystemInfo();
  },

  getWallpaper: function () {
    let deferred = promise.defer();
    getSetting("wallpaper.image").then((blob) => {
      let reader = new FileReader();
      let conn = this.conn;
      reader.addEventListener("load", function () {
        let str = new LongStringActor(conn, reader.result);
        deferred.resolve(str);
      });
      reader.addEventListener("error", function () {
        deferred.reject(reader.error);
      });
      reader.readAsDataURL(blob);
    });
    return deferred.promise;
  },

  screenshotToDataURL: function () {
    let window = Services.wm.getMostRecentWindow(DebuggerServer.chromeWindowType);
    var devicePixelRatio = window.devicePixelRatio;
    let canvas = window.document.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
    let width = window.innerWidth;
    let height = window.innerHeight;
    canvas.setAttribute("width", Math.round(width * devicePixelRatio));
    canvas.setAttribute("height", Math.round(height * devicePixelRatio));
    let context = canvas.getContext("2d");
    let flags =
          context.DRAWWINDOW_DRAW_CARET |
          context.DRAWWINDOW_DRAW_VIEW |
          context.DRAWWINDOW_USE_WIDGET_LAYERS;
    context.scale(devicePixelRatio, devicePixelRatio);
    context.drawWindow(window, 0, 0, width, height, "rgb(255,255,255)", flags);
    let dataURL = canvas.toDataURL("image/png");
    return new LongStringActor(this.conn, dataURL);
  },

  getRawPermissionsTable: function () {
    return {
      rawPermissionsTable: PermissionsTable,
      UNKNOWN_ACTION: Ci.nsIPermissionManager.UNKNOWN_ACTION,
      ALLOW_ACTION: Ci.nsIPermissionManager.ALLOW_ACTION,
      DENY_ACTION: Ci.nsIPermissionManager.DENY_ACTION,
      PROMPT_ACTION: Ci.nsIPermissionManager.PROMPT_ACTION
    };
  }
});
