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

#import "CHBrowserView.h"
#import "BookmarksService.h"
#import "BookmarksDataSource.h"
#import "BookmarkInfoController.h"

#import "StringUtils.h"

#include "nsCRT.h"
#include "nsString.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIAtom.h"
#include "nsITextContent.h"
#include "nsIDOMWindow.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMCharacterData.h"
#include "nsIDOMParser.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIPrefBranch.h"
#include "nsIFile.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIXMLHttpRequest.h"
#include "nsIDOMSerializer.h"
#include "nsIDocumentEncoder.h"
#include "nsNetUtil.h"
#include "nsINamespaceManager.h"
#include "nsIXBLService.h"
#include "nsIWebBrowser.h"


// Helper for stripping whitespace
static void
StripWhitespaceNodes(nsIContent* aElement)
{
  PRInt32 childCount = 0;
  aElement->ChildCount(childCount);
  for (PRInt32 i = 0; i < childCount; i++) {
    nsCOMPtr<nsIContent> child;
    aElement->ChildAt(i, *getter_AddRefs(child));
    nsCOMPtr<nsITextContent> text = do_QueryInterface(child);
    if (text) {
      PRBool isEmpty = PR_FALSE;
      text->IsOnlyWhitespace(&isEmpty);
      if (isEmpty) {
        // This node contained nothing but whitespace.
        // Remove it from the content model.
        aElement->RemoveChildAt(i, PR_TRUE);
        i--; // Decrement our count, since we just removed this child.
        childCount--; // Also decrement our total count.
      }
    }
    else
      StripWhitespaceNodes(child);
  }
}

// the tag of the separator after which to insert bookmarks menu items
// this tag must not conflict with content IDs (which are all >=0)
// and match the tab in the .nib
static const int kBookmarksDividerTag = -1;


PRUint32 BookmarksService::gRefCnt = 0;
nsIDocument* BookmarksService::gBookmarks = nsnull;
NSMutableDictionary* BookmarksService::gDictionary = nil;
MainController* BookmarksService::gMainController = nil;
NSMenu* BookmarksService::gBookmarksMenu = nil;
nsIDOMElement* BookmarksService::gToolbarRoot = nsnull;
nsIAtom* BookmarksService::gBookmarkAtom = nsnull;
nsIAtom* BookmarksService::gDescriptionAtom = nsnull;
nsIAtom* BookmarksService::gFolderAtom = nsnull;
nsIAtom* BookmarksService::gGroupAtom = nsnull;
nsIAtom* BookmarksService::gHrefAtom = nsnull;
nsIAtom* BookmarksService::gKeywordAtom = nsnull;
nsIAtom* BookmarksService::gNameAtom = nsnull;
nsIAtom* BookmarksService::gOpenAtom = nsnull;
nsVoidArray* BookmarksService::gInstances = nsnull;
BOOL BookmarksService::gBookmarksFileReadOK = NO;
int BookmarksService::CHInsertNone = 0;
int BookmarksService::CHInsertInto = 1;
int BookmarksService::CHInsertBefore = 2;
int BookmarksService::CHInsertAfter = 3;

BookmarksService::BookmarksService(BookmarksDataSource* aDataSource)
{
  mDataSource = aDataSource;
  mToolbar = nil;
}

BookmarksService::BookmarksService(CHBookmarksToolbar* aToolbar)
{
  mDataSource = nil;
  mToolbar = aToolbar;
}

BookmarksService::~BookmarksService()
{
}

void
BookmarksService::GetRootContent(nsIContent** aResult)
{
  *aResult = nsnull;
  if (gBookmarks) {
    nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(gBookmarks));
    if (!domDoc) return;

    nsCOMPtr<nsIDOMElement> elt;
    domDoc->GetDocumentElement(getter_AddRefs(elt));
    if (elt)
      elt->QueryInterface(NS_GET_IID(nsIContent), (void**)aResult); // Addref happens here.
  }
}

BookmarkItem*
BookmarksService::GetRootItem()
{
  nsCOMPtr<nsIContent> rootContent;
  BookmarksService::GetRootContent(getter_AddRefs(rootContent));
  BookmarkItem* rootItem = BookmarksService::GetWrapperFor(rootContent);
  return rootItem;
}

BookmarkItem*
BookmarksService::GetWrapperFor(nsIContent* aContent)
{
  if ( !aContent )
    return nil;
    
  if (!gDictionary)
    gDictionary = [[NSMutableDictionary alloc] initWithCapacity: 30];

  PRUint32 contentID = 0;
  aContent->GetContentID(&contentID);

  BookmarkItem* item = [gDictionary objectForKey: [NSNumber numberWithInt: contentID]];
  if (item)
    return item;

  // Create an item.
  item = [[[BookmarkItem alloc] init] autorelease]; // The dictionary retains us.
  [item setContentNode: aContent];
  [gDictionary setObject: item forKey: [NSNumber numberWithInt: contentID]];
  return item;
}

BookmarkItem*
BookmarksService::GetWrapperFor(PRUint32 contentID)
{
  BookmarkItem* item = [gDictionary objectForKey: [NSNumber numberWithUnsignedInt: contentID]];
  return item;
}

NSMenu*
BookmarksService::LocateMenu(nsIContent* aContent)
{
  nsCOMPtr<nsIContent> parent;
  aContent->GetParent(*getter_AddRefs(parent));
  if (!parent) {
    return BookmarksService::gBookmarksMenu;
  }
  
  NSMenu* parentMenu = LocateMenu(parent);
  
  PRUint32 contentID;
  aContent->GetContentID(&contentID);

  NSMenuItem* childMenu = [parentMenu itemWithTag: contentID];
  return [childMenu submenu];
}

