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
*    Nathan Day (original author)
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
* This class is taken whole cloth from "NDRunLoopMessenger", written by
* Nathan Day.  I removed a couple methods, changed some strings, and made
* the tinest tweaks to sendData() & messageInvocation:withResult.  
* The original code can be found at:
* http://homepage.mac.com/nathan_day/pages/source.html#NDRunLoopMessenger
*
* A big problem with this code is that the send loop can fill up, which blocks
* the thread this gets run on.  I noticed this when checking bookmark imports.
* Storing things in a queue would be clever, but I'm not that clever.
*
* ***** END LICENSE BLOCK ***** */

#import "RunLoopMessenger.h"

static NSString* kThreadDictionaryKey = @"nd_rml_tdk";
static NSString* kSendMessageException = @"nd_rlm_sme";
static NSString* kConnectionDoesNotExistsException = @"nd_rlm_cdnee";
//
//struct message
//
struct message
{
  NSConditionLock* resultLock;
  NSInvocation* invocation;
};
//
// function sendData
//
void sendData(NSData * aData, NSPort * aPort);
//
// interface RunLoopMessenger
//
@interface RunLoopMessenger (Private)
- (void)createPortForRunLoop:(NSRunLoop *)aRunLoop;
- (void)registerNotificationObservers;
@end


@implementation RunLoopMessenger

+ (id)runLoopMessengerForCurrentRunLoop
{
  RunLoopMessenger* currentRunLoopMessenger;
  currentRunLoopMessenger = [[[NSThread currentThread] threadDictionary] objectForKey:kThreadDictionaryKey];
  if( currentRunLoopMessenger == nil )
    currentRunLoopMessenger = [[RunLoopMessenger alloc] init];
  return currentRunLoopMessenger;
}

- (id)init
{
  if((self = [super init]))
  {
    NSMutableDictionary* theThreadDictionary;
    id theOneForThisThread;
    theThreadDictionary = [[NSThread currentThread] threadDictionary];
    if((theOneForThisThread = [theThreadDictionary objectForKey:kThreadDictionaryKey]))
    {
      [self release];
      self = theOneForThisThread;
    }
    else
    {
      [self createPortForRunLoop:[NSRunLoop currentRunLoop]];
      [theThreadDictionary setObject:self forKey:kThreadDictionaryKey];
      [self registerNotificationObservers];
    }
  }
  return self;
}

- (void)registerNotificationObservers
{
  [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(threadWillExit:) name:NSThreadWillExitNotification object:nil];
  [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(portDidBecomeInvalid:) name:NSPortDidBecomeInvalidNotification object:mPort];
}

- (void)dealloc
{
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [[[NSThread currentThread] threadDictionary] removeObjectForKey:kThreadDictionaryKey];
  [mPort removeFromRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
  [mPort release];
  [super dealloc];
}

- (void)threadWillExit:(NSNotification *)notification
{
  NSThread* thread = [notification object];
  if( [[thread threadDictionary] objectForKey:kThreadDictionaryKey] == self )
  {
    [[thread threadDictionary] removeObjectForKey:kThreadDictionaryKey];
    [mPort removeFromRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
    mPort= nil;
    [[NSNotificationCenter defaultCenter] removeObserver:self];
  }
}

- (void)portDidBecomeInvalid:(NSNotification *)notification
{
  if( [notification object] == mPort)
  {
    [[[NSThread currentThread] threadDictionary] removeObjectForKey:kThreadDictionaryKey];
    [mPort removeFromRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
    mPort= nil;
    [[NSNotificationCenter defaultCenter] removeObserver:self];
  }
}

- (void)target:(id)aTarget performSelector:(SEL)aSelector
{
  [self target:aTarget performSelector:aSelector withObjects:nil withResult:NO];
}

- (void)target:(id)aTarget performSelector:(SEL)aSelector withObjects:(NSArray *)anArray
{
  [self target:aTarget performSelector:aSelector withObjects:anArray withResult:NO];
}

- (id)target:(id)aTarget performSelector:(SEL)aSelector withResult:(BOOL)aFlag
{
  return [self target:aTarget performSelector:aSelector withObjects:nil withResult:aFlag];
}

- (id)target:(id)aTarget performSelector:(SEL)aSelector withObjects:(NSArray *)anArray withResult:(BOOL)aFlag
{
  NSInvocation* theInvocation;
  id anArgument, theResult = nil;
  unsigned i,count;
  theInvocation = [NSInvocation invocationWithMethodSignature:[aTarget methodSignatureForSelector:aSelector]];
  [theInvocation setSelector:aSelector];
  [theInvocation setTarget:aTarget];
  if (anArray) {
    count = [anArray count];
    for (i=0;i < count;i++) {
      anArgument = [anArray objectAtIndex:i];
      [theInvocation setArgument:&anArgument atIndex:(i+2)];
    }
  }
  [self messageInvocation:theInvocation withResult:aFlag];
  if( aFlag )
    [theInvocation getReturnValue:&theResult];
  return theResult;
}

- (void)messageInvocation:(NSInvocation *)anInvocation withResult:(BOOL)aResultFlag
{
  struct message* theMessage;
  NSMutableData* theData;

  [anInvocation retainArguments];

  theData = [NSMutableData dataWithLength:sizeof(struct message)];
  theMessage = (struct message *)[theData mutableBytes];
  theMessage->invocation = [anInvocation retain]; // will be released by handlePortMessage
  theMessage->resultLock = aResultFlag ? [[NSConditionLock alloc] initWithCondition:NO] : nil;
  NS_DURING
  sendData(theData,mPort);
  NS_HANDLER
    [theMessage->invocation release];
    [theMessage->resultLock unlock];
    [theMessage->resultLock release];
    return;
  NS_ENDHANDLER
  if(aResultFlag)
  {
    [theMessage->resultLock lockWhenCondition:YES];
    [theMessage->resultLock unlock];
    [theMessage->resultLock release];
  }
}

- (void)handlePortMessage:(NSPortMessage *)aPortMessage
{
  struct message* theMessage;
  NSData* theData;
  theData = [[aPortMessage components] lastObject];
  theMessage = (struct message*)[theData bytes];
  [theMessage->invocation invoke];
  if( theMessage->resultLock )
  {
    [theMessage->resultLock lock];
    [theMessage->resultLock unlockWithCondition:YES];
  }
  [theMessage->invocation release];	// to balance messageInvocation:withResult:
}

- (void)createPortForRunLoop:(NSRunLoop *)aRunLoop
{
  mPort = [NSPort port];
  [mPort setDelegate:self];
  [mPort scheduleInRunLoop:aRunLoop forMode:NSDefaultRunLoopMode];
}

void sendData(NSData* aData, NSPort* aPort)
{
  NSPortMessage* thePortMessage;
  if(aPort)
  {
    thePortMessage = [[NSPortMessage alloc] initWithSendPort:aPort receivePort:nil components:[NSArray arrayWithObject:aData]];
    if(![thePortMessage sendBeforeDate:[NSDate dateWithTimeIntervalSinceNow:15.0]])
      [NSException raise:kSendMessageException format:@"An error occured will trying to send the message data %@", [aData description]];
    [thePortMessage release];
  }
  else
  {
    [NSException raise:kConnectionDoesNotExistsException format:@"The connection to the runLoop does not exist"];
  }
}


@end
