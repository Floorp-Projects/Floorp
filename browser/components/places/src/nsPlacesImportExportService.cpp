/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla History System
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brett Wilson <brettw@gmail.com>
 *   Dietrich Ayala <dietrich@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/**
 * Importer/exporter between the mozStorage-based bookmarks and the old-style
 * "bookmarks.html"
 *
 * Format:
 *
 * Primary heading := h1
 *   Old version used this to set attributes on the bookmarks RDF root, such
 *   as the last modified date. We only use H1 to check for the attribute
 *   PLACES_ROOT, which tells us that this hierarchy root is the places root.
 *   For backwards compatability, if we don't find this, we assume that the
 *   hierarchy is rooted at the bookmarks menu.
 * Heading := any heading other than h1
 *   Old version used this to set attributes on the current container. We only
 *   care about the content of the heading container, which contains the title
 *   of the bookmark container.
 * Bookmark := a
 *   HREF is the destination of the bookmark
 *   FEEDURL is the URI of the RSS feed if this is a livemark.
 *   LAST_CHARSET is stored as an annotation so that the next time we go to
 *     that page we remember the user's preference.
 *   WEB_PANEL is set to "true" if the bookmark should be loaded in the sidebar.
 *   ICON will be stored in the favicon service
 *   ICON_URI is new for places bookmarks.html, it refers to the original
 *     URI of the favicon so we don't have to make up favicon URLs.
 *   Text of the <a> container is the name of the bookmark
 *   Ignored: LAST_VISIT, ID (writing out non-RDF IDs can confuse Firefox 2)
 * Bookmark comment := dd
 *   This affects the previosly added bookmark
 * Separator := hr
 *   Insert a separator into the current container
 * The folder hierarchy is defined by <dl>/<ul>/<menu> (the old importing code
 *     handles all these cases, when we write, use <dl>).
 *
 * Overall design
 * --------------
 *
 * We need to emulate a recursive parser. A "Bookmark import frame" is created
 * corresponding to each folder we encounter. These are arranged in a stack,
 * and contain all the state we need to keep track of.
 *
 * A frame is created when we find a heading, which defines a new container.
 * The frame also keeps track of the nesting of <DL>s, (in well-formed
 * bookmarks files, these will have a 1-1 correspondence with frames, but we
 * try to be a little more flexible here). When the nesting count decreases
 * to 0, then we know a frame is complete and to pop back to the previous
 * frame.
 *
 * Note that a lot of things happen when tags are CLOSED because we need to
 * get the text from the content of the tag. For example, link and heading tags
 * both require the content (= title) before actually creating it.
 */

#include "nsPlacesImportExportService.h"
#include "nsNetUtil.h"
#include "nsParserCIID.h"
#include "nsStringAPI.h"
#include "nsUnicharUtils.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsIPrefService.h"
#include "nsToolkitCompsCID.h"
#include "nsIHTMLContentSink.h"
#include "nsIParser.h"
#include "prprf.h"
#include "nsVoidArray.h"
#include "nsIBrowserGlue.h"

static NS_DEFINE_CID(kParserCID, NS_PARSER_CID);

#define KEY_TOOLBARFOLDER_LOWER "personal_toolbar_folder"
#define KEY_BOOKMARKSMENU_LOWER "bookmarks_menu"
#define KEY_UNFILEDFOLDER_LOWER "unfiled_bookmarks_folder"
#define KEY_PLACESROOT_LOWER "places_root"
#define KEY_HREF_LOWER "href"
#define KEY_FEEDURL_LOWER "feedurl"
#define KEY_WEB_PANEL_LOWER "web_panel"
#define KEY_LASTCHARSET_LOWER "last_charset"
#define KEY_ICON_LOWER "icon"
#define KEY_ICON_URI_LOWER "icon_uri"
#define KEY_SHORTCUTURL_LOWER "shortcuturl"
#define KEY_POST_DATA_LOWER "post_data"
#define KEY_NAME_LOWER "name"
#define KEY_MICSUM_GEN_URI_LOWER "micsum_gen_uri"
#define KEY_DATE_ADDED_LOWER "add_date"
#define KEY_LAST_MODIFIED_LOWER "last_modified"
#define KEY_GENERATED_TITLE_LOWER "generated_title"

#define LOAD_IN_SIDEBAR_ANNO NS_LITERAL_CSTRING("bookmarkProperties/loadInSidebar")
#define DESCRIPTION_ANNO NS_LITERAL_CSTRING("bookmarkProperties/description")
#define POST_DATA_ANNO NS_LITERAL_CSTRING("bookmarkProperties/POSTData")
#define STATIC_TITLE_ANNO NS_LITERAL_CSTRING("bookmarks/staticTitle")

#define BOOKMARKS_MENU_ICON_URI "chrome://browser/skin/places/bookmarksMenu.png"

// define to get debugging messages on console about import/export
//#define DEBUG_IMPORT
//#define DEBUG_EXPORT

#if defined(XP_WIN) || defined(XP_OS2)
#define NS_LINEBREAK "\015\012"
#else
#define NS_LINEBREAK "\012"
#endif

class nsIOutputStream;
static const char kWhitespace[] = " \r\n\t\b";
static nsresult WriteEscapedUrl(const nsCString &aString, nsIOutputStream* aOutput);

class BookmarkImportFrame
{
public:
  BookmarkImportFrame(PRInt64 aID) :
      mContainerID(aID),
      mContainerNesting(0),
      mLastContainerType(Container_Normal),
      mInDescription(PR_FALSE),
      mPreviousId(0),
      mPreviousDateAdded(0),
      mPreviousLastModifiedDate(0)
  {
  }

  enum ContainerType { Container_Normal,
                       Container_Places,
                       Container_Menu,
                       Container_Toolbar,
                       Container_Unfiled};

  PRInt64 mContainerID;

  // How many <dl>s have been nested. Each frame/container should start
  // with a heading, and is then followed by a <dl>, <ul>, or <menu>. When
  // that list is complete, then it is the end of this container and we need
  // to pop back up one level for new items. If we never get an open tag for
  // one of these things, we should assume that the container is empty and
  // that things we find should be siblings of it. Normally, these <dl>s won't
  // be nested so this will be 0 or 1.
  PRInt32 mContainerNesting;

  // when we find a heading tag, it actually affects the title of the NEXT
  // container in the list. This stores that heading tag and whether it was
  // special. 'ConsumeHeading' resets this.
  ContainerType mLastContainerType;

  // this contains the text from the last begin tag until now. It is reset
  // at every begin tag. We can check it when we see a </a>, or </h3>
  // to see what the text content of that node should be.
  nsString mPreviousText;

  // true when we hit a <dd>, which contains the description for the preceeding
  // <a> tag. We can't just check for </dd> like we can for </a> or </h3>
  // because if there is a sub-folder, it is actually a child of the <dd>
  // because the tag is never explicitly closed. If this is true and we see a
  // new open tag, that means to commit the description to the previous
  // bookmark.
  //
  // Additional weirdness happens when the previous <dt> tag contains a <h3>:
  // this means there is a new folder with the given description, and whose
  // children are contained in the following <dl> list.
  //
  // This is handled in OpenContainer(), which commits previous text if
  // necessary.
  PRBool mInDescription;

  // contains the URL of the previous bookmark created. This is used so that
  // when we encounter a <dd>, we know what bookmark to associate the text with.
  // This is cleared whenever we hit a <h3>, so that we know NOT to save this
  // with a bookmark, but to keep it until 
  nsCOMPtr<nsIURI> mPreviousLink;

  // contains the URL of the previous livemark, so that when the link ends,
  // and the livemark title is known, we can create it.
  nsCOMPtr<nsIURI> mPreviousFeed;

  // contains the text content of the previous microsummary, so that when the
  // link ends, we can replace the bookmark's title with it and store the user's
  // title in the staticTitle annotation.
  nsString mPreviousMicrosummaryText;

  nsCOMPtr<nsIMicrosummary> mPreviousMicrosummary;

  void ConsumeHeading(nsAString* aHeading, ContainerType* aContainerType)
  {
    *aHeading = mPreviousText;
    *aContainerType = mLastContainerType;
    mPreviousText.Truncate();
  }

  // Contains the id of an imported, or newly created bookmark.
  PRInt64 mPreviousId;

  // Contains the date-added and last-modified-date of an imported item.
  // Used to override the values set by insertBookmark, createFolder, etc.
  PRTime mPreviousDateAdded;
  PRTime mPreviousLastModifiedDate;
};

/**
 * copied from nsEscape.cpp, which requires internal string API
 */
char *
nsEscapeHTML(const char * string)
{
    /* XXX Hardcoded max entity len. The +1 is for the trailing null. */
    char *rv = nsnull;
    PRUint32 len = strlen(string);
    if (len >= (PR_UINT32_MAX / 6))
      return nsnull;

    rv = (char *) NS_Alloc((len * 6) + 1);
    char *ptr = rv;

    if(rv)
      {
        for(; *string != '\0'; string++)
          {
            if(*string == '<')
              {
                *ptr++ = '&';
                *ptr++ = 'l';
                *ptr++ = 't';
                *ptr++ = ';';
              }
            else if(*string == '>')
              {
                *ptr++ = '&';
                *ptr++ = 'g';
                *ptr++ = 't';
                *ptr++ = ';';
              }
            else if(*string == '&')
              {
                *ptr++ = '&';
                *ptr++ = 'a';
                *ptr++ = 'm';
                *ptr++ = 'p';
                *ptr++ = ';';
              }
            else if (*string == '"')
              {
                *ptr++ = '&';
                *ptr++ = 'q';
                *ptr++ = 'u';
                *ptr++ = 'o';
                *ptr++ = 't';
                *ptr++ = ';';
              }            
            else if (*string == '\'')
              {
                *ptr++ = '&';
                *ptr++ = '#';
                *ptr++ = '3';
                *ptr++ = '9';
                *ptr++ = ';';
              }
            else
              {
                *ptr++ = *string;
              }
          }
        *ptr = '\0';
      }

    return(rv);
}

NS_IMPL_ISUPPORTS2(nsPlacesImportExportService, nsIPlacesImportExportService,
                   nsINavHistoryBatchCallback)

nsPlacesImportExportService::nsPlacesImportExportService()
{
  nsresult rv;
  mHistoryService = do_GetService(NS_NAVHISTORYSERVICE_CONTRACTID, &rv);
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "could not get history service");
  mFaviconService = do_GetService(NS_FAVICONSERVICE_CONTRACTID, &rv);
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "could not get favicon service");
  mAnnotationService = do_GetService(NS_ANNOTATIONSERVICE_CONTRACTID, &rv);
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "could not get annotation service");
  mBookmarksService = do_GetService(NS_NAVBOOKMARKSSERVICE_CONTRACTID, &rv);
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "could not get bookmarks service");
  mLivemarkService = do_GetService(NS_LIVEMARKSERVICE_CONTRACTID, &rv);
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "could not get livemark service");
  mMicrosummaryService = do_GetService("@mozilla.org/microsummary/service;1", &rv);
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "could not get microsummary service");
}

nsPlacesImportExportService::~nsPlacesImportExportService()
{
}

/**
 * The content sink stuff is based loosely on 
 */
class BookmarkContentSink : public nsIHTMLContentSink
{
public:
  BookmarkContentSink();

  nsresult Init(PRBool aAllowRootChanges,
                nsINavBookmarksService* bookmarkService,
                PRInt64 aFolder,
                PRBool aIsImportDefaults);

  NS_DECL_ISUPPORTS

