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
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Ryan Flint <rflint@dslr.net> (Original Author)
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
let gSS = Cc["@mozilla.org/browser/search-service;1"].
           getService(Ci.nsIBrowserSearchService);
let gObs = Cc["@mozilla.org/observer-service;1"].
           getService(Ci.nsIObserverService);

function test() {
  waitForExplicitFinish();

  let observer = {
    observe: function(aSubject, aTopic, aData) {
      switch (aData) {
        case "engine-added":
          let engine = gSS.getEngineByName("483086a");
          ok(engine, "Test engine 1 installed");
          isnot(engine.searchForm, "foo://example.com",
                "Invalid SearchForm URL dropped");
          gSS.removeEngine(engine);
          break;
        case "engine-removed":
          gObs.removeObserver(this, "browser-search-engine-modified");
          test2();
          break;
      }
    }
  };

  gObs.addObserver(observer, "browser-search-engine-modified", false);
  gSS.addEngine("http://localhost:8888/browser/browser/components/search/test/483086-1.xml",
                Ci.nsISearchEngine.DATA_XML, "data:image/x-icon;%00",
                false);
}

function test2() {
  let observer = {
    observe: function(aSubject, aTopic, aData) {
      switch (aData) {
        case "engine-added":
          let engine = gSS.getEngineByName("483086b");
          ok(engine, "Test engine 2 installed");
          is(engine.searchForm, "http://example.com", "SearchForm is correct");
          gSS.removeEngine(engine);
          break;
        case "engine-removed":  
          gObs.removeObserver(this, "browser-search-engine-modified");
          finish();
          break;
      }
    }
  };

  gObs.addObserver(observer, "browser-search-engine-modified", false);
  gSS.addEngine("http://localhost:8888/browser/browser/components/search/test/483086-2.xml",
                Ci.nsISearchEngine.DATA_XML, "data:image/x-icon;%00",
                false);
}
