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
 * The Original Code is Places.
 *
 * The Initial Developer of the Original Code is
 * Mozilla.org
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Dietrich Ayala <dietrich@mozilla.com>
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

// put cleanup of the bookmarks test here.

// XPCShell doesn't dispatch quit-application, to ensure cleanup we have to
// dispatch it after each test run.
var os = Cc['@mozilla.org/observer-service;1'].
         getService(Ci.nsIObserverService);
os.notifyObservers(null, "quit-application-granted", null);
os.notifyObservers(null, "quit-application", null);

// try to close the connection so we can remove places.sqlite
var pip = Cc["@mozilla.org/browser/nav-history-service;1"].
          getService(Ci.nsINavHistoryService).
          QueryInterface(Ci.nsPIPlacesDatabase);
if (pip.DBConnection.connectionReady) {
  // Run the event loop to be more like the browser, which normally runs the
  // event loop long before code like this would run.
  // Not doing so could cause us to close the connection between all tasks have
  // been completed, and that would crash badly.
  let tm = Cc["@mozilla.org/thread-manager;1"].getService(Ci.nsIThreadManager);
  while (tm.mainThread.hasPendingEvents())
    tm.mainThread.processNextEvent(false);

  pip.commitPendingChanges();
  pip.finalizeInternalStatements();
  pip.DBConnection.close();
  do_check_false(pip.DBConnection.connectionReady);
}
