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
  const JS_RDFBASE_LOADED        = true;
  const JS_RDFBASE_FILE          = "rdfBase.js";

  const JS_RDFBASE_CONTAINER_PROGID       = '@mozilla.org/rdf/container;1';
  const JS_RDFBASE_CONTAINER_UTILS_PROGID = '@mozilla.org/rdf/container-utils;1';
  const JS_RDFBASE_LOCATOR_PROGID         = '@mozilla.org/filelocator;1';
  const JS_RDFBASE_RDF_PROGID             = '@mozilla.org/rdf/rdf-service;1';
  const JS_RDFBASE_RDF_DS_PROGID          = '@mozilla.org/rdf/datasource;1?name=xml-datasource';

/***************************************
 * RDFBase is the base class for all RDF classes
 *
 */
function RDFBase(aDatasource) {
  this.RDF       = Components.classes[JS_RDFBASE_RDF_PROGID].getService();
  this.RDF       = this.RDF.QueryInterface(Components.interfaces.nsIRDFService);
  this.RDFC      = Components.classes[JS_RDFBASE_CONTAINER_PROGID].getService();
  this.RDFC      = this.RDFC.QueryInterface(Components.interfaces.nsIRDFContainer);
  this.RDFCUtils = Components.classes[JS_RDFBASE_CONTAINER_UTILS_PROGID].getService();
  this.RDFCUtils = this.RDFCUtils.QueryInterface(Components.interfaces.nsIRDFContainerUtils);
  if(aDatasource) {
    this._base_init(aDatasource);
  }
}

RDFBase.prototype = {
  RDF        : null,
  RDFC       : null,
  RDFCUtils  : null,
  dsource    : null,
  valid      : false,

  _base_init : function(aDatasource) {
    this.dsource = aDatasource;
  },

  getDatasource : function() 
  {
    return this.dsource;
  },

  isValid : function()
  {
    return this.valid;
  },

  setValid : function(aTruth)
  {
    if(typeof(aTruth)=='boolean') {
        this.valid = aTruth;
        return this.valid;
    } else {
        return null;
    }
  },

  flush : function()
  {
    if(this.isValid()) {
			        this.dsource.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource).Flush();
    }
  }

};


jslibDebug('*** load: '+JS_RDFBASE_FILE+' OK');

} // END BLOCK JS_LIB_LOADED CHECK

else
{
    dump("JS_RDFBase library not loaded:\n"                                +
         " \tTo load use: chrome://jslib/content/jslib.js\n"            +
         " \tThen: include('chrome://jslib/content/rdf/rdf.js');\n\n");
}

