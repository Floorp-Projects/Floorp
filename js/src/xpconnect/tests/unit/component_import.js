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
 * The Original Code is Module loader test component.
 *
 * The Initial Developer of the Original Code is
 * Alexander J. Vincent <ajvincent@gmail.com>.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

function FooComponent() {
}
FooComponent.prototype =
{
  // nsIClassInfo
  getInterfaces: function getInterfaces(aCount) {
    var interfaces = [Components.interfaces.nsIClassInfo];
    aCount.value = interfaces.length;
    return interfaces;
  },

  // nsIClassInfo
  getHelperForLanguage: function getHelperForLanguage(aLanguage) {
    return null;
  },

  // nsIClassInfo
  classDescription: "Module importer test 1",

  // nsIClassInfo
  classID: Components.ID("{6b933fe6-6eba-4622-ac86-e4f654f1b474}"),

  // nsIClassInfo
  contractID: "@mozilla.org/tests/module-importer;1",

  // nsIClassInfo
  implementationLanguage: Components.interfaces.nsIProgrammingLanguage.JAVASCRIPT,

  // nsIClassInfo
  flags: 0,

  // nsISupports
  QueryInterface: function QueryInterface(aIID) {
    if (aIID.equals(Components.interfaces.nsIClassInfo) ||
        aIID.equals(Components.interfaces.nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
};

function BarComponent() {
}
BarComponent.prototype =
{
  // nsIClassInfo
  getInterfaces: function getInterfaces(aCount) {
    var interfaces = [Components.interfaces.nsIClassInfo];
    aCount.value = interfaces.length;
    return interfaces;
  },

  // nsIClassInfo
  getHelperForLanguage: function getHelperForLanguage(aLanguage) {
    return null;
  },

  // nsIClassInfo
  classDescription: "Module importer test 2",

  // nsIClassInfo
  classID: Components.ID("{708a896a-b48d-4bff-906e-fc2fd134b296}"),

  // nsIClassInfo
  contractID: "@mozilla.org/tests/module-importer;2",

  // nsIClassInfo
  implementationLanguage: Components.interfaces.nsIProgrammingLanguage.JAVASCRIPT,

  // nsIClassInfo
  flags: 0
};

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

var NSGetModule = XPCOMUtils.generateNSGetModule([
  {
    className:  FooComponent.prototype.classDescription,
    cid:        FooComponent.prototype.classID,
    contractID: FooComponent.prototype.contractID,
    factory: XPCOMUtils.generateFactory(FooComponent)
  },
  {
    className:  BarComponent.prototype.classDescription,
    cid:        BarComponent.prototype.classID,
    contractID: BarComponent.prototype.contractID,
    factory: XPCOMUtils.generateFactory(
      BarComponent,
      [Components.interfaces.nsIClassInfo]
    )
  }
], null, null);
