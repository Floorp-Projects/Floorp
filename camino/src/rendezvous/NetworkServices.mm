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


#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>

#include "DNSUtils.h"

#import "nsAlertController.h"
#import "CHBrowserService.h"
#import "NetworkServices.h"

@class NSNetServiceBrowser;


@interface NetworkServices(Private)

- (void)setupNetworkBrowsers;
- (void)foundService:(NSNetService*)inService moreComing:(BOOL)more;

- (void)notifyClientsOfServicesChange;

- (void)notifyClientsOfServiceResolution:(int)serviceID withURL:(NSString*)url;
- (void)notifyClientsOfServiceResolutionFailure:(int)serviceID;

- (void)serviceAppeared:(NSNetService*)service;
- (void)serviceDisappeared:(NSNetService*)service;
- (void)serviceResolved:(NSNetService*)service;

- (int)getServiceID:(NSNetService*)service;

@end

@implementation NetworkServices

- (id)init
{
  if ((self = [super init]))
  {
    mNetworkServices = [[NSMutableDictionary alloc] initWithCapacity:5];
    mCurServiceID = 0;
  }
  
  return self;
}

- (void)dealloc
{
  [self stopServices];
  [mNetworkServices release];
  [mClients release];
  [super dealloc];
}

- (void)startServices
{
  [self setupNetworkBrowsers];
}

- (void)stopServices
{
  if (mHttpBrowser)
  {
    [mHttpBrowser stop];
    [mHttpBrowser release];
    mHttpBrowser = nil;
  }
  
  if (mHttpsBrowser)
  {
    [mHttpsBrowser stop];
    [mHttpsBrowser release];
    mHttpsBrowser = nil;
  }
  
  if (mFtpBrowser)
  {
    [mFtpBrowser stop];
    [mFtpBrowser release];
    mFtpBrowser = nil;
  }
}

- (void)registerClient:(id<NetworkServicesClient>)client
{
  if (!mClients)
    mClients = [[NSMutableArray alloc] initWithCapacity:2];

  if ([mClients indexOfObject:client] == NSNotFound)
    [mClients addObject:client];
}

- (void)unregisterClient:(id<NetworkServicesClient>)client
{
  [mClients removeObject:client];
}

- (NSString*)serviceName:(int)serviceID
{
  NSNetService* service = [mNetworkServices objectForKey:[NSNumber numberWithInt:serviceID]];
  if (!service) return NULL;
  
  return [service name];
}

- (NSString*)serviceProtocol:(int)serviceID
{
  NSNetService* service = [mNetworkServices objectForKey:[NSNumber numberWithInt:serviceID]];
  if (!service) return NULL;
  
  return [service type];
}

- (void)attemptResolveService:(int)serviceID
{
  NSNetService* service = [mNetworkServices objectForKey:[NSNumber numberWithInt:serviceID]];
  if (!service) return;
  
  [service resolve];
}

- (NSEnumerator*)serviceEnumerator
{
  return [mNetworkServices keyEnumerator];
}

#pragma mark -

- (int)getServiceID:(NSNetService*)service
{
  NSArray* serviceKeys = [mNetworkServices allKeysForObject:service];
  // there should only ever be one key
  return [[serviceKeys objectAtIndex:0] intValue];
}

- (void)setupNetworkBrowsers
{
  mHttpBrowser = [[NSNetServiceBrowser alloc] init];
  [mHttpBrowser setDelegate:self];

  mHttpsBrowser = [[NSNetServiceBrowser alloc] init];
  [mHttpsBrowser setDelegate:self];

  mFtpBrowser = [[NSNetServiceBrowser alloc] init];
  [mFtpBrowser setDelegate:self];

  [mHttpBrowser  searchForServicesOfType:@"_http._tcp."  inDomain:@""];
  [mHttpsBrowser searchForServicesOfType:@"_https._tcp." inDomain:@""];
  [mFtpBrowser   searchForServicesOfType:@"_ftp._tcp."   inDomain:@""];
}

- (void)notifyClientsOfServicesChange
{
  for (unsigned int i = 0; i < [mClients count]; i ++)
  {
    id<NetworkServicesClient> client = [mClients objectAtIndex:i];
    [client availableServicesChanged:self];
  }
}

