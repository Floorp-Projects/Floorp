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
  // test to make sure rdfBase base class is loaded
  if(typeof(JS_RDFBASE_LOADED)!='boolean')
      include(JS_LIB_PATH+'rdf/rdfBase.js');

  const JS_RDFRESOURCE_LOADED        = true;
  const JS_RDFRESOURCE_FILE          = "rdfResource.js";

  function RDFResource(aType, aPath, aParentPath, aDatasource) {
      if(aDatasource) {
        this._resource_init(aType, aPath, aParentPath, aDatasource);
      }
  }

  RDFResource.prototype = new RDFBase;

  RDFResource.prototype.type = null;
  RDFResource.prototype.parent = null;
  RDFResource.prototype.resource = null;
  RDFResource.prototype.subject = null;

  RDFResource.prototype._resource_init = function(aType, aPath, aParentPath, aDatasource) {
      this.type = aType;
      this.parent = aParentPath;
      this.subject = aPath;
      this.resource = this.RDF.GetResource(aPath);

      this._base_init(aDatasource);
      if(this.resource) {
        this.setValid(true);
      }
  };

  RDFResource.prototype.getResource = function() {
      return this.resource;
  };

  RDFResource.prototype.getSubject = function() {
      return this.subject;
  };

  RDFResource.prototype.makeSeq = function(aSeq) {
      return this.makeContainer("seq");
  };

  RDFResource.prototype.makeBag = function(aBag) {
      return this.makeContainer("bag");
  };

  RDFResource.prototype.makeAlt = function(aAlt) {
      return this.makeContainer("alt");
  };

  RDFResource.prototype.makeContainer = function(aType) {
      this.RDFC.Init(this.dsource, this.resource );

      if(aType == "bag") {
        this.RDFCUtils.MakeBag(this.dsource, this.resource);
      } else if(aType == "alt") {
        this.RDFCUtils.MakeAlt(this.dsource, this.resource);
      } else {
        this.RDFCUtils.MakeSeq(this.dsource, this.resource);
      }
      jslibPrint("* made cont ...\n");
      return new RDFContainer(aType, this.resource_path+":"+aContainer, this.parent, this.dsource);
      this.setValid(false);
  };

  RDFResource.prototype.setAttribute = function(aName, aValue)
  {
      if(this.isValid()) {
        var oldvalue = this.getAttribute(aName);

        if(oldvalue) {
            this.dsource.Change(this.resource,
                  this.RDF.GetResource(aName),
                  this.RDF.GetLiteral(oldvalue),
                  this.RDF.GetLiteral(aValue) );
            jslibPrint("\n Changing old value in "+this.subject+"\n");
        } else {
            this.dsource.Assert(this.resource,
                  this.RDF.GetResource(aName),
                  this.RDF.GetLiteral(aValue),
                  true );
            jslibPrint("\n Adding a new value in "+this.subject+"\n");
        }
        return true;
      } else {
        return false;
      }
  };

  RDFResource.prototype.getAttribute = function(aName)
  {
      if(this.isValid()) {
        var itemRes = this.RDF.GetResource(aName);
        if (!itemRes) { return null; }
          var target = this.dsource.GetTarget(this.resource, itemRes, true);
        if (target) target = target.QueryInterface(Components.interfaces.nsIRDFLiteral);
        if (!target) { return null; }
        return target.Value;
      } else {
        jslibError(null, "RDFResource is no longer valid!\n", "NS_ERROR_UNEXPECTED",
              JS_RDFRESOURCE_FILE+":getAttribute");
        return null;
      }
  };

  RDFResource.prototype.getContainer = function(aName,aType)
  {
      if(this.isValid()) {
        var itemRes = this.RDF.GetResource(aName);
        if (!itemRes) { return null; }
        var target = this.dsource.GetTarget(this.resource, itemRes, true);
        if (target) target = target.QueryInterface(Components.interfaces.nsIRDFResource);
        if (!target) { return null; }
        if(!aType) aType = "bag";
        return new RDFContainer(aType, target.Value, null, this.dsource);
      } else {
        jslibError(null, "RDFResource is no longer valid!\n", "NS_ERROR_UNEXPECTED",
              JS_RDFRESOURCE_FILE+":getAttribute");
        return null;
      }
  };

  RDFResource.prototype.addContainer = function(aName,aType)
  {
     if(this.isValid()) {
        //var oldvalue = this.getContainer(aName);
        var newC = this.getAnonymousContainer(aType);
        this.dsource.Assert( this.resource,this.RDF.GetResource(aName), newC.getResource(), true );
        jslibPrint("\n Adding a new value in "+this.subject+"\n");
        return newC;
      } else {
        jslibPrint("\n cudnt get anon container\n");
        return null;
      }
  };

  RDFResource.prototype.getAssociationContainers = function(aName)
  {
    if(this.isValid()) {
      var list = new Array();
      var arcs = this.dsource.ArcLabelsIn(this.resource);
      while(arcs.hasMoreElements()) {
        var arc = arcs.getNext();
        arc = arc.QueryInterface(Components.interfaces.nsIRDFResource);
        jslibDebug("Got arc " +arc.Value);
        if(!this.RDFCUtils.IsOrdinalProperty(arc)) {
          continue;
        }
        var targets = this.dsource.GetSources(arc, this.resource,  true);
        var itemRes = this.RDF.GetResource(aName);
        while (targets.hasMoreElements()) {
          var target = targets.getNext();
          target = target.QueryInterface(Components.interfaces.nsIRDFResource);
          if(this.RDFCUtils.IsContainer(this.dsource,target)) {
            if(this.dsource.hasArcIn( target, itemRes)) {
              target = new RDFContainer(null, target.Value, null, this.dsource);
              list.push(target);
            }
          }
        }
      }
      return list;
    } else {
      jslibError(null, "RDFResource is no longer valid!\n", "NS_ERROR_UNEXPECTED",
            JS_RDFRESOURCE_FILE+":getAttribute");
      return null;
    }
  };

  RDFResource.prototype.removeAttribute = function(aName)
  {
      if(this.isValid()) {
        var itemRes = this.RDF.GetResource(aName, true);
        var target = this.dsource.GetTarget(this.resource, itemRes, true);
        this.dsource.Unassert(this.resource, itemRes, target);
      } else {
        jslibError(null, "RDFResource is no longer valid!\n", "NS_ERROR_UNEXPECTED",
              JS_RDFRESOURCE_FILE+":removeAttribute");
        return null;
      }
  };

  RDFResource.prototype.setAllAttributes = function(aList)
  {
    var length = 0;
    try {
      length = aList.length;
    } catch(e) {
      return false;
    }
      if(this.isValid()) {
        var arcs = this.dsource.ArcLabelsOut(this.resource);
        while(arcs.hasMoreElements()) {
            var arc = arcs.getNext();
            arc = arc.QueryInterface(Components.interfaces.nsIRDFResource);
            var obj = new Object;
            var l = arc.Value.split("#");
            obj.name = l[l.length-1];
            var targets = this.dsource.GetTargets(this.resource, arc, true);
            while (targets.hasMoreElements()) {
              var target = targets.getNext();
              this.dsource.Unassert(this.resource, arc, target, true);
            }
        }
        for(var i=0; i<length; i++) {
          this.setAttribute(aList[i].name, aList[i].value);
        }
      }
  };

  RDFResource.prototype.getAllAttributes = function()
  {
      var list = new Array;
      if(this.isValid()) {
        var arcs = this.dsource.ArcLabelsOut(this.resource);
        while(arcs.hasMoreElements()) {
            var arc = arcs.getNext();
            arc = arc.QueryInterface(Components.interfaces.nsIRDFResource);
            var obj = new Object;
            var l = arc.Value.split("#");
            obj.name = l[l.length-1];
            var targets = this.dsource.GetTargets(this.resource, arc, true);
            while (targets.hasMoreElements()) {
              var target = targets.getNext();
              if(target) {
                  try {
                    target = target.QueryInterface(Components.interfaces.nsIRDFLiteral);
                  }
                  catch(e) {
                    jslibPrint('not a literal');
                    target = target.QueryInterface(Components.interfaces.nsIRDFResource);
                  }
                  obj.value = target.Value;
                  list.push(obj);
              }
            }
        }
        return list;
      } else {
        jslibError(null, "RDFResource is no longer valid!\n", "NS_ERROR_UNEXPECTED",
              JS_RDFRESOURCE_FILE+":getAllAttributes");
        return null;
      }
  };

  RDFResource.prototype.remove = function()
  {
      if(this.isValid()) {
        // FIXME:  if we get this node from RDF, it has no parent...
        //   try just removing all arcs and targets...
        //    var parentres = this.RDF.GetResource(this.parent);
        //   this.RDFC.Init(this.dsource, parentres);

        var arcs = this.dsource.ArcLabelsOut(this.resource);
        while(arcs.hasMoreElements()) {
            var arc = arcs.getNext();
            var targets = this.dsource.GetTargets(this.resource, arc, true);
            while (targets.hasMoreElements()) {
              var target = targets.getNext();
              this.dsource.Unassert(this.resource, arc, target, true);
            }
        }
        this.RDFC.RemoveElement(this.resource, false); //removes the parent element
        this.setValid(false);
      } else {
        jslibError(null, "RDFResource is no longer valid!\n", "NS_ERROR_UNEXPECTED",
              JS_RDFRESOURCE_FILE+":remove");
        return null;
      }
  };


  jslibDebug('*** load: '+JS_RDFRESOURCE_FILE+' OK');

} // END BLOCK JS_LIB_LOADED CHECK

else
{
   dump("JS_RDFResource library not loaded:\n"                                +
         " \tTo load use: chrome://jslib/content/jslib.js\n"            +
         " \tThen: include('chrome://jslib/content/rdf/rdf.js');\n\n");
}