void
BookmarksService::BookmarkAdded(nsIContent* aContainer, nsIContent* aChild, bool shouldFlush = true)
{
  if (!gInstances || !gDictionary)
    return;

  PRInt32 count = gInstances->Count();
  for (PRInt32 i = 0; i < count; i++) {
    BookmarksService* instance = (BookmarksService*)gInstances->ElementAt(i);

    if (instance->mDataSource) {
      // We're a tree view.
      nsCOMPtr<nsIContent> parent;
      aContainer->GetParent(*getter_AddRefs(parent));

      BookmarkItem* item = nil;
      if (parent)
        // We're not the root.
        item = GetWrapperFor(aContainer);

      [(instance->mDataSource) reloadDataForItem: item reloadChildren: YES];
    }
    else if (instance->mToolbar) {
      // We're a personal toolbar.
      nsCOMPtr<nsIDOMElement> parentElt(do_QueryInterface(aContainer));
      if (parentElt == gToolbarRoot) {
        // We only care about changes that occur to the personal toolbar's immediate
        // children.
        PRInt32 index = -1;
        aContainer->IndexOf(aChild, index);
        nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(aChild));
        [(instance->mToolbar) addButton: elt atIndex: index];
      }
    }
    else {
      // We're the menu.
      PRInt32 index = -1;
      aContainer->IndexOf(aChild, index);
      NSMenu* menu = LocateMenu(aContainer);
      AddMenuBookmark(menu, aContainer, aChild, index);
    }
  }
  
  if (shouldFlush)
    FlushBookmarks();  
}

void
BookmarksService::BookmarkChanged(nsIContent* aItem, bool shouldFlush = true)
{
  if (!gInstances || !gDictionary)
    return;

  PRInt32 count = gInstances->Count();
  for (PRInt32 i = 0; i < count; i++) {
    BookmarksService* instance = (BookmarksService*)gInstances->ElementAt(i);
   
    if (instance->mDataSource) {
      BookmarkItem* item = GetWrapperFor(aItem);
      [(instance->mDataSource) reloadDataForItem: item reloadChildren: NO];
    }
  }

  if (shouldFlush)
    FlushBookmarks();  
}

void
BookmarksService::BookmarkRemoved(nsIContent* aContainer, nsIContent* aChild, bool shouldFlush = true)
{
  if (!gInstances)
    return;

  PRInt32 count = gInstances->Count();
  for (PRInt32 i = 0; i < count; i++) {
    BookmarksService* instance = (BookmarksService*)gInstances->ElementAt(i);

    if (instance->mDataSource) {
      // We're a tree view.
      nsCOMPtr<nsIContent> parent;
      aContainer->GetParent(*getter_AddRefs(parent));

      BookmarkItem* item = nil;
      if (parent)
        // We're not the root.
        item = GetWrapperFor(aContainer);

      [(instance->mDataSource) reloadDataForItem: item reloadChildren: YES];
    }
    else if (instance->mToolbar) {
      // We're a personal toolbar.
      nsCOMPtr<nsIDOMElement> parentElt(do_QueryInterface(aContainer));
      if (parentElt == gToolbarRoot) {
        // We only care about changes that occur to the personal toolbar's immediate
        // children.
        nsCOMPtr<nsIDOMElement> childElt(do_QueryInterface(aChild));
        [(instance->mToolbar) removeButton: childElt];
      }
    }    
    else {
      // We're the menu.
      NSMenu* menu = LocateMenu(aContainer);
      PRUint32 contentID = 0;
      aChild->GetContentID(&contentID);
      NSMenuItem* childItem = [menu itemWithTag: contentID];
      [menu removeItem: childItem];
    }
  }

  if (shouldFlush)
      FlushBookmarks(); 
}

void
BookmarksService::AddObserver()
{
    gRefCnt++;
    if (gRefCnt == 1) {
        gBookmarkAtom = NS_NewAtom("bookmark");
        gFolderAtom = NS_NewAtom("folder");
        gNameAtom = NS_NewAtom("name");
        gHrefAtom = NS_NewAtom("href");
        gOpenAtom = NS_NewAtom("open");
        gKeywordAtom = NS_NewAtom("id");
        gDescriptionAtom = NS_NewAtom("description");
        gGroupAtom = NS_NewAtom("group");
        gInstances = new nsVoidArray();
    
    ReadBookmarks();
     }
    
    gInstances->AppendElement(this);
}

void
BookmarksService::RemoveObserver()
{
    if (gRefCnt == 0)
        return;
 
    gInstances->RemoveElement(this);
     
    gRefCnt--;
    if (gRefCnt == 0) {
        // Flush Bookmarks before shutting down as some changes are not flushed when
        // they are performed (folder open/closed) as writing a whole bookmark file for
        // that type of operation seems excessive. 
        FlushBookmarks();
          
        NS_IF_RELEASE(gBookmarks);
        NS_RELEASE(gBookmarkAtom);
        NS_RELEASE(gFolderAtom);
        NS_RELEASE(gNameAtom);
        NS_RELEASE(gHrefAtom);
        NS_RELEASE(gOpenAtom);
        [gDictionary release];
    }
}

void
BookmarksService::AddBookmarkToFolder(nsString& aURL, nsString& aTitle, nsIDOMElement* aFolder, nsIDOMElement* aBeforeElt)
{
  // XXX if no folder provided, default to root folder
  if (!aFolder) return;
  
  nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(gBookmarks));
  nsCOMPtr<nsIDOMElement> elt;
  domDoc->CreateElementNS(NS_LITERAL_STRING("http://chimera.mozdev.org/bookmarks/"),
                          NS_LITERAL_STRING("bookmark"),
                          getter_AddRefs(elt));

  elt->SetAttribute(NS_LITERAL_STRING("name"), aTitle);
  elt->SetAttribute(NS_LITERAL_STRING("href"), aURL);

  MoveBookmarkToFolder(elt, aFolder, aBeforeElt);
}

