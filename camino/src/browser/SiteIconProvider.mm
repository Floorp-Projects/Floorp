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
 * The Original Code is Chimera code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Simon Fraser <sfraser@netscape.com>
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

#import "NSString+Utils.h"

#import "SiteIconProvider.h"

#import "SiteIconCache.h"

#include "prtime.h"
#include "nsString.h"
#include "nsISupports.h"
#include "nsNetUtil.h"
#include "nsIURL.h"
#include "nsICacheSession.h"
#include "nsICacheService.h"
#include "nsICacheEntryDescriptor.h"


NSString* const SiteIconLoadNotificationName = @"siteicon_load_notification";
NSString* const SiteIconLoadImageKey         = @"siteicon_load_image";
NSString* const SiteIconLoadURIKey           = @"siteicon_load_uri";
NSString* const SiteIconLoadUsedNetworkKey   = @"siteicon_network_load";    // NSNumber with bool
NSString* const SiteIconLoadUserDataKey      = @"siteicon_load_user_data";

static const char* const kMissingFaviconMetadataKey     = "missing_favicon";
static const char* const kFaviconURILocationMetadataKey = "favicon_location";

//#define VERBOSE_SITE_ICON_LOADING

static inline PRUint32 PRTimeToSeconds(PRTime t_usec)
{
  PRTime usec_per_sec;
  PRUint32 t_sec;
  LL_I2L(usec_per_sec, PR_USEC_PER_SEC);
  LL_DIV(t_usec, t_usec, usec_per_sec);
  LL_L2I(t_sec, t_usec);
  return t_sec;
}

// this class is used for two things:
// 1. to store information about which sites do not have site icons,
//    so that we don't continually hit them.
// 2. to store a mapping between page URIs and site icon URIs,
//    so that we can reload page icons specified in <link> tags
//    without having to reload the original page.

class NeckoCacheHelper
{
public:

  NeckoCacheHelper();
  ~NeckoCacheHelper() {}

  nsresult Init(const char* inCacheSessionName);

  // get and save state about which icons are known missing. note that the URIs here
  // are URIs for the favicons themselves
  nsresult FaviconForURIIsMissing(const nsACString& inFaviconURI, PRBool* outKnownMissing);
  nsresult SetFaviconForURIIsMissing(const nsACString& inFaviconURI, PRUint32 inExpirationTimeSeconds);

  // get and save mapping between page URI and favicon URI, to store information
  // for pages with icons specified in <link> tags. these should only be used
  // for link-specified icons, and not those in the default location, to avoid
  // unnecessary bloat.
  nsresult SaveFaviconLocationForPageURI(const nsACString& inPageURI, const nsACString& inFaviconURI, PRUint32 inExpirationTimeSeconds);
  nsresult GetFaviconLocationForPageURI(const nsACString& inPageURI, nsACString& outFaviconURI);
  
  nsresult ClearCache();

  void     GetCanonicalPageURI(const nsACString& inPageURI, nsACString& outCanonicalURI);

protected:

  nsCOMPtr<nsICacheSession> mCacheSession;

};

static NS_DEFINE_CID(kCacheServiceCID, NS_CACHESERVICE_CID);

NeckoCacheHelper::NeckoCacheHelper()
{
}

nsresult
NeckoCacheHelper::Init(const char* inCacheSessionName)
{
  nsresult rv;
  
  nsCOMPtr<nsICacheService> cacheService = do_GetService(kCacheServiceCID, &rv);
  if (NS_FAILED(rv))
    return rv;
  
  rv = cacheService->CreateSession(inCacheSessionName,
        nsICache::STORE_ANYWHERE, nsICache::STREAM_BASED,
        getter_AddRefs(mCacheSession));
  if (NS_FAILED(rv))
    return rv;

  return NS_OK;
}


nsresult
NeckoCacheHelper::FaviconForURIIsMissing(const nsACString& inFaviconURI, PRBool* outKnownMissing)
{
  *outKnownMissing = PR_FALSE;
  
  NS_ASSERTION(mCacheSession, "No cache session");

  nsCOMPtr<nsICacheEntryDescriptor> entryDesc;
  nsresult rv = mCacheSession->OpenCacheEntry(inFaviconURI, nsICache::ACCESS_READ, nsICache::NON_BLOCKING, getter_AddRefs(entryDesc));
  if (NS_SUCCEEDED(rv) && entryDesc)
  {
    nsXPIDLCString metadataString;
    entryDesc->GetMetaDataElement(kMissingFaviconMetadataKey, getter_Copies(metadataString));
    *outKnownMissing = metadataString.EqualsLiteral("1");
  }
  return NS_OK;
}

