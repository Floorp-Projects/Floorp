/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Haas <haasd@cae.wisc.edu>
 *   Josh Aas <joshmoz@gmail.com>
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
#import <SystemConfiguration/SystemConfiguration.h>

#import "NSThread+Utils.h"
#import "Bookmark.h"
#import "BookmarkFolder.h"
#import "BookmarkManager.h"
#import "BookmarksClient.h"
#import "NSString+Utils.h"
#import "SiteIconProvider.h"

// Notification of URL load
NSString* const URLLoadNotification   = @"url_load";
NSString* const URLLoadSuccessKey     = @"url_bool";

#pragma mark -

@implementation Bookmark

-(id) init
{
  if ((self = [super init]))
  {
    mURL            = [[NSString alloc] init];
    mStatus         = kBookmarkOKStatus;
    mNumberOfVisits = 0;
    mLastVisit      = [[NSDate date] retain];
  }
  return self;
}

-(id) copyWithZone:(NSZone *)zone
{
  id bookmarkCopy = [super copyWithZone:zone];
  [bookmarkCopy setUrl:[self url]];
  [bookmarkCopy setStatus:[self status]];
  [bookmarkCopy setLastVisit:[self lastVisit]];
  [bookmarkCopy setNumberOfVisits:[self numberOfVisits]];
  return bookmarkCopy;
}

- (void) dealloc
{
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  mParent = NULL;	// not retained, so just set to null
  [mURL release];
  [mLastVisit release];
  [super dealloc];
}

- (NSString*)description
{
  return [NSString stringWithFormat:@"Bookmark %08p, url %@, title %@", self, [self url], [self title]];
}

// set/get properties

-(NSString *) url
{
  return mURL;
}

-(NSImage *)icon
{
  if (!mIcon)
  {
    mIcon = [[NSImage imageNamed:@"smallbookmark"] retain];
    [self refreshIcon];
  }
  return mIcon;
}

-(NSDate *) lastVisit
{
  return mLastVisit;
}

-(unsigned) status
{
  return mStatus;
}

-(unsigned) numberOfVisits
{
  return mNumberOfVisits;
}

-(BOOL)	isSeparator
{
  return (mStatus == kBookmarkSpacerStatus);
}

-(NSString*) faviconURL
{
  return mFaviconURL;
}

-(void)setFaviconURL:(NSString*)inURL
{
  [inURL retain];
  [mFaviconURL release];
  mFaviconURL = inURL;
}

-(void) setStatus:(unsigned)aStatus
{
  if (aStatus != mStatus) {
    // There used to be more than two possible status states.
    // Now we regard everything except kBookmarkSpacerStatus
    // as kBookmarkOKStatus.
    if (aStatus != kBookmarkSpacerStatus)
      aStatus = kBookmarkOKStatus;
    
    mStatus = aStatus;
    [self itemUpdatedNote:kBookmarkItemStatusChangedMask];

    if (aStatus == kBookmarkSpacerStatus)
    {
      [self setTitle:NSLocalizedString(@"<Menu Spacer>", @"")];
    }
  }
}

- (void) setUrl:(NSString *)aURL
{
  if (!aURL)
    return;
  
  if (![mURL isEqualToString:aURL])
  {
    [aURL retain];
    [mURL release];
    mURL = aURL;
    [self setStatus:kBookmarkOKStatus];
    
    // clear the icon, so we'll refresh it next time someone asks for it
    [mIcon release];
    mIcon = nil;
    
    [self itemUpdatedNote:kBookmarkItemURLChangedMask];
  }
}

- (void) setLastVisit:(NSDate *)aDate
{
  if (aDate) {
    [aDate retain];
    [mLastVisit release];
    mLastVisit = aDate;
  }
}

-(void) setNumberOfVisits:(unsigned)aNumber
{
  mNumberOfVisits = aNumber;
  [self itemUpdatedNote:kBookmarkItemLastVisitChangedMask];
}

-(void) setIsSeparator:(BOOL)aBool
{
  if (aBool)
    [self setStatus:kBookmarkSpacerStatus];
  else
    [self setStatus:kBookmarkOKStatus];
}

-(void) refreshIcon
{
  // don't invoke loads from the non-main thread (e.g. while loading bookmarks on a thread)
  if ([NSThread inMainThread])
  {
    NSImage* siteIcon = [[SiteIconProvider sharedFavoriteIconProvider] favoriteIconForPage:[self url]];
    if (siteIcon)
      [self setIcon:siteIcon];
    else if ([[BookmarkManager sharedBookmarkManager] showSiteIcons])
    {
      [[SiteIconProvider sharedFavoriteIconProvider] fetchFavoriteIconForPage:[self url]
                                                             withIconLocation:nil
                                                                 allowNetwork:NO
                                                              notifyingClient:self];
    }
  }
}

-(void) notePageLoadedWithSuccess:(BOOL)inSuccess
{
  [self setLastVisit:[NSDate date]];
  if (inSuccess)
    [self setNumberOfVisits:(mNumberOfVisits + 1)];
}

