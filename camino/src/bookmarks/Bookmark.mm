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
*    David Haas <haasd@cae.wisc.edu>
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
-(void) invalidSchemeForUpdating;
-(BOOL) hostIsReachable:(NSURL *)aURL;
-(void) doHTTPUpdateRequest:(NSURL *)aURL;
-(void) doFileUpdateRequest:(NSURL *)aURL;
-(void) interpretHTTPUpdateCode:(UInt32) aCode;
-(void) cleanupHTTPCheck:(NSTimer *)aTimer;
-(NSString *)userAgentString;
@end

// Constants - for update status checking
static const CFOptionFlags kNetworkEvents =  kCFStreamEventEndEncountered | kCFStreamEventErrorOccurred;

// Notification of URL load
NSString *URLLoadNotification = @"url_load";
NSString *URLLoadSuccessKey = @"url_bool";

@implementation Bookmark

-(id) init
{
  if ((self = [super init])) {
    mURL = [[NSString alloc] init];
    mStatus = [[NSNumber alloc] initWithUnsignedInt:0];//retain count +1
    mNumberOfVisits = [mStatus retain]; //retain count +2
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
  if ([self status]==kBookmarkSpacerStatus)
    return YES;
  return NO;
}

-(BOOL) isMoved
{
  if ([self status]==kBookmarkMovedLinkStatus)
    return YES;
  return NO;
}

-(BOOL) isCheckable
{
  unsigned myStatus = [self status];
  if (myStatus!=kBookmarkNeverCheckStatus &&
      myStatus!=kBookmarkBrokenLinkStatus &&
      myStatus!=kBookmarkSpacerStatus)
    return YES;
  return NO;
}

-(BOOL) isSick
{
  if (([self status]==kBookmarkBrokenLinkStatus) || ([self status]==kBookmarkServerErrorStatus))
    return YES;
  return NO;
}

-(void) setStatus:(unsigned)aStatus
{
  if ((aStatus != [mStatus unsignedIntValue]) &&
      ((aStatus == kBookmarkBrokenLinkStatus) ||
       (aStatus == kBookmarkMovedLinkStatus) ||
       (aStatus == kBookmarkOKStatus) ||
       (aStatus == kBookmarkSpacerStatus) ||
       (aStatus == kBookmarkServerErrorStatus) ||
       (aStatus == kBookmarkNeverCheckStatus)))
  {
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
//
// from loads, we only get success or failure - not why things failed.
// so we'll call everything "server error"
//
-(void) urlLoadCheck:(NSNotification *)note
{
  NSString *loadedURL = [note object];
  if ([loadedURL hasSuffix:@"/"])
    loadedURL = [loadedURL substringToIndex:([loadedURL length]-1)];
  NSString *myURL = [self url];
  if ([myURL hasSuffix:@"/"])
    myURL = [myURL substringToIndex:([myURL length]-1)];
  if ([loadedURL isEqualToString:myURL]) {
    unsigned loadStatus = [[[note userInfo] objectForKey:URLLoadSuccessKey] unsignedIntValue];
    [self setLastVisit:[NSDate date]];
    [self setStatus:loadStatus];
    if (loadStatus == kBookmarkOKStatus) 
      [self setNumberOfVisits:([self numberOfVisits]+1)];
  }
}

#pragma mark -
//
// handles checking for updates & whatnot of itself
//

-(NSURL *)urlAsURL
{
  // We'll just assume if there's a # it's a fragment marker.  Yes,
  // this is almost certainly a bug, but just for checking status so who cares.
  NSString *escapedURL = (NSString *)CFURLCreateStringByAddingPercentEscapes
  (NULL,(CFStringRef)[self url],CFSTR("#"),NULL,kCFStringEncodingUTF8);
  NSURL *myURL = [NSURL URLWithString:escapedURL];
  [escapedURL release];
  return myURL;
}

// for now, we'll only check updates on file or http scheme.
// don't know if https will work.
- (void) checkForUpdate;
{
  NSURL* myURL = [self urlAsURL];
  if (myURL) {
    if ([[myURL scheme] isEqualToString:@"http"] && [self hostIsReachable:myURL])
      [self doHTTPUpdateRequest:myURL];
    else if ([[myURL scheme] isEqualToString:@"file"])
      [self doFileUpdateRequest:myURL];
    else
      [self setStatus:kBookmarkNeverCheckStatus];
  }
}

- (BOOL) hostIsReachable:(NSURL *)aURL
{
  const char *hostname = [[aURL host] cString];
  if (!hostname)
    return NO;
  SCNetworkConnectionFlags flags;
  // just like TN 1145 instructs, not that we're using CodeWarrior
  assert(sizeof(SCNetworkConnectionFlags) == sizeof(int));
  BOOL isReachable = NO;
  if ( SCNetworkCheckReachabilityByName(hostname, &flags) ) {
    isReachable =  !(flags & kSCNetworkFlagsConnectionRequired) && (flags & kSCNetworkFlagsReachable);
  }
  return isReachable;
}
//
// CF Callback functions for handling bookmark updating
//

static void doHTTPUpdateCallBack(CFReadStreamRef stream, CFStreamEventType type, void *bookmark)
{
  CFHTTPMessageRef aResponse = NULL;
  CFStreamError anError;
  NSString *newURL = NULL;
  UInt32 errCode;
  switch (type){

    case kCFStreamEventEndEncountered:
      aResponse = (CFHTTPMessageRef) CFReadStreamCopyProperty(stream,kCFStreamPropertyHTTPResponseHeader);
      if (aResponse) {
        errCode = CFHTTPMessageGetResponseStatusCode(aResponse);
        switch (errCode) {
          case 301: //permanent move - update URL
            newURL = (NSString*)CFHTTPMessageCopyHeaderFieldValue(aResponse,CFSTR("Location"));
            [(Bookmark *)bookmark setUrl:newURL];
            [newURL release];
            break;
          default:
            [(Bookmark *)bookmark interpretHTTPUpdateCode:errCode];
            break;
        }
      } else //beats me.  blame the server.
        [(Bookmark *)bookmark interpretHTTPUpdateCode:500];
      break;

    case kCFStreamEventErrorOccurred:
      anError = CFReadStreamGetError(stream);
      if (anError.domain == kCFStreamErrorDomainHTTP) {
        errCode = anError.error; //signed being assigned to unsinged.  oh well
        [(Bookmark *)bookmark interpretHTTPUpdateCode:errCode];
      } else // call it server error
        [(Bookmark *)bookmark interpretHTTPUpdateCode:500];
      break;

    default:
      NSLog(@"If you can read this you're too close to the screen.");
      break;
  }
  //
  // update our last visit date & cleanup
  //
  [(Bookmark *)bookmark setLastVisit:[NSDate date]];
  if (aResponse)
    CFRelease(aResponse);
}

// borrowed heavily from Apple's CFNetworkHTTPDownload example
-(void) doHTTPUpdateRequest:(NSURL *)aURL
{
  CFHTTPMessageRef messageRef = NULL;
  CFReadStreamRef readStreamRef	= NULL;
  CFStreamClientContext	ctxt= { 0, (void*)self, NULL, NULL, NULL }; //pointer to self lets us update
  // get message
  messageRef = CFHTTPMessageCreateRequest(kCFAllocatorDefault, CFSTR("HEAD"), (CFURLRef)aURL, kCFHTTPVersion1_1);
  if (!messageRef) {
    NSLog(@"CheckForUpdate: Can't create CFHTTPMessage for %@",aURL);
    return;
  }
  // set if-modified-since header field, and maybe others if we're bored.
  // really, since we just want to see if it's there, don't even need to
  // do this.
  NSString *httpDate = [[self lastVisit] descriptionWithCalendarFormat:@"%a, %d %b %Y %H:%M:%S GMT" timeZone:[NSTimeZone timeZoneWithAbbreviation:@"GMT"] locale:nil];
  NSString *userAgent = [self userAgentString];
  CFHTTPMessageSetHeaderFieldValue(messageRef,CFSTR("If-Modified-Since"),(CFStringRef)httpDate);
  CFHTTPMessageSetHeaderFieldValue(messageRef,CFSTR("User-Agent"),(CFStringRef)userAgent);

  //setup read stream
  readStreamRef	= CFReadStreamCreateForHTTPRequest(kCFAllocatorDefault, messageRef);
  CFRelease(messageRef);
  if (!readStreamRef) {
    NSLog(@"CheckForUpdate: Can't create CFReadStream for %@",aURL);
    return;
  }

  // handle http proxy, if necessary
  NSDictionary* proxyConfigDict = (NSDictionary *)SCDynamicStoreCopyProxies(NULL);
  if (proxyConfigDict) {
    if ([[proxyConfigDict objectForKey:(NSString*)kSCPropNetProxiesHTTPEnable] intValue] != 0) {
      NSString *proxyURL = [proxyConfigDict objectForKey:(NSString*)kSCPropNetProxiesHTTPProxy];
      NSNumber *proxyPort = [proxyConfigDict objectForKey:(NSString*)kSCPropNetProxiesHTTPPort];
      if (proxyURL && proxyPort) {
        CFHTTPReadStreamSetProxy(readStreamRef,(CFStringRef)proxyURL,(CFIndex)[proxyPort unsignedIntValue]);
      }
    }
    [proxyConfigDict release];
  }
  
 //setup callback function
  if (CFReadStreamSetClient(readStreamRef, kNetworkEvents, doHTTPUpdateCallBack, &ctxt ) == false ) {
    NSLog(@"CheckForUpdate: Can't set CFReadStream callback for %@",aURL);
    CFRelease(readStreamRef);
    return;
  }
  //schedule the stream & open the connection
  CFReadStreamScheduleWithRunLoop(readStreamRef, CFRunLoopGetCurrent(), kCFRunLoopCommonModes);
  if (CFReadStreamOpen(readStreamRef) == false ){
    NSLog(@"CheckForUpdate: Can't open CFReadStream for %@",aURL);
    CFReadStreamSetClient(readStreamRef, NULL, NULL, NULL);
    CFRelease(readStreamRef);
    return;
  }
  //schedule a timeout.  we'll give it, oh, 60 seconds before killing the check
  //this timer is responsible for cleaning up the read stream memory!!!!!!!!!
  [NSTimer scheduledTimerWithTimeInterval:60 target:self selector:@selector(cleanupHTTPCheck:) userInfo:(id)readStreamRef repeats:NO];
}

// My interpretation of what should & shouldn't happen is
// quite possibly incorrect.  Feel free to adjust.
-(void) interpretHTTPUpdateCode:(UInt32) errCode
{
  switch (errCode){
    case 200: //OK - bookmark updated
    case 203: //Non-authoritative info - call it same as OK
    case 302: //Found - new link, but don't update
    case 303: //See other - new link, but don't update
    case 304: //Not modified - do nothing
    case 307: //Temporary redirect - new link, but don't update
      [self setStatus:kBookmarkOKStatus];
      break;

    case 300: //multiple choices - not sure what to do so we'll bail here
    case 301: //Moved permananently - should be handled in callback
    case 305: //Use proxy (specified) - should retry request
      [self setStatus:kBookmarkMovedLinkStatus];
      break;

    case 400: //Bad request - we f'd up
    case 403: //Forbidden - not good
    case 404: //Not found - clearly not good
    case 410: //Gone - nah nah nahnah, nah nah nahnah, hey hey hey, etc.
      [self setStatus:kBookmarkBrokenLinkStatus];
      break;


    case 401: //Unauthorized - need to be clever about checking this
    case 402: //Payment required - funk that.
    case 405: //Method not allowed - funk that, too.
    case 406: //Not Acceptable - shouldn't happen, but oh well.
    case 407: //Proxy Authentication Required - need to be cleverer here
    case 411: //Length required - need to be cleverer
    case 413: //Request entity too large - shouldn't happen
    case 414: //Request URI too large - shouldn't happen
    case 415: //Request URI too large - shouldn't happen
    case 500: //Internal server error
    case 501: //Not Implemented
    case 502: //Bad Gateway
    case 503: //Service Unavailable
    case 504: //Gateway Timeout
    case 505: //HTTP Version not supported
      [self setStatus:kBookmarkServerErrorStatus];
      break;

    case 100: //Continue - just ignore
    case 101: //Switching protocols - not that smart yet
    case 201: //Created - shouldn't happen
    case 202: //Accepted - shouldn't happen
    case 204: //No content - shouldn't happen
    case 205: //Reset content - shouldn't happen
    case 206: //Partial content - shouldn't happen for HEAD request
    case 408: //Request timeout - shouldn't happen
    case 409: //Conflict - shouldn't happen
    case 412: //Precondintion failed - shouldn't happen
    case 416: //requested range not satisfiable - shouldn't happen
    case 417: //Expectation failed - shouldn't happen
    default:
      break;
  }
}

-(void) doFileUpdateRequest:(NSURL *)aURL
{
  // if it's here, it's got a scheme of file, so we can call path directly
  NSFileManager *fM = [NSFileManager defaultManager];
  if (![fM fileExistsAtPath:[aURL path]])
    [self setStatus:kBookmarkBrokenLinkStatus];
}

// this function cleans up after our stream request.
// if you get rid of it, we leak memory
-(void) cleanupHTTPCheck:(NSTimer *)aTimer;
{
  CFReadStreamRef stream = (CFReadStreamRef)[aTimer userInfo];
  CFReadStreamSetClient(stream,NULL,NULL,NULL);
  CFRelease(stream);
}

// this is done poorly.  if someone feels like doing this more correctly, more power to you.
-(NSString *)userAgentString
{
  return [NSString stringWithString:@"Mozilla/5.0 (Macintosh; U; PPC Mac OS X Mach-O) Gecko Camino"];
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
  [self setDescription:[aDict objectForKey:BMDescKey]];
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
        [self setDescription:[[attribDict objectForKey:CaminoDescKey] stringByRemovingAmpEscapes]];
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

