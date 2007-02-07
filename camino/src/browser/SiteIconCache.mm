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
 *   Simon Fraser <smfr@smfr.org>
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
#import "NSFileManager+Utils.h"
#import "PreferenceManager.h"

#import "SiteIconCache.h"

static NSString* const kCacheIndexSaveNotificationName   = @"save_site_icon_cache";

static NSString* const kCacheEntryUUIDStringKey     = @"uuid";
static NSString* const kCacheEntryExpirationDateKey = @"exp_date";

@interface SiteIconCache(Private)

- (NSString*)cacheDirectory;
- (NSString*)imageDataFileWithUUID:(NSString*)inUUID;

- (void)setUUID:(NSString*)inUUID expiration:(NSDate*)inExpirationDate forURL:(NSString*)inURL;
- (NSString*)UUIDForURL:(NSString*)inURL expired:(BOOL*)outExpired;

// Note also the public method without the uuid arg (same functionality, but slower)
- (void)removeImageForURL:(NSString*)inURL uuid:(NSString*)inUUID;

- (void)loadCache;
- (BOOL)readCacheFile;
- (BOOL)writeCacheFile;

- (void)postSaveNotification;
- (void)saveCacheNotification:(NSNotification*)inNotification;

@end

#pragma mark -

@implementation SiteIconCache

+ (SiteIconCache*)sharedSiteIconCache
{
  static SiteIconCache* sSiteIconCache = nil;
  if (!sSiteIconCache)
    sSiteIconCache = [[SiteIconCache alloc] init];

  return sSiteIconCache;
}

- (id)init
{
  if ((self = [super init]))
  {
    [self loadCache];
    mURLToImageMap = [[NSMutableDictionary alloc] initWithCapacity:100];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(saveCacheNotification:)
                                                 name:kCacheIndexSaveNotificationName
                                               object:self];
  }
  return self;
}

- (void)dealloc
{
  [mURLToEntryMap release];
  [mURLToImageMap release];
  [super dealloc];
}

- (NSImage*)siteIconForURL:(NSString*)inURL
{
  BOOL imageExpired;
  NSString* imageUUID = [self UUIDForURL:inURL expired:&imageExpired];
  if (imageUUID && imageExpired)
  {
    // if we've seen this image before, but it has expired, clear our memory cached copy
    [self removeImageForURL:inURL uuid:imageUUID];
    return nil;
  }
  
  NSImage* cachedImage = [mURLToImageMap objectForKey:inURL];
  if (cachedImage)
    return cachedImage;

  if (imageUUID)
  {
    cachedImage = [NSKeyedUnarchiver unarchiveObjectWithFile:[self imageDataFileWithUUID:imageUUID]];
    if (cachedImage)
    {
      // if we got the image, keep it in the memory cache
      [mURLToImageMap setObject:cachedImage forKey:inURL];
    }
    else
    {
      // file is bad or missing. just nuke it from the cache
      [self removeImageForURL:inURL uuid:imageUUID];
    }
  }

  return cachedImage;
}

- (void)setSiteIcon:(NSImage*)inImage forURL:(NSString*)inURL withExpiration:(NSDate*)inExpirationDate memoryOnly:(BOOL)inMemoryOnly
{
  // add to memory cache
  [mURLToImageMap setObject:inImage forKey:inURL];
  if (inMemoryOnly)
    return;

  // avoid redundant image serialization?
  NSString* curUID = [self UUIDForURL:inURL expired:NULL];
  if (!curUID)
    curUID = [NSString stringWithUUID];
  // reset the expiration date
  [self setUUID:curUID expiration:inExpirationDate forURL:inURL];
  
  // and save to disk
  BOOL imageSaved = [NSKeyedArchiver archiveRootObject:inImage toFile:[self imageDataFileWithUUID:curUID]];
  if (!imageSaved)
    NSLog(@"Failed to archive image for %@ to file", inURL);
}
  

#pragma mark -

- (NSString*)cacheDirectory
{
  return [[[PreferenceManager sharedInstance] cacheParentDirPath] stringByAppendingPathComponent:@"IconCache"];
}