// rather than overriding this, it might be better to have a stub for
// -url in the base class
-(BOOL)matchesString:(NSString*)searchString inFieldWithTag:(int)tag
{
  switch (tag)
  {
    case eBookmarksSearchFieldAll:
      return (([[self url] rangeOfString:searchString options:NSCaseInsensitiveSearch].location != NSNotFound) ||
              [super matchesString:searchString inFieldWithTag:tag]);
    
    case eBookmarksSearchFieldURL:
      return ([[self url] rangeOfString:searchString options:NSCaseInsensitiveSearch].location != NSNotFound);
  }

  return [super matchesString:searchString inFieldWithTag:tag];
}

#pragma mark -

- (id)savedURL
{
  return mURL ? mURL : @"";
}

- (id)savedLastVisit
{
  return mLastVisit ? mLastVisit : [NSDate distantPast];
}

- (id)savedStatus
{
  return [NSNumber numberWithUnsignedInt:mStatus];
}

- (id)savedNumberOfVisits
{
  return [NSNumber numberWithUnsignedInt:mNumberOfVisits];
}

- (id)savedFaviconURL
{
  return mFaviconURL ? mFaviconURL : @"";
}

#pragma mark -

//
// for reading/writing from/to disk
//

-(BOOL) readNativeDictionary:(NSDictionary *)aDict
{
  //gather the redundant update notifications
  [self setAccumulateUpdateNotifications:YES];
  
  [self setTitle:[aDict objectForKey:BMTitleKey]];
  [self setItemDescription:[aDict objectForKey:BMDescKey]];
  [self setKeyword:[aDict objectForKey:BMKeywordKey]];
  [self setUrl:[aDict objectForKey:BMURLKey]];
  [self setUUID:[aDict objectForKey:BMUUIDKey]];
  [self setLastVisit:[aDict objectForKey:BMLastVisitKey]];
  [self setNumberOfVisits:[[aDict objectForKey:BMNumberVisitsKey] unsignedIntValue]];
  [self setStatus:[[aDict objectForKey:BMStatusKey] unsignedIntValue]];
  [self setFaviconURL:[aDict objectForKey:BMLinkedFaviconURLKey]];
  
  //fire an update notification
  [self setAccumulateUpdateNotifications:NO];
  return YES;
}

-(BOOL) readSafariDictionary:(NSDictionary *)aDict
{
  //gather the redundant update notifications
  [self setAccumulateUpdateNotifications:YES];
  
  NSDictionary *uriDict = [aDict objectForKey:SafariURIDictKey];
  [self setTitle:[uriDict objectForKey:SafariBookmarkTitleKey]];
  [self setUrl:[aDict objectForKey:SafariURLStringKey]];
  
  //fire an update notification
  [self setAccumulateUpdateNotifications:NO];
  return YES;
}

-(BOOL) readCaminoXML:(CFXMLTreeRef)aTreeRef settingToolbar:(BOOL)setupToolbar
{
  CFXMLNodeRef myNode;
  CFXMLElementInfo* elementInfoPtr;
  myNode = CFXMLTreeGetNode(aTreeRef);
  if (myNode) {
    // Process our info
    if (CFXMLNodeGetTypeCode(myNode)==kCFXMLNodeTypeElement){
      elementInfoPtr = (CFXMLElementInfo *)CFXMLNodeGetInfoPtr(myNode);
      if (elementInfoPtr) {
        NSDictionary* attribDict = (NSDictionary*)elementInfoPtr->attributes;
        //gather the redundant update notifications
        [self setAccumulateUpdateNotifications:YES];
        [self setTitle:[[attribDict objectForKey:CaminoNameKey] stringByRemovingAmpEscapes]];
        [self setKeyword:[[attribDict objectForKey:CaminoKeywordKey] stringByRemovingAmpEscapes]];
        [self setItemDescription:[[attribDict objectForKey:CaminoDescKey] stringByRemovingAmpEscapes]];
        [self setUrl:[[attribDict objectForKey:CaminoURLKey] stringByRemovingAmpEscapes]];
        //fire an update notification
        [self setAccumulateUpdateNotifications:NO];
      } else {
        NSLog(@"Bookmark:readCaminoXML - elementInfoPtr null, load failed");
        return NO;
      }
    } else {
      NSLog(@"Bookmark:readCaminoXML - node not kCFXMLNodeTypeElement, load failed");
      return NO;
    }
  } else {
    NSLog(@"Bookmark:readCaminoXML - urk! CFXMLTreeGetNode null, load failed");
    return NO;
  }
  return YES;
}

