/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 */

//Html Domain object
var htmlobj = null //new Object();
//htmlobj.tree_node = void 0;                       // the dom element of the htmldomain tree
//htmlobj.treeroot_node = void 0;                   // dom element of the htmldomain treechildren
//htmlobj.domain_pref = void 0;                     // dom element of the broadcaster mailhtmldomain

//Plain Text Domain object
var plainobj = null //new Object();
//plainobj.tree_node = void 0;                      // dom element of plaintextdomain tree
//plainobj.treeroot_node = void 0;                  //dom element of plaintextdomain treechildren
//plainobj.domain_pref = void 0;                    // dom element of the broadcaster mailplaintextdomain

var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService();
promptService = promptService.QueryInterface(Components.interfaces.nsIPromptService);


function Init()
{
  try {
    parent.initPanel('chrome://messenger/content/messengercompose/pref-formatting.xul');
  }
  catch(ex) {
    dump("*** Couldn't initialize page switcher and pref loader.\n");
    //pref service backup
  } //catch
  //Initialize the objects
  htmlobj = new Object();
  plainobj = new Object();
  //Initialize the broadcaster value so that you can use it later
  htmlobj.domain_pref            = document.getElementById('mailhtmldomain');
  plainobj.domain_pref           = document.getElementById('mailplaintextdomain');
  htmlobj.tree_node              = document.getElementById('html_domains');
  htmlobj.treeroot_node          = document.getElementById('html_domains_root');
  plainobj.tree_node             = document.getElementById('plaintext_domains');
  plainobj.treeroot_node         = document.getElementById('plaintext_domains_root');

  //Get the values of the Add Domain Dlg boxes and store it in the objects
  var AddDomainDlg               = document.getElementById('domaindlg');
  htmlobj.DlgTitle               = AddDomainDlg.getAttribute("htmldlg_title");
  htmlobj.DlgMsg                 = AddDomainDlg.getAttribute("htmldlg_msg");
  plainobj.DlgTitle              = AddDomainDlg.getAttribute("plaintextdlg_title");
  plainobj.DlgMsg                = AddDomainDlg.getAttribute("plaintextdlg_msg");
  //Id values for the objects for comparison
  htmlobj.id                     = "html";
  plainobj.id                    = "plain";
  LoadDomains(htmlobj);
  LoadDomains(plainobj);
}

function AddDomain(obj)
{
  var DomainName;
  if (promptService)
  {
    var result = {value:null};
    if (promptService.prompt(
      window,
      obj.DlgTitle,
      obj.DlgMsg,
      result,
      null,
      {value:0}
    ))
      DomainName = result.value.replace(/ /g,"");
  }

  if (DomainName) {
    var objPrime;
    if (obj.id == "html")
      objPrime = plainobj;
    else
      objPrime = htmlobj;
    if (!DomainAlreadyPresent(obj, DomainName, true))
      if(!DomainAlreadyPresent(objPrime, DomainName, false)) {
      AddTreeItem(obj.treeroot_node, DomainName);
    }
  }

  UpdateSavePrefString(obj);
}

function AddTreeItem(treeRoot, domainTitle)
{
  try {

      // Create a treerow for the new Domain
      var item = document.createElement('treeitem');
      var row  = document.createElement('treerow');
      var cell = document.createElement('treecell');

      // Copy over the attributes
      cell.setAttribute('label', domainTitle);

      // Add it to the active languages tree
      item.appendChild(row);
      row.appendChild(cell);
      treeRoot.appendChild(item);

  } //try

  catch (ex) {
    dump("*** Failed to add item: " + domainTitle + "\n");
  } //catch

}

function DomainAlreadyPresent(obj, domain_name, dup)
{
  var errorTitle;
  var errorMsg;
  var pref_string = obj.domain_pref.getAttribute('value');
  var found = false;
  try {
    var arrayOfPrefs = pref_string.split(',');
    if (arrayOfPrefs)
      for (var i = 0; i < arrayOfPrefs.length; i++) {
        var str = arrayOfPrefs[i].replace(/ /g,"");
        if (str == domain_name) {
          dump("###ERROR DOMAIN ALREADY EXISTS\n");
          errorTitle = document.getElementById("domainerrdlg").getAttribute("domainerrdlg_title");
          if(dup)
          errorMsg = document.getElementById("domainerrdlg").getAttribute("duperr");
          else
          errorMsg = document.getElementById("domainerrdlg").getAttribute("dualerr");
          var errorMessage = errorMsg.replace(/@string@/, domain_name);
          if (promptService)
            promptService.alert(window, errorTitle, errorMessage);
          else
            window.alert(errorMessage);
          found = true;
          break;
        }//if
    }//for

    return found;
  }//try

  catch(ex){
     return false;
  }//catch
}

function RemoveDomains(obj)
{
  var nextNode = null;
  var numSelected = obj.tree_node.selectedItems.length;
  var deleted_all = false;

  while (obj.tree_node.selectedItems.length > 0) {

  var selectedNode = obj.tree_node.selectedItems[0];
    nextNode = selectedNode.nextSibling;

  if (!nextNode)

    if (selectedNode.previousSibling)
    nextNode = selectedNode.previousSibling;

    var row  =  selectedNode.firstChild;
    var cell =  row.firstChild;

    row.removeChild(cell);
    selectedNode.removeChild(row);
    obj.treeroot_node.removeChild(selectedNode);

   } //while

  if (nextNode) {
    obj.tree_node.selectItem(nextNode)
  } //if

  UpdateSavePrefString(obj);
}

function LoadDomains(obj)
{
  try {
    var arrayOfPrefs = obj.domain_pref.getAttribute('value').split(',');
  }
  catch (ex) {
    dump("failed to split the preference string!\n");
  }

  if (arrayOfPrefs)
    for (var i = 0; i < arrayOfPrefs.length; i++) {

      var str = arrayOfPrefs[i].replace(/ /g,"");
      if (str) {
        AddTreeItem(obj.treeroot_node, str);
      } //if
    } //for
}

function UpdateSavePrefString(obj)
{
  var num_domains = 0;
  var pref_string = "";

  for (var item = obj.treeroot_node.firstChild; item != null; item = item.nextSibling) {

    var row  =  item.firstChild;
    var cell =  row.firstChild;
    var domainid = cell.getAttribute('label');
    if (domainid.length > 1) {

          num_domains++;

      //separate >1 domains by commas
      if (num_domains > 1) {
        pref_string = pref_string + "," + domainid;
      } else {
        pref_string = domainid;
      } //if
    } //if
  }//for

  obj.domain_pref.setAttribute("value", pref_string);
}
