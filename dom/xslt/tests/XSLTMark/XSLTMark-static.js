/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const enablePrivilege = netscape.security.PrivilegeManager.enablePrivilege;
const IOSERVICE_CTRID = "@mozilla.org/network/io-service;1";
const nsIIOService = Ci.nsIIOService;
const SIS_CTRID = "@mozilla.org/scriptableinputstream;1";
const nsISIS = Ci.nsIScriptableInputStream;
const nsIFilePicker = Ci.nsIFilePicker;
const STDURLMUT_CTRID = "@mozilla.org/network/standard-url-mutator;1";
const nsIURIMutator = Ci.nsIURIMutator;

const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

var gStop = false;

function loadFile(aUriSpec) {
  enablePrivilege("UniversalXPConnect");
  var serv = Cc[IOSERVICE_CTRID].getService(nsIIOService);
  if (!serv) {
    throw Components.Exception("", Cr.ERR_FAILURE);
  }
  var chan = NetUtil.newChannel({
    uri: aUriSpec,
    loadUsingSystemPrincipal: true,
  });
  var instream = Cc[SIS_CTRID].createInstance(nsISIS);
  instream.init(chan.open());

  return instream.read(instream.available());
}

function dump20(aVal) {
  const pads = "                    ";
  if (typeof aVal == "string") {
    out = aVal;
  } else if (typeof aVal == "number") {
    out = Number(aVal).toFixed(2);
  } else {
    out = new String(aVal);
  }
  dump(pads.substring(0, 20 - out.length));
  dump(out);
}
