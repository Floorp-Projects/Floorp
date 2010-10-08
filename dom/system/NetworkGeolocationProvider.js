Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

const Ci = Components.interfaces;
const Cc = Components.classes;

var gLoggingEnabled = false;
var gTestingEnabled = false;

function nowInSeconds()
{
    return Date.now() / 1000;
}

function LOG(aMsg) {
  if (gLoggingEnabled)
  {
    aMsg = "*** WIFI GEO: " + aMsg + "\n";
    Cc["@mozilla.org/consoleservice;1"].getService(Ci.nsIConsoleService).logStringMessage(aMsg);
    dump(aMsg);
  }
}

function WifiGeoAddressObject(streetNumber, street, premises, city, county, region, country, countryCode, postalCode) {

  this.streetNumber = streetNumber;
  this.street       = street;
  this.premises     = premises;
  this.city         = city;
  this.county       = county;
  this.region       = region;
  this.country      = country;
  this.countryCode  = countryCode;
  this.postalCode   = postalCode;
}

WifiGeoAddressObject.prototype = {

    QueryInterface:   XPCOMUtils.generateQI([Ci.nsIDOMGeoPositionAddress, Ci.nsIClassInfo]),

    getInterfaces: function(countRef) {
        var interfaces = [Ci.nsIDOMGeoPositionAddress, Ci.nsIClassInfo, Ci.nsISupports];
        countRef.value = interfaces.length;
        return interfaces;
    },

    getHelperForLanguage: function(language) null,
    implementationLanguage: Ci.nsIProgrammingLanguage.JAVASCRIPT,
    flags: Ci.nsIClassInfo.DOM_OBJECT,
};

function WifiGeoCoordsObject(lat, lon, acc, alt, altacc) {
    this.latitude = lat;
    this.longitude = lon;
    this.accuracy = acc;
    this.altitude = alt;
    this.altitudeAccuracy = altacc;
};

WifiGeoCoordsObject.prototype = {

    QueryInterface:   XPCOMUtils.generateQI([Ci.nsIDOMGeoPositionCoords, Ci.nsIClassInfo]),

    getInterfaces: function(countRef) {
        var interfaces = [Ci.nsIDOMGeoPositionCoords, Ci.nsIClassInfo, Ci.nsISupports];
        countRef.value = interfaces.length;
        return interfaces;
    },

    getHelperForLanguage: function(language) null,
    classDescription: "wifi geo position coords object",
    implementationLanguage: Ci.nsIProgrammingLanguage.JAVASCRIPT,
    flags: Ci.nsIClassInfo.DOM_OBJECT,

    latitude: 0,
    longitude: 0,
    accuracy: 0,
    altitude: 0,
    altitudeAccuracy: 0,

};

function WifiGeoPositionObject(location) {

    this.coords = new WifiGeoCoordsObject(location.latitude,
                                          location.longitude,
                                          location.accuracy || 12450, // .5 * circumference of earth.
                                          location.altitude || 0,
                                          location.altitude_accuracy || 0);

    if (location.address) {
        let address = location.address;
        this.address = new WifiGeoAddressObject(address.street_number || null,
                                                address.street || null,
                                                address.premises || null,
                                                address.city || null,
                                                address.county || null,
                                                address.region || null,
                                                address.country || null,
                                                address.country_code || null,
                                                address.postal_code || null);
    }
    else
      this.address = null;

    this.timestamp = Date.now();
};

WifiGeoPositionObject.prototype = {

    QueryInterface:   XPCOMUtils.generateQI([Ci.nsIDOMGeoPosition, Ci.nsIClassInfo]),

    // Class Info is required to be able to pass objects back into the DOM.
    getInterfaces: function(countRef) {
        var interfaces = [Ci.nsIDOMGeoPosition, Ci.nsIClassInfo, Ci.nsISupports];
        countRef.value = interfaces.length;
        return interfaces;
    },

    getHelperForLanguage: function(language) null,
    classDescription: "wifi geo location position object",
    implementationLanguage: Ci.nsIProgrammingLanguage.JAVASCRIPT,
    flags: Ci.nsIClassInfo.DOM_OBJECT,

    coords: null,
    timestamp: 0,
};

function HELD() {};
 // For information about the HELD format, see:
 // http://tools.ietf.org/html/draft-thomson-geopriv-held-measurements-05
HELD.encode = function(requestObject) {
    // XML Header
    var requestString = "<locationRequest xmlns=\"urn:ietf:params:xml:ns:geopriv:held\">";
    // Measurements
    if (requestObject.wifi_towers && requestObject.wifi_towers.length > 0) {
      requestString += "<measurements xmlns=\"urn:ietf:params:xml:ns:geopriv:lm\">";
      requestString += "<wifi xmlns=\"urn:ietf:params:xml:ns:geopriv:lm:wifi\">";
      for (var i=0; i < requestObject.wifi_towers.length; ++i) {
        requestString += "<neighbourWap>";
        requestString += "<bssid>" + requestObject.wifi_towers[i].mac_address     + "</bssid>";
        requestString += "<ssid>"  + requestObject.wifi_towers[i].ssid            + "</ssid>";
        requestString += "<rssi>"  + requestObject.wifi_towers[i].signal_strength + "</rssi>";
        requestString += "</neighbourWap>";
      }
      // XML Footer
      requestString += "</wifi></measurements>";
    }
    requestString += "</locationRequest>";
    return requestString;
};

