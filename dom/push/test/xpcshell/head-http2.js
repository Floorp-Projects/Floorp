// Returns the test H/2 server port, throwing if it's missing or invalid.
function getTestServerPort() {
  let portEnv = Cc["@mozilla.org/process/environment;1"]
                  .getService(Ci.nsIEnvironment).get("MOZHTTP2_PORT");
  let port = parseInt(portEnv, 10);
  if (!Number.isFinite(port) || port < 1 || port > 65535) {
    throw new Error(`Invalid port in MOZHTTP2_PORT env var: ${portEnv}`);
  }
  info(`Using HTTP/2 server on port ${port}`);
  return port;
}

// Support for making sure we can talk to the invalid cert the server presents
var CertOverrideListener = function(host, port, bits) {
  this.host = host;
  this.port = port || 443;
  this.bits = bits;
};

CertOverrideListener.prototype = {
  host: null,
  bits: null,

  getInterface: function(aIID) {
    return this.QueryInterface(aIID);
  },

  QueryInterface: function(aIID) {
    if (aIID.equals(Ci.nsIBadCertListener2) ||
        aIID.equals(Ci.nsIInterfaceRequestor) ||
        aIID.equals(Ci.nsISupports))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  notifyCertProblem: function(socketInfo, sslStatus, targetHost) {
    var cert = sslStatus.QueryInterface(Ci.nsISSLStatus).serverCert;
    var cos = Cc["@mozilla.org/security/certoverride;1"].
              getService(Ci.nsICertOverrideService);
    cos.rememberValidityOverride(this.host, this.port, cert, this.bits, false);
    dump("Certificate Override in place\n");
    return true;
  },
};

function addCertOverride(host, port, bits) {
  var req = new XMLHttpRequest();
  try {
    var url;
    if (port && (port > 0) && (port !== 443)) {
      url = "https://" + host + ":" + port + "/";
    } else {
      url = "https://" + host + "/";
    }
    req.open("GET", url, false);
    req.channel.notificationCallbacks = new CertOverrideListener(host, port, bits);
    req.send(null);
  } catch (e) {
    // This will fail since the server is not trusted yet
  }
}
