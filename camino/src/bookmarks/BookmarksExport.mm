/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Simon Fraser <sfraser@netscape.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <Carbon/Carbon.h>

#import "BookmarksExport.h"
#import "BookmarksService.h"

#include "nsCOMPtr.h"
#include "nsIFileStreams.h"
#include "nsILocalFile.h"
#include "nsILocalFileMac.h"
#include "nsIOutputStream.h"
#include "nsIAtom.h"

#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"
#include "nsIContent.h"

/*
  Netscape HTML bookmarks format is:
  
  <dl><p>
    <dt><h3>folder</h3>
    <dl><p>
      <dt>
      <dt>
      ...
    </dl>
    ..
  </dl>
  
*/


BookmarksExport::BookmarksExport(nsIDOMDocument* inBookmarksDoc, const char* inLinebreakStr)
: mBookmarksDocument(inBookmarksDoc)
, mLinebreakStr(inLinebreakStr)
,	mWriteStatus(NS_OK)
{
}

BookmarksExport::~BookmarksExport()
{
  if (mOutputStream)
    (void)mOutputStream->Close();
}

nsresult
BookmarksExport::ExportBookmarksToHTML(const nsAString& inFilePath)
{
  if (!mBookmarksDocument)
    return NS_ERROR_NOT_INITIALIZED;

  nsresult rv = SetupOutputStream(inFilePath);
  if (NS_FAILED(rv)) return rv;

  WritePrologue();
  nsCOMPtr<nsIDOMElement> docElement;
  mBookmarksDocument->GetDocumentElement(getter_AddRefs(docElement));
  WriteItem(docElement, 0, PR_TRUE);

  CloseOutputStream();
  return mWriteStatus;
}

nsresult
BookmarksExport::SetupOutputStream(const nsAString& inFilePath)
{
  nsCOMPtr<nsILocalFile> destFile;
  nsresult rv = NS_NewLocalFile(inFilePath, PR_FALSE, getter_AddRefs(destFile));
  if (NS_FAILED(rv)) return rv;

  rv = NS_NewLocalFileOutputStream(getter_AddRefs(mOutputStream), destFile);
  if (NS_FAILED(rv)) return rv;

  // we have to give the file a 'TEXT' type for IE to import it correctly
  nsCOMPtr<nsILocalFileMac> macDestFile = do_QueryInterface(destFile);
  if (macDestFile)
    rv = macDestFile->SetFileType('TEXT');
  
  return rv;
}

void
BookmarksExport::CloseOutputStream()
{
	(void)mOutputStream->Close();
  mOutputStream = nsnull;
}

bool
BookmarksExport::WriteChildren(nsIDOMElement* inElement, PRInt32 inDepth)
{
  nsCOMPtr<nsIContent> curContent = do_QueryInterface(inElement);
  if (!curContent) return false;
    
  // recurse to children
  PRInt32 numChildren;
  curContent->ChildCount(numChildren);
  for (PRInt32 i = 0; i < numChildren; i ++)
  {
    nsCOMPtr<nsIContent> curChild;
    curContent->ChildAt(i, *getter_AddRefs(curChild));
    
    nsCOMPtr<nsIDOMElement> curElt = do_QueryInterface(curChild);
    if (!WriteItem(curElt, inDepth))
      return false;
  }

  return true;
}
  
bool
BookmarksExport::WriteItem(nsIDOMElement* inElement, PRInt32 inDepth, PRBool isRoot)
{
  nsCOMPtr<nsIContent> curContent = do_QueryInterface(inElement);
  if (!inElement || !curContent)
    return false;
  
  nsCOMPtr<nsIAtom> tagName;
  curContent->GetTag(*getter_AddRefs(tagName));

  PRBool isContainer = isRoot || (tagName == BookmarksService::gFolderAtom);
  
  nsAutoString href, title;

  const char* const spaces = "                                                                                ";
  const char* indentString = spaces + strlen(spaces) - (inDepth * 4);
  if (indentString < spaces)
    indentString = spaces;
  
  if (isContainer)
  {
    if (!isRoot)
    {
      WriteString(indentString, strlen(indentString));
  
      const char* const prefixString = "<DT><H3";
      WriteString(prefixString, strlen(prefixString));
  
      nsAutoString typeAttribute;
      inElement->GetAttribute(NS_LITERAL_STRING("type"), typeAttribute);
      if (typeAttribute.Equals(NS_LITERAL_STRING("toolbar")))
      {
        const char* persToolbar = " PERSONAL_TOOLBAR_FOLDER=\"true\"";
        WriteString(persToolbar, strlen(persToolbar));
      }
  
      WriteString(">", 1);
  
      inElement->GetAttribute(NS_LITERAL_STRING("name"), title);
  
      NS_ConvertUCS2toUTF8 titleConverter(title);
      const char* utf8String = titleConverter.get();
  
      // we really need to convert entities in the title, like < > etc.
      WriteString(utf8String, strlen(utf8String));
  
      WriteString("</H3>", 5);
      WriteLinebreak();
    }
    
    WriteString(indentString, strlen(indentString));
    WriteString("<DL><p>", 7);    
    WriteLinebreak();
  
    if (!WriteChildren(inElement, inDepth + 1))
      return false;
    
    WriteString(indentString, strlen(indentString));
    WriteString("</DL><p>", 8);
    WriteLinebreak();
  }
  else
  {
    if (tagName == BookmarksService::gBookmarkAtom)
    {
      WriteString(indentString, strlen(indentString));

      const char* const bookmarkPrefix = "<DT><A HREF=\"";
      WriteString(bookmarkPrefix, strlen(bookmarkPrefix));

      inElement->GetAttribute(NS_LITERAL_STRING("href"), href);
      inElement->GetAttribute(NS_LITERAL_STRING("name"), title);

      NS_ConvertUCS2toUTF8 hrefConverter(href);
      const char* utf8String = hrefConverter.get();
      
      WriteString(utf8String, strlen(utf8String));
      WriteString("\">", 2);
      
      NS_ConvertUCS2toUTF8 titleConverter(title);
      utf8String = titleConverter.get();

      WriteString(utf8String, strlen(utf8String));
      WriteString("</A>", 4);
      WriteLinebreak();
    }
    else if (tagName == BookmarksService::gSeparatorAtom)
    {
      WriteString(indentString, strlen(indentString));
      WriteString("<HR>", 4);
      WriteLinebreak();
    }
  }
  
  return true;
}

void
BookmarksExport::WritePrologue()
{
  const char* const gPrologueLines[] = {
    "<!DOCTYPE NETSCAPE-Bookmark-file-1>",
    "<!-- This is an automatically generated file.",
    "It will be read and overwritten.",
    "Do Not Edit! -->",
    "<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=UTF-8\">",
    "<TITLE>Bookmarks</TITLE>",
    "<H1>Bookmarks</H1>",
    "",
    nil
  };

  char* const *linePtr = gPrologueLines;

  while (*linePtr)
  {
    WriteString(*linePtr, strlen(*linePtr));
    WriteLinebreak();
    linePtr++;
  }
}

void
BookmarksExport::WriteString(const char*inString, PRInt32 inLen)
{
  PRUint32 bytesWritten;
  if (mWriteStatus == NS_OK)
    mWriteStatus = mOutputStream->Write(inString, inLen, &bytesWritten);
}

void
BookmarksExport::WriteLinebreak()
{
  WriteString(mLinebreakStr, strlen(mLinebreakStr));
}
  