void
BookmarksService::MoveBookmarkToFolder(nsIDOMElement* aBookmark, nsIDOMElement* aFolder, nsIDOMElement* aBeforeElt)
{
  if (!aBookmark || !aFolder) return;
  
  nsCOMPtr<nsIDOMNode> oldParent;
  aBookmark->GetParentNode(getter_AddRefs(oldParent));

  nsCOMPtr<nsIDOMNode> dummy;
  if (oldParent) {
    nsCOMPtr<nsIDOMNode> bookmarkNode = do_QueryInterface(aBookmark);
    oldParent->RemoveChild(bookmarkNode, getter_AddRefs(dummy));
  }

  if (aBeforeElt) {
    aFolder->InsertBefore(aBookmark, aBeforeElt, getter_AddRefs(dummy));
  } else {
    aFolder->AppendChild(aBookmark, getter_AddRefs(dummy));
  }
  
  nsCOMPtr<nsIContent> childContent(do_QueryInterface(aBookmark));
  nsCOMPtr<nsIContent> parentContent(do_QueryInterface(aFolder));

  if (oldParent) {
    nsCOMPtr<nsIContent> oldParentContent(do_QueryInterface(oldParent));
    BookmarkRemoved(oldParentContent, childContent);
  }
  
  BookmarkAdded(parentContent, childContent);
}

void
BookmarksService::DeleteBookmark(nsIDOMElement* aBookmark)
{
  if (!aBookmark) return;
  
  nsCOMPtr<nsIDOMNode> oldParent;
  aBookmark->GetParentNode(getter_AddRefs(oldParent));

  if (oldParent) {
    nsCOMPtr<nsIDOMNode> dummy;
    nsCOMPtr<nsIDOMNode> bookmarkNode = do_QueryInterface(aBookmark);
    oldParent->RemoveChild(bookmarkNode, getter_AddRefs(dummy));

    nsCOMPtr<nsIContent> childContent(do_QueryInterface(aBookmark));
    nsCOMPtr<nsIContent> oldParentContent(do_QueryInterface(oldParent));
    BookmarkRemoved(oldParentContent, childContent);
  }
}

static PRBool
CheckXMLDocumentParseSuccessful(nsIDOMDocument* inDOMDoc)
{
  nsCOMPtr<nsIDOMElement> docElement;
  inDOMDoc->GetDocumentElement(getter_AddRefs(docElement));
  if (!docElement)
    return PR_FALSE;
  
  nsCOMPtr<nsIAtom> tagName;
  nsCOMPtr<nsIContent>   docContent = do_QueryInterface(docElement);
  docContent->GetTag(*getter_AddRefs(tagName));

  nsCOMPtr<nsIAtom>	parserErrorAtom = do_GetAtom("parsererror");
  if (parserErrorAtom != tagName)
    return PR_TRUE;

  return PR_FALSE;
}

static PRBool
ValidateXMLDocument(nsIDOMDocument* inDOMDoc)
{
  if (!inDOMDoc)
    return PR_FALSE;

  nsCOMPtr<nsIDOMElement> elt;
  inDOMDoc->GetDocumentElement(getter_AddRefs(elt));
  if (!elt)
    return PR_FALSE;

  nsCOMPtr<nsIDOMSerializer> domSerializer = do_CreateInstance(NS_XMLSERIALIZER_CONTRACTID);
  if (!domSerializer)
    return PR_FALSE;
  
  nsXPIDLString encodedDocStr;
  nsresult rv = domSerializer->SerializeToString(inDOMDoc, getter_Copies(encodedDocStr));
  if (NS_FAILED(rv))
    return PR_FALSE;
  
  nsCOMPtr<nsIDOMParser> domParser = do_CreateInstance(NS_DOMPARSER_CONTRACTID);
  if (!domParser)
    return PR_FALSE;

  nsCOMPtr<nsIDOMDocument> newDomDoc;
  domParser->ParseFromString(encodedDocStr.get(), "text/xml", getter_AddRefs(newDomDoc));
  if (newDomDoc)
   	return CheckXMLDocumentParseSuccessful(newDomDoc);
  
  return PR_FALSE;
}

void
BookmarksService::ReadBookmarks()
{
  nsCOMPtr<nsIFile> profileDirBookmarks;
  NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(profileDirBookmarks));
  profileDirBookmarks->Append(NS_LITERAL_STRING("bookmarks.xml"));
  
  PRBool fileExists = PR_FALSE;
  profileDirBookmarks->Exists(&fileExists);
  
  // If the bookmarks file does not exist, copy from the defaults so we don't
  // crash or anything dumb like that. 
  if (!fileExists) {
    nsCOMPtr<nsIFile> defaultBookmarksFile;
    NS_GetSpecialDirectory(NS_APP_PROFILE_DEFAULTS_50_DIR, getter_AddRefs(defaultBookmarksFile));
    defaultBookmarksFile->Append(NS_LITERAL_STRING("bookmarks.xml"));
    
    // XXX for some reason unknown to me, leaving this code in causes the program to crash
    //     with 'cannot dereference null COMPtr.'
#if I_WANT_TO_CRASH
    PRBool defaultFileExists;
    defaultBookmarksFile->Exists(&defaultFileExists);
    if (defaultFileExists)
      return;
#endif
  
    nsCOMPtr<nsIFile> profileDirectory;
    NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(profileDirectory));
  
    defaultBookmarksFile->CopyToNative(profileDirectory, NS_LITERAL_CSTRING("bookmarks.xml"));
  }
  
  nsCAutoString bookmarksFileURL;
  NS_GetURLSpecFromFile(profileDirBookmarks, bookmarksFileURL);
  
  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), bookmarksFileURL.get());
  
  // XXX this is somewhat lame. we have no way of knowing whether or not the parse succeeded
  //     or failed. sigh.
  // Actually, we do. We check for a root <parsererror> node. This relies on the XMLContentSink
  // behaviour.
  nsCOMPtr<nsIXBLService> xblService(do_GetService("@mozilla.org/xbl;1"));    
  xblService->FetchSyncXMLDocument(uri, &gBookmarks);   // addref here
    
  // test for a parser error. The XML parser replaces the document with one
  // that has a <parsererror> node as the root.
  nsCOMPtr<nsIDOMDocument> bookmarksDOMDoc = do_QueryInterface(gBookmarks);
  BOOL validPrefsFile = CheckXMLDocumentParseSuccessful(bookmarksDOMDoc);
  
  if (!validPrefsFile) {
    // uh oh, parser error. Throw some UI
    NSString *alert     = NSLocalizedString(@"CorruptedBookmarksAlert",@"");
    NSString *message   = NSLocalizedString(@"CorruptedBookmarksMsg",@"");
    NSString *okButton  = NSLocalizedString(@"OKButtonText",@"");
    NSRunAlertPanel(alert, message, okButton, nil, nil);

    // maybe we should read the default bookmarks here?
    gBookmarksFileReadOK = PR_FALSE;
    return;
  }
  
  gBookmarksFileReadOK = PR_TRUE;
  
  nsCOMPtr<nsIContent> rootNode;
  GetRootContent(getter_AddRefs(rootNode));
  StripWhitespaceNodes(rootNode);
}

