/*
 * Copyright 2015 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

var EXPORTED_SYMBOLS = ['SpecialStorageUtils'];

Components.utils.import('resource://gre/modules/Services.jsm');

var SpecialStorageUtils = {
  createWrappedSpecialStorage: function (sandbox, swfUrl, privateBrowsing) {
    // Creating internal localStorage object based on url and privateBrowsing setting.
    var uri = Services.io.newURI(swfUrl, null, null);
    var principal = Components.classes["@mozilla.org/scriptsecuritymanager;1"]
                              .getService(Components.interfaces.nsIScriptSecurityManager)
                              .getNoAppCodebasePrincipal(uri);
    var dsm = Components.classes["@mozilla.org/dom/localStorage-manager;1"]
                                .getService(Components.interfaces.nsIDOMStorageManager);
    var storage = dsm.createStorage(null, principal, privateBrowsing);

    // We will return object created in the sandbox/content, with some exposed
    // properties/methods, so we can send data between wrapped object and
    // and sandbox/content.
    var wrapper = Components.utils.cloneInto({
      getItem: function (key) {
        return storage.getItem(key);
      },
      setItem: function (key, value) {
        storage.setItem(key, value);
      },
      removeItem: function (key) {
        storage.removeItem(key);
      }
    }, sandbox, {cloneFunctions:true});
    return wrapper;
  }
};
