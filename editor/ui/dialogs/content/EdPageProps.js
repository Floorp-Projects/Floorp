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
 *   Charles Manske (cmanske@netscape.com)
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

var gNewTitle = "";
var gAuthor = "";
var gDescription = "";
var gAuthorElement;
var gDescriptionElement;
var gInsertNewAuthor = false;
var gInsertNewDescription = false;
var gTitleWasEdited = false;
var gAuthorWasEdited = false;
var gDescWasEdited = false;

//Cancel() is in EdDialogCommon.js
// dialog initialization code
function Startup()
{
  var editor = GetCurrentEditor();
  if (!editor)
  {
    window.close();
    return;
  }

  gDialog.PageLocation     = document.getElementById("PageLocation");
  gDialog.PageModDate      = document.getElementById("PageModDate");
  gDialog.TitleInput       = document.getElementById("TitleInput");
  gDialog.AuthorInput      = document.getElementById("AuthorInput");
  gDialog.DescriptionInput = document.getElementById("DescriptionInput");
  
  // Default string for new page is set from DTD string in XUL,
  //   so set only if not new doc URL
  var location = GetDocumentUrl();
  var lastmodString = GetString("Unknown");

  if (!IsUrlAboutBlank(location))
  {
    // NEVER show username and password in clear text
    gDialog.PageLocation.setAttribute("value", StripPassword(location));

    // Get last-modified file date+time
    // TODO: Convert this to local time?
    var lastmod;
    try {
      lastmod = editor.document.lastModified;  // get string of last modified date
    } catch (e) {}
    // Convert modified string to date (0 = unknown date or January 1, 1970 GMT)
    if(Date.parse(lastmod))
    {
      try {
        const nsScriptableDateFormat_CONTRACTID = "@mozilla.org/intl/scriptabledateformat;1";
        const nsIScriptableDateFormat = Components.interfaces.nsIScriptableDateFormat;
        var dateService = Components.classes[nsScriptableDateFormat_CONTRACTID]
         .getService(nsIScriptableDateFormat);

        var lastModDate = new Date();
        lastModDate.setTime(Date.parse(lastmod));
        lastmodString =  dateService.FormatDateTime("", 
                                      dateService.dateFormatLong,
                                      dateService.timeFormatSeconds,
                                      lastModDate.getFullYear(),
                                      lastModDate.getMonth()+1,
                                      lastModDate.getDate(),
                                      lastModDate.getHours(),
                                      lastModDate.getMinutes(),
                                      lastModDate.getSeconds());
      } catch (e) {}
    }
  }
  gDialog.PageModDate.value = lastmodString;

  gAuthorElement = GetMetaElement("author");
  if (!gAuthorElement)
  {
    gAuthorElement = CreateMetaElement("author");
    if (!gAuthorElement)
    {
      window.close();
      return;
    }
    gInsertNewAuthor = true;
  }

  gDescriptionElement = GetMetaElement("description");
  if (!gDescriptionElement)
  {
    gDescriptionElement = CreateMetaElement("description");
    if (!gDescriptionElement)
      window.close();

    gInsertNewDescription = true;
  }
  
  InitDialog();

  SetTextboxFocus(gDialog.TitleInput);

  SetWindowLocation();
}

function InitDialog()
{
  gDialog.TitleInput.value = GetDocumentTitle();

  var gAuthor = TrimString(gAuthorElement.getAttribute("content"));
  if (!gAuthor)
  {
    // Fill in with value from editor prefs
    var prefs = GetPrefs();
    if (prefs) 
      gAuthor = prefs.getCharPref("editor.author");
  }
  gDialog.AuthorInput.value = gAuthor;
  gDialog.DescriptionInput.value = gDescriptionElement.getAttribute("content");
}

function TextboxChanged(ID)
{
  switch(ID)
  {
    case "TitleInput":
      gTitleWasEdited = true;
      break;
    case "AuthorInput":
      gAuthorWasEdited = true;
      break;
    case "DescriptionInput":
      gDescWasEdited = true;
      break;
  }
}

function ValidateData()
{
  gNewTitle = TrimString(gDialog.TitleInput.value);
  gAuthor = TrimString(gDialog.AuthorInput.value);
  gDescription = TrimString(gDialog.DescriptionInput.value);
  return true;
}

function onAccept()
{
  if (ValidateData())
  {
    var editor = GetCurrentEditor();
    editor.beginTransaction();

    // Set title contents even if string is empty
    //  because TITLE is a required HTML element
    if (gTitleWasEdited)
      SetDocumentTitle(gNewTitle);
    
    if (gAuthorWasEdited)
      SetMetaElementContent(gAuthorElement, gAuthor, gInsertNewAuthor, false);

    if (gDescWasEdited)
      SetMetaElementContent(gDescriptionElement, gDescription, gInsertNewDescription, false);

    editor.endTransaction();

    SaveWindowLocation();
    return true; // do close the window
  }
  return false;
}

