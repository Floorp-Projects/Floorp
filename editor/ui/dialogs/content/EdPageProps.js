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

var MoreSection;
var title = "";
var author = "";
var description = "";
var authorElement;
var descriptionElement;
var headNode;
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

  dialog = new Object;
  if (!dialog)
  {
    dump("Failed to create dialog object!!!\n");
    window.close();
  }
  dialog.PageLocation     = document.getElementById("PageLocation");
  dialog.PageModDate      = document.getElementById("PageModDate");
  dialog.TitleInput       = document.getElementById("TitleInput");
  dialog.AuthorInput      = document.getElementById("AuthorInput");
  dialog.DescriptionInput = document.getElementById("DescriptionInput");
  dialog.MoreSection      = document.getElementById("MoreSection");
  dialog.MoreFewerButton  = document.getElementById("MoreFewerButton");
  dialog.HeadSrcInput     = document.getElementById("HeadSrcInput");
  doSetOKCancel(onOK, null);
  
  // Default string for new page is set from DTD string in XUL,
  //   so set only if not new doc URL
  var location = editorShell.editorDocument.location;
  if (location != "about:blank")
    dialog.PageLocation.setAttribute("value", editorShell.editorDocument.location);

  authorElement = GetMetaElement("author");
  if (!authorElement)
  {
    authorElement = CreateMetaElement("author");
    if (!authorElement)
      window.close();

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
  
  headNode = editorShell.editorDocument.getElementsByTagName("head").item(0);
  if (!headNode)
    window.close();

  InitMoreFewer();
  InitDialog();

  dialog.TitleInput.focus();
}

function InitDialog()
{
  dialog.TitleInput.value = editorShell.GetDocumentTitle();
  dialog.AuthorInput.value = authorElement.getAttribute("content");
  dialog.DescriptionInput.value = descriptionElement.getAttribute("content");
  // Get the entire contents of the "head" region.
  dialog.HeadSrcInput.value = editorShell.GetHeadContentsAsHTML();
}


function GetMetaElement(name)
{
  if (name)
  {
    name = name.toLowerCase();
    if (name != "")
    {
      var metaNodes = editorShell.editorDocument.getElementsByTagName("meta");
      if (metaNodes && metaNodes.length > 0)
      {
        for (var i = 0; i < metaNodes.length; i++)
        {
          var metaNode = metaNodes.item(i);
          if (metaNode && metaNode.getAttribute("name") == name)
            return metaNode;
        }
      }
    }
  }
  return null;
}

function CreateMetaElement(name)
{
  metaElement = editorShell.CreateElementWithDefaults("meta");
  if (metaElement)
    metaElement.setAttribute("name", name);
  else
    dump("Failed to create metaElement!\n");
  
  return metaElement;
}

function CreateHTTPEquivElement(name)
{
  metaElement = editorShell.CreateElementWithDefaults("meta");
  if (metaElement)
    metaElement.setAttribute("http-equiv", name);
  else
    dump("Failed to create metaElement for http-equiv!\n");
  
  return metaElement;
}

// Change "content" attribute on a META element,
//   or delete entire element it if content is empty
// This uses undoable editor transactions 
function SetMetaElementContent(metaElement, content, insertNew)
{
  if (metaElement)
  {
    if(!content || content == "")
    {
      if (!insertNew)
        editorShell.DeleteElement(metaElement);
    }
    else
    {
      if (insertNew)
      {
        // Don't need undo for set attribute, just for InsertElement
        metaElement.setAttribute("content", content);
        AppendHeadElement(metaElement);
      }
      else
        editorShell.SetAttribute(metaElement, "content", content);
    }
  }
}

function GetHeadElement()
{
  var headList = editorShell.editorDocument.getElementsByTagName("head");
  if (headList)
    return headList.item(0);
  
  return null;
}

function AppendHeadElement(element)
{
  var head = GetHeadElement();
  if (head)
    head.appendChild(element);
}

function TextfieldChanged(ID)
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
  title = dialog.TitleInput.value.trimString();
  author = dialog.AuthorInput.value.trimString();
  description = dialog.DescriptionInput.value.trimString();
  return true;
}

function onOK()
{
  if (ValidateData())
  {
    editorShell.BeginBatchChanges();

    // Save only if advanced "head editing" region is open?
    if (SeeMore)
    {
      // Delete existing children of HEAD      
      // Note that we must use editorShell method so this is undoable
      var children = dialog.HeadSrcInput.childNodes;
      if (children)
      {
        for(i=0; i < children.length; i++) 
          editorShell.DeleteElement(children.item(i));
      }

      var headSrcString = dialog.HeadSrcInput.value;
      if (headSrcString.length > 0)
        editorShell.ReplaceHeadContentsWithHTML(headSrcString);        
    }

    //Problem: How do we reconcile changes in same elements in 
    //         advanced region and here? 
    // 1. Save manually-edited results first
    // 2. Save each of the 3 textfield results only if user actually changed a value

    if (titleWasEdited)
    {
      // Set title contents even if string is empty
      //  because TITLE is a required HTML element
      editorShell.SetDocumentTitle(title);
    }
    
    if (authorWasEdited)
      SetMetaElementContent(authorElement, author, insertNewAuthor);
    if (descWasEdited)
      SetMetaElementContent(descriptionElement, description, insertNewDescription);

    editorShell.EndBatchChanges();
    return true; // do close the window
  }
  return false;
}

