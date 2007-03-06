/* -*- Mode: javascript; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is
 *  Oracle Corporation
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mike Shaver <mike.x.shaver@oracle.com>
 *   Dan Mosedale <dan.mosedale@oracle.com>
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

const ioSvc = getService("@mozilla.org/network/io-service;1",
                         "nsIIOService");
 
function URLFromSpec(spec)
{
  return ioSvc.newURI(spec, null, null);
}

function Resource(url)
{
  this.resourceURL = URLFromSpec(url);
}

Resource.prototype = {
   QueryInterface: function(outer, iid) {
       if (iid.equals(CI.nsIWebDAVResource) ||
           iid.equals(CI.nsISupports)) {
           return this;
       }
       
       throw Components.interfaces.NS_ERROR_NO_INTERFACE;
   }
};

function InputStreamForFile(filename)
{
    var file = makeFile(filename);
    var instream = createInstance("@mozilla.org/network/file-input-stream;1",
                                  "nsIFileInputStream");
    instream.init(file, 0x01, 0, 0);

    var buffered = createInstance("@mozilla.org/network/buffered-input-stream;1",
                                  "nsIBufferedInputStream");
    buffered.init(instream, 64 * 1024);
    return buffered;
}

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
     "GET_PROPERTY_NAMES", "GET_TO_STRING", "REPORT"];
for (var i in OperationListener.opNames) {
    var opName = OperationListener.opNames[i];
    OperationListener.opToName[CI.nsIWebDAVOperationListener[opName]] = opName;
}

OperationListener.prototype =
{
    onOperationComplete: function (status, resource, op, closure)
    {
        dump(OperationListener.opToName[op] + " " + resource.resourceURL.spec +
             " complete: " + status + "\n");
        stopEventPump();
    },

    onOperationDetail: function (status, resource, op, detail, closure)
    {
        dump(resource.spec + " " + OperationListener.opToName[op] + " (" +
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
          case CI.nsIWebDAVOperationListener.GET_TO_STRING:
            this.mGetToStringResult = detail.QueryInterface(CI.nsISupportsCString).data;
            break;

          case CI.nsIWebDAVOperationListener.REPORT:

            var xSerializer = getService('@mozilla.org/xmlextras/domparser;1',
                                         CI.nsIDOMParser);
            dump("detail: " + xSerializer.serializeToString(aDetail) + "\n");
            break;
        }
    }
}
    
const thrd =
    C.classes["@mozilla.org/thread-manager;1"].getService().currentThread;

function runEventPump()
{
    pumpRunning = true;
    while (pumpRunning) {
        thrd.processNextEvent();
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
                                 new OperationListener(), null);
    runEventPump();
}

function PROPFIND_names(url, depth)
{
    davSvc.getResourcePropertyNames(new Resource(url), depth,
                                    new OperationListener, null);
    runEventPump();
}

function DELETE(url)
{
    davSvc.remove(new Resource(url), new OperationListener(), null);
    runEventPump();
}

function MKCOL(url)
{
    davSvc.makeCollection(new Resource(url), new OperationListener(), null);
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
                             new OperationListener(), null);
    runEventPump();
}

function GET_string(url)
{
    var listener = new OperationListener();
    davSvc.getToString(new Resource(url), listener, null);
    runEventPump();
    return listener.mGetToStringResult;
}

function COPY(url, target, recursive, overwrite)
{
    davSvc.copyTo(new Resource(url), target, recursive, overwrite,
                  new OperationListener(), null);
    runEventPump();
}

function MOVE(url, target, overwrite)
{
    davSvc.moveTo(new Resource(url), target, overwrite,
                  new OperationListener(), null);
    runEventPump();
}

function PUT(filename, url, contentType)
{
    var stream = InputStreamForFile(filename);
    davSvc.put(new Resource(url), contentType, stream, 
               new OperationListener(), null);
    runEventPump();
}

function PUT_string(string, url, contentType)
{
    davSvc.putFromString(new Resource(url), contentType, string,
                         new OperationListener(), null);
    runEventPump();
}

function REPORT(url, query, depth)
{
    davSvc.report(new Resource(url), query, depth, new OperationListener(),
                  null);
    runEventPump();
}