nsresult
NeckoCacheHelper::SetFaviconForURIIsMissing(const nsACString& inFaviconURI, PRUint32 inExpirationTimeSeconds)
{
  NS_ASSERTION(mCacheSession, "No cache session");

  nsCOMPtr<nsICacheEntryDescriptor> entryDesc;
  nsresult rv = mCacheSession->OpenCacheEntry(inFaviconURI, nsICache::ACCESS_WRITE, nsICache::NON_BLOCKING, getter_AddRefs(entryDesc));
  if (NS_FAILED(rv) || !entryDesc) return rv;
  
  nsCacheAccessMode accessMode;
  rv = entryDesc->GetAccessGranted(&accessMode);
  if (NS_FAILED(rv))
    return rv;
  
  if (accessMode != nsICache::ACCESS_WRITE)
    return NS_ERROR_FAILURE;

  entryDesc->SetMetaDataElement(kMissingFaviconMetadataKey, "1");		// just set a bit of meta data.
  entryDesc->SetExpirationTime(PRTimeToSeconds(PR_Now()) + inExpirationTimeSeconds);
  
  entryDesc->MarkValid();
  entryDesc->Close();
  
  return NS_OK;
}

nsresult
NeckoCacheHelper::SaveFaviconLocationForPageURI(const nsACString& inPageURI, const nsACString& inFaviconURI, PRUint32 inExpirationTimeSeconds)
{
  NS_ASSERTION(mCacheSession, "No cache session");

  // canonicalize the URI
  nsCAutoString canonicalPageURI;
  GetCanonicalPageURI(inPageURI, canonicalPageURI);
  
  nsCOMPtr<nsICacheEntryDescriptor> entryDesc;
  nsresult rv = mCacheSession->OpenCacheEntry(canonicalPageURI, nsICache::ACCESS_WRITE, nsICache::NON_BLOCKING, getter_AddRefs(entryDesc));
  if (NS_FAILED(rv) || !entryDesc) return rv;
  
  nsCacheAccessMode accessMode;
  rv = entryDesc->GetAccessGranted(&accessMode);
  if (NS_FAILED(rv))
    return rv;
  
  if (accessMode != nsICache::ACCESS_WRITE)
    return NS_ERROR_FAILURE;
  
  entryDesc->SetMetaDataElement(kFaviconURILocationMetadataKey, PromiseFlatCString(inFaviconURI).get());
  entryDesc->SetExpirationTime(PRTimeToSeconds(PR_Now()) + inExpirationTimeSeconds);
  
  entryDesc->MarkValid();
  entryDesc->Close();
  
  return NS_OK;
}

nsresult
NeckoCacheHelper::GetFaviconLocationForPageURI(const nsACString& inPageURI, nsACString& outFaviconURI)
{
  NS_ASSERTION(mCacheSession, "No cache session");

  // canonicalize the URI
  nsCAutoString canonicalPageURI;
  GetCanonicalPageURI(inPageURI, canonicalPageURI);

  nsCOMPtr<nsICacheEntryDescriptor> entryDesc;
  nsresult rv = mCacheSession->OpenCacheEntry(canonicalPageURI, nsICache::ACCESS_READ, nsICache::NON_BLOCKING, getter_AddRefs(entryDesc));
  if (NS_FAILED(rv)) return rv;
  if (!entryDesc) return NS_ERROR_FAILURE;
    
  nsXPIDLCString faviconLocation;
  rv = entryDesc->GetMetaDataElement(kFaviconURILocationMetadataKey, getter_Copies(faviconLocation));
  if (NS_FAILED(rv)) return rv;

  outFaviconURI = faviconLocation;
  return NS_OK;
}

