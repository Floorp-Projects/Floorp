/* vim:set ts=2 sw=2 et cindent: */
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
 * The Original Code is Mozilla IPC.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@meer.net>
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

/**
 * This file contains code that mirrors the code in TestDConnect.cpp:DoTest
 */

const ipcIService          = Components.interfaces.ipcIService;
const ipcIDConnectService  = Components.interfaces.ipcIDConnectService;
const nsIFile              = Components.interfaces.nsIFile;
const nsILocalFile         = Components.interfaces.nsILocalFile;
const nsIEventQueueService = Components.interfaces.nsIEventQueueService;

// XXX use directory service for this
const TEST_PATH = "/tmp";

var serverID = 0;

function findServer()
{
  var ipc = Components.classes["@mozilla.org/ipc/service;1"].getService(ipcIService); 
  serverID = ipc.resolveClientName("test-server");
}

function doTest()
{
  var dcon = Components.classes["@mozilla.org/ipc/dconnect-service;1"]
                       .getService(ipcIDConnectService); 

  var file = dcon.createInstanceByContractID(serverID, "@mozilla.org/file/local;1", nsIFile);

  var localFile = file.QueryInterface(nsILocalFile);

  localFile.initWithPath(TEST_PATH);

  if (file.path != TEST_PATH)
  {
    dump("*** path test failed [path=" + file.path + "]\n");
    return;
  }

  dump("file exists: " + file.exists() + "\n");

  var clone = file.clone();

  const node = "hello.txt";
  clone.append(node);
  dump("files are equal: " + file.equals(clone) + "\n");

  if (!clone.exists())
    clone.create(nsIFile.NORMAL_FILE_TYPE, 0600);

  clone.moveTo(null, "helloworld.txt");

  var localObj = Components.classes["@mozilla.org/file/local;1"].createInstance(nsILocalFile);
  localObj.initWithPath(TEST_PATH);
  dump("file.equals(localObj) = " + file.equals(localObj) + "\n");
  dump("localObj.equals(file) = " + localObj.equals(file) + "\n");
}

function setupEventQ()
{
  var eqs = Components.classes["@mozilla.org/event-queue-service;1"]
                      .getService(nsIEventQueueService);  
  eqs.createMonitoredThreadEventQueue();
}

setupEventQ();
findServer();
dump("\n---------------------------------------------------\n");
doTest();
dump("---------------------------------------------------\n\n");
