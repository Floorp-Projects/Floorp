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
