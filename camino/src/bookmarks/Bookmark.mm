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

#import "Bookmark.h"
#import "BookmarkFolder.h"
#import "BookmarkManager.h"
#import "BookmarksClient.h"
#import "NSString+Utils.h"
#import "SiteIconProvider.h"

@interface Bookmark (Private)
-(void) siteIconCheck:(NSNotification *)aNote;
-(void) urlLoadCheck:(NSNotification *)aNote;
@end

// Notification of URL load
NSString* const URLLoadNotification   = @"url_load";
NSString* const URLLoadSuccessKey     = @"url_bool";

@implementation Bookmark

-(id) init
{
  if ((self = [super init])) {
    mURL = [[NSString alloc] init];
    mStatus = [[NSNumber alloc] initWithUnsignedInt:0]; // retain count +1
    mNumberOfVisits = [mStatus retain]; // retain count +2
    mLastVisit = [[NSDate date] retain];
    mIcon = [[NSImage imageNamed:@"smallbookmark"] retain];
    // register for notifications
    NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
    [nc addObserver:self selector:@selector(siteIconCheck:) name:SiteIconLoadNotificationName object:nil];
    [nc addObserver:self selector:@selector(urlLoadCheck:) name:URLLoadNotification object:nil];
  }
  return self;
}

-(id) copyWithZone:(NSZone *)zone
{
  id doppleganger = [super copyWithZone:zone];
  [doppleganger setUrl:[self url]];
  [doppleganger setStatus:[self status]];
  [doppleganger setLastVisit:[self lastVisit]];
  [doppleganger setNumberOfVisits:[self numberOfVisits]];
  return doppleganger;
}

- (void) dealloc
{
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  mParent = NULL;	// not retained, so just set to null
  [mURL release];
  [mLastVisit release];
  [mStatus release];
  [mNumberOfVisits release];
  [super dealloc];
}

// set/get properties

-(NSString *) url
{
  return mURL;
}

-(NSDate *) lastVisit
{
  return mLastVisit;
}

-(unsigned) status
{
  return [mStatus unsignedIntValue];
}

-(unsigned) numberOfVisits
{
  return [mNumberOfVisits unsignedIntValue];
}

-(BOOL)	isSeparator
{
  if ([self status] == kBookmarkSpacerStatus)
    return YES;
  return NO;
}

-(void) setStatus:(unsigned)aStatus
{
  if (aStatus != [mStatus unsignedIntValue]) {
    // There used to be more than two possible status states.
    // Now we regard everything except kBookmarkSpacerStatus
    // as kBookmarkOKStatus.
    if (aStatus != kBookmarkSpacerStatus)
      aStatus = kBookmarkOKStatus;
    
    [mStatus release];
    mStatus = [[NSNumber alloc] initWithUnsignedInt:aStatus];
    [self itemUpdatedNote];
    if (aStatus == kBookmarkSpacerStatus)
      [self setTitle:NSLocalizedString(@"<Menu Spacer>",@"<Menu Spacer>")];
  }
}