void
BookmarksService::FlushBookmarks()
{
    // XXX we need to insert a mechanism here to ensure that we don't write corrupt
    //     bookmarks files (e.g. full disk, program crash, whatever), because our
    //     error handling in the parse stage is NON-EXISTENT.
  //  This is now partially handled by looking for a <parsererror> node at read time.
  if (!gBookmarksFileReadOK)
    return;
  
  nsCOMPtr<nsIFile> bookmarksFile;
  NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(bookmarksFile));
  bookmarksFile->Append(NS_LITERAL_STRING("bookmarks.xml"));
  
  nsCOMPtr<nsIOutputStream> outputStream;
  NS_NewLocalFileOutputStream(getter_AddRefs(outputStream), bookmarksFile);

  nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(gBookmarks));

  nsCOMPtr<nsIDOMSerializer> domSerializer(do_CreateInstance(NS_XMLSERIALIZER_CONTRACTID));
  if (domSerializer)
    domSerializer->SerializeToStream(domDoc, outputStream, nsnull);
}

void BookmarksService::EnsureToolbarRoot()
{
  if (gToolbarRoot)
    return;

  nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(gBookmarks));
  nsCOMPtr<nsIDOMElement> rootElt;
  domDoc->GetDocumentElement(getter_AddRefs(rootElt));
  
  nsCOMPtr<nsIDOMNode> child;
  rootElt->GetFirstChild(getter_AddRefs(child));
  nsAutoString typeValue;
  while (child) {
    nsCOMPtr<nsIDOMElement> childElt(do_QueryInterface(child));
    if (childElt) {
      childElt->GetAttribute(NS_LITERAL_STRING("type"), typeValue);
      if (typeValue.Equals(NS_LITERAL_STRING("toolbar")))
        gToolbarRoot = childElt;
    }
    
    nsCOMPtr<nsIDOMNode> temp;
    child->GetNextSibling(getter_AddRefs(temp));
    child = temp;
  }

  if (!gToolbarRoot) {
    NSLog(@"Repairing personal toolbar");
    nsCOMPtr<nsIDOMElement> elt;
    domDoc->CreateElementNS(NS_LITERAL_STRING("http://chimera.mozdev.org/bookmarks/"),
                            NS_LITERAL_STRING("folder"),
                            getter_AddRefs(elt));

    elt->SetAttribute(NS_LITERAL_STRING("name"), NS_LITERAL_STRING("Toolbar Bookmarks"));
    elt->SetAttribute(NS_LITERAL_STRING("type"), NS_LITERAL_STRING("toolbar"));

    nsCOMPtr<nsIDOMNode> dummy;
    rootElt->AppendChild(elt, getter_AddRefs(dummy));
    gToolbarRoot = elt;
  }
}

static
void RecursiveAddBookmarkConstruct(NSPopUpButton* aPopup, NSMenu* aMenu, int aTagToMatch, int depth = 0)
{
  // Get the menu item children.
  NSArray* children = [aMenu itemArray];
  int startPosition = 0;
  if (aMenu == BookmarksService::gBookmarksMenu)
    startPosition = 3;

  int count = [children count];
  for (int i = startPosition; i < count; ++i) {
    NSMenuItem* menuItem = [children objectAtIndex: i];
    NSMenu* submenu = [menuItem submenu];
    if (submenu) {
      // This is a folder.  Add it to our list and then recur. Indent it
      // the apropriate depth for readability in the menu.
      NSMutableString *title = [NSMutableString stringWithString:[menuItem title]];
      for (int j = 0; j <= depth; ++j) 
        [title insertString:@"    " atIndex: 0];
    
      [aPopup addItemWithTitle: title];
      NSMenuItem* lastItem = [aPopup lastItem];
      if ([menuItem tag] == aTagToMatch)
        [aPopup selectItem: lastItem];
      
      [lastItem setTag: [menuItem tag]];
      RecursiveAddBookmarkConstruct(aPopup, submenu, aTagToMatch, depth+1);
    }
  }
}

void
BookmarksService::ConstructAddBookmarkFolderList(NSPopUpButton* aPopup, BookmarkItem* aItem)
{
  [aPopup removeAllItems];
  [aPopup addItemWithTitle: [gBookmarksMenu title]];
  NSMenuItem* lastItem = [aPopup lastItem];
  [lastItem setTag: -1];
  int tag = -1;
  if (aItem) {
    nsIContent* content = [aItem contentNode];
    PRUint32 utag;
    content->GetContentID(&utag);
    tag = (int)utag;
  }
  RecursiveAddBookmarkConstruct(aPopup, gBookmarksMenu, tag);
}

