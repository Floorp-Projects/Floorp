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

var pop_last=0, pop_chunk=25;
var isinited=false;
var xalan_base, xalan_xml, xalan_elems, xalan_length, content_row, target;
var matchRE, matchNameTag, matchFieldTag;
var tests_run, tests_passed, tests_failed;

function loaderstuff(eve) {
  var ns = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
  content_row = document.createElementNS(ns,"row");
  content_row.setAttribute("flex","1");
  content_row.appendChild(document.createElementNS(ns,"checkbox"));
  content_row.appendChild(document.createElementNS(ns,"text"));
  content_row.appendChild(document.createElementNS(ns,"text"));
  content_row.appendChild(document.createElementNS(ns,"text"));
  content_row.setAttribute("class", "notrun");
  netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');
  target = document.getElementById("xalan_grid");
  matchNameTag = document.getElementById("search-name");
  matchFieldTag = document.getElementById("search-field");
  tests_run = document.getElementById("tests_run");
  tests_passed = document.getElementById("tests_passed");
  tests_failed = document.getElementById("tests_failed");
  xalan_base = document.getElementById("xalan_base");
  xalan_xml = document.implementation.createDocument("","",null);
  xalan_xml.addEventListener("load", xalanIndexLoaded, false);
  xalan_xml.load("complete-xalanindex.xml");
  //xalan_xml.load("xalanindex.xml");
  return true;
}

function xalanIndexLoaded(e) {
  xalan_elems = xalan_xml.documentElement;
  xalan_length = Math.floor(xalan_elems.childNodes.length/2);
  return true;
}

function refresh_xalan() {
  while(target.childNodes.length>1) target.removeChild(target.lastChild);
  pop_last = 0;
  populate_xalan();
  return true;
}


function populate_xalan() {
  var upper=pop_last+pop_chunk;
  var current,test,i,j, lName, re=/\/err\//;
  var searchField = matchFieldTag.getAttribute("data");
  var matchValue = matchNameTag.value;
  if (matchValue.length==0) matchValue='.';
  var matchRE = new RegExp(matchValue);
  for (i=pop_last;i<Math.min(upper,xalan_length);i++){
    current = content_row.cloneNode(true);
    test = xalan_elems.childNodes.item(2*i+1);
    if (!test.getAttribute("file").match(re)){
      current.setAttribute("id", test.getAttribute("file"));
      current.childNodes.item(1).setAttribute("value",
  	  test.getAttribute("file"));
      for (j=1;j<test.childNodes.length;j+=2){
        lName = test.childNodes.item(j).localName;
        if (lName=="purpose")
          current.childNodes.item(2).setAttribute("value",
                             test.childNodes.item(j).firstChild.nodeValue);
        if (lName=="comment")
          current.childNodes.item(3).setAttribute("value",
  	                         test.childNodes.item(j).firstChild.nodeValue);
      }
      if (matchRE.test(current.childNodes.item(searchField)
                       .getAttribute("value"))) target.appendChild(current);
    }
  }
  if (pop_last+pop_chunk<xalan_length){
    pop_last+=pop_chunk;
    window.status="Loaded "+Math.ceil(pop_last/xalan_length*100)+"% of xalanindex.xml";
    setTimeout("populate_xalan()",10);
  } else {
    window.status="";
  }
}

function dump_checked(){
  var nds = target.childNodes;
  var todo = new Array();
  for (i=1;i<nds.length;i++){
    node=nds.item(i);
    if(node.getAttribute("class") != "notrun")
      node.setAttribute("class", "notrun");
    node=node.firstChild;
    if(node.hasAttribute("checked"))
      todo.push(node.nextSibling.getAttribute("value"));
  }
  reset_stats();
  do_transforms(todo);
}

function handle_result(name,success){
  var classname = (success ? "succeeded" : "failed");
  var content_row = document.getElementById(name);
  if (content_row){
    content_row.setAttribute("class",classname);
  }
  tests_run.getAttributeNode("value").nodeValue++;
  if (success)
    tests_passed.getAttributeNode("value").nodeValue++;
  else
    tests_failed.getAttributeNode("value").nodeValue++;
}

function hide_checked(yes){
  var checks = document.getElementsByTagName("checkbox");
  for (i=0;i<checks.length;i++){
    node=checks.item(i);
    if (node.hasAttribute("checked")==yes)
      node.parentNode.parentNode.removeChild(node.parentNode);
  }
}

function select(){
  var searchField = matchFieldTag.getAttribute("data");
  var matchRE = new RegExp(matchNameTag.value);
  var nds = target.childNodes;
  for (i=1;i<nds.length;i++){
    node=nds.item(i).childNodes.item(searchField);
    if (matchRE.test(node.getAttribute("value")))
      nds.item(i).firstChild.setAttribute("checked",true);
  }
}

function check(yes){
  var i, nds = target.childNodes;
  for (i=1;i<nds.length;i++){
    if (yes)
      nds.item(i).firstChild.setAttribute("checked",true);
    else
      nds.item(i).firstChild.removeAttribute("checked");
  }
}

function invert_check(){
  var i, checker, nds = target.childNodes;
  for (i=0;i<nds.length;i++){
    checker = nds.item(i).firstChild;
    if (checker.hasAttribute("checked"))
      checker.removeAttribute("checked");
    else
      checker.setAttribute("checked",true);
  }
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
