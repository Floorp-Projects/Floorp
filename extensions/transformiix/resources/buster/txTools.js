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

var name_array,__docSet;

function do_transforms(new_name_array,verbose){
  if (new_name_array) {
    name_array=new_name_array;
    dump("\n==============================\n");
    dump("TransforMiiX regression buster\n");
    dump("==============================\n");
  }
  if (name_array.length){
    current=name_array.shift();
    __docSet=new txDocSet(current);
    setTimeout("do_transforms()",20);
  }
}

function txDocSet(name,verbose){
  this.mName = name;
  this.mBase = document.getElementById('xalan_base').getAttribute('value');
  if (verbose) this.mVerbose=verbose;
  this.mSource = document.implementation.createDocument("","",null);
  this.mStyle = document.implementation.createDocument("","",null);
  this.mReference = document.implementation.createDocument("","",null);
  this.mResult = document.implementation.createDocument("","",null);
  this.mSource.addEventListener("load",this.docLoaded,false);
  this.mStyle.addEventListener("load",this.styleLoaded,false);
  this.mReference.addEventListener("load",this.refLoaded,false);
  this.mSource.load(this.mBase+"conf/"+name+".xml");
  this.mStyle.load(this.mBase+"conf/"+name+".xsl");
  this.mReference.load(this.mBase+"conf-gold/"+name+".out");
}

txDocSet.prototype = {
  mBase      : null,
  mName      : null,
  mSource    : null,
  mStyle     : null,
  mResult    : null,
  mReference : null,
  mVerbose   : false,
  mLoaded    : 0,

  styleLoaded  : function(e){
    __docSet.stuffLoaded(1);
  },
  docLoaded  : function(e){
    __docSet.stuffLoaded(4);
  },
  refLoaded  : function(e){
    __docSet.stuffLoaded(2);
  },
  stuffLoaded  : function(mask){
    __docSet.mLoaded+=mask;
    if (this.mLoaded==7){
      netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');
      var txProc = new XSLTProcessor(),isGood;
      dump("\nTrying "+this.mName+" ");
      txProc.transformDocument(this.mSource,this.mStyle,this.mResult,null);
      try{
        isGood = DiffDOM(this.mResult.documentElement,this.mReference.documentElement);
      } catch (e) {isGood = false;};
      if (!isGood){
        dump(" and failed\n\n");
        this.mVerbose=true;
        handle_result(this.mName,false);
      } else {
        dump(" and succeeded\n\n");
        handle_result(this.mName,true);
      }
      if (this.mVerbose){
        dump("Result:\n");
        DumpDOM(this.mResult);
        dump("Reference:\n");
        DumpDOM(this.mReference);
      }
    }
  }
};
