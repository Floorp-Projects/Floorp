/*  */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
 * The Original Code is .
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Frank Tang  ftang@netscape.com
 * J.M  Betak  jbetak@netscape.com
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */


var charset="";
var titleWasEdited = false;
var charsetWasChanged = false;
var insertNewContentType = false;
var contenttypeElement;
var initDone = false;
var dialog;

//Cancel() is in EdDialogCommon.js

function Startup()
{
  if (!InitEditorShell())
    return;

  doSetOKCancel(onOK, onCancel);

  // Create dialog object to store controls for easy access
  dialog = new Object;
  dialog.TitleInput    = document.getElementById("TitleInput");
  dialog.charsetTree   = document.getElementById('CharsetTree'); 
  dialog.exportToText  = document.getElementById('ExportToText');

  contenttypeElement = GetHTTPEquivMetaElement("content-type");
  if(!contenttypeElement && (editorShell.contentsMIMEType != 'text/plain')) 
  {
    contenttypeElement = CreateHTTPEquivMetaElement("content-type");
    if( ! contenttypeElement ) 
	{
      window.close();
      return;
    }
    insertNewContentType = true;
  }

  InitDialog();

  // Use the same text as the messagebox for getting title by regular "Save"
  document.getElementById("EnterTitleLabel").setAttribute("value",GetString("NeedDocTitle"));
  // This is an <HTML> element so it wraps -- append a child textnode
  var helpTextParent = document.getElementById("TitleHelp");
  var helpText = document.createTextNode(GetString("DocTitleHelp"));
  if (helpTextParent)
    helpTextParent.appendChild(helpText);
  
  // SET FOCUS TO FIRST CONTROL
  SetTextboxFocus(dialog.TitleInput);
  LoadAvailableCharSets();
  initDone = true;

  SetWindowLocation();
}

  
function InitDialog() 
{
  dialog.TitleInput.value = editorShell.GetDocumentTitle();
  charset = editorShell.GetDocumentCharacterSet();
}


function onOK()
{
  editorShell.BeginBatchChanges();

  if(charsetWasChanged) 
  {
     SetMetaElementContent(contenttypeElement, "text/html; charset=" + charset, insertNewContentType);     
     editorShell.SetDocumentCharacterSet(charset);
  }

  editorShell.EndBatchChanges();

  if(titleWasEdited) 
    window.opener.newTitle = dialog.TitleInput.value.trimString();

  window.opener.ok = true;
  window.opener.exportToText = dialog.exportToText.checked;
  SaveWindowLocation();
  return true;
}


function readRDFString(aDS,aRes,aProp) 
{
  var n = aDS.GetTarget(aRes, aProp, true);
  if (n)
    return n.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
  else
    return "";
}

      
function LoadAvailableCharSets()
{
  try 
  {                                  
    var rdf=Components.classes["@mozilla.org/rdf/rdf-service;1"].getService(Components.interfaces.nsIRDFService);
    var kNC_Root = rdf.GetResource("NC:DecodersRoot");
    var kNC_name = rdf.GetResource("http://home.netscape.com/NC-rdf#Name");
    var rdfDataSource = rdf.GetDataSource("rdf:charset-menu");
    var rdfContainer = Components.classes["@mozilla.org/rdf/container;1"].getService(Components.interfaces.nsIRDFContainer);

    rdfContainer.Init(rdfDataSource, kNC_Root);

    var availableCharsets = rdfContainer.GetElements();
    var charsetNode;
    var selectedItem;
    var item;

    ClearTreelist(dialog.charsetTree);

    for (var i = 0; i < rdfContainer.GetCount(); i++) 
	{
      charsetNode = availableCharsets.getNext().QueryInterface(Components.interfaces.nsIRDFResource);
      item = AppendStringToTreelist(dialog.charsetTree, readRDFString(rdfDataSource, charsetNode, kNC_name));
      item.firstChild.firstChild.setAttribute("value", charsetNode.Value);
      if(charset == charsetNode.Value) 
        selectedItem = item;
    }

    if(selectedItem) 
	{
      dialog.charsetTree.selectItem(selectedItem);
      dialog.charsetTree.ensureElementIsVisible(selectedItem);
    }
  }
  catch(e) {}
}


function SelectCharset()
{
  if(initDone) 
  {
    try 
	{
      charset = GetSelectedTreelistAttribute(dialog.charsetTree, "value");
      if(charset != "")
         charsetWasChanged = true;
    }
    catch(e) {}
  }
}


function TitleChanged()
{
  titleWasEdited = true; 
}
