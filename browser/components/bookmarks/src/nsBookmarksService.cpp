/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert John Churchill    <rjc@netscape.com>
 *   Chris Waterson           <waterson@netscape.com>
 *   David Hyatt              <hyatt@netscape.com>
 *   Ben Goodger              <ben@netscape.com>
 *   Andreas Otte             <andreas.otte@debitel.net>
 *   Pierre Chanial           <chanial@noos.fr>
 *   Jan Varga                <varga@nixcorp.com>
 *   Benjamin Smedberg        <bsmedberg@covad.net>
 *   Vladimir Vukicevic       <vladimir@pobox.com>
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

/*
  The global bookmarks service.
 */

#include "nsBookmarksService.h"
#include "nsArrayEnumerator.h"
#include "nsArray.h"
#include "nsIDOMWindow.h"
#include "nsIObserverService.h"
#include "nsIRDFContainer.h"
#include "nsIRDFContainerUtils.h"
#include "nsIRDFService.h"
#include "nsIRDFXMLSerializer.h"
#include "nsIRDFXMLSource.h"
#include "nsIRDFPropagatableDataSource.h"
#include "nsRDFCID.h"
#include "nsISupportsPrimitives.h"
#include "rdf.h"
#include "nsCRT.h"
#include "nsEnumeratorUtils.h"
#include "nsEscape.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceDefs.h"
#include "nsUnicharUtils.h"
#include "nsICmdLineHandler.h"

#include "nsISound.h"
#include "nsIPrompt.h"
#include "nsIWindowWatcher.h"
#include "nsWidgetsCID.h"

#include "nsIURL.h"
#include "nsIFileURL.h"
#include "nsIFile.h"
#include "nsIInputStream.h"
#include "nsILineInputStream.h"
#include "nsIOutputStream.h"
#include "nsNetUtil.h"
#include "nsICacheEntryDescriptor.h"

#include "nsICharsetConverterManager.h"
#include "nsICharsetAlias.h"
#include "nsIPlatformCharset.h"
#include "nsIPref.h"

#include "plbase64.h"

nsIRDFResource      *kNC_IEFavoritesRoot;
nsIRDFResource      *kNC_SystemBookmarksStaticRoot;
nsIRDFResource      *kNC_Bookmark;
nsIRDFResource      *kNC_BookmarkSeparator;
nsIRDFResource      *kNC_BookmarkAddDate;
nsIRDFResource      *kNC_BookmarksTopRoot;
nsIRDFResource      *kNC_BookmarksRoot;
nsIRDFResource      *kNC_LastModifiedFoldersRoot;
nsIRDFResource      *kNC_child;

nsIRDFResource      *kNC_Description;
nsIRDFResource      *kNC_Folder;
nsIRDFResource      *kNC_IEFavorite;
nsIRDFResource      *kNC_IEFavoriteFolder;
nsIRDFResource      *kNC_Name;
nsIRDFResource      *kNC_Icon;
nsIRDFResource      *kNC_BookmarksToolbarFolder;
nsIRDFResource      *kNC_ShortcutURL;
nsIRDFResource      *kNC_FeedURL;
nsIRDFResource      *kNC_URL;
nsIRDFResource      *kNC_WebPanel;
nsIRDFResource      *kNC_PostData;
nsIRDFResource      *kNC_Livemark;
nsIRDFResource      *kNC_LivemarkLock;
nsIRDFResource      *kNC_LivemarkExpiration;
nsIRDFResource      *kRDF_type;
nsIRDFResource      *kRDF_nextVal;
nsIRDFResource      *kWEB_LastModifiedDate;
nsIRDFResource      *kWEB_LastVisitDate;
nsIRDFResource      *kWEB_Schedule;
nsIRDFResource      *kWEB_ScheduleActive;
nsIRDFResource      *kWEB_Status;
nsIRDFResource      *kWEB_LastPingDate;
nsIRDFResource      *kWEB_LastPingETag;
nsIRDFResource      *kWEB_LastPingModDate;
nsIRDFResource      *kWEB_LastCharset;
nsIRDFResource      *kWEB_LastPingContentLen;
nsIRDFLiteral       *kTrueLiteral;
nsIRDFResource      *kNC_Parent;
nsIRDFResource      *kNC_BookmarkCommand_NewBookmark;
nsIRDFResource      *kNC_BookmarkCommand_NewFolder;
nsIRDFResource      *kNC_BookmarkCommand_NewSeparator;
nsIRDFResource      *kNC_BookmarkCommand_DeleteBookmark;
nsIRDFResource      *kNC_BookmarkCommand_DeleteBookmarkFolder;
nsIRDFResource      *kNC_BookmarkCommand_DeleteBookmarkSeparator;
nsIRDFResource      *kNC_BookmarkCommand_SetPersonalToolbarFolder;
nsIRDFResource      *kNC_BookmarkCommand_Import;
nsIRDFResource      *kNC_BookmarkCommand_Export;
nsIRDFResource      *kNC_BookmarkCommand_RefreshLivemark;

/* RDF Resources for RSS parsing */
#ifndef RSS09_NAMESPACE_URI
#define RSS09_NAMESPACE_URI "http://my.netscape.com/rdf/simple/0.9/"
#endif

nsIRDFResource      *kRSS09_channel;
nsIRDFResource      *kRSS09_item;
nsIRDFResource      *kRSS09_title;
nsIRDFResource      *kRSS09_link;

#ifndef RSS10_NAMESPACE_URI
#define RSS10_NAMESPACE_URI "http://purl.org/rss/1.0/"
#endif

nsIRDFResource      *kRSS10_channel;
nsIRDFResource      *kRSS10_items;
nsIRDFResource      *kRSS10_title;
nsIRDFResource      *kRSS10_link;

#define BOOKMARK_TIMEOUT        15000       // fire every 15 seconds
// #define  DEBUG_BOOKMARK_PING_OUTPUT  1
#define MAX_LAST_MODIFIED_FOLDERS 5

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_CID(kRDFInMemoryDataSourceCID,   NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_CID(kRDFServiceCID,              NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFContainerCID,            NS_RDFCONTAINER_CID);
static NS_DEFINE_CID(kRDFContainerUtilsCID,       NS_RDFCONTAINERUTILS_CID);
static NS_DEFINE_CID(kIOServiceCID,               NS_IOSERVICE_CID);
static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);
static NS_DEFINE_CID(kPrefCID,                    NS_PREF_CID);
static NS_DEFINE_IID(kSoundCID,                   NS_SOUND_CID);
static NS_DEFINE_CID(kStringBundleServiceCID,     NS_STRINGBUNDLESERVICE_CID);
static NS_DEFINE_CID(kPlatformCharsetCID,         NS_PLATFORMCHARSET_CID);
static NS_DEFINE_CID(kCacheServiceCID,            NS_CACHESERVICE_CID);
static NS_DEFINE_CID(kCharsetAliasCID,            NS_CHARSETALIAS_CID);

static const char kURINC_BookmarksTopRoot[]           = "NC:BookmarksTopRoot"; 
static const char kURINC_BookmarksRoot[]              = "NC:BookmarksRoot"; 
static const char kURINC_IEFavoritesRoot[]            = "NC:IEFavoritesRoot"; 
static const char kURINC_SystemBookmarksStaticRoot[]  = "NC:SystemBookmarksStaticRoot"; 
static const char kBookmarkCommand[]                  = "http://home.netscape.com/NC-rdf#command?";

#define bookmark_properties NS_LITERAL_CSTRING("chrome://browser/locale/bookmarks/bookmarks.properties")

/* in nsBookmarksRSSHandler.cpp */
nsresult nsBMSVCClearSeqContainer (nsIRDFDataSource* aDataSource, nsIRDFResource* aResource);
nsresult nsBMSVCUnmakeSeq (nsIRDFDataSource* aDataSource, nsIRDFResource* aResource);

////////////////////////////////////////////////////////////////////////

PRInt32               gRefCnt=0;
nsIRDFService        *gRDF;
nsIRDFContainerUtils *gRDFC;
nsICharsetAlias      *gCharsetAlias;
PRBool                gLoadedBookmarks = PR_FALSE;
PRBool                gImportedSystemBookmarks = PR_FALSE;

static nsresult
bm_AddRefGlobals()
{
    if (gRefCnt++ == 0)
    {
        nsresult rv;
        rv = nsServiceManager::GetService(kRDFServiceCID,
                                          NS_GET_IID(nsIRDFService),
                                          (nsISupports**) &gRDF);

        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF service");
        if (NS_FAILED(rv)) return rv;

        rv = nsServiceManager::GetService(kRDFContainerUtilsCID,
                                          NS_GET_IID(nsIRDFContainerUtils),
                                          (nsISupports**) &gRDFC);

        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF container utils");
        if (NS_FAILED(rv)) return rv;

        rv = nsServiceManager::GetService(kCharsetAliasCID,
                                          NS_GET_IID(nsICharsetAlias),
                                          (nsISupports**) &gCharsetAlias);
        
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get charset alias service");
        if (NS_FAILED(rv)) return rv;

        gRDF->GetResource(NS_LITERAL_CSTRING(kURINC_BookmarksTopRoot),
                          &kNC_BookmarksTopRoot);
        gRDF->GetResource(NS_LITERAL_CSTRING(kURINC_BookmarksRoot),
                          &kNC_BookmarksRoot);
        gRDF->GetResource(NS_LITERAL_CSTRING(kURINC_IEFavoritesRoot),
                          &kNC_IEFavoritesRoot);
        gRDF->GetResource(NS_LITERAL_CSTRING(kURINC_SystemBookmarksStaticRoot),
                          &kNC_SystemBookmarksStaticRoot);
        gRDF->GetResource(NS_LITERAL_CSTRING("NC:LastModifiedFoldersRoot"), 
                          &kNC_LastModifiedFoldersRoot);
        gRDF->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "child"),
                          &kNC_child);
        gRDF->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "BookmarksToolbarFolder"),
                          &kNC_BookmarksToolbarFolder);
        gRDF->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "Bookmark"),
                          &kNC_Bookmark);
        gRDF->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "BookmarkSeparator"),
                          &kNC_BookmarkSeparator);
        gRDF->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "BookmarkAddDate"),
                          &kNC_BookmarkAddDate);
        gRDF->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "Description"),
                          &kNC_Description);
        gRDF->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "Folder"),
                          &kNC_Folder);
        gRDF->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "IEFavorite"),
                          &kNC_IEFavorite);
        gRDF->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "IEFavoriteFolder"),
                          &kNC_IEFavoriteFolder);
        gRDF->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "Name"),
                          &kNC_Name);
        gRDF->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "Icon"),
                          &kNC_Icon);
        gRDF->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "ShortcutURL"),
                          &kNC_ShortcutURL);
        gRDF->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "FeedURL"),
                          &kNC_FeedURL);
        gRDF->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "URL"),
                          &kNC_URL);
        gRDF->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "WebPanel"),
                          &kNC_WebPanel);
        gRDF->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "PostData"),
                          &kNC_PostData);
        gRDF->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "Livemark"),
                          &kNC_Livemark);
        gRDF->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "LivemarkLock"),
                          &kNC_LivemarkLock);
        gRDF->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "LivemarkExpiration"),
                          &kNC_LivemarkExpiration);
        gRDF->GetResource(NS_LITERAL_CSTRING(RDF_NAMESPACE_URI "type"),
                          &kRDF_type);
        gRDF->GetResource(NS_LITERAL_CSTRING(RDF_NAMESPACE_URI "nextVal"),
                          &kRDF_nextVal);

        gRDF->GetResource(NS_LITERAL_CSTRING(WEB_NAMESPACE_URI "LastModifiedDate"),
                          &kWEB_LastModifiedDate);
        gRDF->GetResource(NS_LITERAL_CSTRING(WEB_NAMESPACE_URI "LastVisitDate"),
                          &kWEB_LastVisitDate);
        gRDF->GetResource(NS_LITERAL_CSTRING(WEB_NAMESPACE_URI "LastCharset"),
                          &kWEB_LastCharset);

        gRDF->GetResource(NS_LITERAL_CSTRING(WEB_NAMESPACE_URI "Schedule"),
                          &kWEB_Schedule);
        gRDF->GetResource(NS_LITERAL_CSTRING(WEB_NAMESPACE_URI "ScheduleFlag"),
                          &kWEB_ScheduleActive);
        gRDF->GetResource(NS_LITERAL_CSTRING(WEB_NAMESPACE_URI "status"),
                          &kWEB_Status);
        gRDF->GetResource(NS_LITERAL_CSTRING(WEB_NAMESPACE_URI "LastPingDate"),
                          &kWEB_LastPingDate);
        gRDF->GetResource(NS_LITERAL_CSTRING(WEB_NAMESPACE_URI "LastPingETag"),
                          &kWEB_LastPingETag);
        gRDF->GetResource(NS_LITERAL_CSTRING(WEB_NAMESPACE_URI "LastPingModDate"),
                          &kWEB_LastPingModDate);
        gRDF->GetResource(NS_LITERAL_CSTRING(WEB_NAMESPACE_URI "LastPingContentLen"),
                          &kWEB_LastPingContentLen);

        gRDF->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "parent"), &kNC_Parent);

        gRDF->GetLiteral(NS_LITERAL_STRING("true").get(), &kTrueLiteral);

        gRDF->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "command?cmd=newbookmark"),
                          &kNC_BookmarkCommand_NewBookmark);
        gRDF->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "command?cmd=newfolder"),
                          &kNC_BookmarkCommand_NewFolder);
        gRDF->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "command?cmd=newseparator"),
                          &kNC_BookmarkCommand_NewSeparator);
        gRDF->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "command?cmd=deletebookmark"),
                          &kNC_BookmarkCommand_DeleteBookmark);
        gRDF->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "command?cmd=deletebookmarkfolder"),
                          &kNC_BookmarkCommand_DeleteBookmarkFolder);
        gRDF->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "command?cmd=deletebookmarkseparator"),
                          &kNC_BookmarkCommand_DeleteBookmarkSeparator);
        gRDF->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "command?cmd=setpersonaltoolbarfolder"),
                          &kNC_BookmarkCommand_SetPersonalToolbarFolder);
        gRDF->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "command?cmd=import"),
                          &kNC_BookmarkCommand_Import);
        gRDF->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "command?cmd=export"),
                          &kNC_BookmarkCommand_Export);
        gRDF->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "command?cmd=refreshlivemark"),
                          &kNC_BookmarkCommand_RefreshLivemark);

        /* RSS Resources */
        gRDF->GetResource(NS_LITERAL_CSTRING(RSS09_NAMESPACE_URI "channel"),
                          &kRSS09_channel);
        gRDF->GetResource(NS_LITERAL_CSTRING(RSS09_NAMESPACE_URI "item"),
                          &kRSS09_item);
        gRDF->GetResource(NS_LITERAL_CSTRING(RSS09_NAMESPACE_URI "title"),
                          &kRSS09_title);
        gRDF->GetResource(NS_LITERAL_CSTRING(RSS09_NAMESPACE_URI "link"),
                          &kRSS09_link);

        gRDF->GetResource(NS_LITERAL_CSTRING(RSS10_NAMESPACE_URI "channel"),
                          &kRSS10_channel);
        gRDF->GetResource(NS_LITERAL_CSTRING(RSS10_NAMESPACE_URI "items"),
                          &kRSS10_items);
        gRDF->GetResource(NS_LITERAL_CSTRING(RSS10_NAMESPACE_URI "title"),
                          &kRSS10_title);
        gRDF->GetResource(NS_LITERAL_CSTRING(RSS10_NAMESPACE_URI "link"),
                          &kRSS10_link);
    }
    return NS_OK;
}



static void
bm_ReleaseGlobals()
{
    if (--gRefCnt == 0)
    {
        if (gRDF)
        {
            nsServiceManager::ReleaseService(kRDFServiceCID, gRDF);
            gRDF = nsnull;
        }

        if (gRDFC)
        {
            nsServiceManager::ReleaseService(kRDFContainerUtilsCID, gRDFC);
            gRDFC = nsnull;
        }

        if (gCharsetAlias)
        {
            nsServiceManager::ReleaseService(kCharsetAliasCID, gCharsetAlias);
            gCharsetAlias = nsnull;
        }

        NS_IF_RELEASE(kNC_Bookmark);
        NS_IF_RELEASE(kNC_BookmarkSeparator);
        NS_IF_RELEASE(kNC_BookmarkAddDate);
        NS_IF_RELEASE(kNC_BookmarksTopRoot);
        NS_IF_RELEASE(kNC_BookmarksRoot);
        NS_IF_RELEASE(kNC_LastModifiedFoldersRoot);
        NS_IF_RELEASE(kNC_child);
        NS_IF_RELEASE(kNC_Description);
        NS_IF_RELEASE(kNC_Folder);
        NS_IF_RELEASE(kNC_IEFavorite);
        NS_IF_RELEASE(kNC_IEFavoriteFolder);
        NS_IF_RELEASE(kNC_IEFavoritesRoot);
        NS_IF_RELEASE(kNC_SystemBookmarksStaticRoot);
        NS_IF_RELEASE(kNC_Name);
        NS_IF_RELEASE(kNC_Icon);
        NS_IF_RELEASE(kNC_BookmarksToolbarFolder);
        NS_IF_RELEASE(kNC_ShortcutURL);
        NS_IF_RELEASE(kNC_FeedURL);
        NS_IF_RELEASE(kNC_URL);
        NS_IF_RELEASE(kNC_WebPanel);
        NS_IF_RELEASE(kNC_PostData);
        NS_IF_RELEASE(kNC_Livemark);
        NS_IF_RELEASE(kNC_LivemarkLock);
        NS_IF_RELEASE(kNC_LivemarkExpiration);
        NS_IF_RELEASE(kRDF_type);
        NS_IF_RELEASE(kRDF_nextVal);
        NS_IF_RELEASE(kWEB_LastModifiedDate);
        NS_IF_RELEASE(kWEB_LastVisitDate);
        NS_IF_RELEASE(kNC_Parent);
        NS_IF_RELEASE(kTrueLiteral);
        NS_IF_RELEASE(kWEB_Schedule);
        NS_IF_RELEASE(kWEB_ScheduleActive);
        NS_IF_RELEASE(kWEB_Status);
        NS_IF_RELEASE(kWEB_LastPingDate);
        NS_IF_RELEASE(kWEB_LastPingETag);
        NS_IF_RELEASE(kWEB_LastPingModDate);
        NS_IF_RELEASE(kWEB_LastPingContentLen);
        NS_IF_RELEASE(kWEB_LastCharset);
        NS_IF_RELEASE(kNC_BookmarkCommand_NewBookmark);
        NS_IF_RELEASE(kNC_BookmarkCommand_NewFolder);
        NS_IF_RELEASE(kNC_BookmarkCommand_NewSeparator);
        NS_IF_RELEASE(kNC_BookmarkCommand_DeleteBookmark);
        NS_IF_RELEASE(kNC_BookmarkCommand_DeleteBookmarkFolder);
        NS_IF_RELEASE(kNC_BookmarkCommand_DeleteBookmarkSeparator);
        NS_IF_RELEASE(kNC_BookmarkCommand_SetPersonalToolbarFolder);
        NS_IF_RELEASE(kNC_BookmarkCommand_Import);
        NS_IF_RELEASE(kNC_BookmarkCommand_Export);
        NS_IF_RELEASE(kNC_BookmarkCommand_RefreshLivemark);

        NS_IF_RELEASE(kRSS09_channel);
        NS_IF_RELEASE(kRSS09_item);
        NS_IF_RELEASE(kRSS09_title);
        NS_IF_RELEASE(kRSS09_link);

        NS_IF_RELEASE(kRSS10_channel);
        NS_IF_RELEASE(kRSS10_items);
        NS_IF_RELEASE(kRSS10_title);
        NS_IF_RELEASE(kRSS10_link);
    }
}


class nsSpillableStackBuffer
{
protected:

    enum {
        kStackBufferSize = 256
    };

public:

    nsSpillableStackBuffer() :
        mBufferPtr(mBuffer),
        mCurCapacity(kStackBufferSize)
    { }

    ~nsSpillableStackBuffer()
    {
        DeleteBuffer();
    }

    PRBool EnsureCapacity(PRInt32 inCharsCapacity)
    {
        if (inCharsCapacity < mCurCapacity)
            return PR_TRUE;
    
        if (inCharsCapacity > kStackBufferSize)
        {
            DeleteBuffer();
            mBufferPtr = (PRUnichar*)nsMemory::Alloc(inCharsCapacity * sizeof(PRUnichar));
            mCurCapacity = inCharsCapacity;
            return (mBufferPtr != NULL);
        }
    
        mCurCapacity = kStackBufferSize;
        return PR_TRUE;
    }
                
    PRUnichar*  GetBuffer()     { return mBufferPtr;    }
    PRInt32     GetCapacity()   { return mCurCapacity;  }

protected:

    void DeleteBuffer()
    {
        if (mBufferPtr != mBuffer)
        {
            nsMemory::Free(mBufferPtr);
            mBufferPtr = mBuffer;
        }                
    }
  
protected:

    PRUnichar     *mBufferPtr;
    PRUnichar     mBuffer[kStackBufferSize];
    PRInt32       mCurCapacity;

};


////////////////////////////////////////////////////////////////////////

/**
 * The bookmark parser knows how to read <tt>bookmarks.html</tt> and convert it
 * into an RDF graph.
 */
class BookmarkParser {
private:
    nsCOMPtr<nsIUnicodeDecoder> mUnicodeDecoder;
    nsIRDFDataSource*           mDataSource;
    nsCString                   mIEFavoritesRoot;
    PRBool                      mFoundIEFavoritesRoot;
    PRBool                      mFoundPersonalToolbarFolder;
    PRBool                      mIsImportOperation;
    char*                       mContents;
    PRUint32                    mContentsLen;
    PRInt32                     mStartOffset;
    nsCOMPtr<nsIInputStream>    mInputStream;

    friend  class nsBookmarksService;

protected:
    struct BookmarkField
    {
        const char      *mName;
        const char      *mPropertyName;
        nsIRDFResource  *mProperty;
        nsresult        (*mParse)(nsIRDFResource *arc, nsString& aValue, nsIRDFNode** aResult);
        nsIRDFNode      *mValue;
    };

    static BookmarkField gBookmarkFieldTable[];
    static BookmarkField gBookmarkHeaderFieldTable[];

    nsresult AssertTime(nsIRDFResource* aSource,
                        nsIRDFResource* aLabel,
                        PRInt32 aTime);

    nsresult Unescape(nsString &text);

    nsresult ParseMetaTag(const nsString &aLine, nsIUnicodeDecoder **decoder);

    nsresult ParseBookmarkInfo(BookmarkField *fields, PRBool isBookmarkFlag, const nsString &aLine,
                               const nsCOMPtr<nsIRDFContainer> &aContainer,
                               nsIRDFResource *nodeType, nsCOMPtr<nsIRDFResource> &bookmarkNode);

    nsresult ParseBookmarkSeparator(const nsString &aLine,
                                    const nsCOMPtr<nsIRDFContainer> &aContainer);

    nsresult ParseHeaderBegin(const nsString &aLine,
                              const nsCOMPtr<nsIRDFContainer> &aContainer);

    nsresult ParseHeaderEnd(const nsString &aLine);

    PRInt32 getEOL(const char *whole, PRInt32 startOffset, PRInt32 totalLength);

    nsresult updateAtom(nsIRDFDataSource *db, nsIRDFResource *src,
                        nsIRDFResource *prop, nsIRDFNode *newValue, PRBool *dirtyFlag);

    static    nsresult ParseResource(nsIRDFResource *arc, nsString& aValue, nsIRDFNode** aResult);
    static    nsresult ParseLiteral(nsIRDFResource *arc, nsString& aValue, nsIRDFNode** aResult);
    static    nsresult ParseDate(nsIRDFResource *arc, nsString& aValue, nsIRDFNode** aResult);

public:
    BookmarkParser();
    ~BookmarkParser();

    nsresult Init(nsIFile *aFile, nsIRDFDataSource *aDataSource, 
                  PRBool aIsImportOperation = PR_FALSE);
    nsresult DecodeBuffer(nsString &line, char *buf, PRUint32 aLength);
    nsresult ProcessLine(nsIRDFContainer *aContainer, nsIRDFResource *nodeType,
                         nsCOMPtr<nsIRDFResource> &bookmarkNode, const nsString &line,
                         nsString &description, PRBool &inDescription, PRBool &isActiveFlag);
    nsresult Parse(nsIRDFResource* aContainer, nsIRDFResource *nodeType);

    nsresult SetIEFavoritesRoot(const nsCString& IEFavoritesRootURL)
    {
        mIEFavoritesRoot = IEFavoritesRootURL;
        return NS_OK;
    }
    nsresult ParserFoundIEFavoritesRoot(PRBool *foundIEFavoritesRoot)
    {
        *foundIEFavoritesRoot = mFoundIEFavoritesRoot;
        return NS_OK;
    }
    nsresult ParserFoundPersonalToolbarFolder(PRBool *foundPersonalToolbarFolder)
    {
        *foundPersonalToolbarFolder = mFoundPersonalToolbarFolder;
        return NS_OK;
    }
};

BookmarkParser::BookmarkParser() :
    mContents(nsnull),
    mContentsLen(0L),
    mStartOffset(0L)
{
    bm_AddRefGlobals();
}

static const char kOpenAnchor[]    = "<A ";
static const char kCloseAnchor[]   = "</A>";

static const char kOpenHeading[]   = "<H";
static const char kCloseHeading[]  = "</H";

static const char kSeparator[]     = "<HR";

static const char kOpenUL[]        = "<UL>";
static const char kCloseUL[]       = "</UL>";

static const char kOpenMenu[]      = "<MENU>";
static const char kCloseMenu[]     = "</MENU>";

static const char kOpenDL[]        = "<DL>";
static const char kCloseDL[]       = "</DL>";

static const char kOpenDD[]        = "<DD>";

static const char kOpenMeta[]      = "<META ";

static const char kPersonalToolbarFolderEquals[]  = "PERSONAL_TOOLBAR_FOLDER=\"";

