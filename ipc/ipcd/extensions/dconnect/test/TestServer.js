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
 * This file contains code for a "server" that does nothing more
 * than set a name for itself on the IPC system so that the client
 * can connect and control this process.
 */

const ipcIService          = Components.interfaces.ipcIService;
const nsIEventQueueService = Components.interfaces.nsIEventQueueService;
const nsIEventQueue        = Components.interfaces.nsIEventQueue;

function registerServer()
{
  var ipc = Components.classes["@mozilla.org/ipc/service;1"].getService(ipcIService); 
  ipc.addName("test-server");
}

function runEventQ()
{
  var eqs = Components.classes["@mozilla.org/event-queue-service;1"]
                      .getService(nsIEventQueueService);  
  eqs.createMonitoredThreadEventQueue();
  var queue = eqs.getSpecialEventQueue(eqs.CURRENT_THREAD_EVENT_QUEUE);

  // this never returns
  queue.eventLoop(); 
}

registerServer();
runEventQ();
