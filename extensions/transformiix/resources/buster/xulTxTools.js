/* -*- tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

var pop_last=0, pop_chunk=25;
var isinited=false;
var xalan_base, xalan_xml, xalan_elems, xalan_length, content_row, target;
var matchRE, matchNameTag, matchFieldTag, startFieldTag, endFieldTag;
var tests_run, tests_passed, tests_failed, tests_selected;
var view = ({
// nsIOutlinerView
  rowCount : 0,
  getRowProperties : function(i, prop) {
    netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');
    if (this.success[i]==1) prop.AppendElement(this.smile);
    if (this.success[i]==-1) prop.AppendElement(this.sad);
  },
  getColumnProperties : function(index, prop) {},
  getCellProperties : function(index, prop) {},
  isContainer : function(index) {return false;},
  isSeparator : function(index) {return false;},
  outliner : null,
  setOutliner : function(out) { this.outliner = out; },
  getCellText : function(i, col) {
    switch(col){
      case "name":
        return this.names[i];
      case "purp":
        return this.purps[i];
      case "comm":
        return this.comms[i];
      default:
        return "XXX in "+ col+" and "+i;
     }
   },
// nsIClassInfo
  flags : Components.interfaces.nsIClassInfo.DOM_OBJECT,
  classDescription : "OutlinerView",
  getInterfaces: function() {},
  getHelperForLanguage: function(aLang) {},

  QueryInterface: function(iid) {
    netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');
    if (!iid.equals(Components.interfaces.nsISupports) &&
        !iid.equals(Components.interfaces.nsIOutlinerView) &&
        !iid.equals(Components.interfaces.nsIClassInfo))
      throw Components.results.NS_ERROR_NO_INTERFACE;

    return this;
  },
// privates
  names : null,
  purps : null,
  comms : null,
  success : null,
  smile : null,
  sad : null,
  select : function(index) {
    netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');
    if (!this.selection.isSelected(index)) 
      this.selection.toggleSelect(index);
  },
  unselect : function(index) {
    netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');
    if (this.selection.isSelected(index)) 
      this.selection.toggleSelect(index);
  },
  swallow : function(initial) {
    netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');
    var startt = new Date();
    this.rowCount = initial.length;
    this.success = new Array(this.rowCount);
    this.names = new Array(this.rowCount);
    this.purps = new Array(this.rowCount);
    this.comms = new Array(this.rowCount);
    var cur = initial.item(0);
    var k = 0;
    while (cur.nextSibling) {
      if (cur.nodeType==Node.ELEMENT_NODE) {
        this.names[k] = cur.getAttribute("file");
        if (cur.childNodes.length>1)
          this.purps[k] = cur.childNodes.item(1).firstChild.nodeValue;
        if (cur.childNodes.length>3)
          this.comms[k] = cur.childNodes.item(3).firstChild.nodeValue;
        k = k+1;
      }
      cur = cur.nextSibling;
    }
    this.outliner.rowCountChanged(0,this.rowCount);
    //dump((new Date()-startt)/1000+" secs in swallow for "+k+" items\n");
  },
  remove : function(first,last){
    last = Math.min(this.rowCount,last); 
    first = Math.max(0,first);
    this.names.splice(first,last-first+1);
    this.purps.splice(first,last-first+1);
    this.comms.splice(first,last-first+1);
    this.success.splice(first,last-first+1);
    this.rowCount=this.names.length;
    this.outliner.rowCountChanged(first,this.rowCount);
  },
  getNames : function(first,last){
    last = Math.min(this.rowCount,last); 
    first = Math.max(0,first);
    var list = new Array(last-first+1);
    for (var k=first;k<=last;k++)
      list[k-first] = this.names[k];
    return list;
  }
});

function setView(outliner, v) {
  outliner.boxObject.QueryInterface(Components.interfaces.nsIOutlinerBoxObject).view = v;
}

function loaderstuff(eve) {
  var ns = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
  netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');
  var atomservice = Components.classes["@mozilla.org/atom-service;1"].getService(Components.interfaces.nsIAtomService);
  view.smile = atomservice.getAtom("success");
  view.sad = atomservice.getAtom("fail");
  matchNameTag = document.getElementById("search-name");
  matchFieldTag = document.getElementById("search-field");
  startFieldTag = document.getElementById("start-field");
  endFieldTag = document.getElementById("end-field");
  tests_run = document.getElementById("tests_run");
  tests_passed = document.getElementById("tests_passed");
  tests_failed = document.getElementById("tests_failed");
  tests_selected = document.getElementById("tests_selected");
  xalan_base = document.getElementById("xalan_base");
  setView(document.getElementById('out'), view)
  xalan_xml = document.implementation.createDocument("","",null);
  xalan_xml.addEventListener("load", xalanIndexLoaded, false);
  xalan_xml.load("complete-xalanindex.xml");
  //xalan_xml.load("xalanindex.xml");
  return true;
}

function xalanIndexLoaded(e) {
  populate_xalan();
  return true;
}

function refresh_xalan() {
  populate_xalan();
  return true;
}


function populate_xalan() {
  view.swallow(xalan_xml.getElementsByTagName("test"));
}

function dump_checked(){
  var sels = view.selection,a=new Object(),b=new Object(),k;
  reset_stats();
  var todo = new Array();
  var nums = new Array();
  for (k=0;k<sels.getRangeCount();k++){
    sels.getRangeAt(k,a,b);
    todo = todo.concat(view.getNames(a.value,b.value));
    for (var l=a.value;l<=b.value;l++) nums.push(l);
  }
  do_transforms(todo,nums);
}

function handle_result(index,success){
  tests_run.getAttributeNode("value").nodeValue++;
  if (success) {
    view.success[index] = 1;
    tests_passed.getAttributeNode("value").nodeValue++;
  } else {
    view.success[index] = -1;
    tests_failed.getAttributeNode("value").nodeValue++;
  }
  view.unselect(index);
  netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');
  view.outliner.invalidateRow(index);
}

function hide_checked(yes){
  netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');
  var sels = view.selection,a=new Object(),b=new Object(),k;
  if (yes) {
    for (k=sels.getRangeCount()-1;k>=0;k--){
      sels.getRangeAt(k,a,b);
      view.remove(a.value,b.value);
      view.selection.clearRange(a.value,b.value);
    }
  } else {
    var last=view.rowCount;
    for (k=view.selection.getRangeCount()-1;k>=0;k--){
      view.selection.getRangeAt(k,a,b);
      view.remove(b.value+1,last);
      view.selection.clearRange(a.value,b.value);
      last = a.value-1;
    }
  view.remove(0,last);
  }
}

function select(){
  netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');
  var searchField = matchFieldTag.getAttribute("value");
  var matchRE = new RegExp(matchNameTag.value);
  for (var k=0;k<view.rowCount;k++){
    switch (searchField) {
      case "1":
        if (view.names[k] && view.names[k].match(matchRE))
          view.select(k);
        break;
      case "2":
        if (view.purps[k] && view.purps[k].match(matchRE))
          view.select(k);
        break;
      case "3":
        if (view.comms[k] && view.comms[k].match(matchRE))
          view.select(k);
        break;
      default: dump(searchField+"|"+typeof(searchField)+"\n");
    }
  }
}

function selectRange(){
  var start = startFieldTag.value-1;
  var end = endFieldTag.value-1;
  if (start > end) {
    // hihihi
    var tempStart = start;
    start = end;
    end = startTemp;
  }
  start = Math.max(0, start);
  end = Math.min(end, view.rowCount-1);
  view.selection.rangedSelect(start,end,true);
}

function check(yes){
  netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');
  if (yes)
    view.selection.selectAll();
  else
    view.selection.clearSelection();
}

function invert_check(){
  alert("not yet implemented");
}

function browse_base_dir(){
  netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');
  const nsIFilePicker = Components.interfaces.nsIFilePicker;
  var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(
                               nsIFilePicker);
  fp.init(window,'Xalan Tests Base Dir',nsIFilePicker.modeGetFolder);
  fp.appendFilters(nsIFilePicker.filterAll);
  var res = fp.show();

  if (res == nsIFilePicker.returnOK) {
    var furl = fp.fileURL, fconf=fp.fileURL, fgold=fp.fileURL;
    fconf.path = furl.path+'conf';
    fgold.path = furl.path+'conf-gold';
    if (!fconf.file.exists() || !fconf.file.isDirectory()){
      alert("Xalan Tests not found!");
      return;
    }
    if (!fgold.file.exists() || !fgold.file.isDirectory()){
      alert("Xalan Tests Reference solutions not found!");
      return;
    }
    xalan_base.setAttribute('value',fp.fileURL.path);
  }
}

function reset_stats(){
  tests_run.setAttribute("value", "0");
  tests_passed.setAttribute("value", "0");
  tests_failed.setAttribute("value", "0");
}

function sel_change() {
  netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');
  tests_selected.setAttribute("value", view.selection.count);
}
