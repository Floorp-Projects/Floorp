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
                Martin Kutschker <martin.t.kutschker@blackbox.net>

*/

if(typeof(JS_LIB_LOADED)=='boolean')
{

// test to make sure filesystem base class is loaded
if(typeof(JS_RDFRESOURCE_LOADED)!='boolean')
  include(JS_LIB_PATH+'rdf/rdfResource.js');

const JS_RDFCONTAINER_LOADED    = true;
const JS_RDFCONTAINER_FILE      = "rdfContainer.js";

function RDFContainer(aType, aPath, aParent, aDatasource) {
  if(aDatasource) {
    this._container_init(aType, aPath, aParent, aDatasource);
  }
}

RDFContainer.prototype = new RDFResource;

RDFContainer.prototype._container_init = function(aType, aPath, aParent, aDatasource)
{
  this._resource_init(aType, aPath, aParent, aDatasource);
};

RDFContainer.prototype.addSeq = function(aSeq) {
  return this.addContainer(aSeq, "seq");
};

RDFContainer.prototype.addBag = function(aBag) {
  return this.addContainer(aBag, "bag");
};

RDFContainer.prototype.addAlt = function(aAlt) {
  return this.addContainer(aAlt, "alt");
};

RDFContainer.prototype.addContainer = function(aContainer, aType)
{
  if(this.isValid()) {
    if(!aContainer || !aType)
      jslibError(null, "Must supply two arguments", null, JS_RDFCONTAINER_FILE+":addContainer");

    var res = this.RDF.GetResource(this.subject+":"+aContainer);

    if( this.resource ) {
      this.RDFC.Init(this.dsource, this.resource );

      if(aType == "bag") {
        this.RDFCUtils.MakeBag(this.dsource, res);
      } else if(aType == "alt") {
        this.RDFCUtils.MakeAlt(this.dsource, res);
      } else {
        this.RDFCUtils.MakeSeq(this.dsource, res);
      }
      this.RDFC.AppendElement(res);
    }
    return new RDFContainer(aType, this.subject+":"+aContainer, this.parent, this.dsource);
  } else {
    jslibError(null, "RDFContainer is no longer valid!\n", "NS_ERROR_UNEXPECTED",
          JS_RDFCONTAINER_FILE+":addContainer");
    return null;
  }
};

RDFContainer.prototype.getNode = function(aNode) {
	var rv = null;
  if(this.isValid()) {
		var res = this.RDF.GetResource(this.subject+":"+aNode);
		if(res) {
			return new RDFResource("node", this.subject+":"+aNode, this.subject, this.dsource);
		}
	} else {
      jslibError(null, "RDFContainer is no longer valid!\n", "NS_ERROR_UNEXPECTED",
            JS_RDFCONTAINER_FILE+":getNode");
		return null;
	}
};

RDFContainer.prototype.addNode = function(aNode) {
  if(this.isValid()) {
		var res = this.RDF.GetResource(this.subject+":"+aNode);
		this.RDFC.Init(this.dsource, this.resource);
		this.RDFC.AppendElement(res);
		return new RDFResource("node", this.subject+":"+aNode, this.subject, this.dsource);
	} else {
      jslibError(null, "RDFContainer is no longer valid!\n", "NS_ERROR_UNEXPECTED",
            JS_RDFCONTAINER_FILE+":addNode");
		return null;
	}
};

// FIXME  add a getSeq("relative:path");
//
RDFContainer.prototype.getSubSeqs = function()
{
  return this.getSubResources("seq");
};

RDFContainer.prototype.getSubBags = function()
{
  return this.getSubResources("bag");
};

RDFContainer.prototype.getSubAlts = function()
{
  return this.getSubResources("alt");
};

RDFContainer.prototype.getSubContainers = function()
{
  return this.getSubResources("all");
};

RDFContainer.prototype.getSubNodes = function()
{
  return this.getSubResources("node");
};

RDFContainer.prototype.getSubResources = function(aType)
{
	if(this.isValid()) {
		var list = new Array;

		this.RDFC.Init(this.dsource, this.resource);

		var elems = this.RDFC.GetElements();
		while(elems.hasMoreElements()) {
			var elem = elems.getNext();
			elem = elem.QueryInterface(Components.interfaces.nsIRDFResource);
			if(aType == "bag") {
				if(this.RDFCUtils.IsBag(this.dsource, elem)) {
					list.push(new RDFContainer(aType, elem.Value, this.subject, this.dsource));
				}
			} else if(aType == "alt") {
				if(this.RDFCUtils.IsAlt(this.dsource, elem)) {
					list.push(new RDFContainer(aType, elem.Value, this.subject, this.dsource));
				}
			} else if(aType == "seq") {
				if(this.RDFCUtils.IsSeq(this.dsource, elem)) {
					list.push(new RDFContainer(aType, elem.Value, this.subject, this.dsource));
				}
			} else if(aType == "all") {
				if(this.RDFCUtils.IsContainer(this.dsource, elem)) {
					list.push(new RDFContainer(aType, elem.Value, this.subject, this.dsource));
				}
			} else {
				if(!this.RDFCUtils.IsContainer(this.dsource, elem)) {
					list.push(new RDFResource(aType, elem.Value, this.subject, this.dsource));
				}
			}
		}
		return list;
	} else {
      jslibError(null, "RDFContainer is no longer valid!\n", "NS_ERROR_UNEXPECTED",
            JS_RDFCONTAINER_FILE+":getSubResources");
		return null;
	}
};

RDFContainer.prototype.remove_recursive = function(aPath) 
{
  if(this.isValid()) {
		var res = this.RDF.GetResource(aPath);
		this.RDFC.Init(this.dsource, res);

		var elems = this.RDFC.GetElements();
		while(elems.hasMoreElements()) {
			var elem = elems.getNext();
			if(this.RDFCUtils.IsContainer(this.dsource, elem)) {
				this.remove_recursive(elem.QueryInterface(Components.interfaces.nsIRDFResource).Value);
				this.RDFC.Init(this.dsource, res);
			}
			var arcs = this.dsource.ArcLabelsOut(elem);
			while(arcs.hasMoreElements()) {
				var arc = arcs.getNext();
				var targets = this.dsource.GetTargets(elem, arc, true);
				while (targets.hasMoreElements()) {
					var target = targets.getNext();
					this.dsource.Unassert(elem, arc, target, true);
				}
			}
			this.RDFC.RemoveElement(elem, false);
		}
		this.RDFC.RemoveElement(res, false);
	} else {
      jslibError(null, "RDFContainer is no longer valid!\n", "NS_ERROR_UNEXPECTED",
            JS_RDFCONTAINER_FILE+":remove");
		return null;
	}
};


RDFContainer.prototype.remove = function(aDeep)
{
  if(this.isValid()) {
		if(this.parent != null) {
			var parentres = this.RDF.GetResource(this.parent);
			this.RDFC.Init(this.dsource, parentres);
		}

      if(aDeep) {
         this.remove_recursive(this.subject);
      }

		var arcs = this.dsource.ArcLabelsOut(this.resource);
		while(arcs.hasMoreElements()) {
			var arc = arcs.getNext();
			var targets = this.dsource.GetTargets(this.resource, arc, true);
			while (targets.hasMoreElements()) {
				var target = targets.getNext();
				this.dsource.Unassert(this.resource, arc, target, true);
			}
		}

		if(this.parent != null) {
			this.RDFC.RemoveElement(this.resource, false);
		}
      this.setValid(false);
	} else {
      jslibError(null, "RDFContainer is no longer valid!\n", "NS_ERROR_UNEXPECTED",
            JS_RDFCONTAINER_FILE+":remove");
		return null;
	}
};

jslib_debug('*** load: '+JS_RDFCONTAINER_FILE+' OK');

} // END BLOCK JS_LIB_LOADED CHECK

else
{
    dump("JS_RDF library not loaded:\n"                                +
         " \tTo load use: chrome://jslib/content/jslib.js\n"            +
         " \tThen: include('chrome://jslib/content/rdf/rdf.js');\n\n");
}