- (void)notifyClientsOfServiceResolution:(int)serviceID withURL:(NSString*)url
{
  // ldeally we'd keep track of which client issued the attemptResolveService
  // request, and only give it the callback. But we don't do that yet.
  for (unsigned int i = 0; i < [mClients count]; i ++)
  {
    id<NetworkServicesClient> client = [mClients objectAtIndex:i];
    [client serviceResolved:serviceID withURL:url];
  }
}

- (void)notifyClientsOfServiceResolutionFailure:(int)serviceID
{
  // ldeally we'd keep track of which client issued the attemptResolveService
  // request, and only give it the callback. But we don't do that yet.
  for (unsigned int i = 0; i < [mClients count]; i ++)
  {
    id<NetworkServicesClient> client = [mClients objectAtIndex:i];
    [client serviceResolutionFailed:serviceID];
  }
}

- (void)serviceAppeared:(NSNetService*)service
{
  [mNetworkServices setObject:service forKey:[NSNumber numberWithInt:(++mCurServiceID)]];
}

- (void)serviceDisappeared:(NSNetService*)service
{
  NSArray* serviceKeys = [mNetworkServices allKeysForObject:service];
  // there should only ever be one key
  [mNetworkServices removeObjectForKey:[serviceKeys objectAtIndex:0]];
}

// NSNetService delegate stuff

static inline u_int ns_get16(u_char* buffer)
{
  return ((u_int)buffer[0]) << 8 | ((u_int)buffer[1]);
}

- (void)serviceResolved:(NSNetService*)netService
{  
/*
  NSData* firstResolvedAddress = [[netService addresses] objectAtIndex:0];  
  sockaddr  socketAdrr;
  [firstResolvedAddress getBytes:&socketAdrr length:sizeof(sockaddr)];
*/
  // res_query only works for local link names in 10.2.3 (?) and later,
  NSMutableString* name = [NSMutableString stringWithString:[netService name]];
  // escape slashes and dots
  [name replaceOccurrencesOfString:@"\\" withString:@"\\\\" options:0 range:NSMakeRange(0, [name length])];
  [name replaceOccurrencesOfString:@"." withString:@"\\." options:0 range:NSMakeRange(0, [name length])];
  
  [name appendString:@"."];
  [name appendString:[netService type]];
  [name appendString:[netService domain]];

  // see RFC 1035 for the format of this buffer
  u_char buffer[2048];
  // T_SRV is missing from the headers
  int anslen = res_query([name UTF8String], C_IN, /* T_SRV */ 33, buffer, sizeof(buffer));  
  if (anslen > 12)
  {
    u_int dnsid          = ns_get16(buffer);
    u_int flags          = ns_get16(buffer + 2);
    u_int numQuestions   = ns_get16(buffer + 4);
    u_int numAnswers     = ns_get16(buffer + 6);
    u_int numAuthorities = ns_get16(buffer + 8);
    u_int numAdditionals = ns_get16(buffer + 10);
    u_int rrtype, rrclass, rdlen;
    u_char *end = buffer + anslen;
    u_char *ptr = buffer + 12;
   
    u_int i;

    for (i = 0; i < numQuestions; i++)
      ptr = DNS_SkipQuestion((DNSMessage *)buffer, ptr, end);

    for (i = 0; i < numAnswers; i++)
    {
      domainname name;
      ptr = DNS_GetDomainName((DNSMessage *)buffer, ptr, end, &name);
      if (!ptr || ptr + 10 > end) return;
      rrtype  = ns_get16(ptr);
      rrclass = ns_get16(ptr + 2) & 0x7FFF;
      // TTL is ptr+4,5,6,7
      rdlen   = ns_get16(ptr + 8);
      void* rdata = ptr + 10;
      ptr += 10 + rdlen;

      if (rrtype == /* T_SRV */ 33 && rrclass == C_IN)
      {
        // Priority rdata +0,1
        // Weight rdata +2,3
        u_int port = ns_get16(rdata + 4);
        domainname target;
        if (DNS_GetDomainName((DNSMessage *)buffer, rdata + 6, end, &target))
        {
          char escapedTarget[512];
          ConvertDomainNameToCStringWithEscape(&target, escapedTarget, '\\');
        
          BOOL promptPassword = NO;
          const char* protocol = "http:";
          if ([[netService type] hasPrefix:@"_http._tcp"])    // don't rely on the trailing period
            protocol = "http:";
          else if ([[netService type] hasPrefix:@"_https._tcp"])
            protocol = "https:";
          else if ([[netService type] hasPrefix:@"_ftp._tcp"]) {
            protocol = "ftp:";
            promptPassword = YES;
          }
          
          NSString* serviceTextRecord = [netService protocolSpecificInformation];
          // The text record can contain a series of name/value pairs separated by '\1'.
          // We need to search in there for a "path=" (case insensitive).
          const char* serviceText = [serviceTextRecord cString];
          const char* textPtr = serviceText;
          const char* textEnd = textPtr + strlen(textPtr);
          
          char pathBuffer[1024 + 1];
          pathBuffer[0] = '\0';
          
          BOOL chunkStart = YES;
          while (textPtr < textEnd)
          {
            if (chunkStart && strncasecmp(textPtr, "path=", 5) == 0)
            {
              const char* pathStart = textPtr;
              
              // find the end
              while (*textPtr && (*textPtr != '\1'))
                textPtr ++;
              
              int length = textPtr - pathStart;
              if (length > 1024)
                length = 1024;
              strncpy(pathBuffer, pathStart, length);
              pathBuffer[length] = '\0';
              break;
            }
            
            chunkStart = (*textPtr == '\1');
            textPtr ++;
          }
          
          NSString* urlString = nil;
          if (promptPassword)
          {
            // get a username for ftp, since most local servers won't support anonymous login
            nsAlertController* controller = CHBrowserService::GetAlertController();
            BOOL checked = NO;
            
            NSMutableString* userName = [NSMutableString string];
            
            BOOL confirmed = [controller prompt:[NSApp mainWindow]
                title:NSLocalizedString(@"UserNameRequestTitle", @"")
                text:[NSString stringWithFormat:NSLocalizedString(@"UsernameRequestFormat", @""), [NSString stringWithCString:escapedTarget]]
                promptText:userName
                checkMsg:@"" checkValue:&checked doCheck:NO];
            if (!confirmed)
              return;   // fix
          
            urlString = [NSString stringWithFormat:@"%s//%@@%s:%u%s", protocol, userName, escapedTarget, port, pathBuffer];
          }
          else
            urlString = [NSString stringWithFormat:@"%s//%s:%u%s", protocol, escapedTarget, port, pathBuffer];

          [self notifyClientsOfServiceResolution:[self getServiceID:netService] withURL:urlString];
          return;
        }
      }
    }
  }

  // only get here on failure
  [self notifyClientsOfServiceResolutionFailure:[self getServiceID:netService]];
}

