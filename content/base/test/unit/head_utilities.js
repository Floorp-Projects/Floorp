/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://testing-common/httpd.js");

const nsIDocumentEncoder = Components.interfaces.nsIDocumentEncoder;
const replacementChar = Components.interfaces.nsIConverterInputStream.DEFAULT_REPLACEMENT_CHARACTER;

function loadContentFile(aFile, aCharset) {
    //if(aAsIso == undefined) aAsIso = false;
    if(aCharset == undefined)
        aCharset = 'UTF-8';

    var file = do_get_file(aFile);
    var ios = Components.classes['@mozilla.org/network/io-service;1']
            .getService(Components.interfaces.nsIIOService);
    var chann = ios.newChannelFromURI ( ios.newFileURI (file) );
    chann.contentCharset = aCharset;

    /*var inputStream = Components.classes["@mozilla.org/scriptableinputstream;1"]
                        .createInstance(Components.interfaces.nsIScriptableInputStream);
    inputStream.init(chann.open());
    return inputStream.read(file.fileSize);
    */

    var inputStream = Components.classes["@mozilla.org/intl/converter-input-stream;1"]
                       .createInstance(Components.interfaces.nsIConverterInputStream);
    inputStream.init(chann.open(), aCharset, 1024, replacementChar);
    var str = {}, content = '';
    while (inputStream.readString(4096, str) != 0) {
        content += str.value;
    }
    return content;
}
