/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

const EXSLT_REGEXP_CID = Components.ID("{18a03189-067b-4978-b4f1-bafe35292ed6}");

function txEXSLTRegExFunctions()
{
}

var SingletonInstance = null;

txEXSLTRegExFunctions.prototype = {
    classID: EXSLT_REGEXP_CID,

    QueryInterface: ChromeUtils.generateQI([Ci.txIEXSLTRegExFunctions]),

    // txIEXSLTRegExFunctions
    match: function(str, regex, flags, doc) {
        var docFrag = doc.createDocumentFragment();
        var re = new RegExp(regex, flags);
        var matches = str.match(re);
        if (matches != null) {
            for (var i = 0; i < matches.length; ++i) {
                var match = matches[i];
                var elem = doc.createElementNS(null, "match");
                var text = doc.createTextNode(match ? match : '');
                elem.appendChild(text);
                docFrag.appendChild(elem);
            }
        }
        return docFrag;
    },

    replace: function(str, regex, flags, replace) {
        var re = new RegExp(regex, flags);

        return str.replace(re, replace);
    },

    test: function(str, regex, flags) {
        var re = new RegExp(regex, flags);

        return re.test(str);
    }
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([txEXSLTRegExFunctions]);