- (void)netServiceDidResolveAddress:(NSNetService *)netService
{
  [self serviceResolved:netService];
  // now clear out the state of the service
  [netService stop];
}

- (void)netService:(NSNetService *)sender didNotResolve:(NSDictionary *)errorDict
{
  [self notifyClientsOfServiceResolutionFailure:[self getServiceID:sender]];
  // now clear out the state of the service
  [sender stop];
}

// Network browser stuff

- (void)netServiceBrowser:(NSNetServiceBrowser *)aNetServiceBrowser didFindDomain:(NSString *)domainString moreComing:(BOOL)moreComing
{
  // unused. We just search the local domain for now
}

- (void)netServiceBrowser:(NSNetServiceBrowser *)aNetServiceBrowser didFindService:(NSNetService *)aNetService moreComing:(BOOL)moreComing
{
  [self serviceAppeared:aNetService];
  [aNetService setDelegate:self];
  
  // rebuild the menu
  if (!moreComing)
    [self notifyClientsOfServicesChange];
}

- (void)netServiceBrowser:(NSNetServiceBrowser *)aNetServiceBrowser didNotSearch:(NSDictionary *)errorDict
{
  [self notifyClientsOfServicesChange];
}

- (void)netServiceBrowser:(NSNetServiceBrowser *)aNetServiceBrowser didRemoveDomain:(NSString *)domainString moreComing:(BOOL)moreComing
{
  // this will never get called until Apple implement multicast domains.
}

- (void)netServiceBrowser:(NSNetServiceBrowser *)aNetServiceBrowser didRemoveService:(NSNetService *)aNetService moreComing:(BOOL)moreComing
{
  [self serviceDisappeared:aNetService];
  if (!moreComing)
    [self notifyClientsOfServicesChange];
}

- (void)netServiceBrowserDidStopSearch:(NSNetServiceBrowser *)aNetServiceBrowser
{
}

- (void)netServiceBrowserWillSearch:(NSNetServiceBrowser *)aNetServiceBrowser
{
}

@end
