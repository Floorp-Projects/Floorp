/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Axel Hecht.
 * Portions created by Axel Hecht are Copyright (C) 2001 Axel Hecht.
 * All Rights Reserved.
 *
 * Contributor(s):
 *   Axel Hecht <axel@pike.org> (Original Author)
 */

var name_array,index_array;

function do_transforms(new_name_array,new_number_array,verbose){
  if (new_name_array) {
    name_array=new_name_array;
    index_array=new_number_array;
    dump("\n==============================\n");
    dump("TransforMiiX regression buster\n");
    dump("==============================\n");
  }
  if (name_array.length){
    new txDocSet(name_array.shift(),index_array.shift());
  }
}

function txDocSet(name,index,verbose){
  this.mName = name;
  this.mIndex = index;
  this.mBase = document.getElementById('xalan_base').getAttribute('value');
  if (verbose) this.mVerbose=verbose;
  this.mSource = document.implementation.createDocument("","",null);
  this.mStyle = document.implementation.createDocument("","",null);
  this.mResult = document.implementation.createDocument("","",null);
  this.mSource.addEventListener("load",this.loaded(1),false);
  this.mStyle.addEventListener("load",this.loaded(2),false);
  this.mSource.load(this.mBase+"conf/"+name+".xml");
  this.mStyle.load(this.mBase+"conf/"+name+".xsl");
  this.mIsError = name.match(/\/err\//);
  if (this.mIsError){
    this.stuffLoaded(4);
  } else {
    this.mReference = document.implementation.createDocument("","",null);
    this.mReference.addEventListener("load",this.loaded(4),false);
    this.mReference.load(this.mBase+"conf-gold/"+name+".out");
  }
}

txDocSet.prototype = {
  mBase      : null,
  mName      : null,
  mSource    : null,
  mStyle     : null,
  mResult    : null,
  mReference : null,
  mVerbose   : false,
  mIsError   : false,
  mLoaded    : 0,
  mIndex     : 0,

  loaded  : function(i) {
    var self = this;
    return function(e) { return self.stuffLoaded(i); };
  },
  stuffLoaded  : function(mask) {
    if (mask==4 && this.mReference) {
      var docElement=this.mReference.documentElement;
      if (docElement && docElement.nodeName=="parsererror") {
        try {
          var text=loadFile(docElement.baseURI);
          this.mReference.removeChild(docElement);
          var wrapper=this.mReference.createElement("transformiix:result");
          var textNode=this.mReference.createTextNode(text);
          wrapper.appendChild(textNode);
          this.mReference.appendChild(wrapper);
        }
        catch (e) {
        }
      }
    }
    this.mLoaded+=mask;
    if (this.mLoaded==7){
      netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');
      var txProc = new XSLTProcessor(),isGood=false;
      dump("\nTrying "+this.mName+" ");
      txProc.transformDocument(this.mSource,this.mStyle,this.mResult,null);
      if (this.mIsError) {
        dump("for error test\nResult:\n");
        DumpDOM(this.mResult);
        handle_result(this.mIndex,true);
      } else {
        this.mReference.normalize();
        try{
          isGood = DiffDOM(this.mResult.documentElement,this.mReference.documentElement);
        } catch (e) {isGood = false;};
        if (!isGood){
          dump(" and failed\n\n");
          this.mVerbose=true;
          handle_result(this.mIndex,false);
        } else {
          dump(" and succeeded\n\n");
          handle_result(this.mIndex,true);
        }
        if (this.mVerbose){
          dump("Result:\n");
          DumpDOM(this.mResult);
          dump("Reference:\n");
          DumpDOM(this.mReference);
        }
      }
      do_transforms();
    }
  }
};

const IOSERVICE_CTRID = "@mozilla.org/network/io-service;1";
const nsIIOService    = Components.interfaces.nsIIOService;
const SIS_CTRID       = "@mozilla.org/scriptableinputstream;1"
const nsIScriptableInputStream = Components.interfaces.nsIScriptableInputStream;

function loadFile(url)
{
    netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');
    var serv = Components.classes[IOSERVICE_CTRID].getService(nsIIOService);
    if (!serv)
        throw Components.results.ERR_FAILURE;
    
    var chan = serv.newChannel(url, null);

    var instream = 
        Components.classes[SIS_CTRID].createInstance(nsIScriptableInputStream);
    instream.init(chan.open());

    return instream.read(instream.available());
}
