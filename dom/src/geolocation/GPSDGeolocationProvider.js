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
 * The Original Code is Geolocation.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Martin McNickle <mmcnickle@gmail.com>
 *
 * Based on static_geolocation_provider.js by:
 *  Doug Turner <dougt@meer.net>
 *
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

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

const Ci = Components.interfaces;
const Cc = Components.classes;

var gLoggingEnabled = false;

function LOG(aMsg) {
  if (gLoggingEnabled)
  {
    aMsg = ("*** GPSD GEO: " + aMsg);
    Cc["@mozilla.org/consoleservice;1"].getService(Ci.nsIConsoleService).logStringMessage(aMsg);
    dump(aMsg);
  }
}

function GeoPositionCoordsObject(latitude, longitude, altitude, accuracy, altitudeAccuracy, heading, speed) {

  this.latitude = latitude;
  this.longitude = longitude;
  this.altitude = altitude;
  this.accuracy = accuracy;
  this.altitudeAccuracy = altitudeAccuracy;
  this.heading = heading;
  this.speed = speed;

};

GeoPositionCoordsObject.prototype = {

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMGeoPositionCoords, Ci.nsIClassInfo]),
  
  // Class Info is required to be able to pass objects back into the DOM.
  getInterfaces: function(countRef) {
    var interfaces = [Ci.nsIDOMGeoPositionCoords, Ci.nsIClassInfo, Ci.nsISupports];
    countRef.value = interfaces.length;
    return interfaces;
  },
  
  getHelperForLanguage: function(language) null,
  contractID: null,
  classDescription: "Geoposition Coordinate Object",
  classID: null,
  implementationLanguage: Ci.nsIProgrammingLanguage.JAVASCRIPT,
  flags: Ci.nsIClassInfo.DOM_OBJECT,

  latitude: null,
  longitude: null,
  altitude: null,
  accuracy: null,
  altitudeAccuracy: null,
  heading: null,
  speed: null,

};

function GeoPositionObject(latitude, longitude, altitude, accuracy, altitudeAccuracy, heading, speed, timestamp) {
  this.coords = new GeoPositionCoordsObject(latitude, longitude, altitude, accuracy, altitudeAccuracy, heading, speed);
  this.timestamp = timestamp;
};

GeoPositionObject.prototype = {

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMGeoPosition, Ci.nsIClassInfo]),

  // Class Info is required to be able to pass objects back into the DOM.
  getInterfaces: function(countRef) {
    var interfaces = [Ci.nsIDOMGeoPosition, Ci.nsIClassInfo, Ci.nsISupports];
    countRef.value = interfaces.length;
    return interfaces;
  },

  getHelperForLanguage: function(language) null,
  contractID: null,
  classDescription: "Geoposition Object",
  classID: null,
  implementationLanguage: Ci.nsIProgrammingLanguage.JAVASCRIPT,
  flags: Ci.nsIClassInfo.DOM_OBJECT,

  coords: null,
  timestamp: null,

};

function GPSDProvider() {
  this.prefService = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch).QueryInterface(Ci.nsIPrefService);

  try {
    gLoggingEnabled = this.prefService.getBoolPref("geo.gpsd.logging.enabled");
  } catch (e) {}
};

GPSDProvider.prototype = {
  
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIGeolocationProvider]),
  
  classDescription: "Returns a geolocation from a GPSD source",
  classID: Components.ID("{0A3BE523-0F2A-32CC-CCD8-1E5986D5A79D}"),
  contractID: "@mozilla.org/geolocation/gpsd/provider;1",
  _xpcom_categories: [{
    category: "geolocation-provider",
  }],
  
  prefService: null,

  transport: null,
  outputStream: null,
  inputStream: null,
  
  startup: function() {

    LOG("startup called\n");
    var socketTransportService = Cc["@mozilla.org/network/socket-transport-service;1"].getService(Ci.nsISocketTransportService);
    
    var hostIPAddr = "127.0.0.1";
    var hostPort = "2947";

    try {
      hostIPAddr = this.prefService.getCharPref("geo.gpsd.host.ipaddr");
    } catch (e) {}

    try {
      hostPort = this.prefService.getCharPref("geo.gpsd.host.port");
    } catch (e) {}

    LOG("Host info: " + hostIPAddr + ":" + hostPort + "\n");

    this.transport = socketTransportService.createTransport(null, 0, hostIPAddr, hostPort, null);
    
    // Alright to open streams here as they are non-blocking by default
    this.outputStream = this.transport.openOutputStream(0,0,0);
    this.inputStream = this.transport.openInputStream(0,0,0);

  },
  
  shutdown: function() {
    LOG("shutdown called\n"); 
    this.outputStream.close();
    this.inputStream.close();
    this.transport.close(Components.results.NS_OK);
  },
  
  watch: function(c) {
    LOG("watch called\n");    

    // Turn GPSD buffer on, results in smoother data points which I think we want.
    // Required due to the way that different data arrives in different NMEA sentences.
    var bufferOption = "J=1\n";
    this.outputStream.write(bufferOption, bufferOption.length);

    // Go into "watcher" mode
    var mode = "w\n";
    this.outputStream.write(mode, mode.length);
    
    var dataListener = {
      onStartRequest: function(request, context) {},
      onStopRequest: function(request, context, status) {},
      onDataAvailable: function(request, context, inputStream, offset, count) {
    
        var sInputStream = Cc["@mozilla.org/scriptableinputstream;1"].createInstance(Ci.nsIScriptableInputStream);
        sInputStream.init(inputStream);

        var s = sInputStream.read(count);
        
        var response = s.split('=');
        
        var header = response[0];
        var info = response[1];
        
        // is this location information?
        if (header != 'GPSD,O') {
          // don't do anything
          return;
        }
        
        // is there a fix?
        if (info == '?') {
          // don't do anything
          return;
        }
    
        // get the info from the string
        var fields = info.split(' ');
        
        // we'll only use RMC data as it seems to make sense
        if (fields[0] != 'RMC') {
          return;
        }

        LOG("Got info: " + info);

        for (var i = 0; i < fields.length; i++) {
          if (fields[i] == '?') {
            fields[i] = null;
          }
        }
        
        var timestamp = fields[1]; // UTC
        var timeError = fields[2]; // seconds
        var latitude = fields[3]; // degrees
        var longitude = fields[4]; // degrees
        var altitude = fields[5]; // meters
        var horizontalError = fields[6]; // meters
        var verticalError = fields[7]; // meters
        var course = fields[8]; // degrees;
        var speed = fields[9]; // meters/sec maybe knots depending on GPSD version TODO: figure this out
        
        var geoPos = new GeoPositionObject(latitude, longitude, altitude, horizontalError, verticalError, course, speed, timestamp);
        
        c.update(geoPos);
    
      }
      
    };
    
    var pump = Cc["@mozilla.org/network/input-stream-pump;1"].createInstance(Ci.nsIInputStreamPump);
    pump.init(this.inputStream, -1, -1, 0, 0, false);
    pump.asyncRead(dataListener, null);

  },
  
};

var components = [GPSDProvider];

function NSGetModule(compMgr, fileSpec) {
  return XPCOMUtils.generateModule(components);
}
