/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is TransforMiiX XSLT Processor.
 *
 * The Initial Developer of the Original Code is
 * Axel Hecht.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Axel Hecht <axel@pike.org>
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