//
// -writeBookmarksMetaDatatoPath:
//
// Writes out the meta data for this bookmark to a file with the name of this item's UUID
// in the given path. Using the suffix "webbookmark" allows us to pick up on the Spotlight
// importer already on Tiger for Safari.
//
-(void)writeBookmarksMetadataToPath:(NSString*)inPath
{
  NSDictionary* dict = [NSDictionary dictionaryWithObjectsAndKeys: 
                                      [self savedTitle], @"Name",
                                        [self savedURL], @"URL",
                                                         nil];
  NSString* file = [self UUID];
  NSString* path = [NSString stringWithFormat:@"%@/%@.webbookmark", inPath, file];
  [dict writeToFile:path atomically:YES];
}

//
// -removeBookmarksMetadataFromPath:
//
// Delete the meta data for this bookmark from the cache, which consists of a file with
// this item's UUID.
//
-(void)removeBookmarksMetadataFromPath:(NSString*)inPath
{
  NSString* file = [self UUID];
  NSString* path = [NSString stringWithFormat:@"%@/%@.webbookmark", inPath, file];
  [[NSFileManager defaultManager] removeFileAtPath:path handler:nil];
}

// for plist in native format
-(NSDictionary *)writeNativeDictionary
{
  if ([self isSeparator])
    return [NSDictionary dictionaryWithObject:[self savedStatus] forKey:BMStatusKey];

  NSMutableDictionary* itemDict = [NSMutableDictionary dictionaryWithObjectsAndKeys:
                [self savedTitle], BMTitleKey,
                  [self savedURL], BMURLKey,
            [self savedLastVisit], BMLastVisitKey,
       [self savedNumberOfVisits], BMNumberVisitsKey,
               [self savedStatus], BMStatusKey,
                                   nil];

  if ([[self itemDescription] length])
    [itemDict setObject:[self itemDescription] forKey:BMDescKey]; 

  if ([[self keyword] length])
    [itemDict setObject:[self keyword] forKey:BMKeywordKey]; 

  if ([mUUID length])    // don't call -UUID to avoid generating one
    [itemDict setObject:mUUID forKey:BMUUIDKey]; 

  if ([[self faviconURL] length])
    [itemDict setObject:[self faviconURL] forKey:BMLinkedFaviconURLKey]; 

  return itemDict;
}

-(NSDictionary *)writeSafariDictionary
{
  NSDictionary* dict = nil;
  if (![self isSeparator]) {
    NSDictionary *uriDict = [NSDictionary dictionaryWithObjectsAndKeys:
      [self savedTitle], SafariBookmarkTitleKey,
        [self savedURL], @"",
                         nil];
    if (!uriDict) {
      return nil;   // when would this happen?
    }

    dict = [NSDictionary dictionaryWithObjectsAndKeys:
                uriDict, SafariURIDictKey,
        [self savedURL], SafariURLStringKey,
             SafariLeaf, SafariTypeKey,
            [self UUID], SafariUUIDKey,
                         nil];
  }
  return dict;
}

-(NSString *)writeHTML:(unsigned int)aPad
{
  NSMutableString *padString = [NSMutableString string];
  for (unsigned i = 0; i < aPad; i++)
    [padString insertString:@"    " atIndex:0];

  if ([self isSeparator])
    return [NSString stringWithFormat:@"%@<HR>\n", padString];

  NSString* exportHTMLString = [NSString stringWithFormat:@"%@<DT><A HREF=\"%@\"", padString, [self url]];
  if ([self lastVisit])  // if there is a lastVisit, export it
    exportHTMLString = [exportHTMLString stringByAppendingFormat:@" LAST_VISIT=\"%d\"", [[self lastVisit] timeIntervalSince1970]];

  if ([[self keyword] length] > 0)  // if there is a keyword, export it (bug 307743)
    exportHTMLString = [exportHTMLString stringByAppendingFormat:@" SHORTCUTURL=\"%@\"", [self keyword]];

  // close up the attributes, export the title, close the A tag
  exportHTMLString = [exportHTMLString stringByAppendingFormat:@">%@</A>\n", [mTitle stringByAddingAmpEscapes]];
  if ([mDescription length] > 0)  // if there is a description, export that too
    exportHTMLString = [exportHTMLString stringByAppendingFormat:@"%@<DD>%@\n", padString, [mDescription stringByAddingAmpEscapes]];

  return exportHTMLString;
}

//
// Scripting methods
//

- (NSScriptObjectSpecifier *)objectSpecifier
{
  id parent = [self parent];
  NSScriptObjectSpecifier *containerRef = [parent objectSpecifier];
  unsigned index = [[parent childArray] indexOfObjectIdenticalTo:self];
  if (index != NSNotFound) {
    // Man, this took forever to figure out.
    // DHBookmarks always contained in BookmarkFolder - so make
    // sure we set that as the container class description.
    NSScriptClassDescription *aRef = (NSScriptClassDescription*)[NSClassDescription classDescriptionForClass:[BookmarkFolder class]];
    return [[[NSIndexSpecifier allocWithZone:[self zone]] initWithContainerClassDescription:aRef containerSpecifier:containerRef key:@"childArray" index:index] autorelease];
  } else
    return nil;
}

@end 