static const char kNameEquals[]            = "NAME=\"";
static const char kHREFEquals[]            = "HREF=\"";
static const char kTargetEquals[]          = "TARGET=\"";
static const char kAddDateEquals[]         = "ADD_DATE=\"";
static const char kLastVisitEquals[]       = "LAST_VISIT=\"";
static const char kLastModifiedEquals[]    = "LAST_MODIFIED=\"";
static const char kLastCharsetEquals[]     = "LAST_CHARSET=\"";
static const char kShortcutURLEquals[]     = "SHORTCUTURL=\"";
static const char kFeedURLEquals[]         = "FEEDURL=\"";
static const char kIconEquals[]            = "ICON=\"";
static const char kWebPanelEquals[]        = "WEB_PANEL=\"";
static const char kPostDataEquals[]        = "POST_DATA=\"";
static const char kScheduleEquals[]        = "SCHEDULE=\"";
static const char kLastPingEquals[]        = "LAST_PING=\"";
static const char kPingETagEquals[]        = "PING_ETAG=\"";
static const char kPingLastModEquals[]     = "PING_LAST_MODIFIED=\"";
static const char kPingContentLenEquals[]  = "PING_CONTENT_LEN=\"";
static const char kPingStatusEquals[]      = "PING_STATUS=\"";
static const char kIDEquals[]              = "ID=\"";
static const char kContentEquals[]         = "CONTENT=\"";
static const char kHTTPEquivEquals[]       = "HTTP-EQUIV=\"";
static const char kCharsetEquals[]         = "charset=";        // note: no quote

nsresult
BookmarkParser::Init(nsIFile *aFile, nsIRDFDataSource *aDataSource, 
                     PRBool aIsImportOperation)
{
    mDataSource = aDataSource;
    mFoundIEFavoritesRoot = PR_FALSE;
    mFoundPersonalToolbarFolder = aIsImportOperation;
    mIsImportOperation = aIsImportOperation;

    nsresult rv;

    // determine default platform charset...
    nsCOMPtr<nsIPlatformCharset> platformCharset = 
        do_GetService(kPlatformCharsetCID, &rv);
    if (NS_SUCCEEDED(rv) && (platformCharset))
    {
        nsCAutoString    defaultCharset;
        if (NS_SUCCEEDED(rv = platformCharset->GetCharset(kPlatformCharsetSel_4xBookmarkFile, defaultCharset)))
        {
            // found the default platform charset, now try and get a decoder from it to Unicode
            nsCOMPtr<nsICharsetConverterManager> charsetConv = 
                do_GetService(kCharsetConverterManagerCID, &rv);
            if (NS_SUCCEEDED(rv) && (charsetConv))
            {
                rv = charsetConv->GetUnicodeDecoderRaw(defaultCharset.get(),
                                                       getter_AddRefs(mUnicodeDecoder));
            }
        }
    }

    nsCAutoString   str;
    BookmarkField   *field;
    for (field = gBookmarkFieldTable; field->mName; ++field)
    {
        str = field->mPropertyName;
        rv = gRDF->GetResource(str, &field->mProperty);
        if (NS_FAILED(rv))  return rv;
    }
    for (field = gBookmarkHeaderFieldTable; field->mName; ++field)
    {
        str = field->mPropertyName;
        rv = gRDF->GetResource(str, &field->mProperty);
        if (NS_FAILED(rv))  return rv;
    }

    if (aFile)
    {
        PRInt64 contentsLen;
        rv = aFile->GetFileSize(&contentsLen);
        NS_ENSURE_SUCCESS(rv, rv);

        if(LL_CMP(contentsLen, >, LL_INIT(0,0xFFFFFFFE)))
            return NS_ERROR_FILE_TOO_BIG;

        LL_L2UI(mContentsLen, contentsLen);

        if (mContentsLen > 0)
        {
            mContents = new char [mContentsLen + 1];
            if (mContents)
            {
                nsCOMPtr<nsIInputStream> inputStream;
                rv = NS_NewLocalFileInputStream(getter_AddRefs(inputStream),
                                                aFile,
                                                PR_RDONLY,
                                                -1, 0);

                if (NS_FAILED(rv))
                {
                    delete [] mContents;
                    mContents = nsnull;
                }
                else
                {
                    PRUint32 howMany;
                    rv = inputStream->Read(mContents, mContentsLen, &howMany);
                    if (NS_FAILED(rv))
                    {
                        delete [] mContents;
                        mContents = nsnull;
                        return NS_OK;
                    }

                    if (howMany == mContentsLen)
                    {
                        mContents[mContentsLen] = '\0';
                    }
                    else
                    {
                        delete [] mContents;
                        mContents = nsnull;
                    }
                }                    
            }
        }

        if (!mContents)
        {
            // we were unable to read in the entire bookmark file at once,
            // so let's try reading it in a bit at a time instead

            rv = NS_NewLocalFileInputStream(getter_AddRefs(mInputStream),
                                            aFile, PR_RDONLY, -1, 0);
            NS_ENSURE_SUCCESS(rv, rv);
        }
    }

    return NS_OK;
}

BookmarkParser::~BookmarkParser()
{
    if (mContents)
    {
        delete [] mContents;
        mContents = nsnull;
    }
    if (mInputStream)
    {
        mInputStream->Close();
    }
    BookmarkField   *field;
    for (field = gBookmarkFieldTable; field->mName; ++field)
    {
        NS_IF_RELEASE(field->mProperty);
    }
    for (field = gBookmarkHeaderFieldTable; field->mName; ++field)
    {
        NS_IF_RELEASE(field->mProperty);
    }
    bm_ReleaseGlobals();
}

PRInt32
BookmarkParser::getEOL(const char *whole, PRInt32 startOffset, PRInt32 totalLength)
{
    PRInt32 eolOffset = -1;

    while (startOffset < totalLength)
    {
        char c;
        c = whole[startOffset];
        if ((c == '\n') || (c == '\r') || (c == '\0'))
        {
            eolOffset = startOffset;
            break;
        }
        ++startOffset;
    }
    return eolOffset;
}

nsresult
BookmarkParser::DecodeBuffer(nsString &line, char *buf, PRUint32 aLength)
{
    if (mUnicodeDecoder)
    {
        nsresult    rv;
        char        *aBuffer = buf;
        PRInt32     unicharBufLen = 0;
        mUnicodeDecoder->GetMaxLength(aBuffer, aLength, &unicharBufLen);
        
        nsSpillableStackBuffer    stackBuffer;
        if (!stackBuffer.EnsureCapacity(unicharBufLen + 1))
          return NS_ERROR_OUT_OF_MEMORY;
        
        do
        {
            PRInt32     srcLength = aLength;
            PRInt32     unicharLength = unicharBufLen;
            PRUnichar *unichars = stackBuffer.GetBuffer();
            
            rv = mUnicodeDecoder->Convert(aBuffer, &srcLength, stackBuffer.GetBuffer(), &unicharLength);
            unichars[unicharLength]=0;  //add this since the unicode converters can't be trusted to do so.

            // Move the nsParser.cpp 00 -> space hack to here so it won't break UCS2 file

            // Hack Start
            for(PRInt32 i=0;i<unicharLength-1; i++)
                if(0x0000 == unichars[i])   unichars[i] = 0x0020;
            // Hack End

            line.Append(unichars, unicharLength);
            // if we failed, we consume one byte by replace it with U+FFFD
            // and try conversion again.
            if(NS_FAILED(rv))
            {
                mUnicodeDecoder->Reset();
                line.Append( (PRUnichar)0xFFFD);
                if(((PRUint32) (srcLength + 1)) > (PRUint32)aLength)
                    srcLength = aLength;
                else 
                    srcLength++;
                aBuffer += srcLength;
                aLength -= srcLength;
            }
        } while (NS_FAILED(rv) && (aLength > 0));

    }
    else
    {
        line.AppendWithConversion(buf, aLength);
    }
    return NS_OK;
}

nsresult
BookmarkParser::ProcessLine(nsIRDFContainer *container, nsIRDFResource *nodeType,
            nsCOMPtr<nsIRDFResource> &bookmarkNode, const nsString &line,
            nsString &description, PRBool &inDescription, PRBool &isActiveFlag)
{
    nsresult    rv = NS_OK;
    PRInt32     offset;

    if (inDescription == PR_TRUE)
    {
        offset = line.FindChar('<');
        if (offset < 0)
        {
            if (!description.IsEmpty())
            {
                description.Append(PRUnichar('\n'));
            }
            description += line;
            return NS_OK;
        }

        Unescape(description);

        if (bookmarkNode)
        {
            nsCOMPtr<nsIRDFLiteral> descLiteral;
            if (NS_SUCCEEDED(rv = gRDF->GetLiteral(description.get(), getter_AddRefs(descLiteral))))
            {
                rv = mDataSource->Assert(bookmarkNode, kNC_Description, descLiteral, PR_TRUE);
            }
        }

        inDescription = PR_FALSE;
        description.Truncate();
    }

    if ((offset = line.Find(kFeedURLEquals, PR_TRUE)) >= 0)
    {
        rv = ParseBookmarkInfo(gBookmarkFieldTable, PR_TRUE, line, container, kNC_Livemark, bookmarkNode);
    }
    else if ((offset = line.Find(kHREFEquals, PR_TRUE)) >= 0)
    {
        rv = ParseBookmarkInfo(gBookmarkFieldTable, PR_TRUE, line, container, nodeType, bookmarkNode);
    }
    else if ((offset = line.Find(kOpenMeta, PR_TRUE)) >= 0)
    {
        rv = ParseMetaTag(line, getter_AddRefs(mUnicodeDecoder));
    }
    else if ((offset = line.Find(kOpenHeading, PR_TRUE)) >= 0 &&
         nsCRT::IsAsciiDigit(line.CharAt(offset + 2)))
    {
        nsCOMPtr<nsIRDFResource>    dummy;
        if (line.CharAt(offset + 2) != PRUnichar('1'))
        {
            rv = ParseBookmarkInfo(gBookmarkHeaderFieldTable, PR_FALSE, line, container, nodeType, dummy);
        } else {
            // this is H1, i.e. the bookmarks root.  We use
            // kNC_BookmarksRoot as the nodeType to tell
            // ParseBookmarkInfo to do some magic
            rv = ParseBookmarkInfo(gBookmarkHeaderFieldTable, PR_FALSE, line, container, kNC_BookmarksRoot, dummy);
        }
    }
    else if ((offset = line.Find(kSeparator, PR_TRUE)) >= 0)
    {
        rv = ParseBookmarkSeparator(line, container);
    }
    else if ((offset = line.Find(kCloseUL, PR_TRUE)) >= 0 ||
         (offset = line.Find(kCloseMenu, PR_TRUE)) >= 0 ||
         (offset = line.Find(kCloseDL, PR_TRUE)) >= 0)
    {
        isActiveFlag = PR_FALSE;
        return ParseHeaderEnd(line);
    }
    else if ((offset = line.Find(kOpenUL, PR_TRUE)) >= 0 ||
         (offset = line.Find(kOpenMenu, PR_TRUE)) >= 0 ||
         (offset = line.Find(kOpenDL, PR_TRUE)) >= 0)
    {
        rv = ParseHeaderBegin(line, container);
    }
    else if ((offset = line.Find(kOpenDD, PR_TRUE)) >= 0)
    {
        inDescription = PR_TRUE;
        description = line;
        description.Cut(0, offset+sizeof(kOpenDD)-1);
    }
    else
    {
        // XXX Discard the line?
    }
    return rv;
}



nsresult
BookmarkParser::Parse(nsIRDFResource *aContainer, nsIRDFResource *aNodeType)
{
    // tokenize the input stream.
    // XXX this needs to handle quotes, etc. it'd be nice to use the real parser for this...
    nsresult            rv;

    nsCOMPtr<nsIRDFContainer> container;
    rv = nsComponentManager::CreateInstance(kRDFContainerCID,
                        nsnull,
                        NS_GET_IID(nsIRDFContainer),
                        getter_AddRefs(container));
    if (NS_FAILED(rv)) return rv;

    rv = container->Init(mDataSource, aContainer);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIRDFResource> bookmarkNode = aContainer;
    nsAutoString description, line;
    nsCAutoString cLine;
    PRBool       isActiveFlag = PR_TRUE, inDescriptionFlag = PR_FALSE;

    if ((mContents) && (mContentsLen > 0))
    {
        // we were able to read the entire bookmark file into memory, so process it
        char                *linePtr;
        PRInt32             eol;

        while ((isActiveFlag == PR_TRUE) && (mStartOffset < (signed)mContentsLen))
        {
            linePtr = &mContents[mStartOffset];
            eol = getEOL(mContents, mStartOffset, mContentsLen);

            PRInt32 aLength;

            if ((eol >= mStartOffset) && (eol < (signed)mContentsLen))
            {
                // mContents[eol] = '\0';
                aLength = eol - mStartOffset;
                mStartOffset = eol + 1;
            }
            else
            {
                aLength = mContentsLen - mStartOffset;
                mStartOffset = mContentsLen + 1;
                isActiveFlag = PR_FALSE;
            }
            if (aLength < 1)    continue;

            line.Truncate();
            DecodeBuffer(line, linePtr, aLength);

            rv = ProcessLine(container, aNodeType, bookmarkNode,
                line, description, inDescriptionFlag, isActiveFlag);
            if (NS_FAILED(rv))  break;
        }
    }
    else
    {
        NS_ENSURE_TRUE(mInputStream, NS_ERROR_NULL_POINTER);

        // we were unable to read in the entire bookmark file at once,
        // so let's try reading it in a bit at a time instead, and process it
        nsCOMPtr<nsILineInputStream> lineInputStream =
            do_QueryInterface(mInputStream);
        NS_ENSURE_TRUE(lineInputStream, NS_NOINTERFACE);

        PRBool moreData = PR_TRUE;

        while(NS_SUCCEEDED(rv) && isActiveFlag && moreData)
        {
            rv = lineInputStream->ReadLine(cLine, &moreData);
            CopyASCIItoUTF16(cLine, line);

            if (NS_SUCCEEDED(rv))
            {
                rv = ProcessLine(container, aNodeType, bookmarkNode,
                    line, description, inDescriptionFlag, isActiveFlag);
            }
        }
    }
    return rv;
}

nsresult
BookmarkParser::Unescape(nsString &text)
{
    // convert some HTML-escaped (such as "&lt;") values back

    PRInt32     offset=0;

    while((offset = text.FindChar((PRUnichar('&')), offset)) >= 0)
    {
        if (Substring(text, offset, 4).Equals(NS_LITERAL_STRING("&lt;"), nsCaseInsensitiveStringComparator()))
        {
            text.Cut(offset, 4);
            text.Insert(PRUnichar('<'), offset);
        }
        else if (Substring(text, offset, 4).Equals(NS_LITERAL_STRING("&gt;"), nsCaseInsensitiveStringComparator()))
        {
            text.Cut(offset, 4);
            text.Insert(PRUnichar('>'), offset);
        }
        else if (Substring(text, offset, 5).Equals(NS_LITERAL_STRING("&amp;"), nsCaseInsensitiveStringComparator()))
        {
            text.Cut(offset, 5);
            text.Insert(PRUnichar('&'), offset);
        }
        else if (Substring(text, offset, 6).Equals(NS_LITERAL_STRING("&quot;"), nsCaseInsensitiveStringComparator()))
        {
            text.Cut(offset, 6);
            text.Insert(PRUnichar('\"'), offset);
        }
        else if (Substring(text, offset, 5).Equals(NS_LITERAL_STRING("&#39;")))
        {
            text.Cut(offset, 5);
            text.Insert(PRUnichar('\''), offset);
        }

        ++offset;
    }
    return NS_OK;
}

nsresult
BookmarkParser::ParseMetaTag(const nsString &aLine, nsIUnicodeDecoder **decoder)
{
    nsresult    rv = NS_OK;

    *decoder = nsnull;

    // get the HTTP-EQUIV attribute
    PRInt32 start = aLine.Find(kHTTPEquivEquals, PR_TRUE);
    NS_ASSERTION(start >= 0, "no 'HTTP-EQUIV=\"' string: how'd we get here?");
    if (start < 0)  return NS_ERROR_UNEXPECTED;
    // Skip past the first double-quote
    start += (sizeof(kHTTPEquivEquals) - 1);
    // ...and find the next so we can chop the HTTP-EQUIV attribute
    PRInt32 end = aLine.FindChar(PRUnichar('"'), start);
    nsAutoString    httpEquiv;
    aLine.Mid(httpEquiv, start, end - start);

    // if HTTP-EQUIV isn't "Content-Type", just ignore the META tag
    if (!httpEquiv.EqualsIgnoreCase("Content-Type"))
        return NS_OK;

    // get the CONTENT attribute
    start = aLine.Find(kContentEquals, PR_TRUE);
    NS_ASSERTION(start >= 0, "no 'CONTENT=\"' string: how'd we get here?");
    if (start < 0)  return NS_ERROR_UNEXPECTED;
    // Skip past the first double-quote
    start += (sizeof(kContentEquals) - 1);
    // ...and find the next so we can chop the CONTENT attribute
    end = aLine.FindChar(PRUnichar('"'), start);
    nsAutoString    content;
    aLine.Mid(content, start, end - start);

    // look for the charset value
    start = content.Find(kCharsetEquals, PR_TRUE);
    NS_ASSERTION(start >= 0, "no 'charset=' string: how'd we get here?");
    if (start < 0)  return NS_ERROR_UNEXPECTED;
    start += (sizeof(kCharsetEquals)-1);
    nsCAutoString    charset;
    charset.AssignWithConversion(Substring(content, start, content.Length() - start));
    if (charset.Length() < 1)   return NS_ERROR_UNEXPECTED;

    // found a charset, now try and get a decoder from it to Unicode
    nsICharsetConverterManager  *charsetConv = nsnull;
    rv = nsServiceManager::GetService(kCharsetConverterManagerCID, 
            NS_GET_IID(nsICharsetConverterManager), 
            (nsISupports**)&charsetConv);
    if (NS_SUCCEEDED(rv) && (charsetConv))
    {
        rv = charsetConv->GetUnicodeDecoderRaw(charset.get(), decoder);
        NS_RELEASE(charsetConv);
    }
    return rv;
}

BookmarkParser::BookmarkField
BookmarkParser::gBookmarkFieldTable[] =
{
  // Note: the first entry MUST be the ID/resource of the bookmark
  { kIDEquals,              NC_NAMESPACE_URI  "ID",                nsnull,  BookmarkParser::ParseResource,  nsnull },
  { kHREFEquals,            NC_NAMESPACE_URI  "URL",               nsnull,  BookmarkParser::ParseLiteral,   nsnull },
  { kAddDateEquals,         NC_NAMESPACE_URI  "BookmarkAddDate",   nsnull,  BookmarkParser::ParseDate,      nsnull },
  { kLastVisitEquals,       WEB_NAMESPACE_URI "LastVisitDate",     nsnull,  BookmarkParser::ParseDate,      nsnull },
  { kLastModifiedEquals,    WEB_NAMESPACE_URI "LastModifiedDate",  nsnull,  BookmarkParser::ParseDate,      nsnull },
  { kShortcutURLEquals,     NC_NAMESPACE_URI  "ShortcutURL",       nsnull,  BookmarkParser::ParseLiteral,   nsnull },
  { kFeedURLEquals,         NC_NAMESPACE_URI  "FeedURL",            nsnull,  BookmarkParser::ParseLiteral,   nsnull },
  { kIconEquals,            NC_NAMESPACE_URI  "Icon",              nsnull,  BookmarkParser::ParseLiteral,   nsnull },
  { kWebPanelEquals,        NC_NAMESPACE_URI  "WebPanel",          nsnull,  BookmarkParser::ParseLiteral,   nsnull },
  { kPostDataEquals,        NC_NAMESPACE_URI  "PostData",          nsnull,  BookmarkParser::ParseLiteral,   nsnull },
  { kLastCharsetEquals,     WEB_NAMESPACE_URI "LastCharset",       nsnull,  BookmarkParser::ParseLiteral,   nsnull },
  { kScheduleEquals,        WEB_NAMESPACE_URI "Schedule",          nsnull,  BookmarkParser::ParseLiteral,   nsnull },
  { kLastPingEquals,        WEB_NAMESPACE_URI "LastPingDate",      nsnull,  BookmarkParser::ParseDate,      nsnull },
  { kPingETagEquals,        WEB_NAMESPACE_URI "LastPingETag",      nsnull,  BookmarkParser::ParseLiteral,   nsnull },
  { kPingLastModEquals,     WEB_NAMESPACE_URI "LastPingModDate",   nsnull,  BookmarkParser::ParseLiteral,   nsnull },
  { kPingContentLenEquals,  WEB_NAMESPACE_URI "LastPingContentLen",nsnull,  BookmarkParser::ParseLiteral,   nsnull },
  { kPingStatusEquals,      WEB_NAMESPACE_URI "status",            nsnull,  BookmarkParser::ParseLiteral,   nsnull },
  // Note: end of table
  { nsnull,                 nsnull,                                nsnull,  nsnull,                         nsnull },
};

BookmarkParser::BookmarkField
BookmarkParser::gBookmarkHeaderFieldTable[] =
{
  // Note: the first entry MUST be the ID/resource of the bookmark
  { kIDEquals,                    NC_NAMESPACE_URI  "ID",                nsnull,  BookmarkParser::ParseResource,  nsnull },
  { kAddDateEquals,               NC_NAMESPACE_URI  "BookmarkAddDate",   nsnull,  BookmarkParser::ParseDate,      nsnull },
  { kLastModifiedEquals,          WEB_NAMESPACE_URI "LastModifiedDate",  nsnull,  BookmarkParser::ParseDate,      nsnull },
  { kPersonalToolbarFolderEquals, NC_NAMESPACE_URI "BookmarksToolbarFolder",          nsnull,  BookmarkParser::ParseLiteral,   nsnull },
  // Note: end of table
  { nsnull,                       nsnull,                                nsnull,  nsnull,                         nsnull },
};

