/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 sts=4 et
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
 * The Original Code is Necko Test Code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Marco Bonardo <mak77@bonardo.net> (Original Author)
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

// Test components used in test_xpcomutils.js

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

const CATEGORY_NAME = "test-cat";


// This component will register under the "test-cat" category.
function CatRegisteredComponent() {}
CatRegisteredComponent.prototype = {
  classDescription: "CatRegisteredComponent",
  classID:          Components.ID("{163cd427-1f08-4416-a291-83ea71127b0e}"),
  contractID:       "@unit.test.com/cat-registered-component;1",
  QueryInterface: XPCOMUtils.generateQI([]),
  _xpcom_categories: [
    { category: CATEGORY_NAME }
  ]
};


// This component will register under the "test-cat" category.
function CatAppRegisteredComponent() {}
CatAppRegisteredComponent.prototype = {
  classDescription: "CatAppRegisteredComponent",
  classID:          Components.ID("{b686dc84-f42e-4c94-94fe-89d0ac899578}"),
  contractID:       "@unit.test.com/cat-app-registered-component;1",
  QueryInterface: XPCOMUtils.generateQI([]),
  _xpcom_categories: [
    { category: CATEGORY_NAME,
      apps: [ /* Our app */ "{adb42a9a-0d19-4849-bf4d-627614ca19be}" ]
    }
  ]
};


// This component will not register under the "test-cat" category because
// it does not support our app.
function CatUnregisteredComponent() {}
CatUnregisteredComponent.prototype = {
  classDescription: "CatUnregisteredComponent",
  classID:          Components.ID("{c31a552b-0228-4a1a-8cdf-d8aab7d4eff8}"),
  contractID:       "@unit.test.com/cat-unregistered-component;1",
  QueryInterface: XPCOMUtils.generateQI([]),
  _xpcom_categories: [
    { category: CATEGORY_NAME,
      apps: [ /* Another app */ "{e84fce36-6ef6-435c-bf63-979a8811dcd4}" ]
    }
  ]
};


let components = [
  CatRegisteredComponent,
  CatAppRegisteredComponent,
  CatUnregisteredComponent,
];
function NSGetModule(compMgr, fileSpec) {
  return XPCOMUtils.generateModule(components);
}
