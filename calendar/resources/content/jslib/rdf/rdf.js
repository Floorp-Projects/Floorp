/*** -*- Mode: Javascript; tab-width: 2;

The contents of this file are subject to the Mozilla Public
License Version 1.1 (the "License"); you may not use this file
except in compliance with the License. You may obtain a copy of
the License at http://www.mozilla.org/MPL/

Software distributed under the License is distributed on an "AS
IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
implied. See the License for the specific language governing
rights and limitations under the License.

The Original Code is Urban Rage Software code.
The Initial Developer of the Original Code is Eric Plaster.

Portions created by Urban Rage Software are
Copyright (C) 2000 Urban Rage Software.  All
Rights Reserved.

Contributor(s): Eric Plaster <plaster@urbanrage.com)> (original author)
                Martin Kutschker <martin.t.kutschker@blackbox.net> (polishing)

*/

if(typeof(JS_LIB_LOADED)=='boolean')
{
  // test to make sure rdf base classes are loaded
  if(typeof(JS_RDFBASE_LOADED)!='boolean')
    include(JS_LIB_PATH+'rdf/rdfBase.js');
  if(typeof(JS_RDFRESOURCE_LOADED)!='boolean')
    include(JS_LIB_PATH+'rdf/rdfResource.js');
  if(typeof(JS_RDFCONTAINER_LOADED)!='boolean')
    include(JS_LIB_PATH+'rdf/rdfContainer.js');

  const JS_RDF_LOADED                 = true;
  const JS_RDF_FILE                   = "rdf.js";

  const JS_RDF_FLAG_SYNC = 1;        // load RDF source synchronously

function RDF(src, flags) {
  this.loaded = false;

  if(src) {
    this._rdf_init(src, flags);
  }
}

RDF.prototype = new RDFBase;

RDF.prototype.src = null;

RDF.prototype._rdf_init = function(src, flags) {
  flags = flags || 0;
  this.src = src;

  var load = true; // load source

  // Create an RDF/XML datasource using the XPCOM Component Manager
  this.dsource = Components
    .classes[JS_RDFBASE_RDF_DS_PROGID]
    .createInstance(Components.interfaces.nsIRDFDataSource);

  // The nsIRDFRemoteDataSource interface has the interfaces
  // that we need to setup the datasource.
  var remote = this.dsource.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);

  try {
    remote.Init(src); // throws an exception if URL already in use
  }
  catch(err) {
    // loading already
    load = false;

    jslibDebug(JS_RDF_FILE+":_rdf_init: Init of "+src+" failed.");
  }

  if (load) {
    try {
      remote.Refresh((flags & JS_RDF_FLAG_SYNC) ? true: false);
    }
    catch(err) {
      this.dsource = null;

      jslibError(err, "Error refreshing remote rdf: "+src, "NS_ERROR_UNEXPECTED",
             JS_RDF_FILE+":_rdf_init");
      return;
    }
  }
  else {
    try {
      this.dsource = this.RDF.GetDataSource(src);
      remote = this.dsource.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);
    }
    catch(err) {
      this.dsource = null;

      jslibError(err, "Error getting datasource: "+src, "NS_ERROR_UNEXPECTED",
             JS_RDF_FILE+":_rdf_init");
      return;
    }
  }

  try {
    if (remote.loaded) {
      this.loaded = true;
      this.setValid(true);
    }
    else {
      var obs = {
        rdf: this, // backreference to ourselves

        onBeginLoad: function(aSink)
        {
        },

        onInterrupt: function(aSink)
        {},

        onResume: function(aSink)
        {},

        onEndLoad: function(aSink)
        {
           this.rdf.loaded = true;
           this.rdf.setValid(true);
        },

        onError: function(aSink, aStatus, aErrorMsg)
        {
          jslibError(null,"Error loading datasource: "+aErrorMsg, 
                "NS_ERROR_UNEXPECTED", JS_RDF_FILE+":_rdf_init (observer)");
        }
      };

      // RDF/XML Datasources are all nsIRDFXMLSinks
      var sink = this.dsource.QueryInterface(Components.interfaces.nsIRDFXMLSink);

      // Attach the observer to the datasource-as-sink
      sink.addXMLSinkObserver(obs);
    }
  }
  catch(err) {
     jslibError(err, "Error loading rdf!\n", "NS_ERROR_UNEXPECTED",
           JS_RDF_FILE+":_rdf_init");
     return;
  }
};

RDF.prototype.getSource = function()
{
  return this.src;
};