// Decode a HELD response into a Gears-style object
HELD.decode = function(responseXML) {
    // Find a Circle object in PIDF-LO and decode
    function nsResolver(prefix) {
        var ns = {
            'held': 'urn:ietf:params:xml:ns:geopriv:held',
            'pres': 'urn:ietf:params:xml:ns:pidf',
            'gp': 'urn:ietf:params:xml:ns:pidf:geopriv10',
            'gml': 'http://www.opengis.net/gml',
            'gs': 'http://www.opengis.net/pidflo/1.0',
        };
        return ns[prefix] || null;
    }

    var xpathEval = Components.classes["@mozilla.org/dom/xpath-evaluator;1"].createInstance(Ci.nsIDOMXPathEvaluator);

    // Grab values out of XML via XPath
    var pos = xpathEval.evaluate(
        '/held:locationResponse/pres:presence/pres:tuple/pres:status/gp:geopriv/gp:location-info/gs:Circle/gml:pos',
        responseXML,
        nsResolver,
        Ci.nsIDOMXPathResult.STRING_TYPE,
        null);

    var rad = xpathEval.evaluate(
        '/held:locationResponse/pres:presence/pres:tuple/pres:status/gp:geopriv/gp:location-info/gs:Circle/gs:radius',
        responseXML,
        nsResolver,
        Ci.nsIDOMXPathResult.NUMBER_TYPE,
        null );

    var uom = xpathEval.evaluate(
        '/held:locationResponse/pres:presence/pres:tuple/pres:status/gp:geopriv/gp:location-info/gs:Circle/gs:radius/@uom',
        responseXML,
        nsResolver,
        Ci.nsIDOMXPathResult.STRING_TYPE,
        null);

    // Bail if we don't have a valid result (all values && uom==meters)
    if ((pos.stringValue == null) ||
        (rad.numberValue == null) ||
        (uom.stringValue == null) ||
        (uom.stringValue != "urn:ogc:def:uom:EPSG::9001")) {
        return null;
    }

    // Split the pos value into lat/long
    var coords = pos.stringValue.split(/[ \t\n]+/);

    // Fill out the object to return:
    var obj = {
        location: {
            latitude: parseFloat(coords[0]),
            longitude: parseFloat(coords[1]),
            accuracy: rad.numberValue
        }
    };
    return obj;
}  

function WifiGeoPositionProvider() {
    this.prefService = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch).QueryInterface(Ci.nsIPrefService);
    try {
        gLoggingEnabled = this.prefService.getBoolPref("geo.wifi.logging.enabled");
    } catch (e) {}

    try {
        gTestingEnabled = this.prefService.getBoolPref("geo.wifi.testing");
    } catch (e) {}

};

