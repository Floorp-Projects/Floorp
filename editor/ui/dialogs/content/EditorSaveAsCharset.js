/* 
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
 * 
 * Contributor(s): 
 * Frank Tang  ftang@netscape.com
 * J.M  Betak  jbetak@netscape.com
 */


var charset="";
var titleWasEdited = false;
var charsetWasChanged = false;
var insertNewContentType = false;
var contenttypeElement;
var initDone = false;

//Cancel() is in EdDialogCommon.js

function Startup()
{
  if (!InitEditorShell())
    return;

  doSetOKCancel(onOK, onCancel);

  gDialog.TitleInput    = document.getElementById("TitleInput");
  gDialog.charsetTree   = document.getElementById('CharsetTree'); 
  gDialog.exportToText  = document.getElementById('ExportToText');

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
  SetTextboxFocus(gDialog.TitleInput);
  LoadAvailableCharSets();
  initDone = true;

  SetWindowLocation();
}

  
function InitDialog() 
{
  gDialog.TitleInput.value = editorShell.GetDocumentTitle();
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
    window.opener.newTitle = TrimString(gDialog.TitleInput.value);

  window.opener.ok = true;
  window.opener.exportToText = gDialog.exportToText.checked;
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

    ClearTreelist(gDialog.charsetTree);

    for (var i = 0; i < rdfContainer.GetCount(); i++) 
	{
      charsetNode = availableCharsets.getNext().QueryInterface(Components.interfaces.nsIRDFResource);
      item = AppendStringToTreelist(gDialog.charsetTree, readRDFString(rdfDataSource, charsetNode, kNC_name));
      item.firstChild.firstChild.setAttribute("value", charsetNode.Value);
      if(charset == charsetNode.Value) 
        selectedItem = item;
    }

    if(selectedItem) 
	{
      gDialog.charsetTree.selectItem(selectedItem);
      gDialog.charsetTree.ensureElementIsVisible(selectedItem);
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
      charset = GetSelectedTreelistAttribute(gDialog.charsetTree, "value");
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