void
BookmarksService::GetTitleAndHrefForBrowserView(id aBrowserView, nsString& aTitle, nsString& aHref)
{
  nsCOMPtr<nsIWebBrowser> webBrowser = getter_AddRefs([aBrowserView getWebBrowser]);
  nsCOMPtr<nsIDOMWindow> window;
  webBrowser->GetContentDOMWindow(getter_AddRefs(window));
  nsCOMPtr<nsIDOMDocument> htmlDoc;
  window->GetDocument(getter_AddRefs(htmlDoc));
  nsCOMPtr<nsIDocument> pageDoc(do_QueryInterface(htmlDoc));

  if (pageDoc) {
    nsCOMPtr<nsIURI> url;
    pageDoc->GetDocumentURL(getter_AddRefs(url));
    nsCAutoString spec;
    url->GetSpec(spec);
    aHref.AssignWithConversion(spec.get());
  }

  nsCOMPtr<nsIDOMHTMLDocument> htmlDocument(do_QueryInterface(htmlDoc));
  if (htmlDocument)
    htmlDocument->GetTitle(aTitle);
  if (aTitle.IsEmpty())
    aTitle = aHref;  
}

void
BookmarksService::ConstructBookmarksMenu(NSMenu* aMenu, nsIContent* aContent)
{
    nsCOMPtr<nsIContent> content = aContent;
    if (!content) {
        GetRootContent(getter_AddRefs(content));
        GetWrapperFor(content);
        gBookmarksMenu = aMenu;
    }
    
    // Now walk our children, and for folders also recur into them.
    PRInt32 childCount;
    content->ChildCount(childCount);
    
    for (PRInt32 i = 0; i < childCount; i++) {
      nsCOMPtr<nsIContent> child;
      content->ChildAt(i, *getter_AddRefs(child));
      AddMenuBookmark(aMenu, content, child, -1);
    }
}

void
BookmarksService::AddMenuBookmark(NSMenu* aMenu, nsIContent* aParent, nsIContent* aChild, PRInt32 aIndex)
{
  nsAutoString name;
  aChild->GetAttr(kNameSpaceID_None, gNameAtom, name);
  NSString* title = [NSString stringWithCharacters: name.get() length: name.Length()];

  // Create a menu or menu item for the child.
  NSMenuItem* menuItem = [[[NSMenuItem alloc] initWithTitle: title action: NULL keyEquivalent: @""] autorelease];
  GetWrapperFor(aChild);

  if (aIndex == -1)
    [aMenu addItem: menuItem];
  else {
    PRInt32 insertIndex = aIndex;
    if (aMenu == gBookmarksMenu)	// take static menu items into account
      insertIndex += [aMenu indexOfItemWithTag:kBookmarksDividerTag] + 1;
    
    [aMenu insertItem: menuItem atIndex: insertIndex];
  }
  
  nsCOMPtr<nsIAtom> tagName;
  aChild->GetTag(*getter_AddRefs(tagName));

  nsAutoString group;
  aChild->GetAttr(kNameSpaceID_None, gGroupAtom, group);

  if (group.IsEmpty() && tagName == gFolderAtom) {
    NSMenu* menu = [[[NSMenu alloc] initWithTitle: title] autorelease];
    [aMenu setSubmenu: menu forItem: menuItem];
    [menu setAutoenablesItems: NO];
    [menuItem setImage: [NSImage imageNamed:@"folder"]];
    ConstructBookmarksMenu(menu, aChild);
  }
  else {
    if (group.IsEmpty())
      [menuItem setImage: [NSImage imageNamed:@"smallbookmark"]];
    else
      [menuItem setImage: [NSImage imageNamed:@"groupbookmark"]];
    
    [menuItem setTarget: gMainController];
    [menuItem setAction: @selector(openMenuBookmark:)];
  }

  PRUint32 contentID;
  aChild->GetContentID(&contentID);
  [menuItem setTag: contentID];
}

void 
BookmarksService::OpenMenuBookmark(BrowserWindowController* aController, id aMenuItem)
{
  // Get the corresponding bookmark item.
  BookmarkItem* item = [gDictionary objectForKey: [NSNumber numberWithInt: [aMenuItem tag]]];

  // Get the content node.
  nsIContent* content = [item contentNode];
  nsAutoString group;
  content->GetAttr(kNameSpaceID_None, gGroupAtom, group);
  if (!group.IsEmpty()) {
    nsCOMPtr<nsIDOMElement> elt(do_QueryInterface([item contentNode]));
    return OpenBookmarkGroup([aController getTabBrowser], elt);
  }
  
  // Get the href attribute.  This is the URL we want to load.
  nsAutoString href;
  content->GetAttr(kNameSpaceID_None, gHrefAtom, href);
  if (href.IsEmpty())
    return;

  NSString* url = [NSString stringWithCharacters: href.get() length: href.Length()];

  // Now load the URL in the window.
  [aController loadURL:url];

  // Focus and activate our content area.
  [[[aController getBrowserWrapper] getBrowserView] setActive: YES];
}

static void GetImportTitle(nsIDOMElement* aSrc, nsString& aTitle)
{
  nsCOMPtr<nsIDOMNode> curr;
  aSrc->GetFirstChild(getter_AddRefs(curr));
  while (curr) {
    nsCOMPtr<nsIDOMCharacterData> charData(do_QueryInterface(curr));
    if (charData) {
      nsAutoString data;
      charData->GetData(data);
      aTitle += data;
    }
    else {
      // Handle Omniweb's nesting of <a> inside <h3> for its folders.
      nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(curr));
      if (elt) {
        nsAutoString localName;
        elt->GetLocalName(localName);
        ToLowerCase(localName);
        if (localName.Equals(NS_LITERAL_STRING("a"))) {
          aTitle = NS_LITERAL_STRING("");
          return GetImportTitle(elt, aTitle);
        }
      }
    }
    
    nsCOMPtr<nsIDOMNode> temp = curr;
    temp->GetNextSibling(getter_AddRefs(curr));
  }
}

