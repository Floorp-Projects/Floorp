/*
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with the
 * License. You may obtain a copy of the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License for
 * the specific language governing rights and limitations under the License.
 * 
 * The Original Code is Mozilla XPCOM Dictionary.
 * 
 * The Initial Developer of the Original Code is Digital Creations 2, Inc.
 * Portions created by Digital Creations 2, Inc. are Copyright (C) 2000 Digital
 * Creations 2, Inc.  All Rights Reserved.
 * 
 * Contributor(s): Martijn Pieters <mj@digicool.com> (original author)
 */

/*
 *  nsDictionary XPCOM component
 *  Version: $Revision: 1.6 $
 *
 *  $Id: nsDictionary.js,v 1.6 2002/01/29 21:21:33 dougt%netscape.com Exp $
 */

/*
 * Constants
 */
const DICTIONARY_CONTRACTID = '@mozilla.org/dictionary;1';
const DICTIONARY_CID = Components.ID('{1dd0cb45-aea3-4a52-8b29-01429a542863}');
const DICTIONARY_IID = Components.interfaces.nsIDictionary;

/*
 * Class definitions
 */

/* The nsDictionary class constructor. */
function nsDictionary() {
    this.hash = {};
}

/* the nsDictionary class def */
nsDictionary.prototype= {
    hasKey: function(key) { return this.hash.hasOwnProperty(key) },

    getKeys: function(count) {
        var asKeys = new Array();
        for (var sKey in this.hash) asKeys.push(sKey);
        count.value = asKeys.length;
        return asKeys;
    },

    getValue: function(key) { 
        if (!this.hasKey(key))
            throw Components.Exception("Key doesn't exist");
        return this.hash[key]; 
    },

    setValue: function(key, value) { this.hash[key] = value; },
    
    deleteValue: function(key) {
        if (!this.hasKey(key))
            throw Components.Exception("Key doesn't exist");
        var oOld = this.getValue(key);
        delete this.hash[key];
        return oOld;
    },

    clear: function() { this.hash = {}; },

    QueryInterface: function(iid) {
        if (!iid.equals(Components.interfaces.nsISupports) &&
            !iid.equals(DICTIONARY_IID))
            throw Components.results.NS_ERROR_NO_INTERFACE;
        return this;
    }
};

/*
 * Objects
 */

/* nsDictionary Module (for XPCOM registration) */
var nsDictionaryModule = {
    registerSelf: function(compMgr, fileSpec, location, type) {
        compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);
        compMgr.registerFactoryLocation(DICTIONARY_CID, 
                                        "nsDictionary JS component", 
                                        DICTIONARY_CONTRACTID, 
                                        fileSpec, 
                                        location,
                                        type);
    },

    getClassObject: function(compMgr, cid, iid) {
        if (!cid.equals(DICTIONARY_CID))
            throw Components.results.NS_ERROR_NO_INTERFACE;

        if (!iid.equals(Components.interfaces.nsIFactory))
            throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

        return nsDictionaryFactory;
    },

    canUnload: function(compMgr) { return true; }
};

/* nsDictionary Class Factory */
var nsDictionaryFactory = {
    createInstance: function(outer, iid) {
        if (outer != null)
            throw Components.results.NS_ERROR_NO_AGGREGATION;
    
        if (!iid.equals(DICTIONARY_IID) &&
            !iid.equals(Components.interfaces.nsISupports))
            throw Components.results.NS_ERROR_INVALID_ARG;

        return new nsDictionary();
    }
}

/*
 * Functions
 */

/* module initialisation */
function NSGetModule(comMgr, fileSpec) { return nsDictionaryModule; }

// vim:sw=4:sr:sta:et:sts:
