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

#include "prtime.h"
#include "nsString.h"
#include "nsISupports.h"
#include "nsNetUtil.h"
#include "nsICacheSession.h"
#include "nsICacheService.h"
#include "nsICacheEntryDescriptor.h"


NSString* SiteIconLoadNotificationName = @"siteicon_load_notification";
NSString* SiteIconLoadImageKey         = @"siteicon_load_image";
NSString* SiteIconLoadURIKey           = @"siteicon_load_uri";
NSString* SiteIconLoadUserDataKey      = @"siteicon_load_user_data";


static inline PRUint32 PRTimeToSeconds(PRTime t_usec)
{
  PRTime usec_per_sec;
  PRUint32 t_sec;
  LL_I2L(usec_per_sec, PR_USEC_PER_SEC);
  LL_DIV(t_usec, t_usec, usec_per_sec);
  LL_L2I(t_sec, t_usec);
  return t_sec;
}
 
class NeckoCacheHelper
{
public:

  NeckoCacheHelper(const char* inMetaElement, const char* inMetaValue);
  ~NeckoCacheHelper() {}

  nsresult Init(const char* inCacheSessionName);
  nsresult ExistsInCache(const nsACString& inURI, PRBool* outExists);
  nsresult PutInCache(const nsACString& inURI, PRUint32 inExpirationTimeSeconds);

  nsresult ClearCache();

protected:

  const char*               mMetaElement;
  const char*               mMetaValue;
  nsCOMPtr<nsICacheSession> mCacheSession;

};

static NS_DEFINE_CID(kCacheServiceCID, NS_CACHESERVICE_CID);

NeckoCacheHelper::NeckoCacheHelper(const char* inMetaElement, const char* inMetaValue)
: mMetaElement(inMetaElement)
, mMetaValue(inMetaValue)
{
}

nsresult NeckoCacheHelper::Init(const char* inCacheSessionName)
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


nsresult NeckoCacheHelper::ExistsInCache(const nsACString& inURI, PRBool* outExists)
{
  NS_ASSERTION(mCacheSession, "No cache session");

  nsCOMPtr<nsICacheEntryDescriptor> entryDesc;
  nsresult rv = mCacheSession->OpenCacheEntry(PromiseFlatCString(inURI).get(), nsICache::ACCESS_READ, nsICache::NON_BLOCKING, getter_AddRefs(entryDesc));

  *outExists = NS_SUCCEEDED(rv) && (entryDesc != NULL);
  return NS_OK;
}

nsresult NeckoCacheHelper::PutInCache(const nsACString& inURI, PRUint32 inExpirationTimeSeconds)
{
  NS_ASSERTION(mCacheSession, "No cache session");

  nsCOMPtr<nsICacheEntryDescriptor> entryDesc;
  nsresult rv = mCacheSession->OpenCacheEntry(PromiseFlatCString(inURI).get(), nsICache::ACCESS_WRITE, nsICache::NON_BLOCKING, getter_AddRefs(entryDesc));
  if (NS_FAILED(rv) || !entryDesc) return rv;
  
  nsCacheAccessMode accessMode;
  rv = entryDesc->GetAccessGranted(&accessMode);
  if (NS_FAILED(rv))
    return rv;
  
  if (accessMode != nsICache::ACCESS_WRITE)
    return NS_ERROR_FAILURE;

  entryDesc->SetMetaDataElement(mMetaElement, mMetaValue);		// just set a bit of meta data.
  entryDesc->SetExpirationTime(PRTimeToSeconds(PR_Now()) + inExpirationTimeSeconds);
  
  entryDesc->MarkValid();
  entryDesc->Close();
  
  return NS_OK;
}

nsresult NeckoCacheHelper::ClearCache()
{
  NS_ASSERTION(mCacheSession, "No cache session");

  return mCacheSession->EvictEntries();  
}


#pragma mark -

static nsresult MakeFaviconURIFromURI(const nsAString& inURIString, nsAString& outFaviconURI)
{
  outFaviconURI.Truncate(0);
  
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), inURIString);
  if (NS_FAILED(rv))
    return rv;

  // check for http/https
  PRBool isHTTP = PR_FALSE, isHTTPS = PR_FALSE;
  uri->SchemeIs("http", &isHTTP);
  uri->SchemeIs("https", &isHTTPS);
  if (!isHTTP && !isHTTPS)
    return NS_OK;

  PRInt32 port;
  uri->GetPort(&port);

  nsCAutoString scheme;
  uri->GetScheme(scheme);
  
  nsCAutoString host;
  uri->GetHost(host);
  
  nsCAutoString faviconURI(scheme);
  faviconURI.Append("://");
  faviconURI.Append(host);
  if (port != -1) {
    faviconURI.Append(':');
    faviconURI.AppendInt(port);
  }
  faviconURI.Append("/favicon.ico");

  outFaviconURI.Assign(NS_ConvertUTF8toUCS2(faviconURI));
  return NS_OK;
}