void
NeckoCacheHelper::GetCanonicalPageURI(const nsACString& inPageURI, nsACString& outCanonicalURI)
{
  nsCOMPtr<nsIURI> pageURI;
  nsresult rv = NS_NewURI(getter_AddRefs(pageURI), inPageURI, nsnull, nsnull);
  if (NS_SUCCEEDED(rv))
  {
    nsCOMPtr<nsIURL> pageURL = do_QueryInterface(pageURI);
    if (pageURL)
    {
      // remove any params or refs
      //pageURL->SetParam(NS_LITERAL_CSTRING(""));  // this is not implemented
      pageURL->SetQuery(NS_LITERAL_CSTRING(""));
      pageURL->SetRef(NS_LITERAL_CSTRING(""));
      
      pageURL->GetSpec(outCanonicalURI);
      return;
    }
  }
  
  outCanonicalURI = inPageURI;
}

nsresult
NeckoCacheHelper::ClearCache()
{
  NS_ASSERTION(mCacheSession, "No cache session");

  return mCacheSession->EvictEntries();  
}


#pragma mark -

@interface SiteIconProvider(Private)

- (void)addToMissedIconsCache:(NSString*)inURI withExpirationSeconds:(unsigned int)inExpSeconds;
- (BOOL)inMissedIconsCache:(NSString*)inURI;

@end


@implementation SiteIconProvider

- (id)init
{
  if ((self = [super init]))
  {
    mRequestDict    = [[NSMutableDictionary alloc] initWithCapacity:5];

    mIconsCacheHelper = new NeckoCacheHelper();
    nsresult rv = mIconsCacheHelper->Init("MissedIconsCache");
    if (NS_FAILED(rv)) {
      delete mIconsCacheHelper;
      mIconsCacheHelper = NULL;
    }
  }
  
  return self;
}

- (void)dealloc
{
  delete mIconsCacheHelper;

  [mRequestDict release];
  
  [super dealloc];
}

- (NSString*)favoriteIconURLFromPageURL:(NSString*)inPageURL
{
  if ([inPageURL length] == 0)
    return nil;

  NSString* faviconURL = nil;
  
  // do we have a link icon for this page?
  if (mIconsCacheHelper)
  {
    nsCAutoString pageURLString([inPageURL UTF8String]);
    nsCAutoString iconURLString;
    if (NS_SUCCEEDED(mIconsCacheHelper->GetFaviconLocationForPageURI(pageURLString, iconURLString)))
    {
      faviconURL = [NSString stringWith_nsACString:iconURLString];
#ifdef VERBOSE_SITE_ICON_LOADING
      NSLog(@"found linked icon url %@ for page %@", faviconURL, inPageURL);
#endif
    }
  }
  
  if (!faviconURL)
    faviconURL = [SiteIconProvider defaultFaviconLocationStringFromURI:inPageURL];
    
  return faviconURL;
}

// -removeImageForPageURL:
//
// Public method to remove the favicon image associated with a webpage's URL inURI from cache.
//
- (void)removeImageForPageURL:(NSString*)inURI
{
  [[SiteIconCache sharedSiteIconCache] removeImageForURL:[self favoriteIconURLFromPageURL:inURI]];
}

- (void)addToMissedIconsCache:(NSString*)inURI withExpirationSeconds:(unsigned int)inExpSeconds
{
  if (mIconsCacheHelper)
  {
    mIconsCacheHelper->SetFaviconForURIIsMissing(nsCString([inURI UTF8String]), inExpSeconds);
  }

}

- (BOOL)inMissedIconsCache:(NSString*)inURI
{
  PRBool inCache = PR_FALSE;

  if (mIconsCacheHelper)
  	mIconsCacheHelper->FaviconForURIIsMissing(nsCString([inURI UTF8String]), &inCache);

  return inCache;
}

#define SITE_ICON_EXPIRATION_SECONDS (60 * 60 * 24 * 7)    // 1 week

