/* -*- Mode: Javascript; tab-width: 2; */

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
 * The Original Code is Urban Rage Software code.
 *
 * The Initial Developer of the Original Code is
 * Eric Plaster.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Eric Plaster <plaster@urbanrage.com)> (original author)
 *   Martin Kutschker <martin.t.kutschker@blackbox.net> (polishing)
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

RDFBase.prototype.getAnonymousResource = function()
{
  jslibDebug("entering getAnonymousNode");
  if(this.isValid()) {
    var res = this.RDF.GetAnonymousResource();
    return new RDFResource("node", res.Value, null, this.dsource);
  } else {
      jslibError(null, "RDF is no longer valid!\n", "NS_ERROR_UNEXPECTED",
            JS_RDF_FILE+":getNode");
    return null;
  }
};

RDFBase.prototype.getAnonymousContainer = function(aType)
{
  jslibDebug("entering getAnonymousContainer");
  if(this.isValid()) {
    var res = this.getAnonymousResource();
    jslibDebug("making Container");
    if(aType == "bag") {
      this.RDFCUtils.MakeBag(this.dsource, res.getResource());
    } else if(aType == "alt") {
      this.RDFCUtils.MakeAlt(this.dsource, res.getResource());
    } else {
      this.RDFCUtils.MakeSeq(this.dsource, res.getResource());
    }
    jslibPrint("* made cont ..."+res.getSubject()+"\n");
    return new RDFContainer(aType, res.getSubject(),null, this.dsource);
  } else {
      jslibError(null, "RDF is no longer valid!\n", "NS_ERROR_UNEXPECTED",
            JS_RDF_FILE+":getNode");
    return null;
  }
};

jslibDebug('*** load: '+JS_RDFBASE_FILE+' OK');

} // END BLOCK JS_LIB_LOADED CHECK

else
{
    jslibPrint("JS_RDFBase library not loaded:\n"                                +
         " \tTo load use: chrome://jslib/content/jslib.js\n"            +
         " \tThen: include('chrome://jslib/content/rdf/rdf.js');\n\n");
}