static void CreateBookmark(nsIDOMElement* aSrc, nsIDOMElement* aDst,
                           nsIDOMDocument* aDstDoc, PRBool aIsFolder,
                           nsIDOMElement** aResult)
{
  nsAutoString tagName(NS_LITERAL_STRING("bookmark"));
  if (aIsFolder)
    tagName = NS_LITERAL_STRING("folder");

  aDstDoc->CreateElementNS(NS_LITERAL_STRING("http://chimera.mozdev.org/bookmarks/"),
                           tagName,
                           aResult); // Addref happens here.

  nsAutoString title;
  GetImportTitle(aSrc, title);
  (*aResult)->SetAttribute(NS_LITERAL_STRING("name"), title);

  if (!aIsFolder) {
    nsAutoString href;
    aSrc->GetAttribute(NS_LITERAL_STRING("href"), href);
    (*aResult)->SetAttribute(NS_LITERAL_STRING("href"), href);
  }

  nsCOMPtr<nsIDOMNode> dummy;
  aDst->AppendChild(*aResult, getter_AddRefs(dummy));
}

static void AddImportedBookmarks(nsIDOMElement* aSrc, nsIDOMElement* aDst, nsIDOMDocument* aDstDoc,
                                 PRInt32& aBookmarksType)
{
  nsAutoString localName;
  aSrc->GetLocalName(localName);
  ToLowerCase(localName);
  nsCOMPtr<nsIDOMElement> newBookmark;
  if (localName.Equals(NS_LITERAL_STRING("bookmarkinfo")))
    aBookmarksType = 1; // Omniweb.
  else if (localName.Equals(NS_LITERAL_STRING("dt"))) {
    // We have found either a folder or a leaf.
    nsCOMPtr<nsIDOMNode> curr;
    aSrc->GetFirstChild(getter_AddRefs(curr));
    while (curr) {
      nsCOMPtr<nsIDOMElement> childElt(do_QueryInterface(curr));
      if (childElt) {
        childElt->GetLocalName(localName);
        ToLowerCase(localName);
        if (localName.Equals(NS_LITERAL_STRING("a"))) {
          // Guaranteed to be a bookmark in IE.  Could be either in Omniweb.
          nsCOMPtr<nsIDOMElement> dummy;
          CreateBookmark(childElt, aDst, aDstDoc, PR_FALSE, getter_AddRefs(dummy));
        }
        // Ignore the H3 we encounter.  This will be dealt with later.
      }
      nsCOMPtr<nsIDOMNode> temp = curr;
      temp->GetNextSibling(getter_AddRefs(curr));
    }
  }
  else if (localName.Equals(NS_LITERAL_STRING("dl"))) {
    // The children of a folder.  Recur inside.
    // Locate the parent to create the folder.
    nsCOMPtr<nsIDOMNode> node;
    aSrc->GetPreviousSibling(getter_AddRefs(node));
    nsCOMPtr<nsIDOMElement> folderElt(do_QueryInterface(node));
    if (folderElt) {
      // Make sure it's an H3 folder in Mozilla and IE.  In Mozilla it will probably have an ID.
      PRBool hasID;
      folderElt->HasAttribute(NS_LITERAL_STRING("ID"), &hasID);
      if (aBookmarksType != 1) {
        if (hasID)
          aBookmarksType = 2; // Mozilla
        else
          aBookmarksType = 0; // IE
      }
      nsAutoString localName;
      folderElt->GetLocalName(localName);
      ToLowerCase(localName);
      if (localName.Equals(NS_LITERAL_STRING("h3")))
        CreateBookmark(folderElt, aDst, aDstDoc, PR_TRUE, getter_AddRefs(newBookmark));
    }
    if (!newBookmark)
      newBookmark = aDst;
    // Recur over all our children.
    nsCOMPtr<nsIDOMNode> curr;
    aSrc->GetFirstChild(getter_AddRefs(curr));
    while (curr) {
      nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(curr));
      if (elt)
        AddImportedBookmarks(elt, newBookmark, aDstDoc, aBookmarksType);
      nsCOMPtr<nsIDOMNode> temp = curr;
      temp->GetNextSibling(getter_AddRefs(curr));
    }
  }
  else {
    // Recur over all our children.
    nsCOMPtr<nsIDOMNode> curr;
    aSrc->GetFirstChild(getter_AddRefs(curr));
    while (curr) {
      nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(curr));
      if (elt)
        AddImportedBookmarks(elt, aDst, aDstDoc, aBookmarksType);
      nsCOMPtr<nsIDOMNode> temp = curr;
      temp->GetNextSibling(getter_AddRefs(curr));
    }
  }
}


#define SAFE_BOOKMARKS_IMPORT
//static NS_DEFINE_IID(kXMLDocumentCID, NS_XMLDOCUMENT_CID);