// this is called on the main thread
- (void)doneRemoteLoad:(NSString*)inURI forTarget:(id)target withUserData:(id)userData data:(NSData*)data status:(nsresult)status usingNetwork:(BOOL)usingNetwork
{
#ifdef VERBOSE_SITE_ICON_LOADING
  NSLog(@"Site icon load %@ done, status 0x%08X", inURI, status);
#endif

  nsAutoString uriString;
  [inURI assignTo_nsAString:uriString];

  BOOL loadOK = NS_SUCCEEDED(status) && (data != nil);

  // it's hard to tell if the favicon load succeeded or not. Even if the file
  // does not exist, servers will send back a 404 page with a 0 status.
  // So we just go ahead and try to make the image; it will return nil on
  // failure.
  NSImage*	faviconImage = nil;

  NSURL* inURIAsNSURL = [NSURL URLWithString:inURI];
  if ([inURIAsNSURL isFileURL]) {
    faviconImage = [[NSWorkspace sharedWorkspace] iconForFile:[inURIAsNSURL path]];
  }
  else {
    NS_DURING
      faviconImage = [[[NSImage alloc] initWithData:data] autorelease];
    NS_HANDLER
      NSLog(@"Exception \"%@\" making favicon image for %@", localException, inURI);
      faviconImage = nil;
    NS_ENDHANDLER
  }

  BOOL gotImageData = loadOK && (faviconImage != nil);
  if (NS_SUCCEEDED(status) && !gotImageData) // error status indicates that load was attempted from cache
    [self addToMissedIconsCache:inURI withExpirationSeconds:SITE_ICON_EXPIRATION_SECONDS];

  if (gotImageData) {
    [faviconImage setDataRetained:YES];
    [faviconImage setScalesWhenResized:YES];
    [faviconImage setSize:NSMakeSize(16, 16)];

    // add the image to the cache
    [[SiteIconCache sharedSiteIconCache] setSiteIcon:faviconImage
                                              forURL:inURI
                                      withExpiration:[NSDate dateWithTimeIntervalSinceNow:SITE_ICON_EXPIRATION_SECONDS]
                                          memoryOnly:NO];
  }

  // figure out what URL triggered this favicon request
  NSMutableArray* requestList = [mRequestDict objectForKey:inURI];

  // send out a notification for every object in the request list
  NSEnumerator* requestsEnum = [requestList objectEnumerator];
  NSDictionary* curRequest;
  while ((curRequest = [requestsEnum nextObject]))
  {
    NSString* requestURL = [curRequest objectForKey:@"request_uri"];
    if (!requestURL)
      requestURL = @"";
    id requestor = [curRequest objectForKey:@"icon_requestor"];
    
    // if we got an image, cache the link url, if any
    NSString* linkURL = [curRequest objectForKey:@"icon_uri"];
    if (gotImageData && ![linkURL isEqual:[NSNull null]] && mIconsCacheHelper)
    {
      nsCAutoString pageURI([requestURL UTF8String]);
      nsCAutoString iconURI([linkURL UTF8String]);
      
#ifdef VERBOSE_SITE_ICON_LOADING
      NSLog(@"saving linked icon url %@ for page %@", linkURL, requestURL);
#endif
      mIconsCacheHelper->SaveFaviconLocationForPageURI(pageURI, iconURI, SITE_ICON_EXPIRATION_SECONDS);
    }
    
    // we always send out the notification, so that clients know
    // about failed requests
    NSDictionary*	notificationData = [NSDictionary dictionaryWithObjectsAndKeys:
                                         inURI, SiteIconLoadURIKey,
                                    requestURL, SiteIconLoadUserDataKey,
        [NSNumber numberWithBool:usingNetwork], SiteIconLoadUsedNetworkKey,
                                  faviconImage, SiteIconLoadImageKey,	// may be nil (so put last!)
                                                nil];

    // NSLog(@"siteIconLoad notification, info %@", notificationData);
    NSNotification *note = [NSNotification notificationWithName: SiteIconLoadNotificationName object:requestor userInfo:notificationData];
    [[NSNotificationQueue defaultQueue] enqueueNotification:note postingStyle:NSPostWhenIdle];
  }

  // cleanup our key holder
  [mRequestDict removeObjectForKey:inURI];  
}

#pragma mark -

- (NSImage*)favoriteIconForPage:(NSString*)inPageURI
{
  // map uri to image location uri
  NSString* iconURL = [self favoriteIconURLFromPageURL:inPageURI];
  if ([iconURL length] == 0)
    return nil;

  return [[SiteIconCache sharedSiteIconCache] siteIconForURL:iconURL];
}

