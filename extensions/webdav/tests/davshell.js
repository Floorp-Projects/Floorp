const C = Components;
const CI = C.interfaces;

// I wonder how many copies of this are floating around
function findErr(result)
{
    for (var i in C.results) {
        if (C.results[i] == result) {
            dump(i + "\n");
            return;
        }
    }
    dump("No result code found for " + result + "\n");
}

function getService(contract, iface)
{
    return C.classes[contract].getService(CI[iface]);
}

function createInstance(contract, iface)
{
    return C.classes[contract].createInstance(CI[iface]);
}

const davSvc = getService("@mozilla.org/webdav/service;1",
			  "nsIWebDAVService");

function Resource(url)
{
  this.urlSpec = url;
}

Resource.prototype = {
   QueryInterface: function(outer, iid) {
       if (iid.equals(CI.nsIWebDAVResource) ||
           iid.equals(CI.nsISupports)) {
           return this;
       }
       
       throw Components.interfaces.NS_NO_INTERFACE;
   }
};

function ResourceWithFileData(url, filename)
{
    this.urlSpec = url;
    var file = makeFile(filename);
    var instream = createInstance("@mozilla.org/network/file-input-stream;1",
                                  "nsIFileInputStream");
    instream.init(file, 0x01, 0, 0);

    var buffered = createInstance("@mozilla.org/network/buffered-input-stream;1",
                                  "nsIBufferedInputStream");
    buffered.init(instream, 64 * 1024);
    this.data = buffered;
}

ResourceWithFileData.prototype = {
    QueryInterface: function(outer, iid) {
        if (iid.equals(CI.nsIWebDAVResourceWithData))
            return this;

        return Resource.prototype.QueryInterface.call(this, outer, iid);
    },

    __proto__: Resource.prototype,
};

function propertiesToKeyArray(props)
{
    return props.getKeys({ });
}

function propertiesToObject(props)
{
    var count = { };
    var keys = props.getKeys({ });

    var propObj = { };
    for (var i = 0; i < keys.length; i++) {
        var key = keys[i];
        var val = props.get(keys[i], CI.nsISupportsString);
        val = val.data;
        propObj[key] = val;
    }

    return propObj;
}

function OperationListener()
{
}

OperationListener.opToName = { };
OperationListener.opNames =
    ["PUT", "GET", "MOVE", "COPY", "REMOVE",
     "MAKE_COLLECTION", "LOCK", "UNLOCK", "GET_PROPERTIES",
     "GET_PROPERTY_NAMES"];
for (var i in OperationListener.opNames) {
    var opName = OperationListener.opNames[i];
    OperationListener.opToName[CI.nsIWebDAVOperationListener[opName]] = opName;
}

OperationListener.prototype =
{
    onOperationComplete: function (status, resource, op)
    {
        dump(OperationListener.opToName[op] + " " + resource.urlSpec +
             " complete: " + status + "\n");
        stopEventPump();
    },

    onOperationDetail: function (status, resource, op, detail)
    {
        dump(resource + " " + OperationListener.opToName[op] + " (" +
             status + "):\n");
        switch(op) {
          case CI.nsIWebDAVOperationListener.GET_PROPERTY_NAMES:
            var keys =
                propertiesToKeyArray(detail.QueryInterface(CI.nsIProperties));
            for (var i = 0; i < keys.length; i++ )
                dump("  " + keys[i] + "\n");
            break;
                
          case CI.nsIWebDAVOperationListener.GET_PROPERTIES:
            dump("detail: " + detail + "\n");
            var propObj =
                propertiesToObject(detail.QueryInterface(CI.nsIProperties));
            for (var i in propObj)
                dump("  " + i + " = " + propObj[i] + "\n");

            break;
        }
    }
}
    
const evQSvc = getService("@mozilla.org/event-queue-service;1",
                          "nsIEventQueueService");
const ioSvc = getService("@mozilla.org/network/io-service;1",
                         "nsIIOService");
 
const evQ = evQSvc.getSpecialEventQueue(CI.nsIEventQueueService.CURRENT_THREAD_EVENT_QUEUE);

function runEventPump()
{
    pumpRunning = true;
    while (pumpRunning) {
        evQ.processPendingEvents();
    }
}

function stopEventPump()
{
    pumpRunning = false;
}

function PROPFIND(url, depth, props)
{
    if (props) {
        var length = props.length;
    } else {
        var length = 0;
        props = null;
    }

    davSvc.getResourceProperties(new Resource(url), length, props, depth,
                                 new OperationListener());
    runEventPump();
}

function PROPFIND_names(url, depth)
{
    davSvc.getResourcePropertyNames(new Resource(url), depth,
                                    new OperationListener);
    runEventPump();
}

function DELETE(url)
{
    davSvc.remove(new Resource(url), new OperationListener());
    runEventPump();
}

function MKCOL(url)
{
    davSvc.makeCollection(new Resource(url), new OperationListener());
    runEventPump();
}

function makeFile(path)
{
    var file = createInstance("@mozilla.org/file/local;1", "nsILocalFile");
    file.initWithPath(path);
    return file;
}

function GET(url, filename)
{
    var file = makeFile(filename);
    var outstream = createInstance("@mozilla.org/network/file-output-stream;1",
                                   "nsIFileOutputStream");
    outstream.init(file, 0x02 | 0x08, 0644, 0);

    var buffered = 
      createInstance("@mozilla.org/network/buffered-output-stream;1",
                     "nsIBufferedOutputStream");

    buffered.init(outstream, 64 * 1024);
    davSvc.getToOutputStream(new Resource(url), buffered,
                             new OperationListener());
    runEventPump();
}

function PUT(filename, url, contentType)
{
    var resource = new ResourceWithFileData(url, filename);
    davSvc.put(resource, contentType, new OperationListener());
    runEventPump();
}
