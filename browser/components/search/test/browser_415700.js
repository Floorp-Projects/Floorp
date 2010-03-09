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
 * Portions created by the Initial Developer are Copyright (C) 2008
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
var gSS = Cc["@mozilla.org/browser/search-service;1"].
           getService(Ci.nsIBrowserSearchService);
var gObs = Cc["@mozilla.org/observer-service;1"].
           getService(Ci.nsIObserverService);

function observers(aSubject, aTopic, aData) {
  switch (aData) {
    case "engine-added":
      test2();
      break;
    case "engine-current":
      test3();
      break;
    case "engine-removed":
      test4();
      break;
  }
}

function test() {
  waitForExplicitFinish();
  gObs.addObserver(observers, "browser-search-engine-modified", false);

  gSS.addEngine("http://mochi.test:8888/browser/browser/components/search/test/testEngine.xml",
                Ci.nsISearchEngine.DATA_XML, "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAIAAACQkWg2AAABGklEQVQoz2NgGB6AnZ1dUlJSXl4eSDIyMhLW4Ovr%2B%2Fr168uXL69Zs4YoG%2BLi4i5dusTExMTGxsbNzd3f37937976%2BnpmZmagbHR09J49e5YvX66kpATVEBYW9ubNm2nTphkbG7e2tp44cQLIuHfvXm5urpaWFlDKysqqu7v73LlzECMYIiIiHj58mJCQoKKicvXq1bS0NKBgW1vbjh074uPjgeqAXE1NzSdPnvDz84M0AEUvXLgAsW379u1z5swBen3jxo2zZ892cHB4%2BvQp0KlAfwI1cHJyghQFBwfv2rULokFXV%2FfixYu7d%2B8GGqGgoMDKyrpu3br9%2B%2FcDuXl5eVA%2FAEWBfoWHAdAYoNuAYQ0XAeoUERFhGDYAAPoUaT2dfWJuAAAAAElFTkSuQmCC",
                false);
}

function test2() {
  var engine = gSS.getEngineByName("Foo");
  ok(engine, "Engine was added.");

  var aEngine = gSS.getEngineByAlias("fooalias");
  ok(!aEngine, "Alias was not parsed from engine description");

  gSS.currentEngine = engine;
}

function test3() {
  var engine = gSS.currentEngine;
  is(engine.name, "Foo", "Current engine was changed successfully");

  gSS.removeEngine(engine);
}

function test4() {
  var engine = gSS.currentEngine;
  ok(engine, "An engine is present.");
  isnot(engine.name, "Foo", "Current engine reset after removal");

  gObs.removeObserver(observers, "browser-search-engine-modified");
  finish();
}
