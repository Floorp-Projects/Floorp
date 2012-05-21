/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const enablePrivilege = netscape.security.PrivilegeManager.enablePrivilege;
const IOSERVICE_CTRID = "@mozilla.org/network/io-service;1";
const nsIIOService    = Components.interfaces.nsIIOService;
const SIS_CTRID       = "@mozilla.org/scriptableinputstream;1";
const nsISIS          = Components.interfaces.nsIScriptableInputStream;
const nsIFilePicker   = Components.interfaces.nsIFilePicker;
const STDURL_CTRID    = "@mozilla.org/network/standard-url;1";
const nsIURI          = Components.interfaces.nsIURI;

var gStop = false;

function loadFile(aUriSpec)
{
    enablePrivilege('UniversalXPConnect');
    var serv = Components.classes[IOSERVICE_CTRID].
        getService(nsIIOService);
    if (!serv) {
        throw Components.results.ERR_FAILURE;
    }
    var chan = serv.newChannel(aUriSpec, null, null);
    var instream = 
        Components.classes[SIS_CTRID].createInstance(nsISIS);
    instream.init(chan.open());

    return instream.read(instream.available());
}

function dump20(aVal)
{
    const pads = '                    ';
    if (typeof(aVal)=='string')
        out = aVal;
    else if (typeof(aVal)=='number')
        out = Number(aVal).toFixed(2);
    else
        out = new String(aVal);
    dump(pads.substring(0, 20 - out.length));
    dump(out);
}
