/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Peter Van der Beken.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peterv@propagandism.org>
 *
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

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

const EXSLT_REGEXP_CID = Components.ID("{18a03189-067b-4978-b4f1-bafe35292ed6}");

const CATMAN_CONTRACTID = "@mozilla.org/categorymanager;1";
const NODESET_CONTRACTID = "@mozilla.org/transformiix-nodeset;1";

const Ci = Components.interfaces;

function txEXSLTRegExFunctions()
{
}

var SingletonInstance = null;

txEXSLTRegExFunctions.prototype = {
    classID: EXSLT_REGEXP_CID,

    QueryInterface: function(iid) {
        if (iid.equals(Ci.nsISupports) ||
            iid.equals(Ci.nsIClassInfo) ||
            iid.equals(Ci.txIEXSLTRegExFunctions))
            return this;

        throw Components.results.NS_ERROR_NO_INTERFACE;
    },

    // nsIClassInfo
    getInterfaces: function(countRef) {
        var interfaces = [
            Ci.txIEXSLTRegExFunctions
        ];
        countRef.value = interfaces.length;

        return interfaces;
    },

    getHelperForLanguage: function(language) {
        return null;
    },

    implementationLanguage: Ci.nsIProgrammingLanguage.JAVASCRIPT,
    flags: 0,


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

var NSGetFactory = XPCOMUtils.generateNSGetFactory([txEXSLTRegExFunctions]);