- (void)registerFaviconImage:(NSImage*)inImage forPageURI:(NSString*)inURI
{
  if (inImage == nil || [inURI length] == 0)
    return;

  [[SiteIconCache sharedSiteIconCache] setSiteIcon:inImage
                                            forURL:inURI
                                    withExpiration:[NSDate distantFuture]
                                        memoryOnly:YES];
}

- (BOOL)fetchFavoriteIconForPage:(NSString*)inPageURI
                withIconLocation:(NSString*)inIconURI
                    allowNetwork:(BOOL)inAllowNetwork
                 notifyingClient:(id)inClient
{
  NSString* iconTargetURL = nil;
  
  if (inIconURI)
  {
    // XXX clear any old cached uri for this page here?

    // we should deal with the case of a page with a link element asking
    // for the site icon, then a subsequent request with a nil inIconURI.
    // however, we normally try to load the "default" favicon.ico before
    // we see a link element, so the right thing happens.
    iconTargetURL = inIconURI;
  }
  else
  {
    // look in the cache, and if it's not found, use the default location
    iconTargetURL = [self favoriteIconURLFromPageURL:inPageURI];
  }

  if ([iconTargetURL length] == 0)
    return NO;
  
  // is this uri already in the missing icons cache?
  if ([self inMissedIconsCache:iconTargetURL])
  {
#ifdef VERBOSE_SITE_ICON_LOADING
    NSLog(@"Site icon %@ known missing", iconTargetURL);
#endif
    return NO;
  }
  
  id iconLocationString = inIconURI;
  if (!iconLocationString)
    iconLocationString = [NSNull null];
  
  // preserve requesting URI for later notification
  NSDictionary* loadInfo = [NSDictionary dictionaryWithObjectsAndKeys:
                                            inClient, @"icon_requestor",
                                           inPageURI, @"request_uri",
                                  iconLocationString, @"icon_uri",
                                                      nil];
  NSMutableArray* existingRequests = [mRequestDict objectForKey:iconTargetURL];
  if (existingRequests)
  {
#ifdef VERBOSE_SITE_ICON_LOADING
    NSLog(@"Site icon request %@ added to load list", iconTargetURL);
#endif
    [existingRequests addObject:loadInfo];
    return YES;
  }
  
  existingRequests = [NSMutableArray arrayWithObject:loadInfo];
  [mRequestDict setObject:existingRequests forKey:iconTargetURL];

  RemoteDataProvider* dataProvider = [RemoteDataProvider sharedRemoteDataProvider];
  BOOL loadDispatched = [dataProvider loadURI:iconTargetURL forTarget:inClient withListener:self withUserData:nil allowNetworking:inAllowNetwork];

#ifdef VERBOSE_SITE_ICON_LOADING
  NSLog(@"Site icon load %@ dispatched %d", iconTargetURL, loadDispatched);
#endif
  
  return loadDispatched;
}

#pragma mark -

+ (SiteIconProvider*)sharedFavoriteIconProvider
{
  static SiteIconProvider* sIconProvider = nil;
  if (!sIconProvider)
  {
    sIconProvider = [[SiteIconProvider alloc] init];
  }
  
  return sIconProvider;
}

+ (NSString*)defaultFaviconLocationStringFromURI:(NSString*)inURI
{
  if ([inURI hasPrefix:@"about:"])
    return inURI;

  // If the file exists, return its path, otherwise use generic icon location
  if ([inURI hasPrefix:@"file:"])
    return ([[NSFileManager defaultManager] fileExistsAtPath:[[NSURL URLWithString:inURI] path]] ? inURI : @"about:local_file");

  // we use nsIURI here, rather than NSURL, because the former does
  // a better job with suspect urls (e.g. those containing |), and 
  // allows us go keep the port
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), [inURI UTF8String]);
  if (NS_FAILED(rv))
    return @"";

  // check for http/https
  PRBool isHTTP = PR_FALSE, isHTTPS = PR_FALSE;
  uri->SchemeIs("http", &isHTTP);
  uri->SchemeIs("https", &isHTTPS);
  if (!isHTTP && !isHTTPS)
    return @"";

  nsCAutoString faviconURI;
  uri->GetPrePath(faviconURI);
  faviconURI.Append("/favicon.ico");

  return [NSString stringWith_nsACString:faviconURI];
}

@end