- (NSString*)imageDataFileWithUUID:(NSString*)inUUID
{
  return [[self cacheDirectory] stringByAppendingPathComponent:inUUID];
}

- (void)setUUID:(NSString*)inUUID expiration:(NSDate*)inExpirationDate forURL:(NSString*)inURL
{
  NSDictionary* entryDict = [NSDictionary dictionaryWithObjectsAndKeys:
                                                        inUUID, kCacheEntryUUIDStringKey,
                                              inExpirationDate, kCacheEntryExpirationDateKey,
                                                                nil];
  [mURLToEntryMap setObject:entryDict forKey:inURL];
  [self postSaveNotification];
}

- (NSString*)UUIDForURL:(NSString*)inURL expired:(BOOL*)outExpired
{
  if (outExpired)
    *outExpired = YES;

  NSDictionary* entryDict = [mURLToEntryMap objectForKey:inURL];
  if (!entryDict) return nil;
  
  if (outExpired)
  {
    NSDate* expirationDate = [entryDict objectForKey:kCacheEntryExpirationDateKey];
    if (!expirationDate) return nil;
    *outExpired = ([expirationDate compare:[NSDate date]] == NSOrderedAscending);
  }
  
  return [entryDict objectForKey:kCacheEntryUUIDStringKey];
}

// Simplified public call of |removeImageForURL| without the uuid arg
- (void)removeImageForURL:(NSString*)inURL
{
  [self removeImageForURL:inURL uuid:nil];
}

- (void)removeImageForURL:(NSString*)inURL uuid:(NSString*)inUUID
{
  // remove from memory cache
  [mURLToImageMap removeObjectForKey:inURL];
  
  // remove from disk
  NSString* imageUUID = inUUID;
  if (!imageUUID)
    imageUUID = [self UUIDForURL:inURL expired:NULL];

  if (!imageUUID) return;

  // remove the file
  NSString* filePath = [self imageDataFileWithUUID:imageUUID];
  [[NSFileManager defaultManager] removeFileAtPath:filePath handler:nil];

  // remove from the map
  [mURLToEntryMap removeObjectForKey:inURL];
  [self postSaveNotification];
}

- (BOOL)readCacheFile
{
  NSString* cacheIndex = [[self cacheDirectory] stringByAppendingPathComponent:@"IconCacheIndex.plist"];
  
  NSDictionary* cacheDict = [NSDictionary dictionaryWithContentsOfFile:cacheIndex];
  if (!cacheDict)
    return NO;
  
  mURLToEntryMap = [cacheDict mutableCopy];    // owning ref
  return YES;
}

- (BOOL)writeCacheFile
{
  NSString* cacheIndex = [[self cacheDirectory] stringByAppendingPathComponent:@"IconCacheIndex.plist"];
  return [mURLToEntryMap writeToFile:cacheIndex atomically:YES];
}

- (void)loadCache
{
  if ([self readCacheFile])
    return;

  // create new cache
  NSString* cacheDir = [self cacheDirectory];
  [[NSFileManager defaultManager] createDirectoriesInPath:cacheDir attributes:nil];

  mURLToEntryMap = [[NSMutableDictionary alloc] initWithCapacity:100];
  [self postSaveNotification];
}

- (void)postSaveNotification
{
  NSNotification* saveCacheNote = [NSNotification notificationWithName:kCacheIndexSaveNotificationName
                                                                object:self
                                                              userInfo:nil];

  [[NSNotificationQueue defaultQueue] enqueueNotification:saveCacheNote
                                             postingStyle:NSPostWhenIdle
                                             coalesceMask:(NSNotificationCoalescingOnName | NSNotificationCoalescingOnSender)
                                                 forModes:[NSArray arrayWithObject:NSDefaultRunLoopMode]];   
}

- (void)saveCacheNotification:(NSNotification*)inNotification
{
  BOOL cacheSaved = [self writeCacheFile];
  if (!cacheSaved)
    NSLog(@"Failed to save site icon cache index");
}

@end