WifiGeoPositionProvider.prototype = {
    classID:          Components.ID("{77DA64D3-7458-4920-9491-86CC9914F904}"),
    QueryInterface:   XPCOMUtils.generateQI([Ci.nsIGeolocationProvider,
                                             Ci.nsIWifiListener,
                                             Ci.nsITimerCallback]),

    prefService:     null,
    wifi_service:    null,
    timer:           null,
    hasSeenWiFi:     false,
    started:         false,

    startup:         function() {
        if (this.started == true)
            return;

        this.started = true;

        LOG("startup called.  testing mode is" + gTestingEnabled);
        // if we don't see anything in 5 seconds, kick of one IP geo lookup.
        // if we are testing, just hammer this callback so that we are more or less
        // always sending data.  It doesn't matter if we have an access point or not.
        this.hasSeenWiFi = false;
        this.timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
        if (gTestingEnabled == false)
            this.timer.initWithCallback(this, 5000, this.timer.TYPE_ONE_SHOT);
        else
            this.timer.initWithCallback(this, 200, this.timer.TYPE_REPEATING_SLACK);
    },

    watch: function(c) {
        LOG("watch called");
        if (!this.wifi_service) {
            this.wifi_service = Cc["@mozilla.org/wifi/monitor;1"].getService(Components.interfaces.nsIWifiMonitor);
            this.wifi_service.startWatching(this);
        }
    },

    shutdown: function() { 
        LOG("shutdown  called");
        if(this.wifi_service)
            this.wifi_service.stopWatching(this);
        this.wifi_service = null;

        if (this.timer != null) {
            this.timer.cancel();
            this.timer = null;
        }

        // Although we aren't using cookies, we should err on the side of not
        // saving any access tokens if the user asked us not to save cookies or
        // has changed the lifetimePolicy.  The access token in these cases is
        // used and valid for the life of this object (eg. between startup and
        // shutdown).e
        let prefBranch = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
        if (prefBranch.getIntPref("network.cookie.lifetimePolicy") != 0)
            prefBranch.deleteBranch("geo.wifi.access_token.");

        this.started = false;
    },

    getAccessTokenForURL: function(url)
    {
        // check to see if we have an access token:
        var accessToken = "";
        
        try {
            var accessTokenPrefName = "geo.wifi.access_token." + url;
            accessToken = this.prefService.getCharPref(accessTokenPrefName);
            
            // check to see if it has expired
            var accessTokenDate = this.prefService.getIntPref(accessTokenPrefName + ".time");
            
            var accessTokenInterval = 1209600;  /* seconds in 2 weeks */
            try {
                accessTokenInterval = this.prefService.getIntPref("geo.wifi.access_token.recycle_interval");
            } catch (e) {}
            
            if (nowInSeconds() - accessTokenDate > accessTokenInterval)
                accessToken = "";
        }
        catch (e) {
            accessToken = "";
        }
        return accessToken;
    },

    onChange: function(accessPoints) {

        LOG("onChange called");
        this.hasSeenWiFi = true;

        // send our request to a wifi geolocation network provider:
        var xhr = Components.classes["@mozilla.org/xmlextras/xmlhttprequest;1"]
                            .createInstance(Ci.nsIXMLHttpRequest);

        // This is a background load
        xhr.mozBackgroundRequest = true;

        var provider_url      = this.prefService.getCharPref("geo.wifi.uri");
        var provider_protocol = 0;
        try {
            provider_protocol = this.prefService.getIntPref("geo.wifi.protocol");
        } catch (e) {}

        LOG("provider url = " + provider_url);

        xhr.open("POST", provider_url, false);
        
        // set something so that we can strip cookies
        xhr.channel.loadFlags = Ci.nsIChannel.LOAD_ANONYMOUS;
            
        xhr.onerror = function(req) {
            LOG("onerror: " + req);
        };

        xhr.onload = function (req) {  

            LOG("xhr onload...");

            try { 
                // if we get a bad response, we will throw and never report a location
                var response;
                switch (provider_protocol) {
                    case 1:
                        LOG("service returned: " + req.target.responseXML);
                        response = HELD.decode(req.target.responseXML);
                        break;
                    case 0:
                    default:
                        LOG("service returned: " + req.target.responseText);
                        response = JSON.parse(req.target.responseText);
                }
            } catch (e) {
                LOG("Parse failed");
                return;
            }

            // response looks something like:
            // {"location":{"latitude":51.5090332,"longitude":-0.1212726,"accuracy":150.0},"access_token":"2:jVhRZJ-j6PiRchH_:RGMrR0W1BiwdZs12"}

            // Check to see if we have a new access token
            var newAccessToken = response.access_token;
            if (newAccessToken != undefined)
            {
                var prefService = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
                var accessToken = "";
                var accessTokenPrefName = "geo.wifi.access_token." + req.target.channel.URI.spec;
                try { accessToken = prefService.getCharPref(accessTokenPrefName); } catch (e) {}

                if (accessToken != newAccessToken) {
                    // no match, lets cache
                    LOG("New Access Token: " + newAccessToken + "\n" + accessTokenPrefName);
                    
                    try {
                        prefService.setIntPref(accessTokenPrefName + ".time", nowInSeconds());
                        prefService.setCharPref(accessTokenPrefName, newAccessToken);
                    } catch (x) {
                        // XXX temporary hack for bug 575346 to allow geolocation to function
                    }
                }
            }

            if (response.location) {
                var newLocation = new WifiGeoPositionObject(response.location);

                var update = Cc["@mozilla.org/geolocation/service;1"].getService(Ci.nsIGeolocationUpdate);
                update.update(newLocation);
            }
        };

        var accessToken = this.getAccessTokenForURL(provider_url);

        var request = {
            version: "1.1.0",
            request_address: true,
        };

        if (accessToken != "")
            request.access_token = accessToken;

        if (accessPoints != null) {
            function filterBlankSSIDs(ap) ap.ssid != ""
            function deconstruct(ap) ({
                    mac_address: ap.mac,
                        ssid: ap.ssid,
                        signal_strength: ap.signal
                        })
            request.wifi_towers = accessPoints.filter(filterBlankSSIDs).map(deconstruct);
        }

        var requestString;
        switch (provider_protocol) {
          case 1:
              requestString = HELD.encode(request);
              break;
          case 0:
          default:
              requestString = JSON.stringify(request);
        }
        LOG("client sending: " + requestString);
 
        try {
          xhr.send(requestString);
        } catch (e) {}
    },

    onError: function (code) {
        LOG("wifi error: " + code);
    },

    notify: function (timer) {
        if (!gTestingEnabled) {
            if (this.hasSeenWiFi == false)
                this.onChange(null);
            this.timer = null;
            return;
        }
        // if we are testing, we need to hammer this.
        this.onChange(null);
    },

};

var NSGetFactory = XPCOMUtils.generateNSGetFactory([WifiGeoPositionProvider]);
