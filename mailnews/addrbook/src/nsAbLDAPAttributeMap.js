/* -*- Mode: Javascript; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * Oracle Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dan Mosedale <dan.mosedale@oracle.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

const NS_ABLDAPATTRIBUTEMAP_CID = Components.ID(
  "{127b341a-bdda-4270-85e1-edff569a9b85}");
const NS_ABLDAPATTRIBUTEMAPSERVICE_CID = Components.ID(
  "{4ed7d5e1-8800-40da-9e78-c4f509d7ac5e}");

function nsAbLDAPAttributeMap() {}

nsAbLDAPAttributeMap.prototype = {
  mPropertyMap: {},
  mAttrMap: {}, 

  getAttributeList: function getAttributeList(aProperty) {

    if (!(aProperty in this.mPropertyMap)) {
      return null;
    }

    // return the joined list
    return this.mPropertyMap[aProperty].join(",");
  },

  getAttributes: function getAttributes(aProperty, aCount, aAttrs) {

    // fail if no entry for this
    if (!(aProperty in this.mPropertyMap)) {
      throw Components.results.NS_ERROR_FAILURE;
    }

    aAttrs = this.mPropertyMap[aProperty];
    aCount = aAttrs.length;
    return aAttrs;
  },

  getFirstAttribute: function getFirstAttribute(aProperty) {

    // fail if no entry for this
    if (!(aProperty in this.mPropertyMap)) {
      return null;
    }

    return this.mPropertyMap[aProperty][0];
  },

  setAttributeList: function setAttributeList(aProperty, aAttributeList,
                                              aAllowInconsistencies) {

    var attrs = aAttributeList.split(",");

    // check to make sure this call won't allow multiple mappings to be
    // created, if requested
    if (!aAllowInconsistencies) {
      for each (var attr in attrs) {
        if (attr in this.mAttrMap && this.mAttrMap[attr] != aProperty) {
          throw Components.results.NS_ERROR_FAILURE;
        }
      }
    }

    // delete any attr mappings created by the existing property map entry
    for each (attr in this.mPropertyMap[aProperty]) {
      delete this.mAttrMap[attr];
    }

    // add these attrs to the attrmap
    for each (attr in attrs) {
      this.mAttrMap[attr] = aProperty;
    }

    // add them to the property map
    this.mPropertyMap[aProperty] = attrs;
  },

  getProperty: function getProperty(aAttribute) {

    if (!(aAttribute in this.mAttrMap)) {
      return null;
    }

    return this.mAttrMap[aAttribute];
  },

  getAllCardAttributes: function getAllCardAttributes() {
    var attrs = [];
    for each (var prop in this.mPropertyMap) {
      attrs.push(prop);
    }

    if (!attrs.length) {
      throw Components.results.NS_ERROR_FAILURE;
    }

    return attrs.join(",");
  },

  setFromPrefs: function setFromPrefs(aPrefBranchName) {
    var prefSvc = Components.classes["@mozilla.org/preferences-service;1"].
      getService(Components.interfaces.nsIPrefService);

    // get the right pref branch
    var branch = prefSvc.getBranch(aPrefBranchName + ".");

    // get the list of children
    var childCount = {};
    var children = branch.getChildList("", childCount);

    // do the actual sets
    for each (var child in children) {
      this.setAttributeList(child, branch.getCharPref(child), true);
    }

    // ensure that everything is kosher
    this.checkState();
  },

  setCardPropertiesFromLDAPMessage: function
    setCardPropertiesFromLDAPMessage(aMessage, aCard) {

    var cardValueWasSet = false;

    var msgAttrCount = {};
    var msgAttrs = aMessage.getAttributes(msgAttrCount);

    // downcase the array for comparison
    function toLower(a) { return a.toLowerCase(); }
    msgAttrs = msgAttrs.map(toLower);

    // deal with each addressbook property
    for (var prop in this.mPropertyMap) {

      // go through the list of possible attrs in precedence order
      for each (var attr in this.mPropertyMap[prop]) {

        attr = attr.toLowerCase();

        // find the first attr that exists in this message
        if (msgAttrs.indexOf(attr) != -1) {
          
          try {
            var values = aMessage.getValues(attr, {});
            aCard.setCardValue(prop, values[0]);

            cardValueWasSet = true;
          } catch (ex) {
            // ignore any errors getting message values or setting card values
          }
        }
      }
    }

    if (!cardValueWasSet) {
      throw Components.results.NS_ERROR_FAILURE;
    }

    return;
  },

  checkState: function checkState() {
    
    var attrsSeen = [];

    for each (var attrArray in this.mPropertyMap) {

      for each (var attr in attrArray) {

        // if we've seen this before, there's a problem
        if (attrsSeen.indexOf(attr) != -1) {
          throw Components.results.NS_ERROR_FAILURE;
        }

        // remember that we've seen it now
        attrsSeen.push(attr);
      }
    }

    return;
  },

  QueryInterface: function QueryInterface(iid) {
    if (!iid.equals(Components.interfaces.nsIAbLDAPAttributeMap) &&
        !iid.equals(Components.interfaces.nsISupports)) {
      throw Components.results.NS_ERROR_NO_INTERFACE;
    }

    return this;
  }
}

function nsAbLDAPAttributeMapService() {
}

nsAbLDAPAttributeMapService.prototype = {

  mAttrMaps: {}, 

  getMapForPrefBranch: function getMapForPrefBranch(aPrefBranchName) {

    // if we've already got this map, return it
    if (aPrefBranchName in this.mAttrMaps) {
      return this.mAttrMaps[aPrefBranchName];
    }

    // otherwise, try and create it
    var attrMap = new nsAbLDAPAttributeMap();
    attrMap.setFromPrefs("ldap_2.servers.default.attrmap");
    attrMap.setFromPrefs(aPrefBranchName + ".attrmap");

    // cache
    this.mAttrMaps[aPrefBranchName] = attrMap;

    // and return
    return attrMap;
  },

  QueryInterface: function (iid) {
    if (iid.equals(Components.interfaces.nsIAbLDAPAttributeMapService) ||
        iid.equals(Components.interfaces.nsISupports))
      return this;

    Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
    return null;
  } 
}

var nsAbLDAPAttributeMapModule = {
  registerSelf: function (compMgr, fileSpec, location, type) {
    debug("*** Registering Addressbook LDAP Attribute Map components\n");
    compMgr = compMgr.QueryInterface(
      Components.interfaces.nsIComponentRegistrar);

    compMgr.registerFactoryLocation(
      NS_ABLDAPATTRIBUTEMAP_CID,
      "Addressbook LDAP Attribute Map Component",
      "@mozilla.org/addressbook/ldap-attribute-map;1",
      fileSpec, location, type);

    compMgr.registerFactoryLocation(
      NS_ABLDAPATTRIBUTEMAPSERVICE_CID,
      "Addressbook LDAP Attribute Map Service",
      "@mozilla.org/addressbook/ldap-attribute-map-service;1",
      fileSpec, location, type);
  },

  /*
   * The GetClassObject method is responsible for producing Factory objects
   */
  getClassObject: function (compMgr, cid, iid) {
    if (!iid.equals(Components.interfaces.nsIFactory))
      throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

    if (cid.equals(NS_ABLDAPATTRIBUTEMAP_CID)) {
      return this.nsAbLDAPAttributeMapFactory;
    } 

    if (cid.equals(NS_ABLDAPATTRIBUTEMAPSERVICE_CID)) {
      return this.nsAbLDAPAttributeMapServiceFactory;
    }

    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  /* factory objects */
  nsAbLDAPAttributeMapFactory: {
    /*
     * Construct an instance of the interface specified by iid, possibly
     * aggregating it with the provided outer.  (If you don't know what
     * aggregation is all about, you don't need to.  It reduces even the
     * mightiest of XPCOM warriors to snivelling cowards.)
     */
    createInstance: function (outer, iid) {
      if (outer != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;

      return (new nsAbLdapAttributeMap()).QueryInterface(iid);
    }
  },

  nsAbLDAPAttributeMapServiceFactory: {
    /*
     * Construct an instance of the interface specified by iid, possibly
     * aggregating it with the provided outer.  (If you don't know what
     * aggregation is all about, you don't need to.  It reduces even the
     * mightiest of XPCOM warriors to snivelling cowards.)
     */
    createInstance: function (outer, iid) {
      if (outer != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;

      return (new nsAbLDAPAttributeMapService()).QueryInterface(iid);
    }
  },

  /*
   * The canUnload method signals that the component is about to be unloaded.
   * C++ components can return false to indicate that they don't wish to be
   * unloaded, but the return value from JS components' canUnload is ignored:
   * mark-and-sweep will keep everything around until it's no longer in use,
   * making unconditional ``unload'' safe.
   *
   * You still need to provide a (likely useless) canUnload method, though:
   * it's part of the nsIModule interface contract, and the JS loader _will_
   * call it.
   */
  canUnload: function(compMgr) {
    return true;
  }
};

function NSGetModule(compMgr, fileSpec) {
  return nsAbLDAPAttributeMapModule;
}