nsresult
BookmarkParser::ParseBookmarkInfo(BookmarkField *fields, PRBool isBookmarkFlag,
                const nsString &aLine, const nsCOMPtr<nsIRDFContainer> &aContainer,
                nsIRDFResource *aNodeType, nsCOMPtr<nsIRDFResource> &bookmarkNode)
{
    NS_PRECONDITION(aContainer != nsnull, "null ptr");
    if (! aContainer)
        return NS_ERROR_NULL_POINTER;

    bookmarkNode = nsnull;

    PRInt32     lineLen = aLine.Length();

    PRInt32     attrStart=0;
    if (isBookmarkFlag == PR_TRUE)
    {
        attrStart = aLine.Find(kOpenAnchor, PR_TRUE, attrStart);
        if (attrStart < 0)  return NS_ERROR_UNEXPECTED;
        attrStart += sizeof(kOpenAnchor)-1;
    }
    else
    {
        attrStart = aLine.Find(kOpenHeading, PR_TRUE, attrStart);
        if (attrStart < 0)  return NS_ERROR_UNEXPECTED;
        attrStart += sizeof(kOpenHeading)-1;
    }

    // free up any allocated data in field table BEFORE processing
    for (BookmarkField *preField = fields; preField->mName; ++preField)
    {
        NS_IF_RELEASE(preField->mValue);
    }

    // loop over attributes
    while((attrStart < lineLen) && (aLine[attrStart] != '>'))
    {
        while(nsCRT::IsAsciiSpace(aLine[attrStart]))   ++attrStart;

        PRBool  fieldFound = PR_FALSE;

        nsAutoString id;
        id.AssignWithConversion(kIDEquals);
        for (BookmarkField *field = fields; field->mName; ++field)
        {
            nsAutoString name;
            name.AssignWithConversion(field->mName);
            if (mIsImportOperation && name.Equals(id)) 
                // For import operations, we don't want to save the unique
                // identifier for folders, because this can cause bugs like 
                // 74969 (importing duplicate bookmark folder hierachy causes 
                // bookmarks file to grow infinitely).
                continue;

            if (field->mProperty == kNC_BookmarksToolbarFolder
                && mFoundPersonalToolbarFolder)
                // We don't want to assert a BTF arc twice.
                continue;
  
            if (aLine.Find(field->mName, PR_TRUE, attrStart, 1) == attrStart)
            {
                attrStart += strlen(field->mName);

                // skip to terminating quote of string
                PRInt32 termQuote = aLine.FindChar(PRUnichar('\"'), attrStart);
                if (termQuote > attrStart)
                {
                    // process data
                    nsAutoString    data;
                    aLine.Mid(data, attrStart, termQuote-attrStart);

                    attrStart = termQuote + 1;
                    fieldFound = PR_TRUE;

                    if (!data.IsEmpty())
                    {
                        // XXX Bug 58421 We should not ever hit this assertion
                        NS_ASSERTION(!field->mValue, "Field already has a value");
                        // but prevent a leak if we do hit it
                        NS_IF_RELEASE(field->mValue);

                        nsresult rv = (*field->mParse)(field->mProperty, data, &field->mValue);
                        if (NS_FAILED(rv)) break;
                    }
                }
                break;
            }
        }

        if (fieldFound == PR_FALSE)
        {
            // skip to next attribute
            while((attrStart < lineLen) && (aLine[attrStart] != '>') &&
                (!nsCRT::IsAsciiSpace(aLine[attrStart])))
            {
                ++attrStart;
            }
        }
    }

    nsresult    rv;

    // the root doesn't have an ID, but we fake
    // it here
    if (aNodeType == kNC_BookmarksRoot) {
        NS_ADDREF(kNC_BookmarksRoot);
        fields[0].mValue = kNC_BookmarksRoot;
    }

    // Note: the first entry MUST be the ID/resource of the bookmark
    nsCOMPtr<nsIRDFResource> bookmark = do_QueryInterface(fields[0].mValue);
    if (!bookmark)
    {
        // We've never seen this bookmark/folder before. Assign it an anonymous ID
        rv = gRDF->GetAnonymousResource(getter_AddRefs(bookmark));
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create anonymous resource for folder");
    }

    if (bookmark)
    {
        const char* bookmarkURI;
        bookmark->GetValueConst(&bookmarkURI);

        bookmarkNode = bookmark;

        // assert appropriate node type
        PRBool isIEFavoriteRoot = PR_FALSE;
        if (!mIEFavoritesRoot.IsEmpty())
        {
            if (!nsCRT::strcmp(mIEFavoritesRoot.get(), bookmarkURI))
            {
                mFoundIEFavoritesRoot = PR_TRUE;
                isIEFavoriteRoot = PR_TRUE;
            }
        }
        if ((isIEFavoriteRoot == PR_TRUE) || ((aNodeType == kNC_IEFavorite) && (isBookmarkFlag == PR_FALSE)))
        {
            rv = mDataSource->Assert(bookmark, kRDF_type, kNC_IEFavoriteFolder, PR_TRUE);
        }
        else if (aNodeType == kNC_IEFavorite ||
                 aNodeType == kNC_IEFavoriteFolder ||
                 aNodeType == kNC_BookmarkSeparator ||
                 aNodeType == kNC_Livemark)
        {
            rv = mDataSource->Assert(bookmark, kRDF_type, aNodeType, PR_TRUE);
            if (aNodeType == kNC_Livemark) {
                /* And make it a sequence, if livemark -- we don't add anything
                 * into the sequence here */
                rv = gRDFC->MakeSeq(mDataSource, bookmark, nsnull);
            }
        }

        // process data
        for (BookmarkField *field = fields; field->mName; ++field)
        {
            if (field->mValue && field->mProperty)
            {
                updateAtom(mDataSource, bookmark, field->mProperty,
                           field->mValue, nsnull);
                if (field->mProperty == kNC_BookmarksToolbarFolder)
                    mFoundPersonalToolbarFolder = PR_TRUE;
            }
        }

        // look for bookmark name (and unescape)
        if (aLine[attrStart] == '>')
        {
            PRInt32 nameEnd;
            if (isBookmarkFlag == PR_TRUE)
                nameEnd = aLine.Find(kCloseAnchor, PR_TRUE, ++attrStart);
            else
                nameEnd = aLine.Find(kCloseHeading, PR_TRUE, ++attrStart);

            if (nameEnd > attrStart)
            {
                nsAutoString    name;
                aLine.Mid(name, attrStart, nameEnd-attrStart);
                if (!name.IsEmpty())
                {
                    Unescape(name);

                    nsCOMPtr<nsIRDFNode>    nameNode;
                    rv = ParseLiteral(kNC_Name, name, getter_AddRefs(nameNode));
                    if (NS_SUCCEEDED(rv) && nameNode)
                        updateAtom(mDataSource, bookmark, kNC_Name, nameNode, nsnull);
                }
            }
        }

        if (aNodeType == kNC_BookmarksRoot) {
            // we're done here if it's the root; free up the field values
            // and get out of here.  Note that we ADDREF'd kNC_BookmarksRoot
            // above, so we can RELEASE it here safely.
            for (BookmarkField *postField = fields; postField->mName; ++postField)
            {
                NS_IF_RELEASE(postField->mValue);
            }

            return NS_OK;
        }

        if (isBookmarkFlag == PR_FALSE)
        {
            rv = gRDFC->MakeSeq(mDataSource, bookmark, nsnull);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to make new folder as sequence");
            if (NS_SUCCEEDED(rv))
            {
                // And now recursively parse the rest of the file...
                rv = Parse(bookmark, aNodeType);
                NS_ASSERTION(NS_SUCCEEDED(rv), "unable to parse bookmarks");
            }
        }

        // prevent duplicates                                                       
        PRInt32 aIndex;                                                             
        nsCOMPtr<nsIRDFResource> containerRes;
        aContainer->GetResource(getter_AddRefs(containerRes));
        if (containerRes && NS_SUCCEEDED(gRDFC->IndexOf(mDataSource, containerRes, bookmark, &aIndex)) &&                 
            (aIndex < 0))                                                           
        {                                                                           
          // The last thing we do is add the bookmark to the container.           
          // This ensures the minimal amount of reflow.                           
          rv = aContainer->AppendElement(bookmark);                               
          NS_ASSERTION(NS_SUCCEEDED(rv), "unable to add bookmark to container");  
        }      
    }

    // free up any allocated data in field table AFTER processing
    for (BookmarkField *postField = fields; postField->mName; ++postField)
    {
        NS_IF_RELEASE(postField->mValue);
    }

    return NS_OK;
}

nsresult
BookmarkParser::ParseResource(nsIRDFResource *arc, nsString& url, nsIRDFNode** aResult)
{
    *aResult = nsnull;

    if (arc == kNC_URL)
    {
        // Now do properly replace %22's; this is particularly important for javascript: URLs
        static const char kEscape22[] = "%22";
        PRInt32 offset;
        while ((offset = url.Find(kEscape22)) >= 0)
        {
            url.SetCharAt('\"',offset);
            url.Cut(offset + 1, sizeof(kEscape22) - 2);
        }

        // XXX At this point, the URL may be relative. 4.5 called into
        // netlib to make an absolute URL, and there was some magic
        // "relative_URL" parameter that got sent down as well. We punt on
        // that stuff.

        // hack fix for bug # 21175:
        // if we don't have a protocol scheme, add "http://" as a default scheme
        if (url.FindChar(PRUnichar(':')) < 0)
        {
            url.Assign(NS_LITERAL_STRING("http://") + url);
        }
    }

    nsresult                    rv;
    nsCOMPtr<nsIRDFResource>    result;
    rv = gRDF->GetUnicodeResource(url, getter_AddRefs(result));
    if (NS_FAILED(rv)) return rv;
    return result->QueryInterface(NS_GET_IID(nsIRDFNode), (void**) aResult);
}

nsresult
BookmarkParser::ParseLiteral(nsIRDFResource *arc, nsString& aValue, nsIRDFNode** aResult)
{
    *aResult = nsnull;

    if (arc == kNC_ShortcutURL)
    {
        // lowercase the shortcut URL before storing internally
        ToLowerCase(aValue);
    }
    else if (arc == kWEB_LastCharset)
    {
        if (gCharsetAlias)
        {
            nsCAutoString charset; charset.AssignWithConversion(aValue);
            gCharsetAlias->GetPreferred(charset, charset);
            aValue.AssignWithConversion(charset.get());
        }
    }
    else if (arc == kWEB_LastPingETag)
    {
        // don't allow quotes in etag
        PRInt32 offset;
        while ((offset = aValue.FindChar('\"')) >= 0)
        {
            aValue.Cut(offset, 1);
        }
    }

    nsresult                rv;
    nsCOMPtr<nsIRDFLiteral> result;
    rv = gRDF->GetLiteral(aValue.get(), getter_AddRefs(result));
    if (NS_FAILED(rv)) return rv;
    return result->QueryInterface(NS_GET_IID(nsIRDFNode), (void**) aResult);
}

nsresult
BookmarkParser::ParseDate(nsIRDFResource *arc, nsString& aValue, nsIRDFNode** aResult)
{
    *aResult = nsnull;

    PRInt32 theDate = 0;
    if (!aValue.IsEmpty())
    {
        PRInt32 err;
        theDate = aValue.ToInteger(&err); // ignored.
    }
    if (theDate == 0)   return NS_RDF_NO_VALUE;

    // convert from seconds to microseconds (PRTime)
    PRInt64     dateVal, temp, million;
    LL_I2L(temp, theDate);
    LL_I2L(million, PR_USEC_PER_SEC);
    LL_MUL(dateVal, temp, million);

    nsresult                rv;
    nsCOMPtr<nsIRDFDate>    result;
    if (NS_FAILED(rv = gRDF->GetDateLiteral(dateVal, getter_AddRefs(result))))
    {
        NS_ERROR("unable to get date literal for time");
        return rv;
    }
    return result->QueryInterface(NS_GET_IID(nsIRDFNode), (void**) aResult);
}

nsresult
BookmarkParser::updateAtom(nsIRDFDataSource *db, nsIRDFResource *src,
            nsIRDFResource *prop, nsIRDFNode *newValue, PRBool *dirtyFlag)
{
    nsresult        rv;
    nsCOMPtr<nsIRDFNode>    oldValue;

    if (dirtyFlag != nsnull)
    {
        *dirtyFlag = PR_FALSE;
    }

    if (NS_SUCCEEDED(rv = db->GetTarget(src, prop, PR_TRUE, getter_AddRefs(oldValue))) &&
        (rv != NS_RDF_NO_VALUE))
    {
        rv = db->Change(src, prop, oldValue, newValue);

        if ((oldValue.get() != newValue) && (dirtyFlag != nsnull))
        {
            *dirtyFlag = PR_TRUE;
        }
    }
    else
    {
        rv = db->Assert(src, prop, newValue, PR_TRUE);

        if (prop == kWEB_Schedule)
        {
          // internally mark nodes with schedules so that we can find them quickly
          updateAtom(db, src, kWEB_ScheduleActive, kTrueLiteral, dirtyFlag);
        }
    }
    return rv;
}

nsresult
BookmarkParser::ParseBookmarkSeparator(const nsString &aLine, const nsCOMPtr<nsIRDFContainer> &aContainer)
{
    nsCOMPtr<nsIRDFResource>  separator;
    nsresult rv = gRDF->GetAnonymousResource(getter_AddRefs(separator));
    if (NS_FAILED(rv))
        return rv;

    PRInt32 lineLen = aLine.Length();

    PRInt32 attrStart = 0;
    attrStart = aLine.Find(kSeparator, PR_TRUE, attrStart);
    if (attrStart < 0)
        return NS_ERROR_UNEXPECTED;
    attrStart += sizeof(kSeparator)-1;

    while((attrStart < lineLen) && (aLine[attrStart] != '>')) {
        while(nsCRT::IsAsciiSpace(aLine[attrStart]))
            ++attrStart;

        if (aLine.Find(kNameEquals, PR_TRUE, attrStart, 1) == attrStart) {
            attrStart += sizeof(kNameEquals) - 1;

            // skip to terminating quote of string
            PRInt32 termQuote = aLine.FindChar(PRUnichar('\"'), attrStart);
            if (termQuote > attrStart) {
                nsAutoString name;
                aLine.Mid(name, attrStart, termQuote - attrStart);
                attrStart = termQuote + 1;
                if (!name.IsEmpty()) {
                    nsCOMPtr<nsIRDFLiteral> nameLiteral;
                    rv = gRDF->GetLiteral(name.get(), getter_AddRefs(nameLiteral));
                    if (NS_FAILED(rv))
                        return rv;
                    rv = mDataSource->Assert(separator, kNC_Name, nameLiteral, PR_TRUE);
                    if (NS_FAILED(rv))
                        return rv;
                }
            }
        }
    }

    rv = mDataSource->Assert(separator, kRDF_type, kNC_BookmarkSeparator, PR_TRUE);
    if (NS_FAILED(rv))
        return rv;

    rv = aContainer->AppendElement(separator);

    return rv;
}

nsresult
BookmarkParser::ParseHeaderBegin(const nsString &aLine, const nsCOMPtr<nsIRDFContainer> &aContainer)
{
    return NS_OK;
}

nsresult
BookmarkParser::ParseHeaderEnd(const nsString &aLine)
{
    return NS_OK;
}

nsresult
BookmarkParser::AssertTime(nsIRDFResource* aSource,
                           nsIRDFResource* aLabel,
                           PRInt32 aTime)
{
    nsresult    rv = NS_OK;

    if (aTime != 0)
    {
        // Convert to a date literal
        PRInt64     dateVal, temp, million;

        LL_I2L(temp, aTime);
        LL_I2L(million, PR_USEC_PER_SEC);
        LL_MUL(dateVal, temp, million);     // convert from seconds to microseconds (PRTime)

        nsCOMPtr<nsIRDFDate>    dateLiteral;
        if (NS_FAILED(rv = gRDF->GetDateLiteral(dateVal, getter_AddRefs(dateLiteral))))
        {
            NS_ERROR("unable to get date literal for time");
            return rv;
        }
        updateAtom(mDataSource, aSource, aLabel, dateLiteral, nsnull);
    }
    return rv;
}

////////////////////////////////////////////////////////////////////////
// nsBookmarksService implementation

nsBookmarksService::nsBookmarksService() :
    mInner(nsnull),
    mUpdateBatchNest(0),
    mBookmarksAvailable(PR_FALSE),
    mDirty(PR_FALSE),
    mNeedBackupUpdate(PR_FALSE)

#if defined(XP_MAC) || defined(XP_MACOSX)
    ,mIEFavoritesAvailable(PR_FALSE)
#endif
{ }

nsBookmarksService::~nsBookmarksService()
{
    if (mTimer)
    {
        // be sure to cancel the timer, as it holds a
        // weak reference back to nsBookmarksService
        mTimer->Cancel();
        mTimer = nsnull;
    }

    // Unregister ourselves from the RDF service
    if (gRDF)
        gRDF->UnregisterDataSource(this);

    // Note: can't flush in the DTOR, as the RDF service
    // has probably already been destroyed
    // Flush();
    bm_ReleaseGlobals();
    NS_IF_RELEASE(mInner);
}