RDF.prototype.getNode = function(aPath)
{
  if(this.isValid()) {
    var res = this.RDF.GetResource(aPath);
    return new RDFResource("node", res.Value, null, this.dsource);
  } else {
      jslibError(null, "RDF is no longer valid!\n", "NS_ERROR_UNEXPECTED",
            JS_RDF_FILE+":getNode");
    return null;
  }
};

RDF.prototype.addRootSeq = function(aSeq)
{
  return this.addRootContainer(aSeq, "seq");
};

RDF.prototype.addRootAlt = function(aAlt)
{
  return this.addRootContainer(aAlt, "alt");
};

RDF.prototype.addRootBag = function(aBag)
{
  return this.addRootContainer(aBag, "bag");
};

RDF.prototype.addRootContainer = function(aContainer, aType)
{
  if(this.isValid()) {
    if(!aContainer)
      jslibError(null, "Must supply a container path", null, JSRDFCONTAINER+":addRootContainer");

    var res = this.RDF.GetResource(aContainer);

    // FIXME: should test if exists and is already a container

    if(aType == "bag") {
      this.RDFCUtils.MakeBag(this.dsource, res);
    } else if(aType == "alt") {
      this.RDFCUtils.MakeAlt(this.dsource, res);
    } else if(aType == "seq") {
      this.RDFCUtils.MakeSeq(this.dsource, res);
    } else {
      // FIXME: this.RDFCUtils.MakeContainer....
    }
    return new RDFContainer(aType, aContainer, null, this.dsource);
  } else {
      jslibError(null, "RDF is no longer valid!\n", "NS_ERROR_UNEXPECTED",
            JS_RDF_FILE+":addRootContainer");
    return null;
  }
};


RDF.prototype.getRootSeq = function(aSeq)
{
  return this.getContainer(aSeq, "seq");
};

RDF.prototype.getRootAlt = function(aAlt)
{
  return this.getContainer(aAlt, "alt");
};

RDF.prototype.getRootBag = function(aBag)
{
  return this.getContainer(aBag, "bag");
};

RDF.prototype.getContainer = function(aContainer, aType)
{
  var rv = null;
  if(this.isValid()) {
    var res = this.RDF.GetResource(aContainer);
    if(res) {
      rv = new RDFContainer(aType, aContainer, null, this.dsource);
    }
  }
  return rv;
};

RDF.prototype.getAllSeqs = function()
{
  return this.getRootContainers("seq");
};

RDF.prototype.getAllAlts = function()
{
  return this.getRootContainers("alt");
};

RDF.prototype.getAllBags = function()
{
  return this.getRootContainers("bag");
};

RDF.prototype.getAllContainers = function()
{
  return this.getRootContainers("all");
};

RDF.prototype.getRootContainers = function(aType)
{
  var rv = null;
  if(this.isValid()) {
    var list = new Array;
    var elems = this.dsource.GetAllResources();
    while(elems.hasMoreElements()) {
      var elem = elems.getNext();
      elem = elem.QueryInterface(Components.interfaces.nsIRDFResource);
      if(aType == "bag") {
        if(this.RDFCUtils.IsBag(this.dsource, elem)) {
          list.push(new RDFContainer(aType, elem.Value, null, this.dsource));
        }
      } else if(aType == "alt") {
        if(this.RDFCUtils.IsAlt(this.dsource, elem)) {
          list.push(new RDFContainer(aType, elem.Value, null, this.dsource));
        }
      } else if(aType == "seq") {
        if(this.RDFCUtils.IsSeq(this.dsource, elem)) {
          list.push(new RDFContainer(aType, elem.Value, null, this.dsource));
        }
      } else if(aType == "all") {
        if(this.RDFCUtils.IsContainer(this.dsource, elem)) {
          list.push(new RDFContainer(aType, elem.Value, null, this.dsource));
        }
      } else {
        if(!this.RDFCUtils.IsContainer(this.dsource, elem)) {
          list.push(new RDFResource(aType, elem.Value, null, this.dsource));
        }
      }
    }
    return list;
  } else {
      jslibError(null, "RDF is no longer valid!\n", "NS_ERROR_UNEXPECTED",
            JS_RDF_FILE+":getRootContainers");
    return null;
  }
};

RDF.prototype.flush = function()
{
  if(this.isValid()) {
    this.dsource.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource).Flush();
  }
};


jslibDebug('*** load: '+JS_RDF_FILE+' OK');

} // END BLOCK JS_LIB_LOADED CHECK

else
{
  dump("JS_RDF library not loaded:\n"                                +
      " \tTo load use: chrome://jslib/content/jslib.js\n"            +
      " \tThen: include('chrome://jslib/content/rdf/rdf.js');\n\n");
}

