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

// test to make sure rdf base class is loaded
if(typeof(JS_RDF_LOADED)!='boolean')
  include(JS_LIB_PATH+'rdf/rdf.js');

// test to make sure file class is loaded
if (typeof(JS_FILE_LOADED)!='boolean')
  include(JS_LIB_PATH+'io/file.js');


const JS_RDFFILE_FLAG_SYNC        = 1; // load RDF source synchronously
const JS_RDFFILE_FLAG_DONT_CREATE = 2; // don't create RDF file (RDFFile only)
const JS_RDFFILE_FILE             = "rdfFile.js";

function RDFFile(aPath, aFlags) {
  this.created = false;

  if(aPath) {
    this._file_init(aPath, aFlags);
  }
}
RDFFile.prototype = new RDF;

RDFFile.prototype._file_init = function (aPath, aFlags, aNameSpace, aID) {
  aFlags = aFlags || JS_RDFFILE_FLAG_SYNC; // default to synchronous loading

  if(aNameSpace == null) {
     aNameSpace = "http://jslib.mozdev.org/rdf#";
  }
  if(aID == null) {
     aID = "JSLIB";
  }

  // Ensure we have a base RDF file to work with
  var rdf_file = new File(aPath);

  if (!rdf_file.exists() && !(aFlags & JS_RDFFILE_FLAG_DONT_CREATE)) {
     dump("here!\n");
    if (rdf_file.open("w") != JS_FILE_OK) {
      return;
    }

    var filestr = 
       '<?xml version="1.0" ?>\n' +
       '<RDF:RDF\n' +
       '     xmlns:'+ aID +'="'+ aNameSpace +'"\n' +
       '     xmlns:RDF="http://www.w3.org/1999/02/22-rdf-syntax-ns#">\n' +
       '</RDF:RDF>\n';

    if (rdf_file.write(filestr) != JS_FILE_OK) {
       rdf_file.close();
       return;
    }

    this.created = true;
  }
  rdf_file.close();

  // Get a reference to the available datasources
  var serv = Components.classes["@mozilla.org/network/io-service;1"].
             getService(Components.interfaces.nsIIOService);
  if (!serv) {
      throw Components.results.ERR_FAILURE;
  }
  var uri = serv.newFileURI(rdf_file.nsIFile);
  this._rdf_init(uri.spec, aFlags);
};

jslib_debug('*** load: '+JS_RDFFILE_FILE+' OK');

} // END BLOCK JS_LIB_LOADED CHECK

// If jslib base library is not loaded, dump this error.
else
{
    dump("JS_RDFFILE library not loaded:\n"                                +
         " \tTo load use: chrome://jslib/content/jslib.js\n"            +
         " \tThen: include('chrome://jslib/content/rdf/rdfFile.js');\n\n");
}