- (void) setUrl:(NSString *)aURL
{
  if (aURL) {
    [aURL retain];
    [mURL release];
    mURL = aURL;
    [self setStatus:kBookmarkOKStatus];
    [self refreshIcon];
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
  [mNumberOfVisits release];
  mNumberOfVisits = [[NSNumber alloc] initWithUnsignedInt:aNumber];
  [self itemUpdatedNote];
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
  if ([BookmarkManager sharedBookmarkManager]) {
    [[SiteIconProvider sharedFavoriteIconProvider] loadFavoriteIcon:self forURI:[self url] allowNetwork:NO];
  }
}

-(void) siteIconCheck:(NSNotification *)note
{
  NSDictionary *userInfo = [note userInfo];
  if (userInfo) {
    id loadTarget = [note object];
    NSString *iconString = [userInfo objectForKey:SiteIconLoadUserDataKey];
    if (iconString) {
      NSURL *iconURL= [NSURL URLWithString:[NSString escapedURLString:iconString]];
      NSURL *myURL = [NSURL URLWithString:[NSString escapedURLString:[self url]]];
      if ((loadTarget == self) || [iconURL isEqual:myURL] || [[[iconURL host] stripWWW] isEqualToString:[[myURL host] stripWWW]]) {
        NSImage *iconImage = [userInfo objectForKey:SiteIconLoadImageKey];
        if (iconImage)
          [self setIcon:iconImage];
      }
    }
  }
}

-(void) urlLoadCheck:(NSNotification *)note
{
  NSString *loadedURL = [note object];
  if ([loadedURL hasSuffix:@"/"])
    loadedURL = [loadedURL substringToIndex:([loadedURL length]-1)];
  NSString *myURL = [self url];
  if ([myURL hasSuffix:@"/"])
    myURL = [myURL substringToIndex:([myURL length]-1)];
  if ([loadedURL isEqualToString:myURL]) {
    [self setLastVisit:[NSDate date]];
    if ([[[note userInfo] objectForKey:URLLoadSuccessKey] unsignedIntValue] == kBookmarkOKStatus) 
      [self setNumberOfVisits:([self numberOfVisits]+1)];
  }
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
  [self setLastVisit:[aDict objectForKey:BMLastVisitKey]];
  [self setNumberOfVisits:[[aDict objectForKey:BMNumberVisitsKey] unsignedIntValue]];
  [self setStatus:[[aDict objectForKey:BMStatusKey] unsignedIntValue]];
  
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

-(BOOL) readCaminoXML:(CFXMLTreeRef)aTreeRef
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
                                      mTitle, @"Name",
                                      mURL, @"URL",
                                      nil];
  NSString* file = [self UUID];
  NSString* path = [NSString stringWithFormat:@"%@/%@.webbookmark", inPath, file];
  [dict writeToFile:path atomically:YES];
}

// for plist in native format
-(NSDictionary *)writeNativeDictionary
{
  NSDictionary* dict;
  if (![self isSeparator]) {
    dict = [NSDictionary dictionaryWithObjectsAndKeys:
    mTitle,BMTitleKey,
    mKeyword,BMKeywordKey,
    mURL,BMURLKey,
    mDescription,BMDescKey ,
    mLastVisit,BMLastVisitKey,
    mNumberOfVisits,BMNumberVisitsKey,
    mStatus,BMStatusKey,
    nil];
  } else {
    dict = [NSDictionary dictionaryWithObjectsAndKeys:
      mStatus,BMStatusKey,
      nil];
  }
  return dict;
}

-(NSDictionary *)writeSafariDictionary
{
  NSDictionary* dict = nil;
  if (![self isSeparator]) {
    NSDictionary *uriDict = [NSDictionary dictionaryWithObjectsAndKeys:
      mTitle,SafariBookmarkTitleKey,
      mURL,[NSString string],
      nil];
    if (!uriDict) {
      return nil;
    }

    dict = [NSDictionary dictionaryWithObjectsAndKeys:
      uriDict,SafariURIDictKey,
      mURL,SafariURLStringKey,
      SafariLeaf,SafariTypeKey,
      [self UUID],SafariUUIDKey,
      nil];
  }
  return dict;
}

-(NSString *)writeHTML:(unsigned int)aPad
{
  NSString* formatString;
  NSMutableString *padString = [NSMutableString string];
  for (unsigned i = 0;i<aPad;i++)
    [padString insertString:@"    " atIndex:0];  
  if ([mDescription length] > 0)
    formatString = @"%@<DT><A HREF=\"%@\">%@</A>\n%@<DD>%@\n";
  else
    formatString = @"%@<DT><A HREF=\"%@\">%@</A>\n";
  return [NSString stringWithFormat:formatString,
    padString,
    [self url],
    [mTitle stringByAddingAmpEscapes],
    padString,
    [mDescription stringByAddingAmpEscapes]];
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
