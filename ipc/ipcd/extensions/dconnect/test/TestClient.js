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
