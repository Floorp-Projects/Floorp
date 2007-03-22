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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Frank Tang  ftang@netscape.com
 *   J.M  Betak  jbetak@netscape.com
 *   Neil Rashbrook <neil@parkwaycc.co.uk>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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


var gCharset="";
var gTitleWasEdited = false;
var gCharsetWasChanged = false;
var gInsertNewContentType = false;
var gContenttypeElement;
var gInitDone = false;

//Cancel() is in EdDialogCommon.js

function Startup()
{
  var editor = GetCurrentEditor();
  if (!editor)
  {
    window.close();
    return;
  }

  var observerService = Components.classes["@mozilla.org/observer-service;1"].getService(Components.interfaces.nsIObserverService);
  observerService.notifyObservers(null, "charsetmenu-selected", "other");

  gDialog.TitleInput    = document.getElementById("TitleInput");
  gDialog.charsetTree   = document.getElementById('CharsetTree'); 
  gDialog.exportToText  = document.getElementById('ExportToText');

  gContenttypeElement = GetHTTPEquivMetaElement("content-type");
  if (!gContenttypeElement && (editor.contentsMIMEType != 'text/plain')) 
  {
    gContenttypeElement = CreateHTTPEquivMetaElement("content-type");
    if (!gContenttypeElement ) 
	{
      window.close();
      return;
    }
    gInsertNewContentType = true;
  }

  try {
    gCharset = editor.documentCharacterSet;
  } catch (e) {}

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
  
  gInitDone = true;
  
  SetWindowLocation();
}

  
function InitDialog() 
{
  gDialog.TitleInput.value = GetDocumentTitle();

  var RDF = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService(Components.interfaces.nsIRDFService);
  var tree = gDialog.charsetTree;
  var index = tree.view.getIndexOfResource(RDF.GetResource(gCharset));
  if (index >= 0) {
    tree.view.selection.select(index);
    tree.treeBoxObject.ensureRowIsVisible(index);
  }
}


function onAccept()
{
  var editor = GetCurrentEditor();
  editor.beginTransaction();

  if(gCharsetWasChanged) 
  {
     try {
       SetMetaElementContent(gContenttypeElement, "text/html; charset=" + gCharset, gInsertNewContentType, true);     
      editor.documentCharacterSet = gCharset;
    } catch (e) {}
  }

  editor.endTransaction();

  if(gTitleWasEdited) 
    SetDocumentTitle(TrimString(gDialog.TitleInput.value));

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

      
function SelectCharset()
{
  if(gInitDone) 
  {
    try 
	{
      gCharset = gDialog.charsetTree.builderView.getResourceAtIndex(gDialog.charsetTree.currentIndex).Value;
      if (gCharset)
        gCharsetWasChanged = true;
    }
    catch(e) {}
  }
}


function TitleChanged()
{
  gTitleWasEdited = true; 
}
