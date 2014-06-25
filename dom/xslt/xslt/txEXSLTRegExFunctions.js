/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

const EXSLT_REGEXP_CID = Components.ID("{18a03189-067b-4978-b4f1-bafe35292ed6}");
const EXSLT_REGEXP_CONTRACTID = "@mozilla.org/exslt/regexp;1";

const NODESET_CONTRACTID = "@mozilla.org/transformiix-nodeset;1";

const Ci = Components.interfaces;

function txEXSLTRegExFunctions()
{
}

var SingletonInstance = null;

txEXSLTRegExFunctions.prototype = {
    classID: EXSLT_REGEXP_CID,

    QueryInterface: XPCOMUtils.generateQI([Ci.txIEXSLTRegExFunctions]),

    classInfo: XPCOMUtils.generateCI({classID: EXSLT_REGEXP_CID,
                                      contractID: EXSLT_REGEXP_CONTRACTID,
                                      interfaces: [Ci.txIEXSLTRegExFunctions]}),

    // txIEXSLTRegExFunctions
    match: function(context, str, regex, flags) {
        var nodeset = Components.classes[NODESET_CONTRACTID]
                                .createInstance(Ci.txINodeSet);

        var re = new RegExp(regex, flags);
        var matches = str.match(re);
        if (matches != null && matches.length > 0) {
            var contextNode = context.contextNode;
            var doc = contextNode.nodeType == Ci.nsIDOMNode.DOCUMENT_NODE ?
                      contextNode : contextNode.ownerDocument;
            var docFrag = doc.createDocumentFragment();

            for (var i = 0; i < matches.length; ++i) {
                var match = matches[i];
                var elem = doc.createElementNS(null, "match");
                var text = doc.createTextNode(match ? match : '');
                elem.appendChild(text);
                docFrag.appendChild(elem);
                nodeset.add(elem);
            }
        }

        return nodeset;
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