void
BookmarksService::ImportBookmarks(nsIDOMHTMLDocument* aHTMLDoc)
{
  nsCOMPtr<nsIDOMElement> htmlDocRoot;
  aHTMLDoc->GetDocumentElement(getter_AddRefs(htmlDocRoot));

  nsCOMPtr<nsIDOMElement>  bookmarksRoot;
  nsCOMPtr<nsIDOMDocument> bookmarksDOMDoc(do_QueryInterface(gBookmarks));
  bookmarksDOMDoc->GetDocumentElement(getter_AddRefs(bookmarksRoot));

  nsCOMPtr<nsIDOMNode> dummy;
  
  // Create the root of the new bookmarks by hand.
  nsCOMPtr<nsIDOMElement> importedRootElement;
  bookmarksDOMDoc->CreateElementNS(	NS_LITERAL_STRING("http://chimera.mozdev.org/bookmarks/"),
                                      NS_LITERAL_STRING("folder"),
                                      getter_AddRefs(importedRootElement));
  
  // Now crawl through the file and look for <DT> elements.  They signify folders
  // or leaves.
  PRInt32 bookmarksType = 0; // Assume IE.
  AddImportedBookmarks(htmlDocRoot, importedRootElement, bookmarksDOMDoc, bookmarksType);

  if (bookmarksType == 0)
    importedRootElement->SetAttribute(NS_LITERAL_STRING("name"), NS_LITERAL_STRING("Internet Explorer Favorites"));
  else if (bookmarksType == 1)
    importedRootElement->SetAttribute(NS_LITERAL_STRING("name"), NS_LITERAL_STRING("Omniweb Favorites"));
  else if (bookmarksType == 2)
    importedRootElement->SetAttribute(NS_LITERAL_STRING("name"), NS_LITERAL_STRING("Mozilla/Netscape Favorites"));

  // now put the new child into the doc, and validate it
  bookmarksRoot->AppendChild(importedRootElement, getter_AddRefs(dummy));

  PRBool bookmarksGood = ValidateXMLDocument(bookmarksDOMDoc);
  if (!bookmarksGood) {
    // uh oh, parser error. Remove the new node, and then throw some UI
    bookmarksRoot->RemoveChild(importedRootElement, getter_AddRefs(dummy));
        
    NSString *alert     = NSLocalizedString(@"ErrorImportingBookmarksAlert",@"");
    NSString *message   = NSLocalizedString(@"ErrorImportingBookmarksMsg",@"");
    NSString *okButton  = NSLocalizedString(@"OKButtonText",@"");
    NSRunAlertPanel(alert, message, okButton, nil, nil);
    return;
  }

  // Now do a notification that the root Favorites folder got added.  This
  // will update all our views.
  nsCOMPtr<nsIContent> parentContent(do_QueryInterface(bookmarksRoot));
  nsCOMPtr<nsIContent> childContent(do_QueryInterface(importedRootElement));
  
  // XXX testing
  if (gDictionary)
    [gDictionary removeAllObjects];
  
  // this will save the file
  BookmarkAdded(parentContent, childContent, true /* flush */);
}

void
BookmarksService::OpenBookmarkGroup(id aTabView, nsIDOMElement* aFolder)
{
  // We might conceivably have to make new tabs in order to load all
  // the items in the group.
  int currentIndex = 0;
  int total = [aTabView numberOfTabViewItems];
  nsCOMPtr<nsIDOMNode> child;
  aFolder->GetFirstChild(getter_AddRefs(child));
  while (child) {
    nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(child));
    if (elt) {
      nsAutoString href;
      elt->GetAttribute(NS_LITERAL_STRING("href"), href);
      if (!href.IsEmpty()) {
        NSString* url = [NSString stringWithCharacters: href.get() length: href.Length()];
        NSTabViewItem* tabViewItem = nil;
        if (currentIndex >= total) {
          // We need to make a new tab.
          tabViewItem = [[[NSTabViewItem alloc] initWithIdentifier: nil] autorelease];
          CHBrowserWrapper* newView = [[[CHBrowserWrapper alloc] initWithTab: tabViewItem andWindow: [aTabView window]] autorelease];
          [tabViewItem setLabel: NSLocalizedString(@"UntitledPageTitle", @"")];
          [tabViewItem setView: newView];
          [aTabView addTabViewItem: tabViewItem];
        }
        else
          tabViewItem = [aTabView tabViewItemAtIndex: currentIndex];

        [[[tabViewItem view] getBrowserView] loadURI: url
                                               flags: NSLoadFlagsNone];
      }
    }
    
    nsCOMPtr<nsIDOMNode> temp = child;
    temp->GetNextSibling(getter_AddRefs(child));
    currentIndex++;
  }

  // Select and activate the first tab.
  [aTabView selectTabViewItemAtIndex: 0];
  [[[[aTabView tabViewItemAtIndex: 0] view] getBrowserView] setActive: YES];
}

NSString* 
BookmarksService::ResolveKeyword(NSString* aKeyword)
{
  nsAutoString keyword;
  NSStringTo_nsString(aKeyword, keyword);

  if (keyword.IsEmpty())
    return [NSString stringWithCString:""];
  
  NSLog(@"str = %s", keyword.get());
  
  nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(gBookmarks));
  nsCOMPtr<nsIDOMElement> elt;
  domDoc->GetElementById(keyword, getter_AddRefs(elt));

  nsCOMPtr<nsIContent> content(do_QueryInterface(elt));
  nsAutoString url;
  if (content) {
    content->GetAttr(kNameSpaceID_None, gHrefAtom, url);
    return [NSString stringWithCharacters: url.get() length: url.Length()];
  }
  return [NSString stringWithCString:""];
}

NSImage*
BookmarksService::CreateIconForBookmark(nsIDOMElement* aElement)
{
  nsCOMPtr<nsIAtom> tagName;
  nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
  content->GetTag(*getter_AddRefs(tagName));
  if (tagName == BookmarksService::gFolderAtom)
    return [NSImage imageNamed:@"folder"];
    
  nsAutoString group;
  content->GetAttr(kNameSpaceID_None, gGroupAtom, group);
  if (!group.IsEmpty())
    return [NSImage imageNamed:@"smallgroup"];
  
  return [NSImage imageNamed:@"groupbookmark"];
}

