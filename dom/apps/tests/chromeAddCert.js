const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;

Cu.import("resource://gre/modules/StoreTrustAnchor.jsm");

// This is all that's needed now
TrustedRootCertificate.index = Ci.nsIX509CertDB.AppXPCShellRoot;

sendAsyncMessage("addCertCompleted");
