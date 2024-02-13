"use strict";

// Wraps the given object in an XPConnect wrapper and, if an interface
// is passed, queries the result to that interface.
function xpcWrap(obj, iface) {
  let ifacePointer = Cc[
    "@mozilla.org/supports-interface-pointer;1"
  ].createInstance(Ci.nsISupportsInterfacePointer);

  ifacePointer.data = obj;
  if (iface) {
    return ifacePointer.data.QueryInterface(iface);
  }
  return ifacePointer.data;
}

function createContentWindow(uri) {
  const principal = Services.scriptSecurityManager
    .createContentPrincipalFromOrigin(uri);
  const webnav = Services.appShell.createWindowlessBrowser(false);
  const docShell = webnav.docShell;
  docShell.createAboutBlankDocumentViewer(principal, principal);
  return webnav.document.defaultView;
}

function createChromeWindow() {
  const principal = Services.scriptSecurityManager.getSystemPrincipal();
  const webnav = Services.appShell.createWindowlessBrowser(true);
  const docShell = webnav.docShell;
  docShell.createAboutBlankDocumentViewer(principal, principal);
  return webnav.document.defaultView;
}