nsresult
nsBookmarksService::Init()
{
    nsresult rv;
    rv = bm_AddRefGlobals();
    if (NS_FAILED(rv))  return rv;

    mNetService = do_GetService(kIOServiceCID, &rv);
    if (NS_FAILED(rv))  return rv;

    // create cache service/session, ignoring errors
    mCacheService = do_GetService(kCacheServiceCID, &rv);
    if (NS_SUCCEEDED(rv))
    {
        rv = mCacheService->CreateSession("HTTP", nsICache::STORE_ANYWHERE,
            nsICache::STREAM_BASED, getter_AddRefs(mCacheSession));
    }

    mTransactionManager = do_CreateInstance(NS_TRANSACTIONMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    /* create a URL for the string resource file */
    nsCOMPtr<nsIURI>    uri;
    if (NS_SUCCEEDED(rv = mNetService->NewURI(bookmark_properties, nsnull, nsnull,
        getter_AddRefs(uri))))
    {
        /* create a bundle for the localization */
        nsCOMPtr<nsIStringBundleService>    stringService;
        if (NS_SUCCEEDED(rv = nsServiceManager::GetService(kStringBundleServiceCID,
            NS_GET_IID(nsIStringBundleService), getter_AddRefs(stringService))))
        {
            nsCAutoString spec;
            if (NS_SUCCEEDED(rv = uri->GetSpec(spec)))
            {
                stringService->CreateBundle(spec.get(), getter_AddRefs(mBundle));
            }
        }
    }

    nsCOMPtr<nsIPref> prefServ(do_GetService(kPrefCID, &rv));
    if (NS_SUCCEEDED(rv) && (prefServ))
    {
        // get browser icon pref
        prefServ->GetBoolPref("browser.chrome.site_icons", &mBrowserIcons);
        
    }

    if (mPersonalToolbarName.IsEmpty())
        mBundle->GetStringFromName(NS_LITERAL_STRING("BookmarksToolbarFolder").get(), 
                                   getter_Copies(mPersonalToolbarName));

    // Register as an observer of profile changes
    nsCOMPtr<nsIObserverService> observerService = 
             do_GetService("@mozilla.org/observer-service;1", &rv);
    NS_ASSERTION(observerService, "Could not get observer service.");
    if (observerService) {
        observerService->AddObserver(this, "profile-before-change", PR_TRUE);
        observerService->AddObserver(this, "profile-after-change", PR_TRUE);
        observerService->AddObserver(this, "quit-application", PR_TRUE);
    }

    rv = InitDataSource();
    NS_ENSURE_SUCCESS(rv, rv);

    /* timer initialization */
    busyResource = nsnull;

    if (!mTimer)
    {
        busySchedule = PR_FALSE;
        mTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create a timer");
        if (NS_FAILED(rv)) return rv;
        mTimer->InitWithFuncCallback(nsBookmarksService::FireTimer, this, BOOKMARK_TIMEOUT, 
                                     nsITimer::TYPE_REPEATING_SLACK);
        // Note: don't addref "this" as we'll cancel the timer in the nsBookmarkService destructor
    }

    // register this as a named data source with the RDF
    // service. Do this *last*, because if Init() fails, then the
    // object will be destroyed (leaving the RDF service with a
    // dangling pointer).
    rv = gRDF->RegisterDataSource(this, PR_FALSE);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

nsresult
nsBookmarksService::getLocaleString(const char *key, nsString &str)
{
    PRUnichar    *keyUni = nsnull;
    nsAutoString  keyStr;
    keyStr.AssignWithConversion(key);

    nsresult    rv = NS_RDF_NO_VALUE;
    if (mBundle && (NS_SUCCEEDED(rv = mBundle->GetStringFromName(keyStr.get(), &keyUni)))
        && (keyUni))
    {
        str = keyUni;
        nsCRT::free(keyUni);
    }
    else
    {
        str.Truncate();
    }
    return rv;
}

nsresult
nsBookmarksService::ExamineBookmarkSchedule(nsIRDFResource *theBookmark, PRBool & examineFlag)
{
    examineFlag = PR_FALSE;
    
    nsresult    rv = NS_OK;

    nsCOMPtr<nsIRDFNode>    scheduleNode;
    if (NS_FAILED(rv = mInner->GetTarget(theBookmark, kWEB_Schedule, PR_TRUE,
        getter_AddRefs(scheduleNode))) || (rv == NS_RDF_NO_VALUE))
        return rv;
    
    nsCOMPtr<nsIRDFLiteral> scheduleLiteral = do_QueryInterface(scheduleNode);
    if (!scheduleLiteral)   return NS_ERROR_NO_INTERFACE;
    
    const PRUnichar     *scheduleUni = nsnull;
    if (NS_FAILED(rv = scheduleLiteral->GetValueConst(&scheduleUni)))
        return rv;
    if (!scheduleUni)   return NS_ERROR_NULL_POINTER;

    nsAutoString        schedule(scheduleUni);
    if (schedule.Length() < 1)  return NS_ERROR_UNEXPECTED;

    // convert the current date/time from microseconds (PRTime) to seconds
    // Note: don't change now64, as its used later in the function
    PRTime      now64 = PR_Now(), temp64, million;
    LL_I2L(million, PR_USEC_PER_SEC);
    LL_DIV(temp64, now64, million);
    PRInt32     now32;
    LL_L2I(now32, temp64);

    PRExplodedTime  nowInfo;
    PR_ExplodeTime(now64, PR_LocalTimeParameters, &nowInfo);
    
    // XXX Do we need to do this?
    PR_NormalizeTime(&nowInfo, PR_LocalTimeParameters);

    nsAutoString    dayNum;
    dayNum.AppendInt(nowInfo.tm_wday, 10);

    // a schedule string has the following format:
    // Check Monday, Tuesday, and Friday | 9 AM thru 5 PM | every five minutes | change bookmark icon
    // 125|9-17|5|icon

    nsAutoString    notificationMethod;
    PRInt32     startHour = -1, endHour = -1, duration = -1;

    // should we be checking today?
    PRInt32     slashOffset;
    if ((slashOffset = schedule.FindChar(PRUnichar('|'))) >= 0)
    {
        nsAutoString    daySection;
        schedule.Left(daySection, slashOffset);
        schedule.Cut(0, slashOffset+1);
        if (daySection.Find(dayNum) >= 0)
        {
            // ok, we should be checking today.  Within hour range?
            if ((slashOffset = schedule.FindChar(PRUnichar('|'))) >= 0)
            {
                nsAutoString    hourRange;
                schedule.Left(hourRange, slashOffset);
                schedule.Cut(0, slashOffset+1);

                // now have the "hour-range" segment of the string
                // such as "9-17" or "9-12" from the examples above
                PRInt32     dashOffset;
                if ((dashOffset = hourRange.FindChar(PRUnichar('-'))) >= 1)
                {
                    nsAutoString    startStr, endStr;

                    hourRange.Right(endStr, hourRange.Length() - dashOffset - 1);
                    hourRange.Left(startStr, dashOffset);

                    PRInt32     errorCode2 = 0;
                    startHour = startStr.ToInteger(&errorCode2);
                    if (errorCode2) startHour = -1;
                    endHour = endStr.ToInteger(&errorCode2);
                    if (errorCode2) endHour = -1;
                    
                    if ((startHour >=0) && (endHour >=0))
                    {
                        if ((slashOffset = schedule.FindChar(PRUnichar('|'))) >= 0)
                        {
                            nsAutoString    durationStr;
                            schedule.Left(durationStr, slashOffset);
                            schedule.Cut(0, slashOffset+1);

                            // get duration
                            PRInt32     errorCode = 0;
                            duration = durationStr.ToInteger(&errorCode);
                            if (errorCode)  duration = -1;
                            
                            // what's left is the notification options
                            notificationMethod = schedule;
                        }
                    }
                }
            }
        }
    }
        

#ifdef  DEBUG_BOOKMARK_PING_OUTPUT
    char *methodStr = ToNewCString(notificationMethod);
    if (methodStr)
    {
        printf("Start Hour: %d    End Hour: %d    Duration: %d mins    Method: '%s'\n",
            startHour, endHour, duration, methodStr);
        delete [] methodStr;
        methodStr = nsnull;
    }
#endif

    if ((startHour <= nowInfo.tm_hour) && (endHour >= nowInfo.tm_hour) &&
        (duration >= 1) && (!notificationMethod.IsEmpty()))
    {
        // OK, we're with the start/end time range, check the duration
        // against the last time we've "pinged" the server (if ever)

        examineFlag = PR_TRUE;

        nsCOMPtr<nsIRDFNode>    pingNode;
        if (NS_SUCCEEDED(rv = mInner->GetTarget(theBookmark, kWEB_LastPingDate,
            PR_TRUE, getter_AddRefs(pingNode))) && (rv != NS_RDF_NO_VALUE))
        {
            nsCOMPtr<nsIRDFDate>    pingLiteral = do_QueryInterface(pingNode);
            if (pingLiteral)
            {
                PRInt64     lastPing;
                if (NS_SUCCEEDED(rv = pingLiteral->GetValue(&lastPing)))
                {
                    PRInt64     diff64, sixty;
                    LL_SUB(diff64, now64, lastPing);
                    
                    // convert from milliseconds to seconds
                    LL_DIV(diff64, diff64, million);
                    // convert from seconds to minutes
                    LL_I2L(sixty, 60L);
                    LL_DIV(diff64, diff64, sixty);

                    PRInt32     diff32;
                    LL_L2I(diff32, diff64);
                    if (diff32 < duration)
                    {
                        examineFlag = PR_FALSE;

#ifdef  DEBUG_BOOKMARK_PING_OUTPUT
                        printf("Skipping URL, its too soon.\n");
#endif
                    }
                }
            }
        }
    }
    return rv;
}

nsresult
nsBookmarksService::GetBookmarkToPing(nsIRDFResource **theBookmark)
{
    nsresult    rv = NS_OK;

    *theBookmark = nsnull;

    nsCOMPtr<nsISimpleEnumerator>   srcList;
    if (NS_FAILED(rv = GetSources(kWEB_ScheduleActive, kTrueLiteral, PR_TRUE, getter_AddRefs(srcList))))
        return rv;

    nsCOMPtr<nsISupportsArray>  bookmarkList;
    if (NS_FAILED(rv = NS_NewISupportsArray(getter_AddRefs(bookmarkList))))
        return rv;

    // build up a list of potential bookmarks to check
    PRBool  hasMoreSrcs = PR_TRUE;
    while(NS_SUCCEEDED(rv = srcList->HasMoreElements(&hasMoreSrcs))
        && (hasMoreSrcs == PR_TRUE))
    {
        nsCOMPtr<nsISupports>   aSrc;
        if (NS_FAILED(rv = srcList->GetNext(getter_AddRefs(aSrc))))
            break;
        nsCOMPtr<nsIRDFResource>    aSource = do_QueryInterface(aSrc);
        if (!aSource)   continue;

        // does the bookmark have a schedule, and if so,
        // are we within its bounds for checking the URL?

        PRBool  examineFlag = PR_FALSE;
        if (NS_FAILED(rv = ExamineBookmarkSchedule(aSource, examineFlag))
            || (examineFlag == PR_FALSE))   continue;

        bookmarkList->AppendElement(aSource);
    }

    // pick a random entry from the list of bookmarks to check
    PRUint32    numBookmarks;
    if (NS_SUCCEEDED(rv = bookmarkList->Count(&numBookmarks)) && (numBookmarks > 0))
    {
        PRInt32     randomNum;
        LL_L2I(randomNum, PR_Now());
        PRUint32    randomBookmark = (numBookmarks-1) % randomNum;

        nsCOMPtr<nsISupports>   iSupports;
        if (NS_SUCCEEDED(rv = bookmarkList->GetElementAt(randomBookmark,
            getter_AddRefs(iSupports))))
        {
            nsCOMPtr<nsIRDFResource>    aBookmark = do_QueryInterface(iSupports);
            if (aBookmark)
            {
                *theBookmark = aBookmark;
                NS_ADDREF(*theBookmark);
            }
        }
    }
    return rv;
}

void
nsBookmarksService::FireTimer(nsITimer* aTimer, void* aClosure)
{
    nsBookmarksService *bmks = NS_STATIC_CAST(nsBookmarksService *, aClosure);
    if (!bmks)  return;
    nsresult            rv;

    if (bmks->mNeedBackupUpdate == PR_TRUE)
    {
        bmks->SaveToBackup();
    }

    if ((bmks->mBookmarksAvailable == PR_TRUE) && (bmks->mDirty == PR_TRUE))
    {
        bmks->Flush();
    }

    if (bmks->busySchedule == PR_FALSE)
    {
        nsCOMPtr<nsIRDFResource>    bookmark;
        if (NS_SUCCEEDED(rv = bmks->GetBookmarkToPing(getter_AddRefs(bookmark))) && (bookmark))
        {
            bmks->busyResource = bookmark;

            nsAutoString url;
            rv = bmks->GetURLFromResource(bookmark, url);
            if (NS_FAILED(rv))
                return;

#ifdef  DEBUG_BOOKMARK_PING_OUTPUT
            printf("nsBookmarksService::FireTimer - Pinging '%s'\n",
                   NS_ConvertUCS2toUTF8(url).get());
#endif

            nsCOMPtr<nsIURI>    uri;
            if (NS_SUCCEEDED(rv = NS_NewURI(getter_AddRefs(uri), url)))
            {
                nsCOMPtr<nsIChannel>    channel;
                if (NS_SUCCEEDED(rv = NS_NewChannel(getter_AddRefs(channel), uri, nsnull)))
                {
                    channel->SetLoadFlags(nsIRequest::VALIDATE_ALWAYS);
                    nsCOMPtr<nsIHttpChannel>    httpChannel = do_QueryInterface(channel);
                    if (httpChannel)
                    {
                        bmks->htmlSize = 0;
                        httpChannel->SetRequestMethod(NS_LITERAL_CSTRING("HEAD"));
                        if (NS_SUCCEEDED(rv = channel->AsyncOpen(bmks, nsnull)))
                        {
                            bmks->busySchedule = PR_TRUE;
                        }
                    }
                }
            }
        }
    }
#ifdef  DEBUG_BOOKMARK_PING_OUTPUT
    else
    {
        printf("nsBookmarksService::FireTimer - busy pinging.\n");
    }
#endif
}


// stream observer methods

NS_IMETHODIMP
nsBookmarksService::OnStartRequest(nsIRequest* request, nsISupports *ctxt)
{
    return NS_OK;
}

NS_IMETHODIMP
nsBookmarksService::OnDataAvailable(nsIRequest* request, nsISupports *ctxt, nsIInputStream *aIStream,
                      PRUint32 sourceOffset, PRUint32 aLength)
{
    // calculate html page size if server doesn't tell us in headers
    htmlSize += aLength;

    return NS_OK;
}

NS_IMETHODIMP
nsBookmarksService::OnStopRequest(nsIRequest* request, nsISupports *ctxt,
                    nsresult status)
{
    nsresult rv;

    nsAutoString url;
    if (NS_SUCCEEDED(rv = GetURLFromResource(busyResource, url)))
    {
#ifdef  DEBUG_BOOKMARK_PING_OUTPUT
        printf("Finished polling '%s'\n", NS_ConvertUCS2toUTF8(url).get());
#endif
    }
    nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
    nsCOMPtr<nsIHttpChannel>    httpChannel = do_QueryInterface(channel);
    if (httpChannel)
    {
        nsAutoString            eTagValue, lastModValue, contentLengthValue;

        nsCAutoString val;
        if (NS_SUCCEEDED(httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("ETag"), val)))
            CopyASCIItoUCS2(val, eTagValue);
        if (NS_SUCCEEDED(httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("Last-Modified"), val)))
            CopyASCIItoUCS2(val, lastModValue);
        if (NS_SUCCEEDED(httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("Content-Length"), val)))
            CopyASCIItoUCS2(val, contentLengthValue);
        val.Truncate();

        PRBool      changedFlag = PR_FALSE;

        PRUint32    respStatus;
        if (NS_SUCCEEDED(rv = httpChannel->GetResponseStatus(&respStatus)))
        {
            if ((respStatus >= 200) && (respStatus <= 299))
            {
                if (!eTagValue.IsEmpty())
                {
#ifdef  DEBUG_BOOKMARK_PING_OUTPUT
                    printf("eTag: '%s'\n", NS_ConvertUCS2toUTF8(eTagValue).get());
#endif
                    nsAutoString        eTagStr;
                    nsCOMPtr<nsIRDFNode>    currentETagNode;
                    if (NS_SUCCEEDED(rv = mInner->GetTarget(busyResource, kWEB_LastPingETag,
                        PR_TRUE, getter_AddRefs(currentETagNode))) && (rv != NS_RDF_NO_VALUE))
                    {
                        nsCOMPtr<nsIRDFLiteral> currentETagLit = do_QueryInterface(currentETagNode);
                        if (currentETagLit)
                        {
                            const PRUnichar* currentETagStr = nsnull;
                            currentETagLit->GetValueConst(&currentETagStr);
                            if ((currentETagStr) &&
                                !eTagValue.Equals(nsDependentString(currentETagStr),
                                                  nsCaseInsensitiveStringComparator()))
                            {
                                changedFlag = PR_TRUE;
                            }
                            eTagStr.Assign(eTagValue);
                            nsCOMPtr<nsIRDFLiteral> newETagLiteral;
                            if (NS_SUCCEEDED(rv = gRDF->GetLiteral(eTagStr.get(),
                                getter_AddRefs(newETagLiteral))))
                            {
                                rv = mInner->Change(busyResource, kWEB_LastPingETag,
                                    currentETagNode, newETagLiteral);
                            }
                        }
                    }
                    else
                    {
                        eTagStr.Assign(eTagValue);
                        nsCOMPtr<nsIRDFLiteral> newETagLiteral;
                        if (NS_SUCCEEDED(rv = gRDF->GetLiteral(eTagStr.get(),
                            getter_AddRefs(newETagLiteral))))
                        {
                            rv = mInner->Assert(busyResource, kWEB_LastPingETag,
                                newETagLiteral, PR_TRUE);
                        }
                    }
                }
            }
        }

        if ((changedFlag == PR_FALSE) && (!lastModValue.IsEmpty()))
        {
#ifdef  DEBUG_BOOKMARK_PING_OUTPUT
            printf("Last-Modified: '%s'\n", lastModValue.get());
#endif
            nsAutoString        lastModStr;
            nsCOMPtr<nsIRDFNode>    currentLastModNode;
            if (NS_SUCCEEDED(rv = mInner->GetTarget(busyResource, kWEB_LastPingModDate,
                PR_TRUE, getter_AddRefs(currentLastModNode))) && (rv != NS_RDF_NO_VALUE))
            {
                nsCOMPtr<nsIRDFLiteral> currentLastModLit = do_QueryInterface(currentLastModNode);
                if (currentLastModLit)
                {
                    const PRUnichar* currentLastModStr = nsnull;
                    currentLastModLit->GetValueConst(&currentLastModStr);
                    if ((currentLastModStr) &&
                        !lastModValue.Equals(nsDependentString(currentLastModStr),
                                             nsCaseInsensitiveStringComparator()))
                    {
                        changedFlag = PR_TRUE;
                    }
                    lastModStr.Assign(lastModValue);
                    nsCOMPtr<nsIRDFLiteral> newLastModLiteral;
                    if (NS_SUCCEEDED(rv = gRDF->GetLiteral(lastModStr.get(),
                        getter_AddRefs(newLastModLiteral))))
                    {
                        rv = mInner->Change(busyResource, kWEB_LastPingModDate,
                            currentLastModNode, newLastModLiteral);
                    }
                }
            }
            else
            {
                lastModStr.Assign(lastModValue);
                nsCOMPtr<nsIRDFLiteral> newLastModLiteral;
                if (NS_SUCCEEDED(rv = gRDF->GetLiteral(lastModStr.get(),
                    getter_AddRefs(newLastModLiteral))))
                {
                    rv = mInner->Assert(busyResource, kWEB_LastPingModDate,
                        newLastModLiteral, PR_TRUE);
                }
            }
        }

        if ((changedFlag == PR_FALSE) && (!contentLengthValue.IsEmpty()))
        {
#ifdef  DEBUG_BOOKMARK_PING_OUTPUT
            printf("Content-Length: '%s'\n", contentLengthValue.get());
#endif
            nsAutoString        contentLenStr;
            nsCOMPtr<nsIRDFNode>    currentContentLengthNode;
            if (NS_SUCCEEDED(rv = mInner->GetTarget(busyResource, kWEB_LastPingContentLen,
                PR_TRUE, getter_AddRefs(currentContentLengthNode))) && (rv != NS_RDF_NO_VALUE))
            {
                nsCOMPtr<nsIRDFLiteral> currentContentLengthLit = do_QueryInterface(currentContentLengthNode);
                if (currentContentLengthLit)
                {
                    const PRUnichar *currentContentLengthStr = nsnull;
                    currentContentLengthLit->GetValueConst(&currentContentLengthStr);
                    if ((currentContentLengthStr) &&
                        !contentLengthValue.Equals(nsDependentString(currentContentLengthStr),
                                                   nsCaseInsensitiveStringComparator()))
                    {
                        changedFlag = PR_TRUE;
                    }
                    contentLenStr.Assign(contentLengthValue);
                    nsCOMPtr<nsIRDFLiteral> newContentLengthLiteral;
                    if (NS_SUCCEEDED(rv = gRDF->GetLiteral(contentLenStr.get(),
                        getter_AddRefs(newContentLengthLiteral))))
                    {
                        rv = mInner->Change(busyResource, kWEB_LastPingContentLen,
                            currentContentLengthNode, newContentLengthLiteral);
                    }
                }
            }
            else
            {
                contentLenStr.Assign(contentLengthValue);
                nsCOMPtr<nsIRDFLiteral> newContentLengthLiteral;
                if (NS_SUCCEEDED(rv = gRDF->GetLiteral(contentLenStr.get(),
                    getter_AddRefs(newContentLengthLiteral))))
                {
                    rv = mInner->Assert(busyResource, kWEB_LastPingContentLen,
                        newContentLengthLiteral, PR_TRUE);
                }
            }
        }

        // update last poll date
        nsCOMPtr<nsIRDFDate>    dateLiteral;
        if (NS_SUCCEEDED(rv = gRDF->GetDateLiteral(PR_Now(), getter_AddRefs(dateLiteral))))
        {
            nsCOMPtr<nsIRDFNode>    lastPingNode;
            if (NS_SUCCEEDED(rv = mInner->GetTarget(busyResource, kWEB_LastPingDate, PR_TRUE,
                getter_AddRefs(lastPingNode))) && (rv != NS_RDF_NO_VALUE))
            {
                rv = mInner->Change(busyResource, kWEB_LastPingDate, lastPingNode, dateLiteral);
            }
            else
            {
                rv = mInner->Assert(busyResource, kWEB_LastPingDate, dateLiteral, PR_TRUE);
            }
            NS_ASSERTION(rv == NS_RDF_ASSERTION_ACCEPTED, "unable to assert new time");

//          mDirty = PR_TRUE;
        }
        else
        {
            NS_ERROR("unable to get date literal for now");
        }

        // If its changed, set the appropriate info
        if (changedFlag == PR_TRUE)
        {
#ifdef  DEBUG_BOOKMARK_PING_OUTPUT
            printf("URL has changed!\n\n");
#endif

            nsAutoString        schedule;

            nsCOMPtr<nsIRDFNode>    scheduleNode;
            if (NS_SUCCEEDED(rv = mInner->GetTarget(busyResource, kWEB_Schedule, PR_TRUE,
                getter_AddRefs(scheduleNode))) && (rv != NS_RDF_NO_VALUE))
            {
                nsCOMPtr<nsIRDFLiteral> scheduleLiteral = do_QueryInterface(scheduleNode);
                if (scheduleLiteral)
                {
                    const PRUnichar     *scheduleUni = nsnull;
                    if (NS_SUCCEEDED(rv = scheduleLiteral->GetValueConst(&scheduleUni))
                        && (scheduleUni))
                    {
                        schedule = scheduleUni;
                    }
                }
            }

            // update icon?
            if (FindInReadable(NS_LITERAL_STRING("icon"),
                               schedule,
                               nsCaseInsensitiveStringComparator()))
            {
                nsCOMPtr<nsIRDFLiteral> statusLiteral;
                if (NS_SUCCEEDED(rv = gRDF->GetLiteral(NS_LITERAL_STRING("new").get(), getter_AddRefs(statusLiteral))))
                {
                    nsCOMPtr<nsIRDFNode>    currentStatusNode;
                    if (NS_SUCCEEDED(rv = mInner->GetTarget(busyResource, kWEB_Status, PR_TRUE,
                        getter_AddRefs(currentStatusNode))) && (rv != NS_RDF_NO_VALUE))
                    {
                        rv = mInner->Change(busyResource, kWEB_Status, currentStatusNode, statusLiteral);
                    }
                    else
                    {
                        rv = mInner->Assert(busyResource, kWEB_Status, statusLiteral, PR_TRUE);
                    }
                    NS_ASSERTION(rv == NS_RDF_ASSERTION_ACCEPTED, "unable to assert changed status");
                }
            }
            
            // play a sound?
            if (FindInReadable(NS_LITERAL_STRING("sound"),
                               schedule,
                               nsCaseInsensitiveStringComparator()))
            {
                nsCOMPtr<nsISound>  soundInterface;
                rv = nsComponentManager::CreateInstance(kSoundCID,
                        nsnull, NS_GET_IID(nsISound),
                        getter_AddRefs(soundInterface));
                if (NS_SUCCEEDED(rv))
                {
                    // for the moment, just beep
                    soundInterface->Beep();
                }
            }
            
            PRBool      openURLFlag = PR_FALSE;

            // show an alert?
            if (FindInReadable(NS_LITERAL_STRING("alert"),
                               schedule,
                               nsCaseInsensitiveStringComparator()))
            {
                nsCOMPtr<nsIInterfaceRequestor> interfaces;
                nsCOMPtr<nsIPrompt> prompter;

                channel->GetNotificationCallbacks(getter_AddRefs(interfaces));
                if (interfaces)
                    interfaces->GetInterface(NS_GET_IID(nsIPrompt), getter_AddRefs(prompter));
                if (!prompter)
                {
                    nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
                    if (wwatch)
                        wwatch->GetNewPrompter(0, getter_AddRefs(prompter));
                }

                if (prompter)
                {
                    nsAutoString    promptStr;
                    getLocaleString("WebPageUpdated", promptStr);
                    if (!promptStr.IsEmpty())   promptStr.Append(NS_LITERAL_STRING("\n\n"));

                    nsCOMPtr<nsIRDFNode>    nameNode;
                    if (NS_SUCCEEDED(mInner->GetTarget(busyResource, kNC_Name,
                        PR_TRUE, getter_AddRefs(nameNode))))
                    {
                        nsCOMPtr<nsIRDFLiteral> nameLiteral = do_QueryInterface(nameNode);
                        if (nameLiteral)
                        {
                            const PRUnichar *nameUni = nsnull;
                            if (NS_SUCCEEDED(rv = nameLiteral->GetValueConst(&nameUni))
                                && (nameUni))
                            {
                                nsAutoString    info;
                                getLocaleString("WebPageTitle", info);
                                promptStr += info;
                                promptStr.Append(NS_LITERAL_STRING(" "));
                                promptStr += nameUni;
                                promptStr.Append(NS_LITERAL_STRING("\n"));
                                getLocaleString("WebPageURL", info);
                                promptStr += info;
                                promptStr.Append(NS_LITERAL_STRING(" "));
                            }
                        }
                    }
                    promptStr.Append(url);
                    
                    nsAutoString    temp;
                    getLocaleString("WebPageAskDisplay", temp);
                    if (!temp.IsEmpty())
                    {
                        promptStr.Append(NS_LITERAL_STRING("\n\n"));
                        promptStr += temp;
                    }

                    nsAutoString    stopOption;
                    getLocaleString("WebPageAskStopOption", stopOption);
                    PRBool      stopCheckingFlag = PR_FALSE;
                    rv = prompter->ConfirmCheck(nsnull, promptStr.get(), stopOption.get(),
                                  &stopCheckingFlag, &openURLFlag);
                    if (NS_FAILED(rv))
                    {
                        openURLFlag = PR_FALSE;
                        stopCheckingFlag = PR_FALSE;
                    }
                    if (stopCheckingFlag == PR_TRUE)
                    {
#ifdef  DEBUG_BOOKMARK_PING_OUTPUT
                        printf("\nStop checking this URL.\n");
#endif
                        rv = mInner->Unassert(busyResource, kWEB_Schedule, scheduleNode);
                        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to unassert kWEB_Schedule");
                    }
                }
            }

            // open the URL in a new window?
            if ((openURLFlag == PR_TRUE) ||
                FindInReadable(NS_LITERAL_STRING("open"),
                               schedule,
                               nsCaseInsensitiveStringComparator()))
            {
                if (NS_SUCCEEDED(rv))
                {
                    nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
                    if (wwatch)
                    {
                        nsCOMPtr<nsIDOMWindow> newWindow;
            nsCOMPtr<nsISupportsArray> suppArray;
            rv = NS_NewISupportsArray(getter_AddRefs(suppArray));
            if (NS_FAILED(rv)) return rv;
            nsCOMPtr<nsISupportsString> suppString(do_CreateInstance("@mozilla.org/supports-string;1", &rv));
            if (!suppString) return rv;
            rv = suppString->SetData(url);
            if (NS_FAILED(rv)) return rv;
            suppArray->AppendElement(suppString);
    
            nsCOMPtr<nsICmdLineHandler> handler(do_GetService("@mozilla.org/commandlinehandler/general-startup;1?type=browser", &rv));    
            if (NS_FAILED(rv)) return rv;
   
            nsXPIDLCString chromeUrl;
            rv = handler->GetChromeUrlForTask(getter_Copies(chromeUrl));
            if (NS_FAILED(rv)) return rv;

            wwatch->OpenWindow(0, chromeUrl, "_blank", "chrome,dialog=no,all", 
                               suppArray, getter_AddRefs(newWindow));
                    }
                }
            }
        }
#ifdef  DEBUG_BOOKMARK_PING_OUTPUT
        else
        {
            printf("URL has not changed status.\n\n");
        }
#endif
    }

    busyResource = nsnull;
    busySchedule = PR_FALSE;

    return NS_OK;
}


// nsIObserver methods

NS_IMETHODIMP nsBookmarksService::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *someData)
{
    nsresult rv = NS_OK;

    if (!nsCRT::strcmp(aTopic, "profile-before-change"))
    {
        // The profile has not changed yet.
        rv = Flush();
    
        if (!nsCRT::strcmp(someData, NS_LITERAL_STRING("shutdown-cleanse").get()))
        {
            nsCOMPtr<nsIFile> bookmarksFile;

            rv = GetBookmarksFile(getter_AddRefs(bookmarksFile));

            if (bookmarksFile)
            {
                bookmarksFile->Remove(PR_FALSE);
            }
        }
    }    
    else if (!nsCRT::strcmp(aTopic, "profile-after-change"))
    {
        // The profile has aleady changed.
        rv = LoadBookmarks();
    }
    else if (!nsCRT::strcmp(aTopic, "quit-application"))
    {
        rv = Flush();
    }

    return rv;
}


////////////////////////////////////////////////////////////////////////
// nsISupports methods

NS_IMPL_ADDREF(nsBookmarksService)

NS_IMETHODIMP_(nsrefcnt)
nsBookmarksService::Release()
{
    // We need a special implementation of Release() because our mInner
    // holds a Circular References back to us.
    NS_PRECONDITION(PRInt32(mRefCnt) > 0, "duplicate release");
    --mRefCnt;
    NS_LOG_RELEASE(this, mRefCnt, "nsBookmarksService");

    if (mInner && mRefCnt == 1) {
        nsIRDFDataSource* tmp = mInner;
        mInner = nsnull;
        NS_IF_RELEASE(tmp);
        return 0;
    }
    else if (mRefCnt == 0) {
        delete this;
        return 0;
    }
    else {
        return mRefCnt;
    }
}

NS_IMPL_QUERY_INTERFACE8(nsBookmarksService, 
             nsIBookmarksService,
             nsIRDFDataSource,
             nsIRDFRemoteDataSource,
             nsIRDFObserver,
             nsIStreamListener,
             nsIRequestObserver,
             nsIObserver,
             nsISupportsWeakReference)


////////////////////////////////////////////////////////////////////////
// nsIBookmarksService

nsresult
nsBookmarksService::InsertResource(nsIRDFResource* aResource,
                                   nsIRDFResource* aParentFolder, PRInt32 aIndex)
{
    nsresult rv = NS_OK;
    // Add to container if the parent folder is non null 
    if (aParentFolder)
    {
        nsCOMPtr<nsIRDFContainer> container(do_GetService("@mozilla.org/rdf/container;1", &rv));
        if (NS_FAILED(rv)) 
            return rv;
        rv = container->Init(mInner, aParentFolder);
        if (NS_FAILED(rv)) 
            return rv;
        // if the index in the js call is null or undefined, aIndex will be equal to 0
        if (aIndex > 0) 
            rv = container->InsertElementAt(aResource, aIndex, PR_TRUE);
        else
            rv = container->AppendElement(aResource);
    }
    return rv;
}

NS_IMETHODIMP
nsBookmarksService::CreateFolder(const PRUnichar* aName, 
                                 nsIRDFResource** aResult)
{
    nsresult rv;

    // Resource: Folder ID
    nsCOMPtr<nsIRDFResource> folderResource;
    rv = gRDF->GetAnonymousResource(getter_AddRefs(folderResource));
    if (NS_FAILED(rv)) 
        return rv;

    rv = gRDFC->MakeSeq(mInner, folderResource, nsnull);
    if (NS_FAILED(rv)) 
        return rv;

    // Literal: Folder Name
    nsCOMPtr<nsIRDFLiteral> nameLiteral;
    nsAutoString folderName; 
    folderName.Assign(aName);
    if (folderName.IsEmpty()) {
        getLocaleString("NewFolder", folderName);

        rv = gRDF->GetLiteral(folderName.get(), getter_AddRefs(nameLiteral));
        if (NS_FAILED(rv)) 
            return rv;
    }
    else
    {
        rv = gRDF->GetLiteral(aName, getter_AddRefs(nameLiteral));
        if (NS_FAILED(rv)) 
            return rv;
    }

    rv = mInner->Assert(folderResource, kNC_Name, nameLiteral, PR_TRUE);
    if (NS_FAILED(rv)) 
        return rv;

    // Date: Date of Creation
    // Convert the current date/time from microseconds (PRTime) to seconds.
    nsCOMPtr<nsIRDFDate> dateLiteral;
    rv = gRDF->GetDateLiteral(PR_Now(), getter_AddRefs(dateLiteral));
    if (NS_FAILED(rv)) 
        return rv;
    rv = mInner->Assert(folderResource, kNC_BookmarkAddDate, dateLiteral, PR_TRUE);
    if (NS_FAILED(rv)) 
        return rv;

    *aResult = folderResource;
    NS_ADDREF(*aResult);

    return NS_OK;
}

NS_IMETHODIMP
nsBookmarksService::CreateFolderInContainer(const PRUnichar* aName, 
                                            nsIRDFResource* aParentFolder, PRInt32 aIndex,
                                            nsIRDFResource** aResult)
{
    nsCOMPtr<nsIRDFNode> nodeType;
    GetSynthesizedType(aParentFolder, getter_AddRefs(nodeType));
    if (nodeType == kNC_Livemark)
        return NS_RDF_ASSERTION_REJECTED;

    nsresult rv = CreateFolder(aName, aResult);
    if (NS_SUCCEEDED(rv))
        rv = InsertResource(*aResult, aParentFolder, aIndex);
    return rv;
}

