Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

const Ci = Components.interfaces;
const Cc = Components.classes;

function nowInSeconds()
{
    return Date.now() / 1000;
}

function LOG(aMsg) {
    //aMsg = ("*** WIFI GEO: " + aMsg);
    //Cc["@mozilla.org/consoleservice;1"].getService(Ci.nsIConsoleService).logStringMessage(aMsg);
}

function getAccessTokenForURL(url)
{
    // check to see if we have an access token:
    var accessToken = "";
    
    try {
        var prefService = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);

        var accessTokenPrefName = "geo.wifi.access_token." + url;
        accessToken = prefService.getCharPref(accessTokenPrefName);
        
        // check to see if it has expired
        var accessTokenDate = prefService.getIntPref(accessTokenPrefName + ".time");
        
        var accessTokenInterval = 1209600;  /* seconds in 2 weeks */
        try {
            accessTokenInterval = prefService.getIntPref("geo.wifi.access_token.recycle_interval");
        } catch (e) {}
        
        if (nowInSeconds() - accessTokenDate > accessTokenInterval)
            accessToken = "";
    }
    catch (e) {
        accessToken = "";
        LOG("Error: "+ e);
    }
    return accessToken;
}

function WifiGeoCoordsObject(lat, lon, acc) {
    this.latitude = lat;
    this.longitude = lon;
    this.accuracy = acc;
};

WifiGeoCoordsObject.prototype = {

    QueryInterface:   XPCOMUtils.generateQI([Ci.nsIDOMGeoPositionCoords, Ci.nsIClassInfo]),

    getInterfaces: function(countRef) {
        var interfaces = [Ci.nsIDOMGeoPositionCoords, Ci.nsIClassInfo, Ci.nsISupports];
        countRef.value = interfaces.length;
        return interfaces;
    },

    getHelperForLanguage: function(language) null,
    contractID: "",
    classDescription: "wifi geo position coords object",
    classID: null,
    implementationLanguage: Ci.nsIProgrammingLanguage.JAVASCRIPT,
    flags: Ci.nsIClassInfo.DOM_OBJECT,

    latitude: 0,
    longitude: 0,
    accuracy: 0,

    altitude: 0,
    altitudeAccuracy: 0,
    heading: 0,
    speed: 0,
};

function WifiGeoPositionObject(lat, lon, acc) {
    this.coords = new WifiGeoCoordsObject(lat, lon, acc);
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
    contractID: "",
    classDescription: "wifi geo location position object",
    classID: null,
    implementationLanguage: Ci.nsIProgrammingLanguage.JAVASCRIPT,
    flags: Ci.nsIClassInfo.DOM_OBJECT,

    coords: null,
    timestamp: 0,
};

function WifiGeoPositionProvider() {};
WifiGeoPositionProvider.prototype = {
    classDescription: "A component that returns a geolocation based on WIFI",
    classID:          Components.ID("{77DA64D3-7458-4920-9491-86CC9914F904}"),
    contractID:       "@mozilla.org/geolocation/provider;1",
    QueryInterface:   XPCOMUtils.generateQI([Ci.nsIGeolocationProvider, Ci.nsIWifiListener, Ci.nsITimerCallback]),
  
    provider_url:    null,
    wifi_service:    null,
    timer:           null,
    hasSeenWiFi:     false,

    observe: function (aSubject, aTopic, aData) {
        if (aTopic == "private-browsing") {
            if (aData == "enter" || aData == "exit") {
                let psvc = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefService);
                try {
                    let branch = psvc.getBranch("geo.wifi.access_token.");
                    branch.deleteBranch("");
                } catch (e) {}
            }
        }
    },

    startup:         function() {
        LOG("startup called");

        var prefService = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
        this.provider_url = prefService.getCharPref("geo.wifi.uri");
        LOG("provider url = " + this.provider_url);

        // if we don't see anything in 5 seconds, kick of one IP geo lookup.
        this.hasSeenWiFi = false;
        this.timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
        this.timer.initWithCallback(this, 5000, this.timer.TYPE_ONE_SHOT);

        let os = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);
        os.addObserver(this, "private-browsing", false);
    },

    isReady:         function() {
        LOG("isReady called");
        return true
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

        let os = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);
        os.removeObserver(this, "private-browsing");
    },

    onChange: function(accessPoints) {

        LOG("onChange called");
        this.hasSeenWiFi = true;

        var prefService = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);

        // send our request to a wifi geolocation network provider:
        var xhr = Components.classes["@mozilla.org/xmlextras/xmlhttprequest;1"].createInstance(Ci.nsIXMLHttpRequest);

        // This is a background load
        xhr.mozBackgroundRequest = true;

        xhr.open("POST", this.provider_url, false);
        
        // set something so that we can strip cookies
        xhr.channel.loadFlags = Ci.nsIChannel.LOAD_ANONYMOUS;
            
        xhr.onerror = function(req) {
            LOG("onerror: " + req);
        };

        xhr.onload = function (req) {  

            LOG("service returned: " + req.target.responseText);

            // if we get a bad response, we will throw and never report a location
            var response = JSON.parse(req.target.responseText);

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
                    
                    prefService.setIntPref(accessTokenPrefName + ".time", nowInSeconds());
                    prefService.setCharPref(accessTokenPrefName, newAccessToken);
                }
            }

            var newLocation = new WifiGeoPositionObject(response.location.latitude,
                                                        response.location.longitude,
                                                        response.location.accuracy);

            var update = Cc["@mozilla.org/geolocation/service;1"].getService(Ci.nsIGeolocationUpdate);
            update.update(newLocation);
        };

        var accessToken = getAccessTokenForURL(this.provider_url);

        var request = {
            version: "1.1.0",
//          request_address: true,
        };

        if (accessToken != "")
            request.access_token = accessToken;

        if (accessPoints != null) {
            request.wifi_towers = accessPoints.map(function (ap) ({
                        mac_address: ap.mac,
                        ssid: ap.ssid,
                        signal_strength: ap.signal,
                    }));
        }

        var jsonString = JSON.stringify(request);
        LOG("client sending: " + jsonString);

        xhr.send(jsonString);
    },

    onError: function (code) {
        LOG("wifi error: " + code);
    },

    notify: function (timer) {
        if (this.hasSeenWiFi == false)
            this.onChange(null);
        this.timer = null;
    },

};

var components = [WifiGeoPositionProvider];
function NSGetModule(compMgr, fileSpec) {
  return XPCOMUtils.generateModule(components);
}
