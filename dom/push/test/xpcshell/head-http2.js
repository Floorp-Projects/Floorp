const {NetUtil} = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

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

function getTestProxyPort() {
  let portEnv = Cc["@mozilla.org/process/environment;1"]
    .getService(Ci.nsIEnvironment).get("MOZHTTP2_PROXY_PORT");
  let port = parseInt(portEnv, 10);
  if (!Number.isFinite(port) || port < 1 || port > 65535) {
    throw new Error(`Invalid port in MOZHTTP2_PROXY_PORT env var: ${portEnv}`);
  }
  info(`Using HTTP/2 proxy on port ${port}`);
  return port;
}

function readFile(file) {
  let fstream = Cc["@mozilla.org/network/file-input-stream;1"]
                  .createInstance(Ci.nsIFileInputStream);
  fstream.init(file, -1, 0, 0);
  let data = NetUtil.readInputStreamToString(fstream, fstream.available());
  fstream.close();
  return data;
}

function addCertFromFile(certdb, filename, trustString) {
  let certFile = do_get_file(filename, false);
  let pem = readFile(certFile)
              .replace(/-----BEGIN CERTIFICATE-----/, "")
              .replace(/-----END CERTIFICATE-----/, "")
              .replace(/[\r\n]/g, "");
  certdb.addCertFromBase64(pem, trustString);
}

function trustHttp2CA() {
  let certdb = Cc["@mozilla.org/security/x509certdb;1"]
                  .getService(Ci.nsIX509CertDB);
  addCertFromFile(certdb, "../../../../netwerk/test/unit/http2-ca.pem", "CTu,u,u");
}
