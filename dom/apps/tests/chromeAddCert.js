var Ci = Components.interfaces;
var Cc = Components.classes;
var Cu = Components.utils;

Cu.import("resource://gre/modules/StoreTrustAnchor.jsm");

// This is all that's needed now
TrustedRootCertificate.index = Ci.nsIX509CertDB.AppXPCShellRoot;

sendAsyncMessage("addCertCompleted");