  // nsIContentSink (superclass of nsIHTMLContentSink)
  NS_IMETHOD WillParse() { return NS_OK; }
  NS_IMETHOD WillBuildModel() { return NS_OK; }
  NS_IMETHOD DidBuildModel() { return NS_OK; }
  NS_IMETHOD WillInterrupt() { return NS_OK; }
  NS_IMETHOD WillResume() { return NS_OK; }
  NS_IMETHOD SetParser(nsIParser* aParser) { return NS_OK; }
  virtual void FlushPendingNotifications(mozFlushType aType) { }
  NS_IMETHOD SetDocumentCharset(nsACString& aCharset) { return NS_OK; }
  virtual nsISupports *GetTarget() { return nsnull; }

  // nsIHTMLContentSink
  NS_IMETHOD OpenHead() { return NS_OK; }
  NS_IMETHOD BeginContext(PRInt32 aPosition) { return NS_OK; }
  NS_IMETHOD EndContext(PRInt32 aPosition) { return NS_OK; }
  NS_IMETHOD IsEnabled(PRInt32 aTag, PRBool* aReturn)
    { *aReturn = PR_TRUE; return NS_OK; }
  NS_IMETHOD DidProcessTokens() { return NS_OK; }
  NS_IMETHOD WillProcessAToken() { return NS_OK; }
  NS_IMETHOD DidProcessAToken() { return NS_OK; }
  NS_IMETHOD OpenContainer(const nsIParserNode& aNode);
  NS_IMETHOD CloseContainer(const nsHTMLTag aTag);
  NS_IMETHOD AddLeaf(const nsIParserNode& aNode);
  NS_IMETHOD AddComment(const nsIParserNode& aNode) { return NS_OK; }
  NS_IMETHOD AddProcessingInstruction(const nsIParserNode& aNode) { return NS_OK; }
  NS_IMETHOD AddDocTypeDecl(const nsIParserNode& aNode) { return NS_OK; }
  NS_IMETHOD NotifyTagObservers(nsIParserNode* aNode) { return NS_OK; }
  NS_IMETHOD_(PRBool) IsFormOnStack() { return PR_FALSE; }

protected:
  nsCOMPtr<nsINavBookmarksService> mBookmarksService;
  nsCOMPtr<nsINavHistoryService> mHistoryService;
  nsCOMPtr<nsIAnnotationService> mAnnotationService;
  nsCOMPtr<nsILivemarkService> mLivemarkService;
  nsCOMPtr<nsIMicrosummaryService> mMicrosummaryService;

  // If set, we will move root items to from their existing position
  // in the hierarchy, to where we find them in the bookmarks file
  // being imported. This should be set when we are loading 
  // the default places html file, and should be unset when doing
  // normal imports so that root folders will not get moved when
  // importing bookmarks.html files.
  PRBool mAllowRootChanges;

  // If set, this is an import of initial bookmarks.html content,
  // so we don't want to kick off HTTP traffic
  // and we want the imported personal toolbar folder
  // to be set as the personal toolbar folder. (If not set
  // we will treat it as a normal folder.)
  PRBool mIsImportDefaults;

  // If a folder was specified to import into, then ignore flags to put
  // bookmarks in the bookmarks menu or toolbar and keep them inside
  // the folder.
  PRBool mFolderSpecified;

  void HandleContainerBegin(const nsIParserNode& node);
  void HandleContainerEnd();
  void HandleHead1Begin(const nsIParserNode& node);
  void HandleHeadBegin(const nsIParserNode& node);
  void HandleHeadEnd();
  void HandleLinkBegin(const nsIParserNode& node);
  void HandleLinkEnd();
  void HandleSeparator(const nsIParserNode& node);

  // This is a list of frames. We really want a recursive parser, but the HTML
  // parser gives us tags as a stream. This implements all the state on a stack
  // so we can get the recursive information we need. Use "CurFrame" to get the
  // top "stack frame" with the current state in it.
  nsTArray<BookmarkImportFrame> mFrames;
  BookmarkImportFrame& CurFrame()
  {
    NS_ASSERTION(mFrames.Length() > 0, "Asking for frame when there are none!");
    return mFrames[mFrames.Length() - 1];
  }
  BookmarkImportFrame& PreviousFrame()
  {
    NS_ASSERTION(mFrames.Length() > 1, "Asking for frame when there are not enough!");
    return mFrames[mFrames.Length() - 2];
  }
  nsresult NewFrame();
  nsresult PopFrame();

  nsresult SetFaviconForURI(nsIURI* aPageURI, nsIURI* aFaviconURI,
                            const nsString& aData);

  PRInt64 ConvertImportedIdToInternalId(const nsCString& aId);
  PRTime ConvertImportedDateToInternalDate(const nsACString& aDate);

#ifdef DEBUG_IMPORT
  // prints spaces for indenting to the current frame depth
  void PrintNesting()
  {
    for (PRUint32 i = 0; i < mFrames.Length(); i ++)
      printf("  ");
  }
#endif
};

BookmarkContentSink::BookmarkContentSink() : mFrames(16)
{
}

// BookmarkContentSink::Init
//
//    Note that the bookmark service pointer is passed in. We can not create
//    the bookmark service from here because this can be called from bookmark
//    service creation, making a weird reentrant loop.