nsresult
nsBookmarksService::GetURLFromResource(nsIRDFResource* aResource,
                                       nsAString& aURL)
{
    NS_ENSURE_ARG(aResource);

    nsCOMPtr<nsIRDFNode> urlNode;
    nsresult rv = mInner->GetTarget(aResource, kNC_URL, PR_TRUE, getter_AddRefs(urlNode));
    if (NS_FAILED(rv))
        return rv;

    if (urlNode) {
        nsCOMPtr<nsIRDFLiteral> urlLiteral = do_QueryInterface(urlNode, &rv);
        if (NS_FAILED(rv))
            return rv;

        const PRUnichar* url = nsnull;
        rv = urlLiteral->GetValueConst(&url);
        if (NS_FAILED(rv))
            return rv;

        aURL.Assign(url);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsBookmarksService::CreateBookmark(const PRUnichar* aName,
                                   const PRUnichar* aURL, 
                                   const PRUnichar* aShortcutURL,
                                   const PRUnichar* aDescription,
                                   const PRUnichar* aDocCharSet, 
                                   const PRUnichar* aPostData,
                                   nsIRDFResource** aResult)
{
    // Resource: Bookmark ID
    nsCOMPtr<nsIRDFResource> bookmarkResource;
    nsresult rv = gRDF->GetAnonymousResource(getter_AddRefs(bookmarkResource));

    if (NS_FAILED(rv)) 
        return rv;

    // Literal: Folder Name
    nsCOMPtr<nsIRDFLiteral> nameLiteral;
    nsAutoString bookmarkName; 
    bookmarkName.Assign(aName);
    if (bookmarkName.IsEmpty()) {
        getLocaleString("NewBookmark", bookmarkName);

        rv = gRDF->GetLiteral(bookmarkName.get(), getter_AddRefs(nameLiteral));
        if (NS_FAILED(rv)) 
            return rv;
    }
    else
    {
        rv = gRDF->GetLiteral(aName, getter_AddRefs(nameLiteral));
        if (NS_FAILED(rv)) 
            return rv;
    }

    rv = mInner->Assert(bookmarkResource, kNC_Name, nameLiteral, PR_TRUE);
    if (NS_FAILED(rv)) 
        return rv;

    // Resource: URL
    nsAutoString url;
    url.Assign(aURL);
    nsCOMPtr<nsIRDFLiteral> urlLiteral;
    rv = gRDF->GetLiteral(url.get(), getter_AddRefs(urlLiteral));
    if (NS_FAILED(rv)) 
        return rv;
    rv = mInner->Assert(bookmarkResource, kNC_URL, urlLiteral, PR_TRUE);
    if (NS_FAILED(rv)) 
        return rv;

    // Literal: Shortcut URL
    if (aShortcutURL && *aShortcutURL) {
        nsCOMPtr<nsIRDFLiteral> shortcutLiteral;
        rv = gRDF->GetLiteral(aShortcutURL, getter_AddRefs(shortcutLiteral));
        if (NS_FAILED(rv)) 
            return rv;
        rv = mInner->Assert(bookmarkResource, kNC_ShortcutURL, shortcutLiteral, PR_TRUE);
        if (NS_FAILED(rv)) 
            return rv;
    }

    // Literal: Description
    if (aDescription && *aDescription) {
        nsCOMPtr<nsIRDFLiteral> descriptionLiteral;
        rv = gRDF->GetLiteral(aDescription, getter_AddRefs(descriptionLiteral));
        if (NS_FAILED(rv)) 
            return rv;
        rv = mInner->Assert(bookmarkResource, kNC_Description, descriptionLiteral, PR_TRUE);
        if (NS_FAILED(rv)) 
            return rv;
    }

    // Date: Date of Creation
    // Convert the current date/time from microseconds (PRTime) to seconds.
    nsCOMPtr<nsIRDFDate> dateLiteral;
    rv = gRDF->GetDateLiteral(PR_Now(), getter_AddRefs(dateLiteral));
    if (NS_FAILED(rv)) 
        return rv;
    rv = mInner->Assert(bookmarkResource, kNC_BookmarkAddDate, dateLiteral, PR_TRUE);
    if (NS_FAILED(rv)) 
        return rv;

    // Literal: Charset used when last visited
    nsAutoString charset; 
    charset.Assign(aDocCharSet);
    if (!charset.IsEmpty()) {
        nsCOMPtr<nsIRDFLiteral> charsetLiteral;
        rv = gRDF->GetLiteral(aDocCharSet, getter_AddRefs(charsetLiteral));
        if (NS_FAILED(rv)) 
            return rv;
        rv = mInner->Assert(bookmarkResource, kWEB_LastCharset, charsetLiteral, PR_TRUE);
        if (NS_FAILED(rv)) 
            return rv;
    }

    // Literal: PostData associated with this bookmark (used for smart keywords)
    if (aPostData && *aPostData) {
        nsCOMPtr<nsIRDFLiteral> postDataLiteral;
        rv = gRDF->GetLiteral(aPostData, getter_AddRefs(postDataLiteral));
        if (NS_FAILED(rv)) 
          return rv;
        rv = mInner->Assert(bookmarkResource, kNC_PostData, postDataLiteral, PR_TRUE);
        if (NS_FAILED(rv)) 
          return rv;
    }

    *aResult = bookmarkResource;
    NS_ADDREF(*aResult);

    return rv;
}

NS_IMETHODIMP
nsBookmarksService::CreateBookmarkInContainer(const PRUnichar* aName,
                                              const PRUnichar* aURL, 
                                              const PRUnichar* aShortcutURL, 
                                              const PRUnichar* aDescription, 
                                              const PRUnichar* aDocCharSet, 
                                              const PRUnichar* aPostData,
                                              nsIRDFResource* aParentFolder,
                                              PRInt32 aIndex,
                                              nsIRDFResource** aResult)
{
    nsCOMPtr<nsIRDFNode> nodeType;
    GetSynthesizedType(aParentFolder, getter_AddRefs(nodeType));
    if (nodeType == kNC_Livemark)
        return NS_RDF_ASSERTION_REJECTED;
    
    nsresult rv = CreateBookmark(aName, aURL, aShortcutURL, aDescription, aDocCharSet, aPostData, aResult);
    if (NS_SUCCEEDED(rv))
        rv = InsertResource(*aResult, aParentFolder, aIndex);
    return rv;
}

NS_IMETHODIMP
nsBookmarksService::CreateLivemark(const PRUnichar* aName,
                                   const PRUnichar* aURL, 
                                   const PRUnichar* aFeedURL,
                                   const PRUnichar* aDescription,
                                   nsIRDFResource** aResult)
{
    // Resource: Bookmark ID
    nsCOMPtr<nsIRDFResource> livemarkResource;
    nsresult rv = gRDF->GetAnonymousResource(getter_AddRefs(livemarkResource));

    if (NS_FAILED(rv)) 
        return rv;

    rv = gRDFC->MakeSeq(mInner, livemarkResource, nsnull);
    if (NS_FAILED(rv)) 
        return rv;

    // Literal: Name
    nsCOMPtr<nsIRDFLiteral> nameLiteral;
    nsAutoString bookmarkName; 
    bookmarkName.Assign(aName);
    if (bookmarkName.IsEmpty()) {
        getLocaleString("NewBookmark", bookmarkName);

        rv = gRDF->GetLiteral(bookmarkName.get(), getter_AddRefs(nameLiteral));
        if (NS_FAILED(rv)) 
            return rv;
    }
    else
    {
        rv = gRDF->GetLiteral(aName, getter_AddRefs(nameLiteral));
        if (NS_FAILED(rv)) 
            return rv;
    }

    rv = mInner->Assert(livemarkResource, kNC_Name, nameLiteral, PR_TRUE);
    if (NS_FAILED(rv)) 
        return rv;

    // Resource: URL
    nsAutoString url;
    url.Assign(aURL);
    nsCOMPtr<nsIRDFLiteral> urlLiteral;
    rv = gRDF->GetLiteral(url.get(), getter_AddRefs(urlLiteral));
    if (NS_FAILED(rv)) 
        return rv;
    rv = mInner->Assert(livemarkResource, kNC_URL, urlLiteral, PR_TRUE);
    if (NS_FAILED(rv)) 
        return rv;

    // Literal: FEED URL
    nsAutoString feedurl;
    feedurl.Assign(aFeedURL);
    nsCOMPtr<nsIRDFLiteral> feedurlLiteral;
    rv = gRDF->GetLiteral(feedurl.get(), getter_AddRefs(feedurlLiteral));
    if (NS_FAILED(rv))
        return rv;
    rv = mInner->Assert(livemarkResource, kNC_FeedURL, feedurlLiteral, PR_TRUE);
    if (NS_FAILED(rv))
        return rv;

    // Literal: Description
    if (aDescription && *aDescription) {
        nsCOMPtr<nsIRDFLiteral> descriptionLiteral;
        rv = gRDF->GetLiteral(aDescription, getter_AddRefs(descriptionLiteral));
        if (NS_FAILED(rv)) 
            return rv;
        rv = mInner->Assert(livemarkResource, kNC_Description, descriptionLiteral, PR_TRUE);
        if (NS_FAILED(rv)) 
            return rv;
    }

    // Date: Date of Creation
    // Convert the current date/time from microseconds (PRTime) to seconds.
    nsCOMPtr<nsIRDFDate> dateLiteral;
    rv = gRDF->GetDateLiteral(PR_Now(), getter_AddRefs(dateLiteral));
    if (NS_FAILED(rv)) 
        return rv;
    rv = mInner->Assert(livemarkResource, kNC_BookmarkAddDate, dateLiteral, PR_TRUE);
    if (NS_FAILED(rv)) 
        return rv;

    // Type: LiveMark
    rv = mInner->Assert(livemarkResource, kRDF_type, kNC_Livemark, PR_TRUE);
    if (NS_FAILED(rv))
        return rv;

    *aResult = livemarkResource;
    NS_ADDREF(*aResult);

    return rv;
}

NS_IMETHODIMP
nsBookmarksService::CreateLivemarkInContainer(const PRUnichar* aName,
                                              const PRUnichar* aURL, 
                                              const PRUnichar* aFeedURL, 
                                              const PRUnichar* aDescription, 
                                              nsIRDFResource* aParentFolder,
                                              PRInt32 aIndex,
                                              nsIRDFResource** aResult)
{
    nsCOMPtr<nsIRDFNode> nodeType;
    GetSynthesizedType(aParentFolder, getter_AddRefs(nodeType));
    if (nodeType == kNC_Livemark)
        return NS_RDF_ASSERTION_REJECTED;

    nsresult rv = CreateLivemark(aName, aURL, aFeedURL, aDescription, aResult);
    if (NS_SUCCEEDED(rv))
        rv = InsertResource(*aResult, aParentFolder, aIndex);
    return rv;
}

NS_IMETHODIMP
nsBookmarksService::CreateSeparator(nsIRDFResource** aResult)
{
    nsresult rv;

    // create the anonymous resource for the separator
    nsCOMPtr<nsIRDFResource> separatorResource;
    rv = gRDF->GetAnonymousResource(getter_AddRefs(separatorResource));
    if (NS_FAILED(rv)) 
        return rv;

    // Assert the separator type arc
    rv = mInner->Assert(separatorResource, kRDF_type, kNC_BookmarkSeparator, PR_TRUE);
    if (NS_FAILED(rv))
        return rv;

    *aResult = separatorResource;
    NS_ADDREF(*aResult);

    return rv;
}

NS_IMETHODIMP
nsBookmarksService::CloneResource(nsIRDFResource* aSource,
                                  nsIRDFResource** aResult)
{
    nsCOMPtr<nsIRDFResource> newResource;
    nsresult rv = gRDF->GetAnonymousResource(getter_AddRefs(newResource));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsISimpleEnumerator> arcs;
    rv = mInner->ArcLabelsOut(aSource, getter_AddRefs(arcs));
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool hasMore;
    while (NS_SUCCEEDED(arcs->HasMoreElements(&hasMore)) && hasMore) {
        nsCOMPtr<nsISupports> supports;
        rv = arcs->GetNext(getter_AddRefs(supports));
        NS_ENSURE_SUCCESS(rv, rv);

        nsCOMPtr<nsIRDFResource> property = do_QueryInterface(supports, &rv);
        NS_ENSURE_SUCCESS(rv, rv);
      
        // Don't duplicate the bookmarks toolbar folder arc
        PRBool isBTF;
        rv = property->EqualsNode(kNC_BookmarksToolbarFolder, &isBTF);
        NS_ENSURE_SUCCESS(rv, rv);
        if (isBTF)
            continue;

        nsCOMPtr<nsIRDFNode> target;
        rv = mInner->GetTarget(aSource, property, PR_TRUE, getter_AddRefs(target));
        NS_ENSURE_SUCCESS(rv, rv);
 
        // Test if the arc points to a child.
        PRBool isOrdinal;
        rv = gRDFC->IsOrdinalProperty(property, &isOrdinal);
        NS_ENSURE_SUCCESS(rv, rv);

        if (isOrdinal) {
            nsCOMPtr<nsIRDFResource> oldChild = do_QueryInterface(target);
            nsCOMPtr<nsIRDFResource> newChild;
            rv = CloneResource(oldChild, getter_AddRefs(newChild));
            NS_ENSURE_SUCCESS(rv, rv);

            rv = mInner->Assert(newResource, property, newChild, PR_TRUE);
        }
        else {
            rv = mInner->Assert(newResource, property, target, PR_TRUE);
        }
        NS_ENSURE_SUCCESS(rv, rv);
    }

    NS_ADDREF(*aResult = newResource);

    return NS_OK;
}

NS_IMETHODIMP
nsBookmarksService::GetParent(nsIRDFResource* aSource, nsIRDFResource **aParent)
{
    NS_PRECONDITION(aSource != nsnull, "null ptr");
    if (! aSource)
        return NS_ERROR_NULL_POINTER;
    if (! mInner)
        return NS_ERROR_UNEXPECTED;

    nsresult rv;

    nsCOMPtr<nsISimpleEnumerator> arcs;
    rv = mInner->ArcLabelsIn(aSource, getter_AddRefs(arcs));
    NS_ENSURE_SUCCESS(rv, rv);

    // enumerate the arcs pointing to the resource to find an ordinal one.
    PRBool hasMore;
    while (NS_SUCCEEDED(arcs->HasMoreElements(&hasMore)) && hasMore) {
        nsCOMPtr<nsISupports> supports;
        rv = arcs->GetNext(getter_AddRefs(supports));
        if (NS_FAILED(rv)) continue;

        nsCOMPtr<nsIRDFResource> property = do_QueryInterface(supports, &rv);
        if (NS_FAILED(rv)) continue;
   
        // Test if the arc is ordinal. We will assume that the parent is determined
        // by the first ordinal arc that points to the resource.
        // That's not bullet-proof, since several ordinal arcs can point to the same
        // resource. However, this will be sufficient for most of the cases.
        PRBool isOrdinal;
        rv = gRDFC->IsOrdinalProperty(property, &isOrdinal);
        if (NS_FAILED(rv)) continue;

        if (isOrdinal) {
            nsCOMPtr<nsIRDFResource> parent;
            rv = mInner->GetSource(property, aSource, PR_TRUE, getter_AddRefs(parent));
            if (NS_FAILED(rv)) continue;
            NS_ADDREF(*aParent = parent);
            return NS_OK;
        }
    }

    *aParent = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsBookmarksService::GetParentChain(nsIRDFResource* aSource, nsIArray** aParents)
{
    NS_PRECONDITION(aSource != nsnull, "null ptr");
    if (! aSource)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;
    nsCOMPtr<nsIMutableArray> parentArray;
    rv = NS_NewArray(getter_AddRefs(parentArray));
    NS_ENSURE_SUCCESS(rv, NS_ERROR_OUT_OF_MEMORY);

    nsCOMPtr<nsIRDFResource> source = aSource, parent;
    
    while (NS_SUCCEEDED(rv=GetParent(source, getter_AddRefs(parent)))
           && parent) {
        parentArray->InsertElementAt(parent, 0, PR_FALSE);
        source = parent;
    }

    if (NS_SUCCEEDED(rv))
        NS_ADDREF(*aParents = parentArray);

    return rv;
}

//XXXpch: useless callers of AddBookmarkImmediately and IsBookmarked in nsInternetSearchService.cpp
//to be removed RSN

NS_IMETHODIMP
nsBookmarksService::AddBookmarkImmediately(const PRUnichar *aURI,
                                           const PRUnichar *aTitle, 
                                           PRInt32 aBookmarkType, 
                                           const PRUnichar *aCharset)
{
    return NS_OK;
}
NS_IMETHODIMP
nsBookmarksService::IsBookmarked(const char* aURL, PRBool* aIsBookmarked)
{
    *aIsBookmarked = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsBookmarksService::IsBookmarkedResource(nsIRDFResource *aSource, PRBool *aIsBookmarked)
{
    NS_PRECONDITION(aSource != nsnull, "null ptr");
    if (! aSource)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;
    nsCOMPtr<nsIArray> parentArray;
    rv = GetParentChain(aSource, getter_AddRefs(parentArray));
    if (NS_FAILED(rv)) {
        *aIsBookmarked = PR_FALSE;
        return rv;
    }
    
    // note: returns PR_FALSE for the bookmarks top root
    PRUint32 length = 0;
    rv = parentArray->GetLength(&length);
    if (NS_FAILED(rv))
        return rv;

    if (length == 0) {
        *aIsBookmarked = PR_FALSE;
    } else {
        nsCOMPtr<nsIRDFResource> topParent;
        parentArray->QueryElementAt(0, NS_GET_IID(nsIRDFResource),
                                    getter_AddRefs(topParent));
        *aIsBookmarked = topParent == kNC_BookmarksTopRoot;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsBookmarksService::UpdateBookmarkIcon(const char *aURL, const char *aMIMEType,
                                       const PRUint8* aIconData, const PRUint32 aIconDataLen)
{
    nsCOMPtr<nsIRDFLiteral> urlLiteral;
    nsresult rv = gRDF->GetLiteral(NS_ConvertUTF8toUCS2(aURL).get(),
                                   getter_AddRefs(urlLiteral));
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsISimpleEnumerator> bookmarks;
    rv = mInner->GetSources(kNC_URL, urlLiteral, PR_TRUE, getter_AddRefs(bookmarks));
    if (NS_FAILED(rv))
        return rv;

    // base64 encode the icon data, and create a new literal;
    // or encode data: if it's invalid
    nsCOMPtr<nsIRDFLiteral> iconDataLiteral;
    PRBool isInvalidIcon = PR_FALSE;
    if (aIconData == NULL || aIconDataLen == 0) {
        isInvalidIcon = PR_TRUE;
        rv = gRDF->GetLiteral (NS_LITERAL_STRING("data:").get(), getter_AddRefs(iconDataLiteral));
        if (NS_FAILED(rv)) return rv;
    } else {
        PRInt32 len = ((aIconDataLen + 2) / 3) * 4;
        char *iconDataBase64 = PL_Base64Encode((const char *) aIconData, aIconDataLen, nsnull);
        if (!iconDataBase64) {
            return NS_ERROR_OUT_OF_MEMORY;
        }

        nsString dataUri;
        dataUri += NS_LITERAL_STRING("data:");
        dataUri += NS_ConvertASCIItoUTF16(aMIMEType);
        dataUri += NS_LITERAL_STRING(";base64,");
        dataUri += NS_ConvertASCIItoUTF16(iconDataBase64, len);
        nsMemory::Free(iconDataBase64);

        rv = gRDF->GetLiteral (dataUri.get(), getter_AddRefs(iconDataLiteral));
        if (NS_FAILED(rv)) return rv;
    }

    PRBool hasMoreBookmarks = PR_FALSE;
    while (NS_SUCCEEDED(rv = bookmarks->HasMoreElements(&hasMoreBookmarks)) &&
           hasMoreBookmarks) {
        nsCOMPtr<nsISupports> supports;
        rv = bookmarks->GetNext(getter_AddRefs(supports));
        if (NS_FAILED(rv)) 
            return rv;

        nsCOMPtr<nsIRDFResource> bookmark = do_QueryInterface(supports);
        if (bookmark) {
            nsCOMPtr<nsIRDFNode> iconNode;
            rv = mInner->GetTarget(bookmark, kNC_Icon, PR_TRUE, getter_AddRefs(iconNode));
            if (NS_SUCCEEDED(rv) && rv != NS_RDF_NO_VALUE) {
                (void) mInner->Unassert(bookmark, kNC_Icon, iconNode);
            }

            nsCOMPtr<nsIRDFPropagatableDataSource> propDS = do_QueryInterface(mInner);
            PRBool oldPropChanges = PR_TRUE;

            // if it's just "data:", then don't send notifications, otherwise
            // things will update with a null icon
            if (propDS && isInvalidIcon) {
                (void) propDS->GetPropagateChanges(&oldPropChanges);
                (void) propDS->SetPropagateChanges(PR_FALSE);
            }

            rv = mInner->Assert(bookmark, kNC_Icon, iconDataLiteral, PR_TRUE);

            if (propDS && isInvalidIcon)
                (void) propDS->SetPropagateChanges(oldPropChanges);

            if (NS_FAILED(rv))
                return rv;

            mDirty = PR_TRUE;
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsBookmarksService::RemoveBookmarkIcon(const char *aURL)
{
    nsCOMPtr<nsIRDFLiteral> urlLiteral;
    nsresult rv = gRDF->GetLiteral(NS_ConvertUTF8toUCS2(aURL).get(),
                                   getter_AddRefs(urlLiteral));
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsISimpleEnumerator> bookmarks;
    rv = mInner->GetSources(kNC_URL, urlLiteral, PR_TRUE, getter_AddRefs(bookmarks));
    if (NS_FAILED(rv))
        return rv;

    PRBool hasMoreBookmarks = PR_FALSE;
    while (NS_SUCCEEDED(rv = bookmarks->HasMoreElements(&hasMoreBookmarks)) &&
           hasMoreBookmarks) {
        nsCOMPtr<nsISupports> supports;
        rv = bookmarks->GetNext(getter_AddRefs(supports));
        if (NS_FAILED(rv)) 
            return rv;

        nsCOMPtr<nsIRDFResource> bookmark = do_QueryInterface(supports);
        if (bookmark) {
            nsCOMPtr<nsISimpleEnumerator> iconEnumerator;
            rv = mInner->GetTargets(bookmark, kNC_Icon, PR_TRUE, getter_AddRefs(iconEnumerator));
            if (NS_FAILED(rv))
                return rv;

            PRBool hasMore = PR_FALSE;
            rv = iconEnumerator->HasMoreElements(&hasMore);
            if (NS_FAILED(rv))
                return rv;
            while (hasMore) {
                nsCOMPtr<nsISupports> supports;
                rv = iconEnumerator->GetNext(getter_AddRefs(supports));
                if (NS_FAILED(rv))
                    return rv;

                nsCOMPtr<nsIRDFNode> targetNode = do_QueryInterface(supports);
                if (targetNode)
                    (void)mInner->Unassert(bookmark, kNC_Icon, targetNode);

                rv = iconEnumerator->HasMoreElements(&hasMore);
                if (NS_FAILED(rv))
                    return rv;
            }
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsBookmarksService::UpdateLastVisitedDate(const char *aURL,
                                          const PRUnichar *aCharset)
{
    NS_PRECONDITION(aURL != nsnull, "null ptr");
    if (! aURL)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aCharset != nsnull, "null ptr");
    if (! aCharset)
        return NS_ERROR_NULL_POINTER;

    nsCOMPtr<nsIRDFLiteral> urlLiteral;
    nsresult rv = gRDF->GetLiteral(NS_ConvertUTF8toUCS2(aURL).get(),
                                   getter_AddRefs(urlLiteral));
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsISimpleEnumerator> bookmarks;
    rv = GetSources(kNC_URL, urlLiteral, PR_TRUE, getter_AddRefs(bookmarks));
    if (NS_FAILED(rv))
        return rv;

    PRBool hasMoreBookmarks = PR_FALSE;
    while (NS_SUCCEEDED(rv = bookmarks->HasMoreElements(&hasMoreBookmarks)) &&
           hasMoreBookmarks) {
        nsCOMPtr<nsISupports> supports;
        rv = bookmarks->GetNext(getter_AddRefs(supports));
        if (NS_FAILED(rv)) 
            return rv;

        nsCOMPtr<nsIRDFResource> bookmark = do_QueryInterface(supports);
        if (bookmark) {
            // Always use mInner! Otherwise, we could get into an infinite loop
            // due to Assert/Change calling UpdateBookmarkLastModifiedDate().

            nsCOMPtr<nsIRDFNode> nodeType;
            GetSynthesizedType(bookmark, getter_AddRefs(nodeType));
            if (nodeType == kNC_Bookmark) {
                nsCOMPtr<nsIRDFDate> now;
                rv = gRDF->GetDateLiteral(PR_Now(), getter_AddRefs(now));
                if (NS_FAILED(rv))
                    return rv;

                nsCOMPtr<nsIRDFNode> lastMod;
                rv = mInner->GetTarget(bookmark, kWEB_LastVisitDate, PR_TRUE,
                                       getter_AddRefs(lastMod));
                if (NS_FAILED(rv))
                    return rv;

                if (lastMod) {
                    rv = mInner->Change(bookmark, kWEB_LastVisitDate, lastMod, now);
                }
                else {
                    rv = mInner->Assert(bookmark, kWEB_LastVisitDate, now, PR_TRUE);
                }
                if (NS_FAILED(rv))
                    return rv;

                // Piggy-backing last charset.
                if (aCharset && *aCharset) {
                    nsCOMPtr<nsIRDFLiteral> charsetliteral;
                    rv = gRDF->GetLiteral(aCharset,
                                          getter_AddRefs(charsetliteral));
                    if (NS_FAILED(rv))
                        return rv;

                    nsCOMPtr<nsIRDFNode> charsetNode;
                    rv = mInner->GetTarget(bookmark, kWEB_LastCharset, PR_TRUE,
                                           getter_AddRefs(charsetNode));
                    if (NS_FAILED(rv))
                        return rv;

                    if (charsetNode) {
                        rv = mInner->Change(bookmark, kWEB_LastCharset,
                                            charsetNode, charsetliteral);
                    }
                    else {
                        rv = mInner->Assert(bookmark, kWEB_LastCharset,
                                            charsetliteral, PR_TRUE);
                    }
                    if (NS_FAILED(rv))
                        return rv;
                } 

                // Also update bookmark's "status"!
                nsCOMPtr<nsIRDFNode> statusNode;
                rv = mInner->GetTarget(bookmark, kWEB_Status, PR_TRUE,
                                       getter_AddRefs(statusNode));
                if (NS_SUCCEEDED(rv) && statusNode) {
                    rv = mInner->Unassert(bookmark, kWEB_Status, statusNode);
                    NS_ASSERTION(rv == NS_RDF_ASSERTION_ACCEPTED, "unable to Unassert changed status");
                }

//              mDirty = PR_TRUE;
            }
        }
    }

    return rv;
}

nsresult
nsBookmarksService::GetSynthesizedType(nsIRDFResource *aNode, nsIRDFNode **aType)
{
    *aType = nsnull;
    nsresult rv = mInner->GetTarget(aNode, kRDF_type, PR_TRUE, aType);
    if (NS_FAILED(rv) || (rv == NS_RDF_NO_VALUE)) {
        // if we didn't match anything in the graph, synthesize its type
        // (which is either a bookmark or a bookmark folder, since everything
        // else is annotated)
        PRBool isBookmarked = PR_FALSE;
        if (NS_SUCCEEDED(rv = IsBookmarkedResource(aNode, &isBookmarked))
            && isBookmarked)
        {
            PRBool isContainer = PR_FALSE;
            (void)gRDFC->IsSeq(mInner, aNode, &isContainer);
            *aType = isContainer? kNC_Folder : kNC_Bookmark;
        }
#ifdef XP_BEOS
        else
        {
            //solution for BeOS - bookmarks are stored as file attributes. 
            *aType = kNC_URL;
        }
#endif
        NS_IF_ADDREF(*aType);
    }
    return NS_OK;
}

nsresult
nsBookmarksService::UpdateBookmarkLastModifiedDate(nsIRDFResource *aSource)
{
    nsCOMPtr<nsIRDFDate>    now;
    nsresult        rv;

    if (NS_SUCCEEDED(rv = gRDF->GetDateLiteral(PR_Now(), getter_AddRefs(now))))
    {
        nsCOMPtr<nsIRDFNode>    lastMod;

        // Note: always use mInner!! Otherwise, could get into an infinite loop
        // due to Assert/Change calling UpdateBookmarkLastModifiedDate()

        if (NS_SUCCEEDED(rv = mInner->GetTarget(aSource, kWEB_LastModifiedDate, PR_TRUE,
            getter_AddRefs(lastMod))) && (rv != NS_RDF_NO_VALUE))
        {
            rv = mInner->Change(aSource, kWEB_LastModifiedDate, lastMod, now);
        }
        else
        {
            rv = mInner->Assert(aSource, kWEB_LastModifiedDate, now, PR_TRUE);
        }
    }
    return rv;
}

NS_IMETHODIMP
nsBookmarksService::ResolveKeyword(const PRUnichar *aUserInput, PRUnichar** aPostData, char **aShortcutURL)
{
    NS_PRECONDITION(aUserInput != nsnull, "null ptr");
    if (! aUserInput)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aShortcutURL != nsnull, "null ptr");
    if (! aShortcutURL)
        return NS_ERROR_NULL_POINTER;

    // Shortcuts are always lowercased internally.
    nsAutoString shortcut(aUserInput);
    ToLowerCase(shortcut);

    nsCOMPtr<nsIRDFLiteral> shortcutLiteral;
    nsresult rv = gRDF->GetLiteral(shortcut.get(),
                                   getter_AddRefs(shortcutLiteral));
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsIRDFResource> source;
    rv = GetSource(kNC_ShortcutURL, shortcutLiteral, PR_TRUE,
                   getter_AddRefs(source));
    if (NS_FAILED(rv))
        return rv;

    if (source) {
        // Get postData
        nsCOMPtr<nsIRDFNode> node;
        GetTarget(source, kNC_PostData, PR_TRUE, getter_AddRefs(node));
        if (node) {
            nsCOMPtr<nsIRDFLiteral> postData(do_QueryInterface(node));

            const PRUnichar* postDataVal = nsnull;
            postData->GetValueConst(&postDataVal);

            nsDependentString postDataStr(postDataVal);
            *aPostData = ToNewUnicode(postDataStr);
        }

        nsAutoString url;
        rv = GetURLFromResource(source, url);
        if (NS_FAILED(rv))
           return rv;

        if (!url.IsEmpty()) {
            *aShortcutURL = ToNewUTF8String(url);
            return NS_OK;
        }
    }

    *aShortcutURL = nsnull;
    return NS_RDF_NO_VALUE;
}

NS_IMETHODIMP
nsBookmarksService::ImportSystemBookmarks(nsIRDFResource* aParentFolder)
{
    gImportedSystemBookmarks = PR_TRUE;

#if defined(XP_MAC) || defined(XP_MACOSX)
    nsCOMPtr<nsIFile> ieFavoritesFile;
    nsresult rv = NS_GetSpecialDirectory(NS_MAC_PREFS_DIR, getter_AddRefs(ieFavoritesFile));
    NS_ENSURE_SUCCESS(rv, rv);

    ieFavoritesFile->Append(NS_LITERAL_STRING("Explorer"));
    ieFavoritesFile->Append(NS_LITERAL_STRING("Favorites.html"));

    BookmarkParser parser;
    parser.Init(ieFavoritesFile, mInner);
    BeginUpdateBatch();
    parser.Parse(aParentFolder, kNC_Bookmark);
    EndUpdateBatch();
#endif

    return NS_OK;
}

#if defined(XP_MAC) || defined(XP_MACOSX)
void
nsBookmarksService::HandleSystemBookmarks(nsIRDFNode* aNode) 
{
    if (!gImportedSystemBookmarks && aNode == kNC_SystemBookmarksStaticRoot)
    {
        PRBool isSeq = PR_TRUE;
        gRDFC->IsSeq(mInner, kNC_SystemBookmarksStaticRoot, &isSeq);
    
        if (!isSeq)
        {
            nsCOMPtr<nsIRDFContainer> ctr;
            gRDFC->MakeSeq(mInner, kNC_SystemBookmarksStaticRoot, getter_AddRefs(ctr));
      
            ImportSystemBookmarks(kNC_SystemBookmarksStaticRoot);
        }
    }
#if defined(XP_MAC) || defined(XP_MACOSX)
    // on the Mac, IE favorites are stored in an HTML file.
    // Defer importing the contents of this file until necessary.
    else if ((aNode == kNC_IEFavoritesRoot) && (mIEFavoritesAvailable == PR_FALSE))
        ReadFavorites();
#endif
}
#endif

NS_IMETHODIMP
nsBookmarksService::GetTransactionManager(nsITransactionManager** aTransactionManager)
{
    NS_ENSURE_ARG_POINTER(aTransactionManager);

    NS_ADDREF(*aTransactionManager = mTransactionManager);
    return NS_OK;
}

NS_IMETHODIMP
nsBookmarksService::GetBookmarksToolbarFolder(nsIRDFResource** aResult)
{
    
    nsCOMPtr<nsIRDFResource> btfResource;
    mInner->GetSource(kNC_BookmarksToolbarFolder, kTrueLiteral, PR_TRUE,
                      getter_AddRefs(btfResource));

    NS_IF_ADDREF(*aResult = btfResource);
    return NS_OK;
}

NS_IMETHODIMP
nsBookmarksService::SetBookmarksToolbarFolder(nsIRDFResource* aNewBTF)
{
    nsresult rv;
    nsCOMPtr<nsIRDFResource> oldBTF;
    rv = GetBookmarksToolbarFolder(getter_AddRefs(oldBTF));
    if (rv != NS_RDF_NO_VALUE) {
        rv = mInner->Unassert(oldBTF, kNC_BookmarksToolbarFolder, kTrueLiteral);
        NS_ENSURE_SUCCESS(rv, rv);
    }
    rv = mInner->Assert(aNewBTF, kNC_BookmarksToolbarFolder, kTrueLiteral, PR_TRUE);
    return rv;
}

int
CompareLastModifiedFolders(nsIRDFResource* aResource1, nsIRDFResource* aResource2, void* aOuter)
{
    
    nsCOMPtr<nsIRDFNode> node1, node2;
    nsIRDFDataSource* outer = NS_STATIC_CAST(nsIRDFDataSource*, aOuter);
    outer->GetTarget(aResource1, kWEB_LastModifiedDate, PR_TRUE, getter_AddRefs(node1));
    outer->GetTarget(aResource2, kWEB_LastModifiedDate, PR_TRUE, getter_AddRefs(node2));

    nsCOMPtr<nsIRDFDate> date1 = do_QueryInterface(node1);
    if (!date1)
        return 1;
    nsCOMPtr<nsIRDFDate> date2 = do_QueryInterface(node2);
    if (!date2)
        return -1;

    PRTime value1, value2;
    date1->GetValue(&value1);
    date2->GetValue(&value2);

    PRInt64 delta;
    LL_SUB(delta, value1, value2);

    return LL_GE_ZERO(delta)? -1 : 1;
}

nsresult
nsBookmarksService::GetLastModifiedFolders(nsISimpleEnumerator **aResult)
{
    nsresult rv;
    nsCOMArray<nsIRDFResource> folderArray;

    nsCOMPtr<nsISimpleEnumerator> elements;
    rv = mInner->GetAllResources(getter_AddRefs(elements));
    if (NS_FAILED(rv))
        return rv;

    // fill the array with all the bookmark folder resources
    PRBool hasMore = PR_FALSE;
    while (NS_SUCCEEDED(rv = elements->HasMoreElements(&hasMore)) &&
           hasMore) {
        nsCOMPtr<nsISupports> supports;
        rv = elements->GetNext(getter_AddRefs(supports));
        if (NS_FAILED(rv))
            return rv;

        nsCOMPtr<nsIRDFResource> element = do_QueryInterface(supports, &rv);
        if (NS_FAILED(rv))
            return rv;

        nsCOMPtr<nsIRDFNode> nodeType;
        GetSynthesizedType(element, getter_AddRefs(nodeType));

        if (nodeType == kNC_Folder && element != kNC_BookmarksTopRoot)
            folderArray.AppendObject(element);
    }

    // sort the array containing all the folders
    folderArray.Sort(CompareLastModifiedFolders, NS_STATIC_CAST(void*, mInner));

    // only keep the first elements
    PRInt32 index;
    for (index = folderArray.Count()-1; index >= MAX_LAST_MODIFIED_FOLDERS; index--)
        folderArray.RemoveObjectAt(index);

    // always show the bookmarks root
    if (folderArray.IndexOfObject(kNC_BookmarksRoot) < 0)
        folderArray.ReplaceObjectAt(kNC_BookmarksRoot, MAX_LAST_MODIFIED_FOLDERS-1);

    // always show the bookmarks toolbar folder
    nsCOMPtr<nsIRDFResource> btfResource;
    rv = GetBookmarksToolbarFolder(getter_AddRefs(btfResource));
    if (NS_SUCCEEDED(rv) && folderArray.IndexOfObject(btfResource) < 0) {
        if (folderArray.ObjectAt(MAX_LAST_MODIFIED_FOLDERS-1) == kNC_BookmarksRoot)
            index = MAX_LAST_MODIFIED_FOLDERS - 2;
        else
            index = MAX_LAST_MODIFIED_FOLDERS - 1;
        folderArray.ReplaceObjectAt(btfResource, index);
    }

    return NS_NewArrayEnumerator(aResult, folderArray);
}

////////////////////////////////////////////////////////////////////////
// nsIRDFDataSource

NS_IMETHODIMP
nsBookmarksService::GetURI(char* *aURI)
{
    *aURI = nsCRT::strdup("rdf:bookmarks");
    if (! *aURI)
        return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
}

NS_IMETHODIMP
nsBookmarksService::GetTarget(nsIRDFResource* aSource,
                              nsIRDFResource* aProperty,
                              PRBool aTruthValue,
                              nsIRDFNode** aTarget)
{
    NS_PRECONDITION(aSource != nsnull, "null ptr");
    if (! aSource)
        return NS_ERROR_NULL_POINTER;
    NS_PRECONDITION(aProperty != nsnull, "null ptr");
    if (! aProperty)
        return NS_ERROR_NULL_POINTER;

    *aTarget = nsnull;

    nsresult rv;
    PRBool isLivemark = PR_FALSE;

    if (aTruthValue && (aProperty == kRDF_type))
    {
        rv = GetSynthesizedType(aSource, aTarget);
        return rv;
    }
    else if (aProperty == kNC_Icon)
    {
        if (!mBrowserIcons) {
            // if the user has favicons turned off, don't return anything
            *aTarget = nsnull;
            return NS_RDF_NO_VALUE;
        } else {
            // the user doesn't have favicons turned off, but might have
            // old non-data URLs for icons.  We only want to return the
            // value if it's a data url.
            rv = mInner->GetTarget(aSource, aProperty, aTruthValue, aTarget);
            if (NS_FAILED(rv) || rv == NS_RDF_NO_VALUE)
                return rv;

            nsCOMPtr<nsIRDFLiteral> iconLiteral = do_QueryInterface(*aTarget);
            if (!iconLiteral) {
                // erm, shouldn't happen
                *aTarget = nsnull;
                return NS_RDF_NO_VALUE;
            }

            const PRUnichar *url = nsnull;
            iconLiteral->GetValueConst(&url);
            nsDependentString urlStr(url);

            // if it's a data: url, all is well
            if (Substring(urlStr, 0, 5).Equals(NS_LITERAL_STRING("data:"))) {
                // XXX hack warning!  Sometimes we want to indicate that a site
                // should have no favicon even though it wants to feed us goop.
                // To avoid reloading said goop each time, we stick in a URL
                // consisting of purely "data:".  So, if that's what we have,
                // we pretend it has no icon.
                if (urlStr.Length() == 5) {
                    *aTarget = nsnull;
                    return NS_RDF_NO_VALUE;
                }

                return NS_OK;
            }

            // no data:? no icon!
            *aTarget = nsnull;
            return NS_RDF_NO_VALUE;
        }
    }
    else if ((aProperty == kNC_child || aProperty == kRDF_nextVal) &&
             NS_SUCCEEDED(mInner->HasAssertion(aSource, kRDF_type, kNC_Livemark,
                                               PR_TRUE, &isLivemark)) &&
             isLivemark)
    {
        UpdateLivemarkChildren (aSource);
    }

    rv = mInner->GetTarget(aSource, aProperty, aTruthValue, aTarget);
    return rv;
}

NS_IMETHODIMP
nsBookmarksService::GetTargets(nsIRDFResource* aSource,
                               nsIRDFResource* aProperty,
                               PRBool aTruthValue,
                               nsISimpleEnumerator** aTargets)
{

  NS_PRECONDITION(aSource != nsnull, "null ptr");
  if (! aSource)
    return NS_ERROR_NULL_POINTER;

  NS_PRECONDITION(aProperty != nsnull, "null ptr");
  if (! aProperty)
    return NS_ERROR_NULL_POINTER;

  if (!aTruthValue)
    return NS_NewEmptyEnumerator(aTargets);

  if (aSource == kNC_LastModifiedFoldersRoot && aProperty == kNC_child) {
    return GetLastModifiedFolders(aTargets);
  }

  if ((aProperty == kNC_Icon) && !mBrowserIcons) {
    // if the user has favicons turned off, don't return anything
    return NS_NewEmptyEnumerator(aTargets);
  }

  PRBool isLivemark = PR_FALSE;
  if (aProperty == kNC_child &&
      NS_SUCCEEDED(mInner->HasAssertion(aSource, kRDF_type, kNC_Livemark,
                                        PR_TRUE, &isLivemark)) &&
      isLivemark)
  {
    UpdateLivemarkChildren(aSource);
  }

  return mInner->GetTargets(aSource, aProperty, aTruthValue, aTargets);

}

void
nsBookmarksService::AnnotateBookmarkSchedule(nsIRDFResource* aSource, PRBool scheduleFlag)
{
    if (scheduleFlag)
    {
        PRBool exists = PR_FALSE;
        if (NS_SUCCEEDED(mInner->HasAssertion(aSource, kWEB_ScheduleActive,
                                              kTrueLiteral, PR_TRUE, &exists)) && (!exists))
        {
            (void)mInner->Assert(aSource, kWEB_ScheduleActive, kTrueLiteral, PR_TRUE);
        }
    }
    else
    {
        (void)mInner->Unassert(aSource, kWEB_ScheduleActive, kTrueLiteral);
    }
}

NS_IMETHODIMP
nsBookmarksService::Assert(nsIRDFResource* aSource,
                           nsIRDFResource* aProperty,
                           nsIRDFNode* aTarget,
                           PRBool aTruthValue)
{
    nsresult rv = NS_RDF_ASSERTION_REJECTED;

    if (CanAccept(aSource, aProperty, aTarget))
    {
        rv = mInner->Assert(aSource, aProperty, aTarget, aTruthValue);
        if (NS_FAILED(rv))
            return rv;

        UpdateBookmarkLastModifiedDate(aSource);
            
        if (aProperty == kWEB_Schedule) {
              AnnotateBookmarkSchedule(aSource, PR_TRUE);
        } else if (aProperty == kNC_FeedURL) {
            /* Reload feed URL - also allow only one LivemarkExpiration */
            nsCOMPtr<nsIRDFNode> oldExpiration;
            rv = mInner->GetTarget(aSource, kNC_LivemarkExpiration, PR_TRUE, getter_AddRefs(oldExpiration));
            if (rv == NS_OK)
                mInner->Unassert(aSource, kNC_LivemarkExpiration, oldExpiration);
            rv = UpdateLivemarkChildren(aSource);
            return rv;
        } else if (aProperty == kRDF_type && aTarget == kNC_Livemark) {
            rv = gRDFC->MakeSeq(mInner, aSource, nsnull);
        }
    }

    return rv;
}

NS_IMETHODIMP
nsBookmarksService::Unassert(nsIRDFResource* aSource,
                             nsIRDFResource* aProperty,
                             nsIRDFNode* aTarget)
{
    nsresult rv = NS_RDF_ASSERTION_REJECTED;

    if (CanAccept(aSource, aProperty, aTarget)) {
        rv = mInner->Unassert(aSource, aProperty, aTarget);
        if (NS_FAILED(rv))
            return rv;

        UpdateBookmarkLastModifiedDate(aSource);

        if (aProperty == kWEB_Schedule) {
            AnnotateBookmarkSchedule(aSource, PR_FALSE);
        } else if (aProperty == kRDF_type && aTarget == kNC_Livemark) {
            rv = nsBMSVCUnmakeSeq(mInner, aSource);
        } else if (aProperty == kNC_LivemarkExpiration) {
            // reload livemark if someone unasserted the expiration
            // clear the Seq to make the command feel more responsive
            nsBMSVCClearSeqContainer(mInner, aSource);
            rv = UpdateLivemarkChildren(aSource);
        }
    }

    return rv;
}

NS_IMETHODIMP
nsBookmarksService::Change(nsIRDFResource* aSource,
                           nsIRDFResource* aProperty,
                           nsIRDFNode* aOldTarget,
                           nsIRDFNode* aNewTarget)
{
    nsresult rv = NS_RDF_ASSERTION_REJECTED;

    if (CanAccept(aSource, aProperty, aNewTarget)) {
        rv = mInner->Change(aSource, aProperty, aOldTarget, aNewTarget);
        if (NS_FAILED(rv))
            return rv;

        UpdateBookmarkLastModifiedDate(aSource);

        if (aProperty == kWEB_Schedule) {
            AnnotateBookmarkSchedule(aSource, PR_TRUE);
        } else if (aProperty == kNC_FeedURL) {
            /* Reload feed data */
            nsCOMPtr<nsIRDFNode> oldExpiration;
            rv = mInner->GetTarget(aSource, kNC_LivemarkExpiration, PR_TRUE, getter_AddRefs(oldExpiration));
            if (rv == NS_OK)
                mInner->Unassert(aSource, kNC_LivemarkExpiration, oldExpiration);
            rv = UpdateLivemarkChildren(aSource);
            return rv;
        } else if (aProperty == kRDF_type) {
            if (aNewTarget == kNC_Livemark) {
                rv = gRDFC->MakeSeq(mInner, aSource, nsnull);
            } else if (aNewTarget == kNC_Bookmark) {
                rv = nsBMSVCUnmakeSeq(mInner, aSource);
            }
        }
    }

    return rv;
}

NS_IMETHODIMP
nsBookmarksService::Move(nsIRDFResource* aOldSource,
                         nsIRDFResource* aNewSource,
                         nsIRDFResource* aProperty,
                         nsIRDFNode* aTarget)
{
    nsresult    rv = NS_RDF_ASSERTION_REJECTED;

    if (CanAccept(aNewSource, aProperty, aTarget))
    {
        rv = mInner->Move(aOldSource, aNewSource, aProperty, aTarget);
        if (NS_FAILED(rv))
            return rv;

        UpdateBookmarkLastModifiedDate(aOldSource);
        UpdateBookmarkLastModifiedDate(aNewSource);
    }
    return rv;
}

NS_IMETHODIMP
nsBookmarksService::HasAssertion(nsIRDFResource* source,
                nsIRDFResource* property,
                nsIRDFNode* target,
                PRBool tv,
                PRBool* hasAssertion)
{
#if defined(XP_MAC) || defined(XP_MACOSX)
    HandleSystemBookmarks(source);
#endif

    PRBool isLivemark = PR_FALSE;
    if (property != kNC_LivemarkLock &&
        (property == kRDF_nextVal || property == kNC_child) &&
        NS_SUCCEEDED(mInner->HasAssertion(source, kRDF_type, kNC_Livemark, PR_TRUE, &isLivemark)) &&
        isLivemark)
    {
        const char *cval;
        property->GetValueConst (&cval);
        UpdateLivemarkChildren(source);
    }

    return mInner->HasAssertion(source, property, target, tv, hasAssertion);
}

NS_IMETHODIMP
nsBookmarksService::AddObserver(nsIRDFObserver* aObserver)
{
    if (! aObserver)
        return NS_ERROR_NULL_POINTER;

    if (! mObservers.AppendObject(aObserver)) {
        return NS_ERROR_FAILURE;
    }
    
    return NS_OK;
}

NS_IMETHODIMP
nsBookmarksService::RemoveObserver(nsIRDFObserver* aObserver)
{
    if (! aObserver)
        return NS_ERROR_NULL_POINTER;

    mObservers.RemoveObject(aObserver);

    return NS_OK;
}

NS_IMETHODIMP
nsBookmarksService::HasArcIn(nsIRDFNode *aNode, nsIRDFResource *aArc, PRBool *_retval)
{
#if defined(XP_MAC) || defined(XP_MACOSX)
    HandleSystemBookmarks(aNode);
#endif

    return mInner->HasArcIn(aNode, aArc, _retval);
}

NS_IMETHODIMP
nsBookmarksService::HasArcOut(nsIRDFResource *aSource, nsIRDFResource *aArc, PRBool *_retval)
{
#if defined(XP_MAC) || defined(XP_MACOSX)
    HandleSystemBookmarks(aSource);
#endif

    PRBool isLivemark = PR_FALSE;
    if (NS_SUCCEEDED(mInner->HasAssertion(aSource, kRDF_type, kNC_Livemark, PR_TRUE, &isLivemark)) &&
        isLivemark)
    {
        UpdateLivemarkChildren(aSource);
    }

    return mInner->HasArcOut(aSource, aArc, _retval);
}

NS_IMETHODIMP
nsBookmarksService::ArcLabelsOut(nsIRDFResource* source,
                nsISimpleEnumerator** labels)
{
#if defined(XP_MAC) || defined(XP_MACOSX)
    HandleSystemBookmarks(source);
#endif

    return mInner->ArcLabelsOut(source, labels);
}

NS_IMETHODIMP
nsBookmarksService::GetAllResources(nsISimpleEnumerator** aResult)
{
#if defined(XP_MAC) || defined(XP_MACOSX)
    HandleSystemBookmarks(kNC_SystemBookmarksStaticRoot);
#endif
  
    return mInner->GetAllResources(aResult);
}

NS_IMETHODIMP
nsBookmarksService::GetAllCmds(nsIRDFResource* source,
                   nsISimpleEnumerator/*<nsIRDFResource>*/** commands)
{
    nsCOMPtr<nsISupportsArray>  cmdArray;
    nsresult            rv;
    rv = NS_NewISupportsArray(getter_AddRefs(cmdArray));
    if (NS_FAILED(rv))  return rv;

    // determine type
    nsCOMPtr<nsIRDFNode> nodeType;
    GetSynthesizedType(source, getter_AddRefs(nodeType));

    PRBool  isBookmark, isBookmarkFolder, isBookmarkSeparator, isLivemark;
    isBookmark = (nodeType == kNC_Bookmark) ? PR_TRUE : PR_FALSE;
    isBookmarkFolder = (nodeType == kNC_Folder) ? PR_TRUE : PR_FALSE;
    isBookmarkSeparator = (nodeType == kNC_BookmarkSeparator) ? PR_TRUE : PR_FALSE;
    isLivemark = (nodeType == kNC_Livemark) ? PR_TRUE : PR_FALSE;

    if (isBookmark || isBookmarkFolder || isBookmarkSeparator || isLivemark)
    {
        cmdArray->AppendElement(kNC_BookmarkCommand_NewBookmark);
        cmdArray->AppendElement(kNC_BookmarkCommand_NewFolder);
        cmdArray->AppendElement(kNC_BookmarkCommand_NewSeparator);
        cmdArray->AppendElement(kNC_BookmarkSeparator);
    }
    if (isBookmark || isLivemark)
    {
        cmdArray->AppendElement(kNC_BookmarkCommand_DeleteBookmark);
    }
    if (isLivemark)
    {
        cmdArray->AppendElement(kNC_BookmarkCommand_RefreshLivemark);
    }
    if (isBookmarkFolder && (source != kNC_BookmarksRoot) && (source != kNC_IEFavoritesRoot))
    {
        cmdArray->AppendElement(kNC_BookmarkCommand_DeleteBookmarkFolder);
    }
    if (isBookmarkSeparator)
    {
        cmdArray->AppendElement(kNC_BookmarkCommand_DeleteBookmarkSeparator);
    }
    if (isBookmarkFolder)
    {
        nsCOMPtr<nsIRDFResource> personalToolbarFolder;
        GetBookmarksToolbarFolder(getter_AddRefs(personalToolbarFolder));

        cmdArray->AppendElement(kNC_BookmarkSeparator);
        if (source != personalToolbarFolder.get())  cmdArray->AppendElement(kNC_BookmarkCommand_SetPersonalToolbarFolder);
    }

    // always append a separator last (due to aggregation of commands from multiple datasources)
    cmdArray->AppendElement(kNC_BookmarkSeparator);

    nsISimpleEnumerator *result = new nsArrayEnumerator(cmdArray);
    if (!result)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(result);
    *commands = result;
    return NS_OK;
}

NS_IMETHODIMP
nsBookmarksService::IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                         nsIRDFResource*   aCommand,
                                         nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                                         PRBool* aResult)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsBookmarksService::getArgumentN(nsISupportsArray *arguments, nsIRDFResource *res,
                PRInt32 offset, nsIRDFNode **argValue)
{
    nsresult        rv;
    PRUint32        loop, numArguments;

    *argValue = nsnull;

    if (NS_FAILED(rv = arguments->Count(&numArguments)))    return rv;

    // format is argument, value, argument, value, ... [you get the idea]
    // multiple arguments can be the same, by the way, thus the "offset"
    for (loop = 0; loop < numArguments; loop += 2)
    {
        nsCOMPtr<nsIRDFResource> src = do_QueryElementAt(arguments, loop, &rv);
        if (!src) return rv;
        
        if (src == res)
        {
            if (offset > 0)
            {
                --offset;
                continue;
            }

            nsCOMPtr<nsIRDFNode> val = do_QueryElementAt(arguments, loop + 1,
                                                         &rv);
            if (!val) return rv;

            *argValue = val;
            NS_ADDREF(*argValue);
            return NS_OK;
        }
    }
    return NS_ERROR_INVALID_ARG;
}

nsresult
nsBookmarksService::importBookmarks(nsISupportsArray *aArguments)
{
    // look for #URL which is the file path to import
    nsresult rv;
    nsCOMPtr<nsIRDFNode> aNode;
    rv = getArgumentN(aArguments, kNC_URL, 0, getter_AddRefs(aNode));
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIRDFLiteral> pathLiteral = do_QueryInterface(aNode, &rv);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_NO_INTERFACE);
    const PRUnichar *pathUni = nsnull;
    pathLiteral->GetValueConst(&pathUni);
    NS_ENSURE_TRUE(pathUni, NS_ERROR_NULL_POINTER);

    nsCOMPtr<nsILocalFile> file;
    rv = NS_NewLocalFile(nsDependentString(pathUni), PR_TRUE, getter_AddRefs(file));
    NS_ENSURE_SUCCESS(rv, rv);
    PRBool isFile;
    rv = file->IsFile(&isFile);
    NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && isFile, NS_ERROR_UNEXPECTED);

    // read 'em in
    BookmarkParser parser;
    parser.Init(file, mInner, PR_TRUE);

    // Note: can't Begin|EndUpdateBatch() this as notifications are required
    nsCOMPtr<nsIRDFNode> parentNode;
    nsCOMPtr<nsIRDFResource> parentFolder;
    rv = getArgumentN(aArguments, kNC_Folder, 0, getter_AddRefs(parentNode));
    if (NS_FAILED(rv) || !parentNode)
      parentFolder = kNC_BookmarksRoot;
    else
      parentFolder = do_QueryInterface(parentNode);
    parser.Parse(parentFolder, kNC_Bookmark);

    return NS_OK;
}

nsresult
nsBookmarksService::exportBookmarks(nsISupportsArray *aArguments)
{
    // look for #URL which is the file path to export
    nsCOMPtr<nsIRDFNode> node;
    nsresult rv = getArgumentN(aArguments, kNC_URL, 0, getter_AddRefs(node));
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIRDFLiteral> literal = do_QueryInterface(node, &rv);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_NO_INTERFACE);
    const PRUnichar* pathUni = nsnull;
    literal->GetValueConst(&pathUni);
    NS_ENSURE_TRUE(pathUni, NS_ERROR_NULL_POINTER);

    // determine file type to export; default to HTML unless told otherwise
    const PRUnichar* format = nsnull;
    rv = getArgumentN(aArguments, kRDF_type, 0, getter_AddRefs(node));
    if (NS_SUCCEEDED(rv))
    {
        literal = do_QueryInterface(node, &rv);
        NS_ENSURE_SUCCESS(rv, NS_ERROR_NO_INTERFACE);
        literal->GetValueConst(&format);
        NS_ENSURE_TRUE(format, NS_ERROR_NULL_POINTER);
    }

    nsCOMPtr<nsILocalFile> file;
    rv = NS_NewLocalFile(nsDependentString(pathUni), PR_TRUE, getter_AddRefs(file));
    NS_ENSURE_SUCCESS(rv, rv);

    if (format && NS_LITERAL_STRING("RDF").Equals(format, nsCaseInsensitiveStringComparator()))
    {
        nsCOMPtr<nsIURI> uri;
        nsresult rv = NS_NewFileURI(getter_AddRefs(uri), file);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = SerializeBookmarks(uri);
    }
    else
    {
        // write 'em out
        rv = WriteBookmarks(file, mInner, kNC_BookmarksRoot);
    }

    return rv;
}

NS_IMETHODIMP
nsBookmarksService::DoCommand(nsISupportsArray *aSources, nsIRDFResource *aCommand,
                nsISupportsArray *aArguments)
{
    nsresult        rv = NS_OK;
    PRInt32         loop;
    PRUint32        numSources;
    if (NS_FAILED(rv = aSources->Count(&numSources)))   return rv;
    if (numSources < 1)
    {
        return NS_ERROR_ILLEGAL_VALUE;
    }

    // Note: some commands only run once (instead of looping over selection);
    //       if that's the case, be sure to "break" (if success) so that "mDirty"
    //       is set (and "bookmarks.html" will be flushed out shortly afterwards)

    for (loop=((PRInt32)numSources)-1; loop>=0; loop--)
    {
        nsCOMPtr<nsIRDFResource> src = do_QueryElementAt(aSources, loop, &rv);
        if (!src) return rv;

        if (aCommand == kNC_BookmarkCommand_NewBookmark)
        {
           return NS_ERROR_NOT_IMPLEMENTED;
        }
        else if (aCommand == kNC_BookmarkCommand_NewFolder)
        {
           return NS_ERROR_NOT_IMPLEMENTED;
        }
        else if (aCommand == kNC_BookmarkCommand_NewSeparator)
        {
           return NS_ERROR_NOT_IMPLEMENTED;
        }
        else if (aCommand == kNC_BookmarkCommand_DeleteBookmark ||
            aCommand == kNC_BookmarkCommand_DeleteBookmarkFolder ||
            aCommand == kNC_BookmarkCommand_DeleteBookmarkSeparator)
        {
           return NS_ERROR_NOT_IMPLEMENTED;
        }
        else if (aCommand == kNC_BookmarkCommand_SetPersonalToolbarFolder)
        {
            rv = SetBookmarksToolbarFolder(src);
            NS_ENSURE_SUCCESS(rv, rv);
            break;
        }
        else if (aCommand == kNC_BookmarkCommand_Import)
        {
            rv = importBookmarks(aArguments);
            NS_ENSURE_SUCCESS(rv, rv);
            break;
        }
        else if (aCommand == kNC_BookmarkCommand_Export)
        {
            rv = exportBookmarks(aArguments);
            NS_ENSURE_SUCCESS(rv, rv);
            break;
        }
    }

    mDirty = PR_TRUE;

    return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// nsIRDFRemoteDataSource

NS_IMETHODIMP
nsBookmarksService::GetLoaded(PRBool* _result)
{
    *_result = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsBookmarksService::Init(const char* aURI)
{
    return NS_OK;
}

NS_IMETHODIMP
nsBookmarksService::Refresh(PRBool aBlocking)
{
    // XXX re-sync with the bookmarks file, if necessary.
    return NS_OK;
}

NS_IMETHODIMP
nsBookmarksService::Flush()
{
    nsresult    rv = NS_OK;

    if (mBookmarksAvailable == PR_TRUE)
    {
        nsCOMPtr<nsIFile> bookmarksFile;
        rv = GetBookmarksFile(getter_AddRefs(bookmarksFile));

        // Oh well, couldn't get the bookmarks file. Guess there
        // aren't any bookmarks for us to write out.
        if (NS_FAILED(rv))  return NS_OK;

        if (mNeedBackupUpdate)
            SaveToBackup();

        rv = WriteBookmarks(bookmarksFile, mInner, kNC_BookmarksRoot);
        if (NS_SUCCEEDED(rv))
            mNeedBackupUpdate = PR_TRUE;
    }
    return rv;
}

NS_IMETHODIMP
nsBookmarksService::FlushTo(const char *aURI)
{
  // Do not ever implement this (security)
  return NS_ERROR_NOT_IMPLEMENTED;
}


////////////////////////////////////////////////////////////////////////
// Implementation methods

// save a copy of the last bookmarks file, if it exists, to bookmarks.bak
void
nsBookmarksService::SaveToBackup()
{
    nsresult rv;

    nsCOMPtr<nsIFile> bookmarksFile;
    rv = GetBookmarksFile(getter_AddRefs(bookmarksFile));

    if (NS_FAILED(rv) || !bookmarksFile)
        return;

    PRBool exists;
    bookmarksFile->Exists(&exists);

    if (!exists)
        return;

    nsCOMPtr<nsIFile> backupFile, parentFolder;
    bookmarksFile->GetParent(getter_AddRefs(parentFolder));
    if (parentFolder) {
        rv = parentFolder->Clone(getter_AddRefs(backupFile));
        if (NS_FAILED(rv))
            return;

        rv = backupFile->Append(NS_LITERAL_STRING("bookmarks.bak"));
        if (NS_FAILED(rv))
            return;

        rv = backupFile->Remove(PR_FALSE);
        /* ignore error */

        rv = bookmarksFile->CopyTo(parentFolder, NS_LITERAL_STRING("bookmarks.bak"));
        if (NS_SUCCEEDED(rv))
            mNeedBackupUpdate = PR_FALSE;
    }
}

// Attempt to restore a truncated or non-existent bookmarks.html file
// from the backup file generated by the last successful write.
void
nsBookmarksService::MaybeRestoreFromBackup(nsIFile* aBookmarkFile, nsIFile* aParentFolder)
{
    if (!aBookmarkFile) 
        return;

    PRBool exists;
    aBookmarkFile->Exists(&exists);
    if (exists)
    {
        PRInt64 fileSize;
        aBookmarkFile->GetFileSize(&fileSize);
        if (fileSize == 0)
        {
            aBookmarkFile->Remove(PR_FALSE);
            exists = PR_FALSE;
        }
    }
    if (!exists) 
    {
        nsCOMPtr<nsIFile> backupFile;
        aParentFolder->Clone(getter_AddRefs(backupFile));
        if (aParentFolder && backupFile) 
        {
            backupFile->Append(NS_LITERAL_STRING("bookmarks.bak"));
            backupFile->Exists(&exists);
            if (exists) 
            {
                nsAutoString bookmarksFileName;
                aBookmarkFile->GetLeafName(bookmarksFileName);

                backupFile->CopyTo(aParentFolder, bookmarksFileName);
            }
        }
    }
}

nsresult
nsBookmarksService::GetBookmarksFile(nsIFile* *aResult)
{
    nsresult rv;
    nsCOMPtr<nsIFile> bookmarksFile, parentFolder;

    // First we see if the user has set a pref for the location of the 
    // bookmarks file.
    nsCOMPtr<nsIPref> prefServ(do_GetService(kPrefCID, &rv));
    if (NS_SUCCEEDED(rv))
    {
        nsCOMPtr<nsISupportsString> prefVal;
        rv = prefServ->GetComplexValue("browser.bookmarks.file",
                                       NS_GET_IID(nsISupportsString),
                                       getter_AddRefs(prefVal));      
        if (NS_SUCCEEDED(rv))
        {
            nsAutoString bookmarkPath;
            prefVal->GetData(bookmarkPath);
            rv = NS_NewLocalFile(bookmarkPath, PR_TRUE,
                                 (nsILocalFile**)(nsIFile**) getter_AddRefs(bookmarksFile));

            if (NS_SUCCEEDED(rv))
            {
                *aResult = bookmarksFile;
                NS_ADDREF(*aResult);

                bookmarksFile->GetParent(getter_AddRefs(parentFolder));
                if (parentFolder)
                    MaybeRestoreFromBackup(*aResult, parentFolder);

                return NS_OK;
            }
        }
    }


    // Otherwise, we look for bookmarks.html in the current profile
    // directory using the magic directory service.
    rv = NS_GetSpecialDirectory(NS_APP_BOOKMARKS_50_FILE, aResult);
    NS_ENSURE_SUCCESS(rv, rv);

    NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(parentFolder));
    if (parentFolder)
        MaybeRestoreFromBackup(*aResult, parentFolder);

    return NS_OK;
}


#if defined(XP_MAC) || defined(XP_MACOSX)

nsresult
nsBookmarksService::ReadFavorites()
{
    mIEFavoritesAvailable = PR_TRUE;
    nsresult rv;
            
#ifdef DEBUG_varga
    PRTime      now;
#if defined(XP_MAC)
    Microseconds((UnsignedWide *)&now);
#else
    now = PR_Now();
#endif
    printf("Start reading in IE Favorites.html\n");
#endif

    // look for and import any IE Favorites
    nsAutoString    ieTitle;
    getLocaleString("ImportedIEFavorites", ieTitle);

    nsCOMPtr<nsIFile> ieFavoritesFile;
    rv = NS_GetSpecialDirectory(NS_MAC_PREFS_DIR, getter_AddRefs(ieFavoritesFile));
    NS_ENSURE_SUCCESS(rv, rv);

    ieFavoritesFile->Append(NS_LITERAL_STRING("Explorer"));
    ieFavoritesFile->Append(NS_LITERAL_STRING("Favorites.html"));

    if (NS_SUCCEEDED(rv = gRDFC->MakeSeq(mInner, kNC_IEFavoritesRoot, nsnull)))
    {
        BookmarkParser parser;
        parser.Init(ieFavoritesFile, mInner);
        BeginUpdateBatch();
        parser.Parse(kNC_IEFavoritesRoot, kNC_IEFavorite);
        EndUpdateBatch();
            
        nsCOMPtr<nsIRDFLiteral> ieTitleLiteral;
        rv = gRDF->GetLiteral(ieTitle.get(), getter_AddRefs(ieTitleLiteral));
        if (NS_SUCCEEDED(rv) && ieTitleLiteral)
        {
            rv = mInner->Assert(kNC_IEFavoritesRoot, kNC_Name, ieTitleLiteral, PR_TRUE);
        }
    }
#ifdef DEBUG_varga
    PRTime      now2;
#if defined(XP_MAC)
    Microseconds((UnsignedWide *)&now2);
#else
    now = PR_Now();
#endif
    PRUint64    loadTime64;
    LL_SUB(loadTime64, now2, now);
    PRUint32    loadTime32;
    LL_L2UI(loadTime32, loadTime64);
    printf("Finished reading in IE Favorites.html  (%u microseconds)\n", loadTime32);
#endif
    return rv;
}

#endif

NS_IMETHODIMP
nsBookmarksService::ReadBookmarks(PRBool *didLoadBookmarks)
{
    if (!gLoadedBookmarks)
    {
#if 0
        PRTime      now;
#if defined(XP_MAC)
        Microseconds((UnsignedWide *)&now);
#else
        now = PR_Now();
#endif
        printf("Time reading in bookmarks.html: ");
#endif
        LoadBookmarks();
#if 0
        PRTime      now2;
#if defined(XP_MAC)
        Microseconds((UnsignedWide *)&now2);
#else
        now2 = PR_Now();
#endif
        PRUint64    loadTime64;
        LL_SUB(loadTime64, now2, now);
        PRUint32    loadTime32;
        LL_L2UI(loadTime32, loadTime64);
        printf("%u microseconds\n", loadTime32);
#endif
        gLoadedBookmarks = PR_TRUE;
        *didLoadBookmarks = PR_TRUE;
    } else {
        *didLoadBookmarks = PR_FALSE;
    }
    return NS_OK;
}

nsresult
nsBookmarksService::InitDataSource()
{
    // the profile manager might call Readbookmarks() in certain circumstances
    // so we need to forget about any previous bookmarks
    NS_IF_RELEASE(mInner);

    // don't change this to an xml-ds, it will cause serious perf problems
    nsresult rv = CallCreateInstance(kRDFInMemoryDataSourceCID, &mInner);
    if (NS_FAILED(rv)) return rv;

    rv = mInner->AddObserver(this);
    if (NS_FAILED(rv)) return rv;

    rv = gRDFC->MakeSeq(mInner, kNC_BookmarksTopRoot, nsnull);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Unable to make NC:BookmarksTopRoot a sequence");
    if (NS_FAILED(rv)) return rv;

    rv = gRDFC->MakeSeq(mInner, kNC_BookmarksRoot, nsnull);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Unable to make NC:BookmarksRoot a sequence");
    if (NS_FAILED(rv)) return rv;

    // Make sure bookmark's root has the correct type
    rv = mInner->Assert(kNC_BookmarksTopRoot, kRDF_type, kNC_Folder, PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    rv = mInner->Assert(kNC_BookmarksRoot, kRDF_type, kNC_Folder, PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    // Insert NC:BookmarksRoot in NC:BookmarksTopRoot
    nsCOMPtr<nsIRDFContainer> container(do_CreateInstance(kRDFContainerCID, &rv));
    if (NS_FAILED(rv)) return rv;
    rv = container->Init(mInner, kNC_BookmarksTopRoot);
    if (NS_FAILED(rv)) return rv;
    rv = container->AppendElement(kNC_BookmarksRoot);

    // create livemark bookmarks
    {
        nsXPIDLString lmloadingName;
        rv = mBundle->GetStringFromName(NS_LITERAL_STRING("BookmarksLivemarkLoading").get(), getter_Copies(lmloadingName));
        if (NS_FAILED(rv)) {
            lmloadingName.Assign(NS_LITERAL_STRING("Live Bookmark loading..."));
        }

        nsXPIDLString lmfailedName;
        rv = mBundle->GetStringFromName(NS_LITERAL_STRING("BookmarksLivemarkFailed").get(), getter_Copies(lmfailedName));
        if (NS_FAILED(rv)) {
            lmfailedName.Assign(NS_LITERAL_STRING("Live Bookmark feed failed to load."));
        }

        CreateBookmark(lmloadingName.get(),
                       NS_LITERAL_STRING("about:livemark-loading").get(),
                       nsnull,
                       nsnull,
                       nsnull,
                       nsnull,
                       getter_AddRefs(mLivemarkLoadingBookmark));

        CreateBookmark(lmfailedName.get(),
                       NS_LITERAL_STRING("about:livemark-failed").get(),
                       nsnull,
                       nsnull,
                       nsnull,
                       nsnull,
                       getter_AddRefs(mLivemarkLoadFailedBookmark));

        rv = NS_OK;
    }

    return rv;
}

nsresult
nsBookmarksService::LoadBookmarks()
{
    nsresult rv;

    rv = InitDataSource();
    if (NS_FAILED(rv)) return NS_OK;

    nsCOMPtr<nsIFile> bookmarksFile;
    rv = GetBookmarksFile(getter_AddRefs(bookmarksFile));

    // Lack of Bookmarks file is non-fatal
    if (NS_FAILED(rv)) return NS_OK;

    PRBool foundIERoot = PR_FALSE;

    nsCOMPtr<nsIPrefService> prefSvc(do_GetService(NS_PREF_CONTRACTID));
    nsCOMPtr<nsIPrefBranch>  bookmarksPrefs;
    if (prefSvc)
        prefSvc->GetBranch("browser.bookmarks.", getter_AddRefs(bookmarksPrefs));

#ifdef DEBUG_varga
    PRTime now;
#if defined(XP_MAC)
    Microseconds((UnsignedWide *)&now);
#else
    now = PR_Now();
#endif
    printf("Start reading in bookmarks.html\n");
#endif
  
    // System Bookmarks Strategy
    //
    // * By default, we do a one-off import of system bookmarks when the browser 
    //   is run for the first time. This creates a hierarchy of bona-fide Mozilla
    //   bookmarks that is fully manipulable by the user but is not a live view.
    //
    // * As an option, the user can enable a "live view" of his or her system
    //   bookmarks which are not user manipulable but does update automatically.

    // Determine whether or not the user wishes to see the live view of system
    // bookmarks. This is controlled by the 
    //
    //   browser.bookmarks.import_system_favorites
    //
    // pref. See bug 22642 for details. 
    //
    PRBool useDynamicSystemBookmarks;
#ifdef XP_BEOS
    // always dynamic in BeOS
    useDynamicSystemBookmarks = PR_TRUE;
#else
    useDynamicSystemBookmarks = PR_FALSE;
    if (bookmarksPrefs)
        bookmarksPrefs->GetBoolPref("import_system_favorites", &useDynamicSystemBookmarks);
#endif

#ifdef XP_BEOS 
    nsCOMPtr<nsIRDFResource> systemFavoritesFolder;
    nsCOMPtr<nsIFile> systemBookmarksFolder;

    rv = NS_GetSpecialDirectory(NS_BEOS_SETTINGS_DIR, getter_AddRefs(systemBookmarksFolder));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = systemBookmarksFolder->AppendNative(NS_LITERAL_CSTRING("NetPositive"));
    NS_ENSURE_SUCCESS(rv, rv);
   
    rv = systemBookmarksFolder->AppendNative(NS_LITERAL_CSTRING("Bookmarks"));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIURI> bookmarksURI;
    rv = NS_NewFileURI(getter_AddRefs(bookmarksURI), systemBookmarksFolder);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCAutoString bookmarksURICString;
    rv = bookmarksURI->GetSpec(bookmarksURICString);
    NS_ENSURE_SUCCESS(rv, rv);
#endif
    {
        BookmarkParser parser;
        parser.Init(bookmarksFile, mInner);
        if (useDynamicSystemBookmarks)
        {
#if defined(XP_MAC) || defined(XP_MACOSX)
            parser.SetIEFavoritesRoot(nsCString(kURINC_IEFavoritesRoot));
#elif defined(XP_BEOS)
            parser.SetIEFavoritesRoot(bookmarksURICString);
#endif
            parser.ParserFoundIEFavoritesRoot(&foundIERoot);
        }

        BeginUpdateBatch();
        parser.Parse(kNC_BookmarksRoot, kNC_Bookmark);
        EndUpdateBatch();
        mBookmarksAvailable = PR_TRUE;
        
        // try to ensure that we end up with a bookmarks toolbar folder
        PRBool foundPTFolder = PR_FALSE;
        PRBool isBookmarked = PR_FALSE;
        parser.ParserFoundPersonalToolbarFolder(&foundPTFolder);
        if (!foundPTFolder)
        {
            // first, let's try to see if there is a NC:PersonalToolbarFolder
            // resource around coming from a corrupted Mozilla or Phoenix 
            // bookmarks file
            nsCOMPtr<nsIRDFResource> btf;
            gRDF->GetResource(NS_LITERAL_CSTRING("NC:PersonalToolbarFolder"), getter_AddRefs(btf));
            if (NS_SUCCEEDED(IsBookmarkedResource(btf, &isBookmarked)) && !isBookmarked)
                // there is not such a resource. Let's create the BTF
                CreateFolderInContainer(mPersonalToolbarName.get(), kNC_BookmarksRoot, 1, getter_AddRefs(btf));
            rv = mInner->Assert(btf, kNC_BookmarksToolbarFolder, kTrueLiteral, PR_TRUE);
            NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't set the BTF");
        }

        // Sets the default bookmarks root name.
        nsXPIDLString brName;
        rv = mBundle->GetStringFromName(NS_LITERAL_STRING("BookmarksRoot").get(), getter_Copies(brName));
        if NS_SUCCEEDED(rv) {
            // remove any previous NC_Name assertion
            nsCOMPtr<nsIRDFNode> oldName;
            rv = mInner->GetTarget(kNC_BookmarksRoot, kNC_Name, PR_TRUE, getter_AddRefs(oldName));
            if (NS_SUCCEEDED(rv) && rv != NS_RDF_NO_VALUE)
                (void) mInner->Unassert(kNC_BookmarksRoot, kNC_Name, oldName);

            nsCOMPtr<nsIRDFLiteral> brNameLiteral;
            rv = gRDF->GetLiteral(brName.get(), getter_AddRefs(brNameLiteral));
            if (NS_SUCCEEDED(rv))
                mInner->Assert(kNC_BookmarksRoot, kNC_Name, brNameLiteral, PR_TRUE);
        }

    } // <-- scope the stream to get the open/close automatically.

    // Now append the one-time-per-profile empty "Full" System Bookmarks Root. 
    // When the user opens this folder for the first time, system bookmarks are 
    // imported into this folder. A pref is used to keep track of whether or 
    // not to perform this operation. 
#if defined(XP_MAC) || defined(XP_MACOSX)
    PRBool addedStaticRoot = PR_FALSE;
    if (bookmarksPrefs) 
        bookmarksPrefs->GetBoolPref("added_static_root", 
                                    &addedStaticRoot);

    // Add the root that System bookmarks are imported into as real bookmarks. This is 
    // only done once. 
    if (!addedStaticRoot)
    {
        nsCOMPtr<nsIRDFContainer> rootContainer(do_CreateInstance(kRDFContainerCID, &rv));
        if (NS_FAILED(rv)) return rv;

        rv = rootContainer->Init(this, kNC_BookmarksRoot);
        if (NS_FAILED(rv)) return rv;

        rv = mInner->Assert(kNC_SystemBookmarksStaticRoot, kRDF_type, kNC_Folder, PR_TRUE);
        if (NS_FAILED(rv)) return rv;

        nsAutoString importedStaticTitle;
        getLocaleString("ImportedIEStaticFavorites", importedStaticTitle);

        nsCOMPtr<nsIRDFLiteral> staticTitleLiteral;
        rv = gRDF->GetLiteral(importedStaticTitle.get(), getter_AddRefs(staticTitleLiteral));
        if (NS_FAILED(rv)) return rv;

        rv = mInner->Assert(kNC_SystemBookmarksStaticRoot, kNC_Name, staticTitleLiteral, PR_TRUE);
        if (NS_FAILED(rv)) return rv;

        rv = rootContainer->AppendElement(kNC_SystemBookmarksStaticRoot);
        if (NS_FAILED(rv)) return rv;

        // If the user has not specifically asked for the Dynamic bookmarks root 
        // via the pref, remove it from an existing bookmarks file as it serves
        // only to add confusion. 
        if (!useDynamicSystemBookmarks)
        {
            nsCOMPtr<nsIRDFContainer> container(do_CreateInstance(kRDFContainerCID, &rv));
            if (NS_FAILED(rv)) return rv;

            rv = container->Init(this, kNC_BookmarksRoot);
            if (NS_FAILED(rv)) return rv;

            nsCOMPtr<nsIRDFResource> systemFolderResource;
#if defined(XP_MAC) || defined(XP_MACOSX)
            rv = gRDF->GetResource(NS_LITERAL_CSTRING(kURINC_IEFavoritesRoot),
                                   getter_AddRefs(systemFolderResource));
#endif
      
            rv = container->RemoveElement(systemFolderResource, PR_TRUE);
            if (NS_FAILED(rv)) return rv;
        }

        bookmarksPrefs->SetBoolPref("added_static_root", PR_TRUE);
    }
#endif

    // Add the dynamic system bookmarks root if the user has asked for it
    // by setting the pref. 
    if (useDynamicSystemBookmarks)
    {
#if defined(XP_MAC) || defined(XP_MACOSX)
        // if the IE Favorites root isn't somewhere in bookmarks.html, add it
        if (!foundIERoot)
        {
            nsCOMPtr<nsIRDFContainer> bookmarksRoot(do_CreateInstance(kRDFContainerCID, &rv));
            if (NS_FAILED(rv)) return rv;

            rv = bookmarksRoot->Init(this, kNC_BookmarksRoot);
            if (NS_FAILED(rv)) return rv;

            rv = bookmarksRoot->AppendElement(kNC_IEFavoritesRoot);
            if (NS_FAILED(rv)) return rv;

            // make sure IE Favorites root folder has the proper type     
            rv = mInner->Assert(kNC_IEFavoritesRoot, kRDF_type, kNC_IEFavoriteFolder, PR_TRUE);
            if (NS_FAILED(rv)) return rv;
        }
#elif defined(XP_BEOS)
        nsCOMPtr<nsIRDFResource> systemFolderResource;
        rv = gRDF->GetResource(bookmarksURICString,
                               getter_AddRefs(systemFolderResource));
        if (NS_SUCCEEDED(rv))
        {
            nsAutoString systemBookmarksFolderTitle;
            getLocaleString("ImportedNetPositiveBookmarks", systemBookmarksFolderTitle);

            nsCOMPtr<nsIRDFLiteral>   systemFolderTitleLiteral;
            rv = gRDF->GetLiteral(systemBookmarksFolderTitle.get(), 
                                  getter_AddRefs(systemFolderTitleLiteral));
            if (NS_SUCCEEDED(rv) && systemFolderTitleLiteral)
                rv = mInner->Assert(systemFolderResource, kNC_Name, 
                                    systemFolderTitleLiteral, PR_TRUE);
    
            // if the IE Favorites root isn't somewhere in bookmarks.html, add it
            if (!foundIERoot)
            {
                nsCOMPtr<nsIRDFContainer> container(do_CreateInstance(kRDFContainerCID, &rv));
                if (NS_FAILED(rv)) return rv;

                rv = container->Init(this, kNC_BookmarksRoot);
                if (NS_FAILED(rv)) return rv;

                rv = container->AppendElement(systemFolderResource);
                if (NS_FAILED(rv)) return rv;
            }
        }
#endif
    }

#ifdef DEBUG_varga
    PRTime      now2;
#if defined(XP_MAC)
    Microseconds((UnsignedWide *)&now2);
#else
    now2 = PR_Now();
#endif
    PRUint64    loadTime64;
    LL_SUB(loadTime64, now2, now);
    PRUint32    loadTime32;
    LL_L2UI(loadTime32, loadTime64);
    printf("Finished reading in bookmarks.html  (%u microseconds)\n", loadTime32);
#endif

    return NS_OK;
}

static char kFileIntro[] = 
    "<!DOCTYPE NETSCAPE-Bookmark-file-1>" NS_LINEBREAK
    "<!-- This is an automatically generated file." NS_LINEBREAK
    "     It will be read and overwritten." NS_LINEBREAK
    "     DO NOT EDIT! -->" NS_LINEBREAK
    // Note: we write bookmarks in UTF-8
    "<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=UTF-8\">" NS_LINEBREAK
    "<TITLE>Bookmarks</TITLE>" NS_LINEBREAK;
static const char kRootIntro[] = "<H1";
static const char kCloseRootH1[] = ">Bookmarks</H1>" NS_LINEBREAK NS_LINEBREAK;

nsresult
nsBookmarksService::WriteBookmarks(nsIFile* aBookmarksFile,
                                   nsIRDFDataSource* aDataSource,
                                   nsIRDFResource *aRoot)
{
    if (!aBookmarksFile || !aDataSource || !aRoot)
        return NS_ERROR_NULL_POINTER;

    // get a safe output stream, so we don't clobber the bookmarks file unless
    // all the writes succeeded.
    nsCOMPtr<nsIOutputStream> out;
    nsresult rv = NS_NewSafeLocalFileOutputStream(getter_AddRefs(out),
                                                  aBookmarksFile,
                                                  -1,
                                                  /*octal*/ 0600);
    if (NS_FAILED(rv)) return rv;

    // We need a buffered output stream for performance.
    // See bug 202477.
    nsCOMPtr<nsIOutputStream> strm;
    rv = NS_NewBufferedOutputStream(getter_AddRefs(strm), out, 4096);
    if (NS_FAILED(rv)) return rv;

    PRUint32 dummy;
    strm->Write(kFileIntro, sizeof(kFileIntro)-1, &dummy);

    // Write the bookmarks root
    // output <H1
    strm->Write(kRootIntro, sizeof(kRootIntro)-1, &dummy);
    // output LAST_MODIFIED
    rv = WriteBookmarkProperties(aDataSource, strm, aRoot, kWEB_LastModifiedDate, kLastModifiedEquals, PR_FALSE);
    // output Bookmarks and close H1
    strm->Write(kCloseRootH1, sizeof(kCloseRootH1)-1, &dummy);

    nsCOMArray<nsIRDFResource> parentArray;
    rv |= WriteBookmarksContainer(aDataSource, strm, aRoot, 0, parentArray);

    // All went ok. Maybe except for problems in Write(), but the stream detects
    // that for us
    nsCOMPtr<nsISafeOutputStream> safeStream = do_QueryInterface(strm);
    NS_ASSERTION(safeStream, "expected a safe output stream!");
    if (NS_SUCCEEDED(rv) && safeStream)
        rv = safeStream->Finish();
  
    if (NS_FAILED(rv)) {
        NS_WARNING("failed to save bookmarks file! possible dataloss");
        return rv;
    }

    mDirty = PR_FALSE;
    return rv;
}

static const char kBookmarkIntro[] = "<DL><p>" NS_LINEBREAK;
static const char kIndent[] = "    ";
static const char kContainerIntro[] = "<DT><H3";
static const char kSpaceStr[] = " ";
static const char kTrueEnd[] = "true\"";
static const char kQuoteStr[] = "\"";
static const char kCloseAngle[] = ">";
static const char kCloseH3[] = "</H3>" NS_LINEBREAK;
static const char kHROpen[] = "<HR";
static const char kAngleNL[] = ">" NS_LINEBREAK;
static const char kDTOpen[] = "<DT><A";
static const char kAClose[] = "</A>" NS_LINEBREAK;
static const char kBookmarkClose[] = "</DL><p>" NS_LINEBREAK;
static const char kNL[] = NS_LINEBREAK;

nsresult
nsBookmarksService::WriteBookmarksContainer(nsIRDFDataSource *ds,
                                            nsIOutputStream* strm,
                                            nsIRDFResource *parent, PRInt32 level,
                                            nsCOMArray<nsIRDFResource>& parentArray)
{
    // rv is used for various functions
    nsresult rv;

    nsCOMPtr<nsIRDFContainer> container;
    rv = nsComponentManager::CreateInstance(kRDFContainerCID, nsnull,
                                            NS_GET_IID(nsIRDFContainer),
                                            getter_AddRefs(container));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCAutoString   indentation;

    // STRING USE WARNING: converting in a loop.  Probably not a good idea
    for (PRInt32 loop=0; loop<level; loop++)
        indentation.Append(kIndent, sizeof(kIndent)-1);

    PRUint32 dummy;
    rv = strm->Write(indentation.get(), indentation.Length(), &dummy);
    rv |= strm->Write(kBookmarkIntro, sizeof(kBookmarkIntro)-1, &dummy);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_UNEXPECTED);

    rv = container->Init(ds, parent);
    if (NS_SUCCEEDED(rv) && (parentArray.IndexOfObject(parent) < 0))
    {
        // Note: once we've added something into the parentArray, don't "return" out
        //       of this function without removing it from the parentArray!
        parentArray.InsertObjectAt(parent, 0);

        nsCOMPtr<nsISimpleEnumerator>   children;
        if (NS_SUCCEEDED(rv = container->GetElements(getter_AddRefs(children))))
        {
            PRBool  more = PR_TRUE;
            while (more)
            {
                if (NS_FAILED(rv = children->HasMoreElements(&more)) || !more) break;

                nsCOMPtr<nsISupports>   iSupports;                  
                if (NS_FAILED(rv = children->GetNext(getter_AddRefs(iSupports))))   break;

                nsCOMPtr<nsIRDFResource> child = do_QueryInterface(iSupports);
                if (!child) break;

                PRBool isSomething = PR_FALSE;
                PRBool isLivemark = PR_FALSE;
                if (child.get() != kNC_IEFavoritesRoot)
                {
                    rv = gRDFC->IsContainer(ds, child, &isSomething);
                    if (NS_FAILED(rv)) break;
                }

                if (NS_SUCCEEDED(mInner->HasAssertion(child, kRDF_type,
                                                      kNC_Livemark, PR_TRUE, &isLivemark))
                    && isLivemark)
                {
                    // it's not a folder (the current something), for serialization purposes
                    isSomething = PR_FALSE;
                }

                rv = strm->Write(indentation.get(), indentation.Length(), &dummy);
                rv |= strm->Write(kIndent, sizeof(kIndent)-1, &dummy);
                if (NS_FAILED(rv)) break;

                if (isSomething)
                {
                    // child is a folder
                    rv = strm->Write(kContainerIntro, sizeof(kContainerIntro)-1, &dummy);
                    // output ADD_DATE
                    rv |= WriteBookmarkProperties(ds, strm, child, kNC_BookmarkAddDate, kAddDateEquals, PR_FALSE);

                    // output LAST_MODIFIED
                    rv |= WriteBookmarkProperties(ds, strm, child, kWEB_LastModifiedDate, kLastModifiedEquals, PR_FALSE);

                    // output PERSONAL_TOOLBAR_FOLDER
                    rv |= WriteBookmarkProperties(ds, strm, child, kNC_BookmarksToolbarFolder, kPersonalToolbarFolderEquals, PR_FALSE);
                    
                    // output ID and NAME
                    rv |= WriteBookmarkIdAndName(ds, strm, child);

                    rv |= strm->Write(kCloseH3, sizeof(kCloseH3)-1, &dummy);

                    // output description (if one exists)
                    rv |= WriteBookmarkProperties(ds, strm, child, kNC_Description, kOpenDD, PR_TRUE);

                    rv |= WriteBookmarksContainer(ds, strm, child, level+1, parentArray);
                }
                else if (NS_SUCCEEDED(mInner->HasAssertion(child, kRDF_type,
                         kNC_BookmarkSeparator, PR_TRUE, &isSomething)) 
                         && isSomething)
                {
                    // child is a separator
                    rv = strm->Write(kHROpen, sizeof(kHROpen)-1, &dummy);

                    // output NAME
                    rv |= WriteBookmarkProperties(ds, strm, child, kNC_Name, kNameEquals, PR_FALSE);

                    rv |= strm->Write(kAngleNL, sizeof(kAngleNL)-1, &dummy);
                    if (NS_FAILED(rv)) break;
                }
                else  // child is a bookmark
                {
                    rv = strm->Write(kDTOpen, sizeof(kDTOpen)-1, &dummy);

                    // output URL
                    rv |= WriteBookmarkProperties(ds, strm, child, kNC_URL, kHREFEquals, PR_FALSE);

                    // output ADD_DATE
                    rv |= WriteBookmarkProperties(ds, strm, child, kNC_BookmarkAddDate, kAddDateEquals, PR_FALSE);

                    // output LAST_VISIT
                    rv |= WriteBookmarkProperties(ds, strm, child, kWEB_LastVisitDate, kLastVisitEquals, PR_FALSE);

                    // output LAST_MODIFIED
                    rv |= WriteBookmarkProperties(ds, strm, child, kWEB_LastModifiedDate, kLastModifiedEquals, PR_FALSE);

                    // output SHORTCUTURL
                    rv |= WriteBookmarkProperties(ds, strm, child, kNC_ShortcutURL, kShortcutURLEquals, PR_FALSE);

                    // output kNC_Icon
                    rv |= WriteBookmarkProperties(ds, strm, child, kNC_Icon, kIconEquals, PR_FALSE);

                    // output kNC_WebPanel
                    rv |= WriteBookmarkProperties(ds, strm, child, kNC_WebPanel,
                                                  kWebPanelEquals, PR_FALSE);

                    // output kNC_PostData
                    rv |= WriteBookmarkProperties(ds, strm, child, kNC_PostData,
                                                  kPostDataEquals, PR_FALSE);
                    
                    // output SCHEDULE
                    rv |= WriteBookmarkProperties(ds, strm, child, kWEB_Schedule, kScheduleEquals, PR_FALSE);

                    // output LAST_PING
                    rv |= WriteBookmarkProperties(ds, strm, child, kWEB_LastPingDate, kLastPingEquals, PR_FALSE);

                    // output PING_ETAG
                    rv |= WriteBookmarkProperties(ds, strm, child, kWEB_LastPingETag, kPingETagEquals, PR_FALSE);

                    // output PING_LAST_MODIFIED
                    rv |= WriteBookmarkProperties(ds, strm, child, kWEB_LastPingModDate, kPingLastModEquals, PR_FALSE);

                    // output LAST_CHARSET
                    rv |= WriteBookmarkProperties(ds, strm, child, kWEB_LastCharset, kLastCharsetEquals, PR_FALSE);

                    // output PING_CONTENT_LEN
                    rv |= WriteBookmarkProperties(ds, strm, child, kWEB_LastPingContentLen, kPingContentLenEquals, PR_FALSE);

                    // output PING_STATUS
                    rv |= WriteBookmarkProperties(ds, strm, child, kWEB_Status, kPingStatusEquals, PR_FALSE);

                    // output livemark
                    if (isLivemark) {
                        // output FEEDURL
                        rv |= WriteBookmarkProperties(ds, strm, child, kNC_FeedURL, kFeedURLEquals, PR_FALSE);
                    }

                    // output ID and NAME
                    rv |= WriteBookmarkIdAndName(ds, strm, child);

                    rv |= strm->Write(kAClose, sizeof(kAClose)-1, &dummy);

                    // output description (if one exists)
                    rv |= WriteBookmarkProperties(ds, strm, child, kNC_Description, kOpenDD, PR_TRUE);
                }

                if (NS_FAILED(rv))  break;
            }
        }

        // cleanup: remove current parent element from parentArray
        parentArray.RemoveObjectAt(0);
    }

    rv |= strm->Write(indentation.get(), indentation.Length(), &dummy);
    rv |= strm->Write(kBookmarkClose, sizeof(kBookmarkClose)-1, &dummy);

    NS_ENSURE_SUCCESS(rv, NS_ERROR_UNEXPECTED);

    return NS_OK;
}

nsresult
nsBookmarksService::WriteBookmarkIdAndName(nsIRDFDataSource *aDs,
                                         nsIOutputStream* aStrm, nsIRDFResource* aChild)
{
    nsresult rv;
    PRUint32 dummy;

    // output ID
    // <A ... ID="rdf:#$Rd48+1">Name</A>
    //       ^^^^^^^^^^^^^^^^^^
    const char  *id = nsnull;
    rv = aChild->GetValueConst(&id);
    if (NS_SUCCEEDED(rv) && (id))
    {
        rv  = aStrm->Write(kSpaceStr, sizeof(kSpaceStr)-1, &dummy);
        rv |= aStrm->Write(kIDEquals, sizeof(kIDEquals)-1, &dummy);
        rv |= aStrm->Write(id, strlen(id), &dummy);
        rv |= aStrm->Write(kQuoteStr, sizeof(kQuoteStr)-1, &dummy);
    }

    // <A ... ID="rdf:#$Rd48+1">Name</A>
    //                         ^
    rv |= aStrm->Write(kCloseAngle, sizeof(kCloseAngle)-1, &dummy);

    // output NAME
    // <A ... ID="rdf:#$Rd48+1">Name</A>
    //                          ^^^^
    nsCOMPtr<nsIRDFNode> nameNode;
    rv |= aDs->GetTarget(aChild, kNC_Name, PR_TRUE, getter_AddRefs(nameNode));
    if (NS_FAILED(rv) || !nameNode)
        return rv;

    nsCOMPtr<nsIRDFLiteral> nameLiteral = do_QueryInterface(nameNode, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    const PRUnichar *title = nsnull;
    rv = nameLiteral->GetValueConst(&title);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoString  nameString(title);
    nsCAutoString name = NS_ConvertUCS2toUTF8(nameString);
    if (name.IsEmpty())
        return NS_OK;

    // see bug #65098
    char *escapedAttrib = nsEscapeHTML(name.get());
    if (escapedAttrib)
    {
        rv = aStrm->Write(escapedAttrib, strlen(escapedAttrib), &dummy);
        nsCRT::free(escapedAttrib);
    }
    return rv;
}

nsresult
nsBookmarksService::WriteBookmarkProperties(nsIRDFDataSource *aDs,
    nsIOutputStream* aStrm, nsIRDFResource *aChild, nsIRDFResource *aProperty,
    const char *aHtmlAttrib, PRBool aIsFirst)
{
    nsresult  rv;
    PRUint32  dummy;

    nsCOMPtr<nsIRDFNode>    node;
    if (NS_SUCCEEDED(rv = aDs->GetTarget(aChild, aProperty, PR_TRUE, getter_AddRefs(node)))
        && (rv != NS_RDF_NO_VALUE))
    {
        nsAutoString    literalString;
        if (NS_SUCCEEDED(rv = GetTextForNode(node, literalString)))
        {
            if (aProperty == kNC_URL) {
                // Now do properly replace %22's; this is particularly important for javascript: URLs
                PRInt32 offset;
                while ((offset = literalString.FindChar('\"')) >= 0) {
                    literalString.Cut(offset, 1);
                    literalString.Insert(NS_LITERAL_STRING("%22"), offset);
                }
            }

            char        *attribute = ToNewUTF8String(literalString);
            if (nsnull != attribute)
            {
                if (aIsFirst == PR_FALSE)
                {
                    rv |= aStrm->Write(kSpaceStr, sizeof(kSpaceStr)-1, &dummy);
                }

                if (aProperty == kNC_Description)
                {
                    if (!literalString.IsEmpty())
                    {
                        char *escapedAttrib = nsEscapeHTML(attribute);
                        if (escapedAttrib)
                        {
                            rv |= aStrm->Write(aHtmlAttrib, strlen(aHtmlAttrib), &dummy);
                            rv |= aStrm->Write(escapedAttrib, strlen(escapedAttrib), &dummy);
                            rv |= aStrm->Write(kNL, sizeof(kNL)-1, &dummy);

                            nsCRT::free(escapedAttrib);
                            escapedAttrib = nsnull;
                        }
                    }
                }
                else
                {
                    rv |= aStrm->Write(aHtmlAttrib, strlen(aHtmlAttrib), &dummy);
                    rv |= aStrm->Write(attribute, strlen(attribute), &dummy);
                    rv |= aStrm->Write(kQuoteStr, sizeof(kQuoteStr)-1, &dummy);
                }
                nsCRT::free(attribute);
                attribute = nsnull;
            }
        }
    }
    if (NS_FAILED(rv))
        return NS_ERROR_UNEXPECTED;
    
    return NS_OK;
}

nsresult
nsBookmarksService::SerializeBookmarks(nsIURI* aURI)
{
    NS_ASSERTION(aURI, "null ptr");

    nsresult rv;
    nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(aURI, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIFile> file;
    rv = fileURL->GetFile(getter_AddRefs(file));
    if (NS_FAILED(rv)) return rv;

    // if file doesn't exist, create it
    (void)file->Create(nsIFile::NORMAL_FILE_TYPE, 0666);

    nsCOMPtr<nsIOutputStream> out;
    rv = NS_NewLocalFileOutputStream(getter_AddRefs(out), file);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIOutputStream> bufferedOut;
    rv = NS_NewBufferedOutputStream(getter_AddRefs(bufferedOut), out, 4096);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIRDFXMLSerializer> serializer =
        do_CreateInstance("@mozilla.org/rdf/xml-serializer;1", &rv);
    if (NS_FAILED(rv)) return rv;

    rv = serializer->Init(this);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIRDFXMLSource> source = do_QueryInterface(serializer);
    if (! source)
        return NS_ERROR_FAILURE;

    return source->Serialize(bufferedOut);
}

/*
    Note: this routine is similar, yet distinctly different from, nsRDFContentUtils::GetTextForNode
*/

nsresult
nsBookmarksService::GetTextForNode(nsIRDFNode* aNode, nsString& aResult)
{
    nsresult        rv;
    nsIRDFResource  *resource;
    nsIRDFLiteral   *literal;
    nsIRDFDate      *dateLiteral;
    nsIRDFInt       *intLiteral;

    if (! aNode)
    {
        aResult.Truncate();
        rv = NS_OK;
    }
    else if (NS_SUCCEEDED(rv = aNode->QueryInterface(NS_GET_IID(nsIRDFResource), (void**) &resource)))
    {
        const char  *p = nsnull;
        if (NS_SUCCEEDED(rv = resource->GetValueConst( &p )) && (p))
        {
            aResult.AssignWithConversion(p);
        }
        NS_RELEASE(resource);
    }
    else if (NS_SUCCEEDED(rv = aNode->QueryInterface(NS_GET_IID(nsIRDFDate), (void**) &dateLiteral)))
    {
    PRInt64     theDate, million;
        if (NS_SUCCEEDED(rv = dateLiteral->GetValue( &theDate )))
        {
            LL_I2L(million, PR_USEC_PER_SEC);
            LL_DIV(theDate, theDate, million);          // convert from microseconds (PRTime) to seconds
            PRInt32     now32;
            LL_L2I(now32, theDate);
            aResult.Truncate();
            aResult.AppendInt(now32, 10);
        }
        NS_RELEASE(dateLiteral);
    }
    else if (NS_SUCCEEDED(rv = aNode->QueryInterface(NS_GET_IID(nsIRDFInt), (void**) &intLiteral)))
    {
        PRInt32     theInt;
        aResult.Truncate();
        if (NS_SUCCEEDED(rv = intLiteral->GetValue( &theInt )))
        {
            aResult.AppendInt(theInt, 10);
        }
        NS_RELEASE(intLiteral);
    }
    else if (NS_SUCCEEDED(rv = aNode->QueryInterface(NS_GET_IID(nsIRDFLiteral), (void**) &literal)))
    {
        const PRUnichar     *p = nsnull;
        if (NS_SUCCEEDED(rv = literal->GetValueConst( &p )) && (p))
        {
            aResult = p;
        }
        NS_RELEASE(literal);
    }
    else
    {
        NS_ERROR("not a resource or a literal");
        rv = NS_ERROR_UNEXPECTED;
    }

    return rv;
}

PRBool
nsBookmarksService::CanAccept(nsIRDFResource* aSource,
                  nsIRDFResource* aProperty,
                  nsIRDFNode* aTarget)
{
    nsresult    rv;
    PRBool      isBookmarkedFlag = PR_FALSE, canAcceptFlag = PR_FALSE, isOrdinal;

    if (NS_SUCCEEDED(rv = IsBookmarkedResource(aSource, &isBookmarkedFlag)) &&
        (isBookmarkedFlag == PR_TRUE) &&
        (NS_SUCCEEDED(rv = gRDFC->IsOrdinalProperty(aProperty, &isOrdinal))))
    {
        if (isOrdinal == PR_TRUE)
        {
            canAcceptFlag = PR_TRUE;
        }
        else if ((aProperty == kNC_Description) ||
             (aProperty == kNC_Name) ||
             (aProperty == kNC_ShortcutURL) ||
             (aProperty == kNC_URL) ||
             (aProperty == kNC_FeedURL) ||
             (aProperty == kNC_WebPanel) ||
             (aProperty == kNC_PostData) ||
             (aProperty == kNC_Livemark) ||
             (aProperty == kNC_LivemarkLock) ||
             (aProperty == kNC_LivemarkExpiration) ||
             (aProperty == kWEB_LastModifiedDate) ||
             (aProperty == kWEB_LastVisitDate) ||
             (aProperty == kNC_BookmarkAddDate) ||
             (aProperty == kRDF_nextVal) ||
             (aProperty == kRDF_type) ||
             (aProperty == kWEB_Schedule))
        {
            canAcceptFlag = PR_TRUE;
        }
    }
    return canAcceptFlag;
}


//----------------------------------------------------------------------
//
// nsIRDFObserver interface
//

NS_IMETHODIMP
nsBookmarksService::OnAssert(nsIRDFDataSource* aDataSource,
                 nsIRDFResource* aSource,
                 nsIRDFResource* aProperty,
                 nsIRDFNode* aTarget)
{
    if (mUpdateBatchNest != 0)  return NS_OK;

    PRInt32 count = mObservers.Count();
    for (PRInt32 i = 0; i < count; ++i)
    {
        (void) mObservers[i]->OnAssert(this, aSource, aProperty, aTarget);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsBookmarksService::OnUnassert(nsIRDFDataSource* aDataSource,
                   nsIRDFResource* aSource,
                   nsIRDFResource* aProperty,
                   nsIRDFNode* aTarget)
{
    if (mUpdateBatchNest != 0)  return NS_OK;

    PRInt32 count = mObservers.Count();
    for (PRInt32 i = 0; i < count; ++i)
    {
        (void) mObservers[i]->OnUnassert(this, aSource, aProperty, aTarget);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsBookmarksService::OnChange(nsIRDFDataSource* aDataSource,
                 nsIRDFResource* aSource,
                 nsIRDFResource* aProperty,
                 nsIRDFNode* aOldTarget,
                 nsIRDFNode* aNewTarget)
{
    if (mUpdateBatchNest != 0)  return NS_OK;

    PRInt32 count = mObservers.Count();
    for (PRInt32 i = 0; i < count; ++i)
    {
        (void) mObservers[i]->OnChange(this, aSource, aProperty, aOldTarget, aNewTarget);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsBookmarksService::OnMove(nsIRDFDataSource* aDataSource,
               nsIRDFResource* aOldSource,
               nsIRDFResource* aNewSource,
               nsIRDFResource* aProperty,
               nsIRDFNode* aTarget)
{
    if (mUpdateBatchNest != 0)  return NS_OK;

    PRInt32 count = mObservers.Count();
    for (PRInt32 i = 0; i < count; ++i)
    {
        (void) mObservers[i]->OnMove(this, aOldSource, aNewSource, aProperty, aTarget);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsBookmarksService::OnBeginUpdateBatch(nsIRDFDataSource* aDataSource)
{
    if (mUpdateBatchNest++ == 0)
    {
        PRInt32 count = mObservers.Count();
        for (PRInt32 i = 0; i < count; ++i) {
            (void) mObservers[i]->OnBeginUpdateBatch(this);
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsBookmarksService::OnEndUpdateBatch(nsIRDFDataSource* aDataSource)
{
    NS_ASSERTION(mUpdateBatchNest > 0, "badly nested update batch");

    if (--mUpdateBatchNest == 0)
    {
        PRInt32 count = mObservers.Count();
        for (PRInt32 i = 0; i < count; ++i) {
            (void) mObservers[i]->OnEndUpdateBatch(this);
        }
    }

    return NS_OK;
}
