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
 */

var newTitle = "";
var author = "";
var description = "";
var authorElement;
var descriptionElement;
var insertNewAuthor = false;
var insertNewDescription = false;
var titleWasEdited = false;
var authorWasEdited = false;
var descWasEdited = false;

//Cancel() is in EdDialogCommon.js
// dialog initialization code
function Startup()
{
  if (!InitEditorShell())
    return;

  gDialog.PageLocation     = document.getElementById("PageLocation");
  gDialog.TitleInput       = document.getElementById("TitleInput");
  gDialog.AuthorInput      = document.getElementById("AuthorInput");
  gDialog.DescriptionInput = document.getElementById("DescriptionInput");
  doSetOKCancel(onOK, onCancel);
  
  // Default string for new page is set from DTD string in XUL,
  //   so set only if not new doc URL
  var location = editorShell.editorDocument.location;
  var lastmodString = GetString("Unknown");

  if (location != "about:blank")
  {
    gDialog.PageLocation.setAttribute("value", editorShell.editorDocument.location);

    // Get last-modified file date+time
    // TODO: Convert this to local time?
    var lastmod = editorShell.editorDocument.lastModified;  // get string of last modified date
    var lastmoddate = Date.parse(lastmod);                  // convert modified string to date
    if(lastmoddate != 0)                                    // unknown date (or January 1, 1970 GMT)
      lastmodString = lastmoddate;

  }
  document.getElementById("PageModDate").setAttribute("value", lastmodString);

  authorElement = GetMetaElement("author");
  if (!authorElement)
  {
    authorElement = CreateMetaElement("author");
    if (!authorElement)
    {
      window.close();
      return;
    }
    insertNewAuthor = true;
  }

  descriptionElement = GetMetaElement("description");
  if (!descriptionElement)
  {
    descriptionElement = CreateMetaElement("description");
    if (!descriptionElement)
      window.close();

    insertNewDescription = true;
  }
  
  InitDialog();

  SetTextboxFocus(gDialog.TitleInput);

  SetWindowLocation();
}

function InitDialog()
{
  gDialog.TitleInput.value = editorShell.GetDocumentTitle();
  var author = authorElement.getAttribute("content").trimString();
  if (author.length == 0)
  {
    // Fill in with value from editor prefs
    var prefs = GetPrefs();
    if (prefs) 
      author = prefs.CopyCharPref("editor.author");
  }
  gDialog.AuthorInput.value = author;
  gDialog.DescriptionInput.value = descriptionElement.getAttribute("content");
}

function TextboxChanged(ID)
{
  switch(ID)
  {
    case "TitleInput":
      titleWasEdited = true;
      break;
    case "AuthorInput":
      authorWasEdited = true;
      break;
    case "DescriptionInput":
      descWasEdited = true;
      break;
  }
}

function ValidateData()
{
  newTitle = gDialog.TitleInput.value.trimString();
  author = gDialog.AuthorInput.value.trimString();
  description = gDialog.DescriptionInput.value.trimString();
  return true;
}

function onOK()
{
  if (ValidateData())
  {
    editorShell.BeginBatchChanges();
    if (titleWasEdited)
    {
      // Set title contents even if string is empty
      //  because TITLE is a required HTML element
      editorShell.SetDocumentTitle(newTitle);
    }
    
    if (authorWasEdited)
      SetMetaElementContent(authorElement, author, insertNewAuthor);
    if (descWasEdited)
      SetMetaElementContent(descriptionElement, description, insertNewDescription);

    editorShell.EndBatchChanges();

    SaveWindowLocation();
    return true; // do close the window
  }
  return false;
}