nsresult
BookmarkContentSink::Init(PRBool aAllowRootChanges,
                          nsINavBookmarksService* bookmarkService,
                          PRInt64 aFolder,
                          PRBool aIsImportDefaults)
{
  nsresult rv;
  mBookmarksService = bookmarkService;
  mHistoryService = do_GetService(NS_NAVHISTORYSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  mAnnotationService = do_GetService(NS_ANNOTATIONSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  mLivemarkService = do_GetService(NS_LIVEMARKSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  mMicrosummaryService = do_GetService("@mozilla.org/microsummary/service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  mAllowRootChanges = aAllowRootChanges;
  mIsImportDefaults = aIsImportDefaults;

  // initialize the root frame with the menu root
  PRInt64 menuRoot;
  if (aFolder == 0) {
    rv = mBookmarksService->GetBookmarksMenuFolder(&menuRoot);
    NS_ENSURE_SUCCESS(rv, rv);
    mFolderSpecified = false;
  }
  else {
    menuRoot = aFolder;
    mFolderSpecified = true;
  }
  if (!mFrames.AppendElement(BookmarkImportFrame(menuRoot)))
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}


NS_IMPL_ISUPPORTS2(BookmarkContentSink,
                   nsIContentSink,
                   nsIHTMLContentSink)

// nsIContentSink **************************************************************

NS_IMETHODIMP
BookmarkContentSink::OpenContainer(const nsIParserNode& aNode)
{
  switch(aNode.GetNodeType()) {
    case eHTMLTag_h1:
      HandleHead1Begin(aNode);
      break;
    case eHTMLTag_h2:
    case eHTMLTag_h3:
    case eHTMLTag_h4:
    case eHTMLTag_h5:
    case eHTMLTag_h6:
      HandleHeadBegin(aNode);
      break;
    case eHTMLTag_a:
      HandleLinkBegin(aNode);
      break;
    case eHTMLTag_dl:
    case eHTMLTag_ul:
    case eHTMLTag_menu:
      HandleContainerBegin(aNode);
      break;
    case eHTMLTag_dd:
      CurFrame().mInDescription = PR_TRUE;
      break;
  }
  return NS_OK;
}

NS_IMETHODIMP
BookmarkContentSink::CloseContainer(const nsHTMLTag aTag)
{
  BookmarkImportFrame& frame = CurFrame();

  // see the comment for the definition of mInDescription. Basically, we commit
  // any text in mPreviousText to the description of the node/folder if there
  // is any.
  if (frame.mInDescription) {
    frame.mPreviousText.Trim(kWhitespace); // important!
    if (!frame.mPreviousText.IsEmpty()) {

      PRInt64 itemId = !frame.mPreviousLink ?
                       frame.mContainerID : frame.mPreviousId;
                    
      PRBool hasDescription = PR_FALSE;
      nsresult rv = mAnnotationService->ItemHasAnnotation(itemId,
                                                          DESCRIPTION_ANNO,
                                                          &hasDescription);
      if (NS_SUCCEEDED(rv) && !hasDescription) {
        mAnnotationService->SetItemAnnotationString(itemId, DESCRIPTION_ANNO,
                                                    frame.mPreviousText, 0,
                                                    nsIAnnotationService::EXPIRE_NEVER);
      }
      frame.mPreviousText.Truncate();

      // Set last-modified a 2nd time for all items with descriptions
      // we need to set last-modified as the *last* step in processing 
      // any item type in the bookmarks.html file, so that we do
      // not overwrite the imported value. for items without descriptions, 
      // setting this value after setting the item title is that 
      // last point at which we can save this value before it gets reset.
      // for items with descriptions, it must set after that point.
      // however, at the point at which we set the title, there's no way 
      // to determine if there will be a description following, 
      // so we need to set the last-modified-date at both places.

      PRTime lastModified;
      if (!frame.mPreviousLink) {
        lastModified = PreviousFrame().mPreviousLastModifiedDate;
      } else {
        lastModified = frame.mPreviousLastModifiedDate;
      }

      if (itemId > 0 && lastModified > 0) {
        rv = mBookmarksService->SetItemLastModified(itemId, lastModified);
        NS_ASSERTION(NS_SUCCEEDED(rv), "SetItemLastModified failed");
      }
    }
    frame.mInDescription = PR_FALSE;
  }

  switch (aTag) {
    case eHTMLTag_dl:
    case eHTMLTag_ul:
    case eHTMLTag_menu:
      HandleContainerEnd();
      break;
    case eHTMLTag_dt:
      break;
    case eHTMLTag_h1:
      // ignore
      break;
    case eHTMLTag_h2:
    case eHTMLTag_h3:
    case eHTMLTag_h4:
    case eHTMLTag_h5:
    case eHTMLTag_h6:
      HandleHeadEnd();
      break;
    case eHTMLTag_a:
      HandleLinkEnd();
      break;
    default:
      break;
  }
  return NS_OK;
}


// BookmarkContentSink::AddLeaf
//
//    XXX on the branch, we should be calling CollectSkippedContent as in
//    nsHTMLFragmentContentSink.cpp:AddLeaf when we encounter title, script,
//    style, or server tags. Apparently if we don't, we'll leak the next DOM
//    node. However, this requires that we keep a reference to the parser we'll
//    introduce a circular reference because it has a reference to us.
//
//    This is annoying to fix and these elements are not allowed in bookmarks
//    files anyway. So if somebody tries to import a crazy bookmarks file, it
//    will leak a little bit.

NS_IMETHODIMP
BookmarkContentSink::AddLeaf(const nsIParserNode& aNode)
{
  switch (aNode.GetNodeType()) {
  case eHTMLTag_text:
    // save any text we find
    CurFrame().mPreviousText += aNode.GetText();
    break;
  case eHTMLTag_entity: {
    nsAutoString tmp;
    PRInt32 unicode = aNode.TranslateToUnicodeStr(tmp);
    if (unicode < 0) {
      // invalid entity - just use the text of it
      CurFrame().mPreviousText += aNode.GetText();
    } else {
      CurFrame().mPreviousText.Append(unicode);
    }
    break;
  }
  case eHTMLTag_whitespace:
    CurFrame().mPreviousText.Append(PRUnichar(' '));
    break;
  case eHTMLTag_hr:
    HandleSeparator(aNode);
    break;
  }

  return NS_OK;
}

// BookmarkContentSink::HandleContainerBegin

void
BookmarkContentSink::HandleContainerBegin(const nsIParserNode& node)
{
  CurFrame().mContainerNesting ++;
}


// BookmarkContentSink::HandleContainerEnd
//
//    Our "indent" count has decreased, and when we hit 0 that means that this
//    container is complete and we need to pop back to the outer frame. Never
//    pop the toplevel frame

void
BookmarkContentSink::HandleContainerEnd()
{
  BookmarkImportFrame& frame = CurFrame();
  if (frame.mContainerNesting > 0)
    frame.mContainerNesting --;
  if (mFrames.Length() > 1 && frame.mContainerNesting == 0) {
    // we also need to re-set the imported last-modified date here. Otherwise
    // the addition of items will override the imported field.
    BookmarkImportFrame& prevFrame = PreviousFrame();
    if (prevFrame.mPreviousLastModifiedDate > 0) {
      nsresult rv = mBookmarksService->SetItemLastModified(frame.mContainerID,
                                                           prevFrame.mPreviousLastModifiedDate);
      NS_ASSERTION(NS_SUCCEEDED(rv), "SetItemLastModified failed");
    }
    PopFrame();
  }
}


// BookmarkContentSink::HandleHead1Begin
//
//    Handles <H1>. We check for the attribute PLACES_ROOT and reset the
//    container id if it's found. Otherwise, the default bookmark menu
//    root is assumed and imported things will go into the bookmarks menu.

void
BookmarkContentSink::HandleHead1Begin(const nsIParserNode& node)
{
  PRInt32 attrCount = node.GetAttributeCount();
  for (PRInt32 i = 0; i < attrCount; i ++) {
    if (node.GetKeyAt(i).LowerCaseEqualsLiteral(KEY_PLACESROOT_LOWER)) {
      if (mFrames.Length() > 1) {
        NS_WARNING("Trying to set the places root from the middle of the hierarchy. "
                   "This can only be set at the beginning.");
        return;
      }

      PRInt64 placesRoot;
      nsresult rv = mBookmarksService->GetPlacesRoot(&placesRoot);
      NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "could not get placesRoot");
      CurFrame().mContainerID = placesRoot;
      break;
    }
  }
}


// BookmarkContentSink::HandleHeadBegin
//
//    Called for h2,h3,h4,h5,h6. This just stores the correct information in
//    the current frame; the actual new frame corresponding to the container
//    associated with the heading will be created when the tag has been closed
//    and we know the title (we don't know to create a new folder or to merge
//    with an existing one until we have the title).

void
BookmarkContentSink::HandleHeadBegin(const nsIParserNode& node)
{
  BookmarkImportFrame& frame = CurFrame();

  // after a heading, a previous bookmark is not applicable (for example, for
  // the descriptions contained in a <dd>). Neither is any previous head type
  frame.mPreviousLink = nsnull;
  frame.mLastContainerType = BookmarkImportFrame::Container_Normal;

  // It is syntactically possible for a heading to appear after another heading
  // but before the <dl> that encloses that folder's contents.  This should not
  // happen in practice, as the file will contain "<dl></dl>" sequence for
  // empty containers.
  //
  // Just to be on the safe side, if we encounter
  //   <h3>FOO</h3>
  //   <h3>BAR</h3>
  //   <dl>...content 1...</dl>
  //   <dl>...content 2...</dl>
  // we'll pop the stack when we find the h3 for BAR, treating that as an
  // implicit ending of the FOO container. The output will be FOO and BAR as
  // siblings. If there's another <dl> following (as in "content 2"), those
  // items will be treated as further siblings of FOO and BAR
  if (frame.mContainerNesting == 0)
    PopFrame();

  // We have to check for some attributes to see if this is a "special"
  // folder, which will have different creation rules when the end tag is
  // processed.
  PRInt32 attrCount = node.GetAttributeCount();
  frame.mLastContainerType = BookmarkImportFrame::Container_Normal;
  if (!mFolderSpecified) {
    for (PRInt32 i = 0; i < attrCount; i ++) {
      if (node.GetKeyAt(i).LowerCaseEqualsLiteral(KEY_TOOLBARFOLDER_LOWER)) {
        if (mIsImportDefaults)
          frame.mLastContainerType = BookmarkImportFrame::Container_Toolbar;
        break;
      } else if (node.GetKeyAt(i).LowerCaseEqualsLiteral(KEY_BOOKMARKSMENU_LOWER)) {
        if (mIsImportDefaults)
          frame.mLastContainerType = BookmarkImportFrame::Container_Menu;
        break;
      } else if (node.GetKeyAt(i).LowerCaseEqualsLiteral(KEY_UNFILEDFOLDER_LOWER)) {
        if (mIsImportDefaults)
          frame.mLastContainerType = BookmarkImportFrame::Container_Unfiled;
        break;
      } else if (node.GetKeyAt(i).LowerCaseEqualsLiteral(KEY_PLACESROOT_LOWER)) {
        if (mIsImportDefaults)
          frame.mLastContainerType = BookmarkImportFrame::Container_Places;
        break;
      } else if (node.GetKeyAt(i).LowerCaseEqualsLiteral(KEY_DATE_ADDED_LOWER)) {
        frame.mPreviousDateAdded =
          ConvertImportedDateToInternalDate(NS_ConvertUTF16toUTF8(node.GetValueAt(i)));
      } else if (node.GetKeyAt(i).LowerCaseEqualsLiteral(KEY_LAST_MODIFIED_LOWER)) {
        frame.mPreviousLastModifiedDate =
          ConvertImportedDateToInternalDate(NS_ConvertUTF16toUTF8(node.GetValueAt(i)));
      }
    }
  }
  CurFrame().mPreviousText.Truncate();
}


// BookmarkContentSink::HandleHeadEnd
//
//    Creates the new frame for this heading now that we know the name of the
//    container (tokens since the heading open tag will have been placed in
//    mPreviousText).

void
BookmarkContentSink::HandleHeadEnd()
{
  NewFrame();
}


// BookmarkContentSink::HandleLinkBegin
//
//    Handles "<a" tags by creating a new bookmark. The title of the bookmark
//    will be the text content, which will be stuffed in mPreviousText for us
//    and which will be saved by HandleLinkEnd

void
BookmarkContentSink::HandleLinkBegin(const nsIParserNode& node)
{
  nsresult rv;

  BookmarkImportFrame& frame = CurFrame();

  // We need to make sure that the feed URIs from previous frames are emptied. 
  frame.mPreviousFeed = nsnull;

  // We need to make sure that the bookmark id from previous frames are emptied. 
  frame.mPreviousId = 0;

  // mPreviousText will hold our link text, clear it so that can be appended to
  frame.mPreviousText.Truncate();

  // Empty our microsummary items from the previous frame.
  frame.mPreviousMicrosummary = nsnull;
  frame.mPreviousMicrosummaryText.Truncate();
  
  // get the attributes we care about
  nsAutoString href;
  nsAutoString feedUrl;
  nsAutoString icon;
  nsAutoString iconUri;
  nsAutoString lastCharset;
  nsAutoString keyword;
  nsAutoString postData;
  nsAutoString webPanel;
  nsAutoString itemId;
  nsAutoString micsumGenURI;
  nsAutoString generatedTitle;
  nsAutoString dateAdded;
  nsAutoString lastModified;

  PRInt32 attrCount = node.GetAttributeCount();
  for (PRInt32 i = 0; i < attrCount; i ++) {
    const nsAString& key = node.GetKeyAt(i);
    if (key.LowerCaseEqualsLiteral(KEY_HREF_LOWER)) {
      href = node.GetValueAt(i);
    } else if (key.LowerCaseEqualsLiteral(KEY_FEEDURL_LOWER)) {
      feedUrl = node.GetValueAt(i);
    } else if (key.LowerCaseEqualsLiteral(KEY_ICON_LOWER)) {
      icon = node.GetValueAt(i);
    } else if (key.LowerCaseEqualsLiteral(KEY_ICON_URI_LOWER)) {
      iconUri = node.GetValueAt(i);
    } else if (key.LowerCaseEqualsLiteral(KEY_LASTCHARSET_LOWER)) {
      lastCharset = node.GetValueAt(i);
    } else if (key.LowerCaseEqualsLiteral(KEY_SHORTCUTURL_LOWER)) {
      keyword = node.GetValueAt(i);
    } else if (key.LowerCaseEqualsLiteral(KEY_POST_DATA_LOWER)) {
      postData = node.GetValueAt(i);
    } else if (key.LowerCaseEqualsLiteral(KEY_WEB_PANEL_LOWER)) {
      webPanel = node.GetValueAt(i);
    } else if (key.LowerCaseEqualsLiteral(KEY_MICSUM_GEN_URI_LOWER)) {
      micsumGenURI = node.GetValueAt(i);
    } else if (key.LowerCaseEqualsLiteral(KEY_GENERATED_TITLE_LOWER)) {
      generatedTitle = node.GetValueAt(i);
    } else if (key.LowerCaseEqualsLiteral(KEY_DATE_ADDED_LOWER)) {
      dateAdded = node.GetValueAt(i);
    } else if (key.LowerCaseEqualsLiteral(KEY_LAST_MODIFIED_LOWER)) {
      lastModified = node.GetValueAt(i);
    }
  }
  href.Trim(kWhitespace);
  feedUrl.Trim(kWhitespace);
  icon.Trim(kWhitespace);
  iconUri.Trim(kWhitespace);
  lastCharset.Trim(kWhitespace);
  keyword.Trim(kWhitespace);
  postData.Trim(kWhitespace);
  webPanel.Trim(kWhitespace);
  itemId.Trim(kWhitespace);
  micsumGenURI.Trim(kWhitespace);
  generatedTitle.Trim(kWhitespace);
  dateAdded.Trim(kWhitespace);
  lastModified.Trim(kWhitespace);

  // For feeds, get the feed URL. If it is invalid, it will leave mPreviousFeed
  // NULL and we'll continue trying to create it as a normal bookmark.
  if (!feedUrl.IsEmpty()) {
    NS_NewURI(getter_AddRefs(frame.mPreviousFeed),
              NS_ConvertUTF16toUTF8(feedUrl), nsnull);
  }

  // Ignore <a> tags that have no href: we don't know what to do with them.
  if (href.IsEmpty()) {
    frame.mPreviousLink = nsnull;

    // The exception is for feeds, where the href is an optional component
    // indicating the source web site.
    if (!frame.mPreviousFeed)
      return;
  } else {
    // Save this so the link text and descriptions can be associated with it.
    // Note that we ignore errors if this is a feed: URLs aren't strictly
    // necessary in these cases.
    nsresult rv = NS_NewURI(getter_AddRefs(frame.mPreviousLink),
                   href, nsnull);
    if (NS_FAILED(rv) && !frame.mPreviousFeed) {
      frame.mPreviousLink = nsnull;
      return; // invalid link
    }
  }

  // if there's a pre-existing Places bookmark ITEM_ID, use it
  frame.mPreviousId = ConvertImportedIdToInternalId(NS_ConvertUTF16toUTF8(itemId));

  // Save last-modified-date, for setting after all the bookmark properties have been set.
  if (!lastModified.IsEmpty()) {
    frame.mPreviousLastModifiedDate = ConvertImportedDateToInternalDate(NS_ConvertUTF16toUTF8(lastModified));
  }

  // if there is a feedURL, this is a livemark, which is a special case
  // that we handle in HandleLinkEnd(): don't create normal bookmarks
  if (frame.mPreviousFeed)
    return;

  // attempt to get a property for the supposedly pre-existing bookmark
  PRInt64 parent;
  if (frame.mPreviousId > 0) {
    rv = mBookmarksService->GetFolderIdForItem(frame.mPreviousId, &parent);
    if (NS_FAILED(rv) || frame.mContainerID != parent)
      frame.mPreviousId = 0;
  }

  // if no previous id (or a legacy id), create a new bookmark
  if (frame.mPreviousId == 0) {
    // create the bookmark
    rv = mBookmarksService->InsertBookmark(frame.mContainerID,
                                           frame.mPreviousLink,
                                           mBookmarksService->DEFAULT_INDEX,
                                           EmptyCString(),
                                           &frame.mPreviousId);
    NS_ASSERTION(NS_SUCCEEDED(rv), "InsertBookmark failed");

    // set the date added value, if we have it
    // important:  this has to happen after InsertBookmark
    // so that we set the imported value
    if (!dateAdded.IsEmpty()) {
      PRTime convertedDateAdded = ConvertImportedDateToInternalDate(NS_ConvertUTF16toUTF8(dateAdded));
      if (convertedDateAdded) {
        rv = mBookmarksService->SetItemDateAdded(frame.mPreviousId, convertedDateAdded);
        NS_ASSERTION(NS_SUCCEEDED(rv), "SetItemDateAdded failed");
      }
    }
  }

  // save the favicon, ignore errors
  if (!icon.IsEmpty() || !iconUri.IsEmpty()) {
    nsCOMPtr<nsIURI> iconUriObject;
    NS_NewURI(getter_AddRefs(iconUriObject), iconUri);
    if (!icon.IsEmpty() || iconUriObject) {
      rv = SetFaviconForURI(frame.mPreviousLink, iconUriObject, icon);
    }
  }

  // save the keyword, ignore errors
  if (!keyword.IsEmpty()) {
    mBookmarksService->SetKeywordForBookmark(frame.mPreviousId, keyword);

    // post data
    if (!postData.IsEmpty()) {
      mAnnotationService->SetItemAnnotationString(frame.mPreviousId, POST_DATA_ANNO,
                                                  postData, 0,
                                                  nsIAnnotationService::EXPIRE_NEVER);
    }
  }

  if (webPanel.LowerCaseEqualsLiteral("true")) {
    // set load-in-sidebar annotation for the bookmark
    mAnnotationService->SetItemAnnotationInt32(frame.mPreviousId, LOAD_IN_SIDEBAR_ANNO,
                                               1, 0,
                                               nsIAnnotationService::EXPIRE_NEVER);
  }

  // import microsummary
  if (!micsumGenURI.IsEmpty()) {
    nsCOMPtr<nsIURI> micsumGenURIObject;
    if (NS_SUCCEEDED(NS_NewURI(getter_AddRefs(micsumGenURIObject), micsumGenURI))) {
      mMicrosummaryService->CreateMicrosummary(frame.mPreviousLink, micsumGenURIObject,
                                               getter_AddRefs(frame.mPreviousMicrosummary));
      frame.mPreviousMicrosummaryText = generatedTitle;
    }
  }

  // import last charset
  if (!lastCharset.IsEmpty()) {
    rv = mHistoryService->SetCharsetForURI(frame.mPreviousLink,lastCharset);
    NS_ASSERTION(NS_SUCCEEDED(rv), "setCharsetForURI failed");
  }
}


// BookmarkContentSink::HandleLinkEnd
//
//    Saves the title for the given bookmark. This only writes the user title.
//    Any previous title will be untouched. If this is a new entry, it will have
//    an empty "official" title until you visit it.

void
BookmarkContentSink::HandleLinkEnd()
{
  nsresult rv;
  BookmarkImportFrame& frame = CurFrame();
  frame.mPreviousText.Trim(kWhitespace);
  if (frame.mPreviousFeed) {
    // The bookmark is actually a livemark.  Create it here.
    // (It gets created here instead of in HandleLinkBegin()
    // because we need to know the title before creating it.)

    // check id validity
    if (frame.mPreviousId > 0) {
      PRInt64 parent;
      nsresult rv = mBookmarksService->GetFolderIdForItem(frame.mPreviousId, &parent);
      if (NS_FAILED(rv) || parent != frame.mContainerID) {
        frame.mPreviousId = 0;
      }
    }

    PRBool isLivemark = PR_FALSE;
    if (frame.mPreviousId > 0) {
      mLivemarkService->IsLivemark(frame.mPreviousId, &isLivemark);
      if (isLivemark) {
        // It's a pre-existing livemark, so update its properties
#ifdef DEBUG_IMPORT
        PrintNesting();
        printf("Updating livemark '%s' %lld\n",
               NS_ConvertUTF16toUTF8(frame.mPreviousText).get(), frame.mPreviousId);
#endif
        rv = mLivemarkService->SetSiteURI(frame.mPreviousId, frame.mPreviousLink);
        NS_ASSERTION(NS_SUCCEEDED(rv), "SetSiteURI failed!");
        rv = mLivemarkService->SetFeedURI(frame.mPreviousId, frame.mPreviousFeed);
        NS_ASSERTION(NS_SUCCEEDED(rv), "SetFeedURI failed!");
        rv = mBookmarksService->SetItemTitle(frame.mPreviousId, NS_ConvertUTF16toUTF8(frame.mPreviousText));
        NS_ASSERTION(NS_SUCCEEDED(rv), "SetItemTitle failed!");
      }
    }

    if (!isLivemark) {
      if (mIsImportDefaults) {
        rv = mLivemarkService->CreateLivemarkFolderOnly(frame.mContainerID,
                                                        frame.mPreviousText,
                                                        frame.mPreviousLink,
                                                        frame.mPreviousFeed,
                                                        -1,
                                                        &frame.mPreviousId);
        NS_ASSERTION(NS_SUCCEEDED(rv), "CreateLivemarkFolderOnly failed!");
      } else {
        rv = mLivemarkService->CreateLivemark(frame.mContainerID,
                                         frame.mPreviousText,
                                         frame.mPreviousLink,
                                         frame.mPreviousFeed,
                                         -1,
                                         &frame.mPreviousId);
        NS_ASSERTION(NS_SUCCEEDED(rv), "CreateLivemark failed!");
      }
#ifdef DEBUG_IMPORT
      PrintNesting();
      printf("Creating livemark '%s' %lld\n",
             NS_ConvertUTF16toUTF8(frame.mPreviousText).get(), frame.mPreviousId);
#endif
    }
  }
  else if (frame.mPreviousLink) {
#ifdef DEBUG_IMPORT
    PrintNesting();
    printf("Creating bookmark '%s' %lld\n",
           NS_ConvertUTF16toUTF8(frame.mPreviousText).get(), frame.mPreviousId);
#endif
    if (frame.mPreviousMicrosummary) {
      rv = mAnnotationService->SetItemAnnotationString(frame.mPreviousId, STATIC_TITLE_ANNO,
                                                       frame.mPreviousText, 0,
                                                       nsIAnnotationService::EXPIRE_NEVER);
      NS_ASSERTION(NS_SUCCEEDED(rv), "Could not store user's bookmark title!");

      mBookmarksService->SetItemTitle(frame.mPreviousId, NS_ConvertUTF16toUTF8(frame.mPreviousMicrosummaryText));
      mMicrosummaryService->SetMicrosummary(frame.mPreviousId, frame.mPreviousMicrosummary);
    }
    else
      mBookmarksService->SetItemTitle(frame.mPreviousId, NS_ConvertUTF16toUTF8(frame.mPreviousText));
  }

  // Set last-modified-date for bookmarks and livemarks here so that the
  // imported date overrides the date from the call to that sets the description
  // that we made above.
  if (frame.mPreviousId > 0 && frame.mPreviousLastModifiedDate > 0) {
    rv = mBookmarksService->SetItemLastModified(frame.mPreviousId, frame.mPreviousLastModifiedDate);
    NS_ASSERTION(NS_SUCCEEDED(rv), "SetItemLastModified failed");
    // Note: don't clear mPreviousLastModifiedDate, because if this item has a
    // description, we'll need to set it again.
  }

  frame.mPreviousText.Truncate();
}


// BookmarkContentSink::HandleSeparator
//
//    Inserts a separator into the current container
void
BookmarkContentSink::HandleSeparator(const nsIParserNode& aNode)
{
  BookmarkImportFrame& frame = CurFrame();

  // create the separator

#ifdef DEBUG_IMPORT
  PrintNesting();
  printf("--------\n");
#endif

  mBookmarksService->InsertSeparator(frame.mContainerID,
                                     mBookmarksService->DEFAULT_INDEX,
                                     &frame.mPreviousId);
  // Import separator title if set
  nsAutoString name;
  PRInt32 attrCount = aNode.GetAttributeCount();
  for (PRInt32 i = 0; i < attrCount; i ++) {
    const nsAString& key = aNode.GetKeyAt(i);
    if (key.LowerCaseEqualsLiteral(KEY_NAME_LOWER))
      name = aNode.GetValueAt(i);
  }
  name.Trim(kWhitespace);

  if (!name.IsEmpty())
    mBookmarksService->SetItemTitle(frame.mPreviousId, NS_ConvertUTF16toUTF8(name));

  // Note: we do not need to import ADD_DATE or LAST_MODIFIED for separators
  // because pre-Places bookmarks does not support them.
  // and we can't write them out because attributes other than NAME
  // will make Firefox 2.x crash/hang - see bug #381129
}


// BookmarkContentSink::NewFrame
//
//    This is called when there is a new folder found. The folder takes the
//    name from the previous frame's heading.

nsresult
BookmarkContentSink::NewFrame()
{
  nsresult rv;

  PRInt64 ourID = 0;
  nsString containerName;
  BookmarkImportFrame::ContainerType containerType;
  BookmarkImportFrame& frame = CurFrame();
  frame.ConsumeHeading(&containerName, &containerType);

  PRBool updateFolder = PR_FALSE;
  
  switch (containerType) {
    case BookmarkImportFrame::Container_Normal:
      // append a new folder
      rv = mBookmarksService->CreateFolder(CurFrame().mContainerID,
                                           NS_ConvertUTF16toUTF8(containerName),
                                           mBookmarksService->DEFAULT_INDEX, 
                                           &ourID);
      NS_ENSURE_SUCCESS(rv, rv);
      break;
    case BookmarkImportFrame::Container_Places:
      // places root, never reparent here, when we're building the initial
      // hierarchy, it will only be defined at the top level
      rv = mBookmarksService->GetPlacesRoot(&ourID);
      NS_ENSURE_SUCCESS(rv, rv);
      break;
    case BookmarkImportFrame::Container_Menu:
      // menu folder
      rv = mBookmarksService->GetBookmarksMenuFolder(&ourID);
      NS_ENSURE_SUCCESS(rv, rv);
      if (mAllowRootChanges)
        updateFolder = PR_TRUE;
      break;
    case BookmarkImportFrame::Container_Unfiled:
      // unfiled bookmarks folder
      rv = mBookmarksService->GetUnfiledBookmarksFolder(&ourID);
      NS_ENSURE_SUCCESS(rv, rv);
      if (mAllowRootChanges)
        updateFolder = PR_TRUE;
      break;
    case BookmarkImportFrame::Container_Toolbar:
      // get toolbar folder
      rv = mBookmarksService->GetToolbarFolder(&ourID);
      NS_ENSURE_SUCCESS(rv, rv);
      
      // In Fx2, the toolbar folder is a child of the bookmarks menu, listed
      // between two separators:
      // 1) Get Bookmarks Addons 2) Separator 3) Bookmarks Toolbar Folder
      // 4) Separator 5) Mozilla Firefox Folder
      // In Places, the toolbar folder is a direct child of the places root,
      // meaning that we end up with two sequential separators.
      if (frame.mPreviousId > 0) {
        PRUint16 itemType;
        rv = mBookmarksService->GetItemType(frame.mPreviousId, &itemType);
        NS_ENSURE_SUCCESS(rv, rv);
        if (itemType == nsINavBookmarksService::TYPE_SEPARATOR) {
          // remove it
          rv = mBookmarksService->RemoveItem(frame.mPreviousId);
          NS_ENSURE_SUCCESS(rv, rv);
        }
      }      
      break;
    default:
      NS_NOTREACHED("Unknown container type");
  }

#ifdef DEBUG_IMPORT
  PrintNesting();
  printf("Folder %lld \'%s\'", ourID, NS_ConvertUTF16toUTF8(containerName).get());
#endif

  if (updateFolder) {
    // move the menu folder to the current position
    mBookmarksService->MoveItem(ourID, CurFrame().mContainerID, -1);
    mBookmarksService->SetItemTitle(ourID, NS_ConvertUTF16toUTF8(containerName));
#ifdef DEBUG_IMPORT
    printf(" [reparenting]");
#endif
  }

#ifdef DEBUG_IMPORT
  printf("\n");
#endif

  if (frame.mPreviousDateAdded > 0) {
    nsresult rv = mBookmarksService->SetItemDateAdded(ourID, frame.mPreviousDateAdded);
    NS_ASSERTION(NS_SUCCEEDED(rv), "SetItemDateAdded failed");
    frame.mPreviousDateAdded = 0;
  }
  if (frame.mPreviousLastModifiedDate > 0) {
    nsresult rv = mBookmarksService->SetItemLastModified(ourID, frame.mPreviousLastModifiedDate);
    NS_ASSERTION(NS_SUCCEEDED(rv), "SetItemLastModified failed");
    // don't clear last-modified, in case there's a description
  }

  frame.mPreviousId = ourID;

  if (!mFrames.AppendElement(BookmarkImportFrame(ourID)))
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}


// BookmarkContentSink::PopFrame
//

nsresult
BookmarkContentSink::PopFrame()
{
  // we must always have one frame
  if (mFrames.Length() <= 1) {
    NS_NOTREACHED("Trying to complete more bookmark folders than you started");
    return NS_ERROR_FAILURE;
  }
  mFrames.RemoveElementAt(mFrames.Length() - 1);
  return NS_OK;
}


// BookmarkContentSink::SetFaviconForURI
//
//    aData is a string that is a data URI for the favicon. Our job is to
//    decode it and store it in the favicon service.
//
//    When aIconURI is non-null, we will use that as the URI of the favicon
//    when storing in the favicon service.
//
//    When aIconURI is null, we have to make up a URI for this favicon so that
//    it can be stored in the service. The real one will be set the next time
//    the user visits the page. Our made up one should get expired when the
//    page no longer references it.

nsresult
BookmarkContentSink::SetFaviconForURI(nsIURI* aPageURI, nsIURI* aIconURI,
                                      const nsString& aData)
{
  nsresult rv;
  static PRUint32 serialNumber = 0; // for made-up favicon URIs

  nsCOMPtr<nsIFaviconService> faviconService(do_GetService(NS_FAVICONSERVICE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  // if the input favicon URI is a chrome: URI, then we just save it and don't
  // worry about data
  if (aIconURI) {
    nsCString faviconScheme;
    aIconURI->GetScheme(faviconScheme);
    if (faviconScheme.EqualsLiteral("chrome")) {
      return faviconService->SetFaviconUrlForPage(aPageURI, aIconURI);
    }
  }

  // some bookmarks have placeholder URIs that contain just "data:"
  // ignore these
  if (aData.Length() <= 5)
    return NS_OK;

  nsCOMPtr<nsIURI> faviconURI;
  if (aIconURI) {
    faviconURI = aIconURI;
  } else {
    // make up favicon URL
    nsCAutoString faviconSpec;
    faviconSpec.AssignLiteral("http://www.mozilla.org/2005/made-up-favicon/");
    faviconSpec.AppendInt(serialNumber);
    faviconSpec.AppendLiteral("-");
    char buf[32];
    PR_snprintf(buf, sizeof(buf), "%lld", PR_Now());
    faviconSpec.Append(buf);
    rv = NS_NewURI(getter_AddRefs(faviconURI), faviconSpec);
    NS_ENSURE_SUCCESS(rv, rv);
    serialNumber++;
  }

  // save the favicon data
  // This could fail if the favicon is bigger than defined limit, in such a
  // case data will not be saved to the db but we will still continue.
  (void) faviconService->SetFaviconDataFromDataURL(faviconURI, aData, 0);

  rv = faviconService->SetFaviconUrlForPage(aPageURI, faviconURI);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK; 
}

// Converts a string id (ITEM_ID) into an int id
PRInt64
BookmarkContentSink::ConvertImportedIdToInternalId(const nsCString& aId) {
  PRInt64 intId = 0;
  if (aId.IsEmpty())
    return intId;
  nsresult rv;
  intId = aId.ToInteger(&rv);
  if (NS_FAILED(rv))
    intId = 0;
  return intId;
}

// Converts a string date in seconds to an int date in microseconds
PRTime
BookmarkContentSink::ConvertImportedDateToInternalDate(const nsACString& aDate) {
  PRTime convertedDate = 0;
  if (!aDate.IsEmpty()) {
    nsresult rv;
    convertedDate = aDate.ToInteger(&rv);
    if (NS_SUCCEEDED(rv)) {
      convertedDate *= 1000000; // in bookmarks.html this value is in seconds, not microseconds
    }
    else {
      convertedDate = 0;
    }
  }
  return convertedDate;
}

// SyncChannelStatus
//
//    If a function returns an error, we need to set the channel status to be
//    the same, but only if the channel doesn't have its own error. This returns
//    the error code that should be sent to OnStopRequest.

static nsresult
SyncChannelStatus(nsIChannel* channel, nsresult status)
{
  nsresult channelStatus;
  channel->GetStatus(&channelStatus);
  if (NS_FAILED(channelStatus))
    return channelStatus;

  if (NS_SUCCEEDED(status))
    return NS_OK; // caller and the channel are happy

  // channel was OK, but caller wasn't: set the channel state
  channel->Cancel(status);
  return status;
}


static char kFileIntro[] =
    "<!DOCTYPE NETSCAPE-Bookmark-file-1>" NS_LINEBREAK
    // Note: we write bookmarks in UTF-8
    "<!-- This is an automatically generated file." NS_LINEBREAK
    "     It will be read and overwritten." NS_LINEBREAK
    "     DO NOT EDIT! -->" NS_LINEBREAK
    "<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=UTF-8\">" NS_LINEBREAK
    "<TITLE>Bookmarks</TITLE>" NS_LINEBREAK;
static const char kRootIntro[] = "<H1";
static const char kCloseRootH1[] = "</H1>" NS_LINEBREAK NS_LINEBREAK;

static const char kBookmarkIntro[] = "<DL><p>" NS_LINEBREAK;
static const char kBookmarkClose[] = "</DL><p>" NS_LINEBREAK;
static const char kContainerIntro[] = "<DT><H3";
static const char kContainerClose[] = "</H3>" NS_LINEBREAK;
static const char kItemOpen[] = "<DT><A";
static const char kItemClose[] = "</A>" NS_LINEBREAK;
static const char kSeparator[] = "<HR";
static const char kQuoteStr[] = "\"";
static const char kCloseAngle[] = ">";
static const char kIndent[] = "    ";
static const char kDescriptionIntro[] = "<DD>";
static const char kDescriptionClose[] = NS_LINEBREAK;

static const char kPlacesRootAttribute[] = " PLACES_ROOT=\"true\"";
static const char kBookmarksRootAttribute[] = " BOOKMARKS_MENU=\"true\"";
static const char kToolbarFolderAttribute[] = " PERSONAL_TOOLBAR_FOLDER=\"true\"";
static const char kUnfiledBookmarksFolderAttribute[] = " UNFILED_BOOKMARKS_FOLDER=\"true\"";
static const char kIconAttribute[] = " ICON=\"";
static const char kIconURIAttribute[] = " ICON_URI=\"";
static const char kHrefAttribute[] = " HREF=\"";
static const char kFeedURIAttribute[] = " FEEDURL=\"";
static const char kWebPanelAttribute[] = " WEB_PANEL=\"true\"";
static const char kKeywordAttribute[] = " SHORTCUTURL=\"";
static const char kPostDataAttribute[] = " POST_DATA=\"";
static const char kItemIdAttribute[] = " ITEM_ID=\"";
static const char kNameAttribute[] = " NAME=\"";
static const char kMicsumGenURIAttribute[]    = " MICSUM_GEN_URI=\"";
static const char kDateAddedAttribute[] = " ADD_DATE=\"";
static const char kLastModifiedAttribute[] = " LAST_MODIFIED=\"";
static const char kLastCharsetAttribute[] = " LAST_CHARSET=\"";

// WriteContainerPrologue
//
//    <DL><p>
//
//    Goes after the container header (<H3...) but before the contents

static nsresult
WriteContainerPrologue(const nsACString& aIndent, nsIOutputStream* aOutput)
{
  PRUint32 dummy;
  nsresult rv = aOutput->Write(PromiseFlatCString(aIndent).get(), aIndent.Length(), &dummy);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aOutput->Write(kBookmarkIntro, sizeof(kBookmarkIntro)-1, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}


// WriteContainerEpilogue
//
//    </DL><p>
//
//    Goes after the container contents to close the container

static nsresult
WriteContainerEpilogue(const nsACString& aIndent, nsIOutputStream* aOutput)
{
  PRUint32 dummy;
  nsresult rv = aOutput->Write(PromiseFlatCString(aIndent).get(), aIndent.Length(), &dummy);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aOutput->Write(kBookmarkClose, sizeof(kBookmarkClose)-1, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}


// WriteFaviconAttribute
//
//    This writes the 'ICON="data:asdlfkjas;ldkfja;skdljfasdf"' attribute for
//    an item. We special-case chrome favicon URIs by just writing the chrome:
//    URI.

static nsresult
WriteFaviconAttribute(const nsACString& aURI, nsIOutputStream* aOutput)
{
  nsresult rv;
  PRUint32 dummy;

  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  // get favicon
  nsCOMPtr<nsIFaviconService> faviconService = do_GetService(NS_FAVICONSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIURI> faviconURI;
  rv = faviconService->GetFaviconForPage(uri, getter_AddRefs(faviconURI));
  if (rv == NS_ERROR_NOT_AVAILABLE)
    return NS_OK; // no favicon
  NS_ENSURE_SUCCESS(rv, rv); // anything else is error

  nsCAutoString faviconScheme;
  nsCAutoString faviconSpec;
  rv = faviconURI->GetSpec(faviconSpec);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = faviconURI->GetScheme(faviconScheme);
  NS_ENSURE_SUCCESS(rv, rv);

  // write favicon URI: 'ICON_URI="..."'
  rv = aOutput->Write(kIconURIAttribute, sizeof(kIconURIAttribute)-1, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = WriteEscapedUrl(faviconSpec, aOutput);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aOutput->Write(kQuoteStr, sizeof(kQuoteStr)-1, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!faviconScheme.EqualsLiteral("chrome")) {
    // only store data for non-chrome URIs

    nsAutoString faviconContents;
    rv = faviconService->GetFaviconDataAsDataURL(faviconURI, faviconContents);
    NS_ENSURE_SUCCESS(rv, rv);
    if (faviconContents.Length() > 0) {
      rv = aOutput->Write(kIconAttribute, sizeof(kIconAttribute)-1, &dummy);
      NS_ENSURE_SUCCESS(rv, rv);
      NS_ConvertUTF16toUTF8 utf8Favicon(faviconContents);
      rv = aOutput->Write(utf8Favicon.get(), utf8Favicon.Length(), &dummy);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = aOutput->Write(kQuoteStr, sizeof(kQuoteStr)-1, &dummy);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  return NS_OK;
}

// WriteDateAttribute
//
//    This writes the '{attr value=}"{time in seconds}"' attribute for
//    an item.

static nsresult
WriteDateAttribute(const char aAttributeStart[], PRInt32 aLength, PRTime aAttributeValue, nsIOutputStream* aOutput)
{
  // write attribute start
  PRUint32 dummy;
  nsresult rv = aOutput->Write(aAttributeStart, aLength, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);

  // in bookmarks.html this value is in seconds, not microseconds
  aAttributeValue /= 1000000; 

  // write attribute value
  char dateInSeconds[32];
  PR_snprintf(dateInSeconds, sizeof(dateInSeconds), "%lld", aAttributeValue);
  rv = aOutput->Write(dateInSeconds, strlen(dateInSeconds), &dummy);

  NS_ENSURE_SUCCESS(rv, rv);
  return aOutput->Write(kQuoteStr, sizeof(kQuoteStr)-1, &dummy);
}


// nsPlacesImportExportService::WriteContainer
//
//    Writes out all the necessary parts of a bookmarks folder.

nsresult
nsPlacesImportExportService::WriteContainer(nsINavHistoryResultNode* aFolder, const nsACString& aIndent,
                               nsIOutputStream* aOutput)
{
  nsresult rv = WriteContainerHeader(aFolder, aIndent, aOutput);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = WriteContainerPrologue(aIndent, aOutput);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = WriteContainerContents(aFolder, aIndent, aOutput);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = WriteContainerEpilogue(aIndent, aOutput);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}


// nsPlacesImportExportService::WriteContainerHeader
//
//    This writes '<DL><H3>Title</H3>'
//    Remember folders can also have favicons, which we put in the H3 tag

nsresult
nsPlacesImportExportService::WriteContainerHeader(nsINavHistoryResultNode* aFolder, const nsACString& aIndent,
                                     nsIOutputStream* aOutput)
{
  PRUint32 dummy;
  nsresult rv;

  // indent
  if (!aIndent.IsEmpty()) {
    rv = aOutput->Write(PromiseFlatCString(aIndent).get(), aIndent.Length(), &dummy);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // "<DL H3"
  rv = aOutput->Write(kContainerIntro, sizeof(kContainerIntro)-1, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);

  // get folder id
  PRInt64 folderId;
  rv = aFolder->GetItemId(&folderId);
  NS_ENSURE_SUCCESS(rv, rv);

  // write ADD_DATE
  PRTime dateAdded = 0;
  rv = aFolder->GetDateAdded(&dateAdded);
  NS_ENSURE_SUCCESS(rv, rv);

  if (dateAdded) {
    rv = WriteDateAttribute(kDateAddedAttribute, sizeof(kDateAddedAttribute)-1, dateAdded, aOutput);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // write LAST_MODIFIED
  PRTime lastModified = 0;
  rv = aFolder->GetLastModified(&lastModified);
  NS_ENSURE_SUCCESS(rv, rv);

  if (lastModified) {
    rv = WriteDateAttribute(kLastModifiedAttribute, sizeof(kLastModifiedAttribute)-1, lastModified, aOutput);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  PRInt64 placesRoot;
  rv = mBookmarksService->GetPlacesRoot(&placesRoot);
  NS_ENSURE_SUCCESS(rv,rv);

  PRInt64 bookmarksMenuFolder;
  rv = mBookmarksService->GetBookmarksMenuFolder(&bookmarksMenuFolder);
  NS_ENSURE_SUCCESS(rv,rv);

  PRInt64 toolbarFolder;
  rv = mBookmarksService->GetToolbarFolder(&toolbarFolder);
  NS_ENSURE_SUCCESS(rv,rv);

  PRInt64 unfiledBookmarksFolder;
  rv = mBookmarksService->GetUnfiledBookmarksFolder(&unfiledBookmarksFolder);
  NS_ENSURE_SUCCESS(rv,rv);

  // " PERSONAL_TOOLBAR_FOLDER="true"", etc.
  if (folderId == placesRoot) {
    rv = aOutput->Write(kPlacesRootAttribute, sizeof(kPlacesRootAttribute)-1, &dummy);
    NS_ENSURE_SUCCESS(rv, rv);
  } else if (folderId == bookmarksMenuFolder) {
    rv = aOutput->Write(kBookmarksRootAttribute, sizeof(kBookmarksRootAttribute)-1, &dummy);
    NS_ENSURE_SUCCESS(rv, rv);
  } else if (folderId == unfiledBookmarksFolder) {
    rv = aOutput->Write(kUnfiledBookmarksFolderAttribute, sizeof(kUnfiledBookmarksFolderAttribute)-1, &dummy);
    NS_ENSURE_SUCCESS(rv, rv);
  } else if (folderId == toolbarFolder) {
    rv = aOutput->Write(kToolbarFolderAttribute, sizeof(kToolbarFolderAttribute)-1, &dummy);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // ">"
  rv = aOutput->Write(kCloseAngle, sizeof(kCloseAngle)-1, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);

  // title
  rv = WriteTitle(aFolder, aOutput);
  NS_ENSURE_SUCCESS(rv, rv);

  // "</H3>\n"
  rv = aOutput->Write(kContainerClose, sizeof(kContainerClose)-1, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);

  // description
  rv = WriteDescription(folderId, nsINavBookmarksService::TYPE_FOLDER, aOutput);
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
}


// nsPlacesImportExportService::WriteTitle
//
//    Retrieves, escapes and writes the title to the stream.

nsresult
nsPlacesImportExportService::WriteTitle(nsINavHistoryResultNode* aItem, nsIOutputStream* aOutput)
{
  // XXX Bug 381767 - support titles for separators
  PRUint32 type = 0;
  nsresult rv = aItem->GetType(&type);
  NS_ENSURE_SUCCESS(rv, rv);
  if (type == nsINavHistoryResultNode::RESULT_TYPE_SEPARATOR)
    return NS_ERROR_INVALID_ARG;

  nsCAutoString title;
  rv = aItem->GetTitle(title);
  NS_ENSURE_SUCCESS(rv, rv);

  char* escapedTitle = nsEscapeHTML(title.get());
  if (escapedTitle) {
    PRUint32 dummy;
    rv = aOutput->Write(escapedTitle, strlen(escapedTitle), &dummy);
    nsMemory::Free(escapedTitle);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}


// nsPlacesImportExportService::WriteDescription
//
//  Write description out for all item types.
nsresult
nsPlacesImportExportService::WriteDescription(PRInt64 aItemId, PRInt32 aType,
                                              nsIOutputStream* aOutput)
{
  PRBool hasDescription = PR_FALSE;
  nsresult rv = mAnnotationService->ItemHasAnnotation(aItemId,
                                                      DESCRIPTION_ANNO,
                                                      &hasDescription);
  if (NS_FAILED(rv) || !hasDescription)
    return rv;

  nsAutoString description;
  rv = mAnnotationService->GetItemAnnotationString(aItemId, DESCRIPTION_ANNO,
                                                   description);
  NS_ENSURE_SUCCESS(rv, rv);

  char* escapedDesc = nsEscapeHTML(NS_ConvertUTF16toUTF8(description).get());
  if (escapedDesc) {
    PRUint32 dummy;
    rv = aOutput->Write(kDescriptionIntro, sizeof(kDescriptionIntro)-1, &dummy);
    if (NS_FAILED(rv)) {
      nsMemory::Free(escapedDesc);
      return rv;
    }
    rv = aOutput->Write(escapedDesc, strlen(escapedDesc), &dummy);
    nsMemory::Free(escapedDesc);
    NS_ENSURE_SUCCESS(rv, rv);
    aOutput->Write(kDescriptionClose, sizeof(kDescriptionClose)-1, &dummy);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

// nsBookmarks::WriteItem
//
//    "<DT><A HREF="..." ICON="...">Name</A>"

nsresult
nsPlacesImportExportService::WriteItem(nsINavHistoryResultNode* aItem,
                          const nsACString& aIndent,
                          nsIOutputStream* aOutput)
{
  PRUint32 dummy;
  nsresult rv;

  // indent
  if (!aIndent.IsEmpty()) {
    rv = aOutput->Write(PromiseFlatCString(aIndent).get(), aIndent.Length(), &dummy);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // '<DT><A'
  rv = aOutput->Write(kItemOpen, sizeof(kItemOpen)-1, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);

  // ' HREF="http://..."' - note that we need to call GetURI on the result
  // node because some nodes (eg queries) generate this lazily.
  rv = aOutput->Write(kHrefAttribute, sizeof(kHrefAttribute)-1, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCAutoString uri;
  rv = aItem->GetUri(uri);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = WriteEscapedUrl(uri, aOutput);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aOutput->Write(kQuoteStr, sizeof(kQuoteStr)-1, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);

  // write ADD_DATE
  PRTime dateAdded = 0;
  rv = aItem->GetDateAdded(&dateAdded);
  NS_ENSURE_SUCCESS(rv, rv);

  if (dateAdded) {
    rv = WriteDateAttribute(kDateAddedAttribute, sizeof(kDateAddedAttribute)-1, dateAdded, aOutput);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // write LAST_MODIFIED
  PRTime lastModified = 0;
  rv = aItem->GetLastModified(&lastModified);
  NS_ENSURE_SUCCESS(rv, rv);

  if (lastModified) {
    rv = WriteDateAttribute(kLastModifiedAttribute, sizeof(kLastModifiedAttribute)-1, lastModified, aOutput);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // ' ICON="..."'
  rv = WriteFaviconAttribute(uri, aOutput);
  NS_ENSURE_SUCCESS(rv, rv);

  // get item id 
  PRInt64 itemId;
  rv = aItem->GetItemId(&itemId);
  NS_ENSURE_SUCCESS(rv, rv);

  // keyword (shortcuturl)
  nsAutoString keyword;
  rv = mBookmarksService->GetKeywordForBookmark(itemId, keyword);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!keyword.IsEmpty()) {
    rv = aOutput->Write(kKeywordAttribute, sizeof(kKeywordAttribute)-1, &dummy);
    NS_ENSURE_SUCCESS(rv, rv);
    char* escapedKeyword = nsEscapeHTML(NS_ConvertUTF16toUTF8(keyword).get());
    rv = aOutput->Write(escapedKeyword, strlen(escapedKeyword), &dummy);
    nsMemory::Free(escapedKeyword);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aOutput->Write(kQuoteStr, sizeof(kQuoteStr)-1, &dummy);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // post data
  nsCOMPtr<nsIURI> pageURI;
  rv = NS_NewURI(getter_AddRefs(pageURI), uri, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);
  
  PRBool hasPostData;
  rv = mAnnotationService->ItemHasAnnotation(itemId, POST_DATA_ANNO,
                                             &hasPostData);
  NS_ENSURE_SUCCESS(rv, rv);
  if (hasPostData) {
    nsAutoString postData;
    rv = mAnnotationService->GetItemAnnotationString(itemId, POST_DATA_ANNO,
                                                     postData);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aOutput->Write(kPostDataAttribute, sizeof(kPostDataAttribute)-1, &dummy);
    NS_ENSURE_SUCCESS(rv, rv);
    char* escapedPostData = nsEscapeHTML(NS_ConvertUTF16toUTF8(postData).get());
    rv = aOutput->Write(escapedPostData, strlen(escapedPostData), &dummy);
    nsMemory::Free(escapedPostData);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aOutput->Write(kQuoteStr, sizeof(kQuoteStr)-1, &dummy);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Write WEB_PANEL="true" if the load-in-sidebar annotation is set for the
  // item
  PRBool loadInSidebar = PR_FALSE;
  rv = mAnnotationService->ItemHasAnnotation(itemId, LOAD_IN_SIDEBAR_ANNO,
                                             &loadInSidebar);
  NS_ENSURE_SUCCESS(rv, rv);
  if (loadInSidebar)
    aOutput->Write(kWebPanelAttribute, sizeof(kWebPanelAttribute)-1, &dummy);

  // microsummary
  nsCOMPtr<nsIMicrosummary> microsummary;
  rv = mMicrosummaryService->GetMicrosummary(itemId, getter_AddRefs(microsummary));
  NS_ENSURE_SUCCESS(rv, rv);
  if (microsummary) {
    nsCOMPtr<nsIMicrosummaryGenerator> generator;
    rv = microsummary->GetGenerator(getter_AddRefs(generator));
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIURI> genURI;
    rv = generator->GetUri(getter_AddRefs(genURI));
    NS_ENSURE_SUCCESS(rv, rv);
    nsCAutoString spec;
    rv = genURI->GetSpec(spec);
    NS_ENSURE_SUCCESS(rv, rv);

    // write out generator URI
    rv = aOutput->Write(kMicsumGenURIAttribute, sizeof(kMicsumGenURIAttribute)-1, &dummy);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = WriteEscapedUrl(spec, aOutput);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aOutput->Write(kQuoteStr, sizeof(kQuoteStr)-1, &dummy);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // last charset
  nsAutoString lastCharset;
  if (NS_SUCCEEDED(mHistoryService->GetCharsetForURI(pageURI, lastCharset)) &&
      !lastCharset.IsEmpty()) {
    rv = aOutput->Write(kLastCharsetAttribute, sizeof(kLastCharsetAttribute)-1, &dummy);
    NS_ENSURE_SUCCESS(rv, rv);
    char* escapedLastCharset = nsEscapeHTML(NS_ConvertUTF16toUTF8(lastCharset).get());
    rv = aOutput->Write(escapedLastCharset, strlen(escapedLastCharset), &dummy);
    nsMemory::Free(escapedLastCharset);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aOutput->Write(kQuoteStr, sizeof(kQuoteStr)-1, &dummy);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // '>'
  rv = aOutput->Write(kCloseAngle, sizeof(kCloseAngle)-1, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);

  // title
  nsCAutoString title;
  rv = aItem->GetTitle(title);
  NS_ENSURE_SUCCESS(rv, rv);
  char* escapedTitle = nsEscapeHTML(title.get());
  if (escapedTitle) {
    rv = aOutput->Write(escapedTitle, strlen(escapedTitle), &dummy);
    nsMemory::Free(escapedTitle);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // '</A>\n'
  rv = aOutput->Write(kItemClose, sizeof(kItemClose)-1, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);

  // description
  rv = WriteDescription(itemId, nsINavBookmarksService::TYPE_BOOKMARK, aOutput);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


// WriteLivemark
//
//    Similar to WriteItem, this has an additional FEEDURL attribute and
//    the HREF is optional and points to the source page.

nsresult
nsPlacesImportExportService::WriteLivemark(nsINavHistoryResultNode* aFolder, const nsACString& aIndent,
                              nsIOutputStream* aOutput)
{
  PRUint32 dummy;
  nsresult rv;

  // indent
  if (!aIndent.IsEmpty()) {
    rv = aOutput->Write(PromiseFlatCString(aIndent).get(), aIndent.Length(), &dummy);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // '<DT><A'
  rv = aOutput->Write(kItemOpen, sizeof(kItemOpen)-1, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);

  // get folder id
  PRInt64 folderId;
  rv = aFolder->GetItemId(&folderId);
  NS_ENSURE_SUCCESS(rv, rv);

  // get feed URI
  nsCOMPtr<nsIURI> feedURI;
  rv = mLivemarkService->GetFeedURI(folderId, getter_AddRefs(feedURI));
  NS_ENSURE_SUCCESS(rv, rv);
  if (feedURI) {
    nsCString feedSpec;
    rv = feedURI->GetSpec(feedSpec);
    NS_ENSURE_SUCCESS(rv, rv);

    // write feed URI
    rv = aOutput->Write(kFeedURIAttribute, sizeof(kFeedURIAttribute)-1, &dummy);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = WriteEscapedUrl(feedSpec, aOutput);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aOutput->Write(kQuoteStr, sizeof(kQuoteStr)-1, &dummy);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // get the optional site URI
  nsCOMPtr<nsIURI> siteURI;
  rv = mLivemarkService->GetSiteURI(folderId, getter_AddRefs(siteURI));
  NS_ENSURE_SUCCESS(rv, rv);
  if (siteURI) {
    nsCString siteSpec;
    rv = siteURI->GetSpec(siteSpec);
    NS_ENSURE_SUCCESS(rv, rv);

    // write site URI
    rv = aOutput->Write(kHrefAttribute, sizeof(kHrefAttribute)-1, &dummy);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = WriteEscapedUrl(siteSpec, aOutput);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aOutput->Write(kQuoteStr, sizeof(kQuoteStr)-1, &dummy);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // '>'
  rv = aOutput->Write(kCloseAngle, sizeof(kCloseAngle)-1, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);

  // title
  rv = WriteTitle(aFolder, aOutput);
  NS_ENSURE_SUCCESS(rv, rv);

  // '</A>\n'
  rv = aOutput->Write(kItemClose, sizeof(kItemClose)-1, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);

  // description
  rv = WriteDescription(folderId, nsINavBookmarksService::TYPE_BOOKMARK, aOutput);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


// nsPlacesImportExportService::WriteSeparator
//
//    "<HR NAME="...">"

nsresult
nsPlacesImportExportService::WriteSeparator(nsINavHistoryResultNode* aItem,
                                            const nsACString& aIndent,
                                            nsIOutputStream* aOutput)
{
  PRUint32 dummy;
  nsresult rv;

  // indent
  if (!aIndent.IsEmpty()) {
    rv = aOutput->Write(PromiseFlatCString(aIndent).get(), aIndent.Length(),
                        &dummy);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = aOutput->Write(kSeparator, sizeof(kSeparator)-1, &dummy);

  // XXX: separator result nodes don't support the title getter yet
  PRInt64 itemId;
  rv = aItem->GetItemId(&itemId);
  NS_ENSURE_SUCCESS(rv, rv);

  // Note: we can't write the separator ID or anything else other than NAME
  // because it makes Firefox 2.x crash/hang - see bug #381129

  nsCAutoString title;
  rv = mBookmarksService->GetItemTitle(itemId, title);
  if (NS_SUCCEEDED(rv) && !title.IsEmpty()) {
    rv = aOutput->Write(kNameAttribute, strlen(kNameAttribute), &dummy);
    NS_ENSURE_SUCCESS(rv, rv);

    char* escapedTitle = nsEscapeHTML(title.get());
    if (escapedTitle) {
      PRUint32 dummy;
      rv = aOutput->Write(escapedTitle, strlen(escapedTitle), &dummy);
      nsMemory::Free(escapedTitle);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = aOutput->Write(kQuoteStr, sizeof(kQuoteStr)-1, &dummy);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // '>'
  rv = aOutput->Write(kCloseAngle, sizeof(kCloseAngle)-1, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);

  // line break
  rv = aOutput->Write(NS_LINEBREAK, sizeof(NS_LINEBREAK)-1, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
}


// WriteEscapedUrl
//
//    Writes the given string to the stream escaped as necessary for URLs.
//
//    Unfortunately, the old bookmarks system uses a custom hardcoded and
//    braindead escaping scheme that we need to emulate. It just replaces
//    quotes with %22 and that's it.

nsresult
WriteEscapedUrl(const nsCString& aString, nsIOutputStream* aOutput)
{
  nsCAutoString escaped(aString);
  PRInt32 offset;
  while ((offset = escaped.FindChar('\"')) >= 0) {
    escaped.Cut(offset, 1);
    escaped.Insert(NS_LITERAL_CSTRING("%22"), offset);
  }
  PRUint32 dummy;
  return aOutput->Write(escaped.get(), escaped.Length(), &dummy);
}


// nsPlacesImportExportService::WriteContainerContents
//
//    The indent here is the indent of the parent. We will add an additional
//    indent before writing data.

nsresult
nsPlacesImportExportService::WriteContainerContents(nsINavHistoryResultNode* aFolder, const nsACString& aIndent,
                                       nsIOutputStream* aOutput)
{
  nsCAutoString myIndent(aIndent);
  myIndent.Append(kIndent);

  PRInt64 folderId;
  nsresult rv = aFolder->GetItemId(&folderId);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsINavHistoryContainerResultNode> folderNode = do_QueryInterface(aFolder, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = folderNode->SetContainerOpen(PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 childCount = 0;
  folderNode->GetChildCount(&childCount);
  for (PRUint32 i = 0; i < childCount; ++i) {
    nsCOMPtr<nsINavHistoryResultNode> child;
    rv = folderNode->GetChild(i, getter_AddRefs(child));
    NS_ENSURE_SUCCESS(rv, rv);
    PRUint32 type = 0;
    rv = child->GetType(&type);
    NS_ENSURE_SUCCESS(rv, rv);
    if (type == nsINavHistoryResultNode::RESULT_TYPE_FOLDER) {
      // bookmarks folder
      PRInt64 childFolderId;
      rv = child->GetItemId(&childFolderId);
      NS_ENSURE_SUCCESS(rv, rv);

      // it could be a regular folder or it could be a livemark
      PRBool isLivemark;
      rv = mLivemarkService->IsLivemark(childFolderId, &isLivemark);
      NS_ENSURE_SUCCESS(rv, rv);

      if (isLivemark)
        rv = WriteLivemark(child, myIndent, aOutput);
      else
        rv = WriteContainer(child, myIndent, aOutput);
    } else if (type == nsINavHistoryResultNode::RESULT_TYPE_SEPARATOR) {
      rv = WriteSeparator(child, myIndent, aOutput);
    } else {
      rv = WriteItem(child, myIndent, aOutput);
    }
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}


// nsIPlacesImportExportService::ImportHTMLFromFile
//
NS_IMETHODIMP
nsPlacesImportExportService::ImportHTMLFromFile(nsILocalFile* aFile, PRBool aIsInitialImport)
{
  // this version is exposed on the interface and disallows changing of roots
  return ImportHTMLFromFileInternal(aFile, PR_FALSE, 0, aIsInitialImport);
}

// nsIPlacesImportExportService::ImportHTMLFromFileToFolder
//
NS_IMETHODIMP
nsPlacesImportExportService::ImportHTMLFromFileToFolder(nsILocalFile* aFile, PRInt64 aFolderId, PRBool aIsInitialImport)
{
  // this version is exposed on the interface and disallows changing of roots
  return ImportHTMLFromFileInternal(aFile, PR_FALSE, aFolderId, aIsInitialImport);
}

nsresult
nsPlacesImportExportService::ImportHTMLFromFileInternal(nsILocalFile* aFile,
                                       PRBool aAllowRootChanges,
                                       PRInt64 aFolder,
                                       PRBool aIsImportDefaults)
{
  nsresult rv = EnsureServiceState();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFile> file(do_QueryInterface(aFile));
#ifdef DEBUG_IMPORT
  nsAutoString path;
  file->GetPath(path);
  printf("\nImporting %s\n", NS_ConvertUTF16toUTF8(path).get());
#endif

  // confirm file exists
  PRBool exists;
  rv = file->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!exists) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsIParser> parser = do_CreateInstance(kParserCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<BookmarkContentSink> sink = new BookmarkContentSink;
  NS_ENSURE_TRUE(sink, NS_ERROR_OUT_OF_MEMORY);
  rv = sink->Init(aAllowRootChanges, mBookmarksService, aFolder, aIsImportDefaults);
  NS_ENSURE_SUCCESS(rv, rv);
  parser->SetContentSink(sink);

  // channel: note we have to set the content type or the default "unknown" type
  // will confuse the parser
  nsCOMPtr<nsIIOService> ioservice = do_GetIOService(&rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIURI> fileURI;
  rv = ioservice->NewFileURI(file, getter_AddRefs(fileURI));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = ioservice->NewChannelFromURI(fileURI, getter_AddRefs(mImportChannel));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mImportChannel->SetContentType(NS_LITERAL_CSTRING("text/html"));
  NS_ENSURE_SUCCESS(rv, rv);

  // init parser
  rv = parser->Parse(fileURI, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  // wrap the import in a transaction to make it faster
  mIsImportDefaults = aIsImportDefaults;
  mBookmarksService->RunInBatchMode(this, parser);
  mImportChannel = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsPlacesImportExportService::RunBatched(nsISupports* aUserData)
{
  nsresult rv;
  if (mIsImportDefaults) {
    PRInt64 bookmarksMenuFolder;
    rv = mBookmarksService->GetBookmarksMenuFolder(&bookmarksMenuFolder);
    NS_ENSURE_SUCCESS(rv,rv);

    rv = mBookmarksService->RemoveFolderChildren(bookmarksMenuFolder);
    NS_ENSURE_SUCCESS(rv, rv);

    PRInt64 toolbarFolder;
    rv = mBookmarksService->GetToolbarFolder(&toolbarFolder);
    NS_ENSURE_SUCCESS(rv,rv);

    rv = mBookmarksService->RemoveFolderChildren(toolbarFolder);
    NS_ENSURE_SUCCESS(rv, rv);

    PRInt64 unfiledBookmarksFolder;
    rv = mBookmarksService->GetUnfiledBookmarksFolder(&unfiledBookmarksFolder);
    NS_ENSURE_SUCCESS(rv,rv);

    rv = mBookmarksService->RemoveFolderChildren(unfiledBookmarksFolder);
    NS_ENSURE_SUCCESS(rv,rv);

    // add the "Places" folder
    nsCOMPtr<nsIBrowserGlue> glue(do_GetService("@mozilla.org/browser/browserglue;1", &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = glue->EnsurePlacesDefaultQueriesInitialized();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // streams
  nsCOMPtr<nsIInputStream> stream;
  rv = mImportChannel->Open(getter_AddRefs(stream));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIInputStream> bufferedstream;
  rv = NS_NewBufferedInputStream(getter_AddRefs(bufferedstream), stream, 4096);
  NS_ENSURE_SUCCESS(rv, rv);

  // feed the parser the data
  // Note: on error, we always need to set the channel's status to be the
  // same, and to always call OnStopRequest with the channel error.
  nsCOMPtr<nsIStreamListener> listener = do_QueryInterface(aUserData, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = listener->OnStartRequest(mImportChannel, nsnull);
  rv = SyncChannelStatus(mImportChannel, rv);
  while (NS_SUCCEEDED(rv))
  {
    PRUint32 available;
    rv = bufferedstream->Available(&available);
    if (rv == NS_BASE_STREAM_CLOSED) {
      rv = NS_OK;
      available = 0;
    }
    if (NS_FAILED(rv)) {
      mImportChannel->Cancel(rv);
      break;
    }
    if (!available)
      break; // blocking input stream has none available when done

    rv = listener->OnDataAvailable(mImportChannel, nsnull, bufferedstream, 0,
                                   available);
    rv = SyncChannelStatus(mImportChannel, rv);
    if (NS_FAILED(rv))
      break;
  }
  listener->OnStopRequest(mImportChannel, nsnull, rv);
  return NS_OK;
}

// nsIPlacesImportExportService::ExportHMTLToFile
//
NS_IMETHODIMP
nsPlacesImportExportService::ExportHTMLToFile(nsILocalFile* aBookmarksFile)
{
  if (!aBookmarksFile)
    return NS_ERROR_NULL_POINTER;

#ifdef DEBUG_EXPORT
  nsAutoString path;
  aBookmarksFile->GetPath(path);
  printf("\nExporting %s\n", NS_ConvertUTF16toUTF8(path).get());

  PRTime startTime = PR_Now();
  printf("\nStart time: %lld\n", startTime);
#endif

  nsresult rv = EnsureServiceState();
  NS_ENSURE_SUCCESS(rv, rv);

  // get a safe output stream, so we don't clobber the bookmarks file unless
  // all the writes succeeded.
  nsCOMPtr<nsIOutputStream> out;
  rv = NS_NewSafeLocalFileOutputStream(getter_AddRefs(out),
                                       aBookmarksFile,
                                       PR_WRONLY | PR_CREATE_FILE,
                                       /*octal*/ 0600,
                                       0);
  NS_ENSURE_SUCCESS(rv, rv);

  // We need a buffered output stream for performance.
  // See bug 202477.
  nsCOMPtr<nsIOutputStream> strm;
  rv = NS_NewBufferedOutputStream(getter_AddRefs(strm), out, 4096);
  NS_ENSURE_SUCCESS(rv, rv);

  // get bookmarks menu folder id
  PRInt64 bookmarksMenuFolder;
  rv = mBookmarksService->GetBookmarksMenuFolder(&bookmarksMenuFolder);
  NS_ENSURE_SUCCESS(rv,rv);

  PRInt64 toolbarFolder;
  rv = mBookmarksService->GetToolbarFolder(&toolbarFolder);
  NS_ENSURE_SUCCESS(rv,rv);

  PRInt64 unfiledBookmarksFolder;
  rv = mBookmarksService->GetUnfiledBookmarksFolder(&unfiledBookmarksFolder);
  NS_ENSURE_SUCCESS(rv,rv);

  // file header
  PRUint32 dummy;
  rv = strm->Write(kFileIntro, sizeof(kFileIntro)-1, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);

  // get empty options
  nsCOMPtr<nsINavHistoryQueryOptions> options;
  rv = mHistoryService->GetNewQueryOptions(getter_AddRefs(options));
  NS_ENSURE_SUCCESS(rv, rv);

  // get a new query object
  nsCOMPtr<nsINavHistoryQuery> query;
  rv = mHistoryService->GetNewQuery(getter_AddRefs(query));
  NS_ENSURE_SUCCESS(rv, rv);

  // query for just this folder
  rv = query->SetFolders(&bookmarksMenuFolder, 1);
  NS_ENSURE_SUCCESS(rv, rv);

  // execute query
  nsCOMPtr<nsINavHistoryResult> result;
  rv = mHistoryService->ExecuteQuery(query, options, getter_AddRefs(result));
  NS_ENSURE_SUCCESS(rv, rv);

  // get root (folder) node
  nsCOMPtr<nsINavHistoryContainerResultNode> rootNode;
  rv = result->GetRoot(getter_AddRefs(rootNode));
  NS_ENSURE_SUCCESS(rv, rv);

  // '<H1'
  rv = strm->Write(kRootIntro, sizeof(kRootIntro)-1, &dummy); // <H1
  NS_ENSURE_SUCCESS(rv, rv);

  // '>Bookmarks</H1>
  rv = strm->Write(kCloseAngle, sizeof(kCloseAngle)-1, &dummy); // >
  NS_ENSURE_SUCCESS(rv, rv);
  rv = WriteTitle(rootNode, strm);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = strm->Write(kCloseRootH1, sizeof(kCloseRootH1)-1, &dummy); // </H1>
  NS_ENSURE_SUCCESS(rv, rv);

  // prologue
  rv = WriteContainerPrologue(EmptyCString(), strm);
  NS_ENSURE_SUCCESS(rv, rv);

  // indents
  nsCAutoString indent;
  indent.Assign(kIndent);

  // write out bookmarks menu contents
  rv = WriteContainerContents(rootNode, EmptyCString(), strm);
  NS_ENSURE_SUCCESS(rv, rv);

  // write out the toolbar folder and unfiled-bookmarks folder (if not empty)
  // under the bookmarks-menu for backwards compatibility
  rv = query->SetFolders(&toolbarFolder, 1);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mHistoryService->ExecuteQuery(query, options, getter_AddRefs(result));
  NS_ENSURE_SUCCESS(rv, rv);

  // get root (folder) node
  rv = result->GetRoot(getter_AddRefs(rootNode));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = WriteContainer(rootNode, nsDependentCString(kIndent), strm);
  NS_ENSURE_SUCCESS(rv, rv);

  // unfiled bookmarks
  rv = query->SetFolders(&unfiledBookmarksFolder, 1);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mHistoryService->ExecuteQuery(query, options, getter_AddRefs(result));
  NS_ENSURE_SUCCESS(rv, rv);

  // get root (folder) node
  rv = result->GetRoot(getter_AddRefs(rootNode));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = rootNode->SetContainerOpen(PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 childCount = 0;
  rv = rootNode->GetChildCount(&childCount);
  NS_ENSURE_SUCCESS(rv, rv);

  if (childCount > 0) {
    rv = WriteContainer(rootNode, nsDependentCString(kIndent), strm);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // epilogue
  rv = WriteContainerEpilogue(EmptyCString(), strm);
  NS_ENSURE_SUCCESS(rv, rv);

  // commit the write
  nsCOMPtr<nsISafeOutputStream> safeStream = do_QueryInterface(strm, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = safeStream->Finish();
#ifdef DEBUG_EXPORT
  printf("\nTotal time in seconds: %lld\n", (PR_Now() - startTime)/1000000);
#endif
  return rv;
}

#define BROWSER_BOOKMARKS_MAX_BACKUPS_PREF  "browser.bookmarks.max_backups"

NS_IMETHODIMP
nsPlacesImportExportService::BackupBookmarksFile()
{
  nsresult rv = EnsureServiceState();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  // get bookmarks file
  nsCOMPtr<nsIFile> bookmarksFileDir;
  rv = NS_GetSpecialDirectory(NS_APP_BOOKMARKS_50_FILE,
                              getter_AddRefs(bookmarksFileDir));

  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsILocalFile> bookmarksFile(do_QueryInterface(bookmarksFileDir));

  // create if it doesn't exist
  PRBool exists;
  rv = bookmarksFile->Exists(&exists);
  if (NS_FAILED(rv)) {
    rv = bookmarksFile->Create(nsIFile::NORMAL_FILE_TYPE, 0600);
    NS_ASSERTION(rv, "Unable to create bookmarks.html!");
    return rv;
  }

  // export bookmarks.html
  rv = ExportHTMLToFile(bookmarksFile);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}