@interface SiteIconProvider(Private)

- (void)addToMissedIconsCache:(const nsAString&)inURI withExpirationSeconds:(unsigned int)inExpSeconds;
- (BOOL)inMissedIconsCache:(const nsAString&)inURI;

@end


@implementation SiteIconProvider

- (id)init
{
  if ((self = [super init]))
  {
    mMissedIconsCacheHelper = new NeckoCacheHelper("Favicon", "Missed");
    nsresult rv = mMissedIconsCacheHelper->Init("MissedIconsCache");
    if (NS_FAILED(rv)) {
      delete mMissedIconsCacheHelper;
      mMissedIconsCacheHelper = NULL;
    }
  }
  
  return self;
}

- (void)dealloc
{
  delete mMissedIconsCacheHelper;
  [super dealloc];
}

- (void)addToMissedIconsCache:(const nsAString&)inURI withExpirationSeconds:(unsigned int)inExpSeconds
{
  if (mMissedIconsCacheHelper)
  {
    nsresult rv = mMissedIconsCacheHelper->PutInCache(NS_ConvertUCS2toUTF8(inURI), inExpSeconds);
  }

}

- (BOOL)inMissedIconsCache:(const nsAString&)inURI
{
  PRBool inCache = PR_FALSE;

  if (mMissedIconsCacheHelper)
  	mMissedIconsCacheHelper->ExistsInCache(NS_ConvertUCS2toUTF8(inURI), &inCache);

  return inCache;
}

- (BOOL)loadFavoriteIcon:(id)sender forURI:(NSString *)inURI withUserData:(id)userData allowNetwork:(BOOL)inAllowNetwork
{
  // look for a favicon
  nsAutoString uriString;
  [inURI assignTo_nsAString:uriString];

  nsAutoString faviconURIString;
  MakeFaviconURIFromURI(uriString, faviconURIString);
  if (faviconURIString.Length() == 0)
    return NO;

  NSString* faviconString = [NSString stringWith_nsAString:faviconURIString];
  
  // is this uri already in the missing icons cache?
  if ([self inMissedIconsCache:faviconURIString])
  {
    return NO;
  }
  
  RemoteDataProvider* dataProvider = [RemoteDataProvider sharedRemoteDataProvider];
  return [dataProvider loadURI:faviconString forTarget:sender withListener:self withUserData:userData allowNetworking:inAllowNetwork];
}

#define SITE_ICON_EXPIRATION_SECONDS (60 * 60 * 24 * 7)    // 1 week

// this is called on the main thread
- (void)doneRemoteLoad:(NSString*)inURI forTarget:(id)target withUserData:(id)userData data:(NSData*)data status:(nsresult)status
{
  nsAutoString uriString;
  [inURI assignTo_nsAString:uriString];

  BOOL loadOK = NS_SUCCEEDED(status) && (data != nil);

  // it's hard to tell if the favicon load succeeded or not. Even if the file
  // does not exist, servers will send back a 404 page with a 0 status.
  // So we just go ahead and try to make the image; it will return nil on
  // failure.
  NSImage*	faviconImage = nil;
  
  NS_DURING
    faviconImage = [[NSImage alloc] initWithData:data];
  NS_HANDLER
    NSLog(@"Exception \"%@ making\" favicon image for %@", localException, inURI);
    faviconImage = nil;
  NS_ENDHANDLER

  BOOL gotImageData = loadOK && (faviconImage != nil);
  if (NS_SUCCEEDED(status) && !gotImageData)		// error status indicates that load was attempted from cache
  {	
    [self addToMissedIconsCache:uriString withExpirationSeconds:SITE_ICON_EXPIRATION_SECONDS];
  }
  
  if (gotImageData)
  {
    [faviconImage setDataRetained:YES];
    [faviconImage setScalesWhenResized:YES];
    [faviconImage setSize:NSMakeSize(16, 16)];
  }
  
  // we always send out the notification, so that clients know
  // about failed requests
  NSDictionary*	notificationData = [NSDictionary dictionaryWithObjectsAndKeys:
                                       inURI, SiteIconLoadURIKey,
                                faviconImage, SiteIconLoadImageKey,	// may be nil
                                    userData, SiteIconLoadUserDataKey,
                                         nil];

  [[NSNotificationCenter defaultCenter] postNotificationName: SiteIconLoadNotificationName
        object:target userInfo:notificationData];  
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


+ (NSString*)faviconLocationStringFromURI:(NSString*)inURI
{
  nsAutoString uriString;
  [inURI assignTo_nsAString:uriString];

  nsAutoString faviconURIString;
  MakeFaviconURIFromURI(uriString, faviconURIString);
  return [NSString stringWith_nsAString:faviconURIString];
}

@end
