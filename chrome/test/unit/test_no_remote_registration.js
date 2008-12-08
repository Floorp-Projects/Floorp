/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2 tw=78 expandtab :
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
 * The Original Code is Chrome Registration Test Code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
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

let manifests = [
  do_get_file("chrome/test/unit/data/test_no_remote_registration.manifest"),
];
registerManifests(manifests);

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

let XULAppInfo = {
  vendor: "Mozilla",
  name: "XPCShell",
  ID: "{39885e5f-f6b4-4e2a-87e5-6259ecf79011}",
  version: "5",
  appBuildID: "2007010101",
  platformVersion: "1.9",
  platformBuildID: "2007010101",
  inSafeMode: false,
  logConsoleErrors: true,
  OS: "XPCShell",
  XPCOMABI: "noarch-spidermonkey",
  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIXULAppInfo,
    Ci.nsIXULRuntime,
  ])
};

let XULAppInfoFactory = {
  // These two are used when we register all our factories (and unregister)
  CID: XULAPPINFO_CID,
  scheme: "XULAppInfo",
  contractID: XULAPPINFO_CONTRACTID,
  createInstance: function (outer, iid) {
    if (outer != null)
      throw Cr.NS_ERROR_NO_AGGREGATION;
    return XULAppInfo.QueryInterface(iid);
  }
};

function ProtocolHandler(aScheme, aFlags)
{
  this.scheme = aScheme;
  this.protocolFlags = aFlags;
  this.contractID = "@mozilla.org/network/protocol;1?name=" + aScheme;
}

ProtocolHandler.prototype =
{
  defaultPort: -1,
  allowPort: function() false,
  newURI: function(aSpec, aCharset, aBaseURI)
  {
    let uri = Cc["@mozilla.org/network/standard-url;1"].
              createInstance(Ci.nsIURI);
    uri.spec = aSpec;
    if (!uri.scheme) {
      // We got a partial uri, so let's resolve it with the base one
      uri.spec = aBaseURI.resolve(aSpec);
    }
    return uri;
  },
  newChannel: function() { throw Cr.NS_ERROR_NOT_IMPLEMENTED },
  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIProtocolHandler
  ])
};

let testProtocols = [
  {scheme: "moz-protocol-ui-resource",
   flags: Ci.nsIProtocolHandler.URI_IS_UI_RESOURCE,
   CID: Components.ID("{d6dedc93-558f-44fe-90f4-3b4bba8a0b14}"),
   shouldRegister: true
  },
  {scheme: "moz-protocol-local-file",
   flags: Ci.nsIProtocolHandler.URI_IS_LOCAL_FILE,
   CID: Components.ID("{ee30d594-0a2d-4f47-89cc-d4cde320ab69}"),
   shouldRegister: true
  },
  {scheme: "moz-protocol-loadable-by-anyone",
   flags: Ci.nsIProtocolHandler.URI_LOADABLE_BY_ANYONE,
   CID: Components.ID("{c3735f23-3b0c-4a33-bfa0-79436dcd40b2}"),
   shouldRegister: false
  },
  {scheme: "moz-protocol-local-resource",
   flags: Ci.nsIProtocolHandler.URI_IS_LOCAL_RESOURCE,
   CID: Components.ID("{b79e977c-f840-469a-b413-0125cc1b62a5}"),
   shouldRegister: true
  },
];
function run_test()
{
  // Create factories
  let factories = [];
  for (let i = 0; i < testProtocols.length; i++) {
    factories[i] = {
      scheme: testProtocols[i].scheme,
      flags: testProtocols[i].flags,
      CID: testProtocols[i].CID,
      contractID: "@mozilla.org/network/protocol;1?name=" + testProtocols[i].scheme,
      createInstance: function(aOuter, aIID)
      {
        if (aOuter != null)
          throw Cr.NS_ERROR_NO_AGGREGATION;
        let handler = new ProtocolHandler(this.scheme, this.flags, this.CID);
        return handler.QueryInterface(aIID);
      }
    };
  }
  // Add our XULAppInfo factory
  factories.push(XULAppInfoFactory);

  // Register our factories
  for (let i = 0; i < factories.length; i++) {
    let factory = factories[i];
    Components.manager.QueryInterface(Ci.nsIComponentRegistrar)
              .registerFactory(factory.CID, "test-" + factory.scheme,
                               factory.contractID, factory);
  }

  // Check for new chrome
  let cr = Cc["@mozilla.org/chrome/chrome-registry;1"].
           getService(Ci.nsIChromeRegistry);
  cr.checkForNewChrome();

  // See if our various things were able to register
  let registrationTypes = [
    "content",
    "locale",
    "skin",
    "override",
    "resource",
  ];
  for (let i = 0; i < testProtocols.length; i++) {
    let protocol = testProtocols[i];
    for (let j = 0; j < registrationTypes.length; j++) {
      let type = registrationTypes[j];
      dump("Testing protocol '" + protocol.scheme + "' with type '" + type +
           "'\n");
      let expectedURI = protocol.scheme + "://foo/";
      let sourceURI = "chrome://" + protocol.scheme + "/" + type + "/";
      switch (type) {
        case "content":
          expectedURI += protocol.scheme + ".xul";
          break;
        case "locale":
          expectedURI += protocol.scheme + ".dtd";
          break;
        case "skin":
          expectedURI += protocol.scheme + ".css";
          break;
        case "override":
          sourceURI = "chrome://good-package/content/override-" +
                      protocol.scheme + ".xul";
          break;
        case "resource":
          sourceURI = "resource://" + protocol.scheme + "/";
          break;
      };
      try {
        let ios = Cc["@mozilla.org/network/io-service;1"].
                  getService(Ci.nsIIOService);
        sourceURI = ios.newURI(sourceURI, null, null);
        let uri;
        if (type == "resource") {
          // resources go about a slightly different way than everything else
          let rph = ios.getProtocolHandler("resource").
                    QueryInterface(Ci.nsIResProtocolHandler);
          // this throws for packages that are not registered
          uri = rph.resolveURI(sourceURI);
        }
        else {
          // this throws for packages that are not registered
          uri = cr.convertChromeURL(sourceURI).spec;
        }

        if (protocol.shouldRegister) {
          do_check_eq(expectedURI, uri);
        }
        else {
          // Overrides will not throw, so we'll get to here.  We want to make
          // sure that the two strings are not the same in this situation.
          do_check_neq(expectedURI, uri);
        }
      }
      catch (e) {
        if (protocol.shouldRegister) {
          dump(e + "\n");
          do_throw("Should have registered our URI for protocol " +
                   protocol.scheme);
        }
      }
    }
  }

  // Unregister our factories so we do not leak
  for (let i = 0; i < factories.length; i++) {
    let factory = factories[i];
    Components.manager.QueryInterface(Ci.nsIComponentRegistrar)
              .unregisterFactory(factory.CID, factory);
  }
}