// Is searchItem equal to bookmark or bookmark's parent, grandparent, etc?
BOOL
BookmarksService::DoAncestorsIncludeNode(BookmarkItem* bookmark, BookmarkItem* searchItem)
{
  nsCOMPtr<nsIContent> search = [searchItem contentNode];
  nsCOMPtr<nsIContent> current = [bookmark contentNode];
  nsCOMPtr<nsIContent> root;
  GetRootContent(getter_AddRefs(root));
    
  // If the search item is the root node, return yes immediatly
  if (search == root)
    return YES;

  //  for each ancestor
  while (current) {
    // If this is the root node we can't search farther, and there was no match
    if (current == root)
      return NO;
    
    // If the two nodes match, then the search term is an ancestor of the given bookmark
    if (search == current)
      return YES;

    // If a match wasn't found, set up the next node to compare
    nsCOMPtr<nsIContent> oldCurrent = current;
    oldCurrent->GetParent(*getter_AddRefs(current));
  }
  
  return NO;
}


#ifdef FILTER_DESCENDANT_ON_DRAG
/*
this has been disabled because it is too slow, and can cause a large
delay when the user is dragging lots of items.  this needs to get
fixed someday.

It should filter out every node whose parent is also being dragged.
*/

NSArray*
BookmarksService::FilterOutDescendantsForDrag(NSArray* nodes)
{
  NSMutableArray *toDrag = [NSMutableArray arrayWithArray: nodes];
  unsigned int i = 0;

  while (i < [toDrag count]) {
    BookmarkItem* item = [toDrag objectAtIndex: i];
    bool matchFound = false;
    
    for (unsigned int j = 0; j < [toDrag count] && matchFound == NO; j++) {
      if (i != j) // Don't compare to self, will always match
        matchFound = BookmarksService::DoAncestorsIncludeNode(item, [toDrag objectAtIndex: j]);
    }
    
    //  if a match was found, remove the node from the array
    if (matchFound)
      [toDrag removeObjectAtIndex: i];
    else
      i++;
  }
  
  return toDrag;
}
#endif

bool
BookmarksService::IsBookmarkDropValid(BookmarkItem* proposedParent, int index, NSArray* draggedIDs)
{
  if ( !draggedIDs )
    return NO;

  NSMutableArray *draggedItems = [NSMutableArray arrayWithCapacity: [draggedIDs count]];
  BOOL toolbarRootMoving = NO;
  
  for (unsigned int i = 0; i < [draggedIDs count]; i++) {
    NSNumber* contentID = [draggedIDs objectAtIndex: i];
    BookmarkItem* bookmarkItem = BookmarksService::GetWrapperFor([contentID unsignedIntValue]);
    nsCOMPtr<nsIContent> itemContent = [bookmarkItem contentNode];    
    nsCOMPtr<nsIDOMElement> itemElement(do_QueryInterface(itemContent));

    if (itemElement == BookmarksService::gToolbarRoot)
      toolbarRootMoving = YES;

    if (bookmarkItem)
      [draggedItems addObject: bookmarkItem];
  }
      
  // If we are being dropped into the top level, allow it
  if ([proposedParent contentNode] == [BookmarksService::GetRootItem() contentNode])
    return true;
    
  // If we are not being dropped on the top level, and the toolbar root is being moved, disallow
  if (toolbarRootMoving)
    return false;

  // Make sure that we are not being dropped into one of our own children
  // If the proposed parent, or any of it's ancestors matches one of the nodes being dragged
  // then deny the drag.
  
  for (unsigned int i = 0; i < [draggedItems count]; i++) {
    if (BookmarksService::DoAncestorsIncludeNode(proposedParent, [draggedItems objectAtIndex: i])) {
      return false;
    }
  }
  
  return true;
}


bool
BookmarksService::PerformProxyDrop(BookmarkItem* parentItem, BookmarkItem* beforeItem, NSDictionary* data)
{
  if ( !data )
    return NO;

  nsCOMPtr<nsIDOMElement> parentElt;
  parentElt = do_QueryInterface([parentItem contentNode]);

  nsCOMPtr<nsIDOMElement> beforeElt;
  beforeElt = do_QueryInterface([beforeItem contentNode]);

  nsAutoString url; url.AssignWithConversion([[data objectForKey:@"url"] cString]);
  nsAutoString title; title.AssignWithConversion([[data objectForKey:@"title"] cString]);
  BookmarksService::AddBookmarkToFolder(url, title, parentElt, beforeElt);
  return YES;  
}


bool
BookmarksService::PerformBookmarkDrop(BookmarkItem* parent, int index, NSArray* draggedIDs)
{
  NSEnumerator *enumerator = [draggedIDs reverseObjectEnumerator];
  NSNumber *contentID;
  
  //  for each item being dragged
  while ( (contentID = [enumerator nextObject]) ) {
  
    //  get dragged node
    nsCOMPtr<nsIContent> draggedNode = [GetWrapperFor([contentID unsignedIntValue]) contentNode];
    
    //  get the dragged nodes parent
    nsCOMPtr<nsIContent> draggedParent;
    if (draggedNode)
      draggedNode->GetParent(*getter_AddRefs(draggedParent));

    //  get the proposed parent
    nsCOMPtr<nsIContent> proposedParent = [parent contentNode];

    PRInt32 existingIndex = 0;
    if (draggedParent)
      draggedParent->IndexOf(draggedNode, existingIndex);
    
    //  if the deleted nodes parent and the proposed parents are equal
    //  and if the deleted point is eariler in the list than the inserted point
    if (proposedParent == draggedParent && existingIndex < index) {
      index--;  //  if so, move the inserted point up one to compensate
    }
    
    //  remove it from the tree
    if (draggedParent)
      draggedParent->RemoveChildAt(existingIndex, PR_TRUE);
    BookmarkRemoved(draggedParent, draggedNode, false);
    
    //  insert into new position
    if (proposedParent)
      proposedParent->InsertChildAt(draggedNode, index, PR_TRUE, PR_TRUE);
    BookmarkAdded(proposedParent, draggedNode, false);
  }

  FlushBookmarks();
  
  return true;
}

void
BookmarksService::DropURL(NSString* title, NSURL* url, BookmarkItem* parent, int index) 
{
  NSLog(@"DropURL not implemented yet\n");
}

