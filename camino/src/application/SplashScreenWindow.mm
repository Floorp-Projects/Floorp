/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
*
* The contents of this file are subject to the Mozilla Public
* License Version 1.1 (the "License"); you may not use this file
* except in compliance with the License. You may obtain a copy of
* the License at http://www.mozilla.org/MPL/
*
* Software distributed under the License is distributed on an "AS
* IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
* implied. See the License for the specific language governing
* rights and limitations under the License.
*
* The Original Code is the Mozilla browser.
*
* The Initial Developer of the Original Code is Matt Judy.
*
*/

#import "SplashScreenWindow.h"

#include "nsBuildID.h"

//#define USE_STATUS_FIELD 1

@interface SplashScreenWindow (Private)
-(void)fadeIn;
-(void)fadeInThread;
-(void)fadeOut;
-(void)fadeOutThread;
@end

@implementation SplashScreenWindow

- (id)initWithImage:(NSImage*)splashImage withFade:(BOOL)shouldFade
{
  if (!splashImage)
  {
    if ([NSImage imageNamed:@"splash"])
      splashImage = [NSImage imageNamed:@"splash"];
    else
      splashImage = [NSImage imageNamed:@"NSApplicationIcon"];
  }

  NSRect splashRect = NSMakeRect(0.0, 0.0, [splashImage size].width, [splashImage size].height);

  if ( (self = [super initWithContentRect:splashRect
                                styleMask:NSBorderlessWindowMask
                                  backing:NSBackingStoreBuffered
                                    defer:NO]))
  {
    NSImageView* contentImageView = [[[NSImageView alloc] initWithFrame:splashRect] autorelease];

#if USE_STATUS_FIELD
    NSRect statusFieldRect = NSMakeRect(0.0, 170.0, (splashRect.size.width - 5.0), 16.0);
    mStatusField = [[[NSTextField alloc] initWithFrame:statusFieldRect] autorelease];
#endif

    NSRect versionFieldRect = NSMakeRect(2.0, 2.0, (splashRect.size.width - 5.0), 16.0);
    NSTextField* versionField = [[[NSTextField alloc] initWithFrame:versionFieldRect] autorelease];

    [contentImageView setImage:splashImage];

#if USE_STATUS_FIELD
    [mStatusField setDrawsBackground:NO];
    [mStatusField setEditable:NO];
    [mStatusField setSelectable:NO];
    [mStatusField setBezeled:NO];
    [mStatusField setBordered:NO];
    [mStatusField setFont:[NSFont fontWithName:@"Monaco" size:10.0]];
    [mStatusField setTextColor:[NSColor whiteColor]];
    [mStatusField setAlignment:NSRightTextAlignment];
    [mStatusField setStringValue:@"Loading..."];
#endif

    [versionField setDrawsBackground:NO];
    [versionField setEditable:NO];
    [versionField setSelectable:NO];
    [versionField setBezeled:NO];
    [versionField setBordered:NO];
    [versionField setFont:[NSFont labelFontOfSize:10.0]];
    [versionField setTextColor:[NSColor grayColor]];
    NSString* versionString = [[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleGetInfoString"];
    NSString* buildIDString = [NSString stringWithFormat: NSLocalizedString(@"BuildID", @""), NS_BUILD_ID];
    [versionField setStringValue:[NSString stringWithFormat:@"%@ (%@)", versionString, buildIDString]];

    [[self contentView] addSubview:contentImageView];
#if USE_STATUS_FIELD
    [[self contentView] addSubview:mStatusField];
#endif
    [[self contentView] addSubview:versionField];

    [self setOpaque:(shouldFade ? NO : YES)];
    [self setHasShadow:YES];
    [self setReleasedWhenClosed:YES];
    [self center];

    if (shouldFade) {
      mFadeDelay = (0.5 / 60.0);
      [self fadeIn];
    }
    else
      [self makeKeyAndOrderFront:self];

    mDidFadeIn = shouldFade;
  }
  
  return self;
}

-(void)dealloc
{
  [mFadeThreadLock release];
  [super dealloc];
}

#if FADE_OUT_WORKS
-(void)close
{
  if (mDidFadeIn)
    ; // [self fadeOut]; //Fade out is still problematic...

  [super close];
}
#endif

-(NSString *)statusText
{
  return [mStatusField stringValue];
}

-(void)setStatusText:(NSString *)newText
{
#if USE_STATUS_FIELD
  [mStatusField setStringValue:newText];
  [mStatusField display];
#endif
}

-(void)fadeIn
{
  [self setAlphaValue:0.0];
  [self makeKeyAndOrderFront:self];
  
  if (mFadeThreadLock == nil) {
      mFadeThreadLock = [[NSLock allocWithZone:[self zone]] init];
  }
  
  [NSThread detachNewThreadSelector:@selector(fadeInThread) toTarget:self withObject:nil];
}

-(void)fadeInThread
{
  float fadeLevel = 0.0;
  NSAutoreleasePool *threadMainPool = [[NSAutoreleasePool alloc] init];

  [mFadeThreadLock lock];

  while ( fadeLevel < 1.0 ) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    fadeLevel += 0.20;
    [self setAlphaValue:fadeLevel];
    [self flushWindow];
    [NSThread sleepUntilDate:[NSDate dateWithTimeIntervalSinceNow:mFadeDelay]];
    [pool release];
  }

  [mFadeThreadLock unlock];
  [threadMainPool release];
}

-(void)fadeOut
{
  if (mFadeThreadLock == nil) {
    mFadeThreadLock = [[NSLock allocWithZone:[self zone]] init];
  }

  [NSThread detachNewThreadSelector:@selector(fadeOutThread) toTarget:self withObject:nil];
}

-(void)fadeOutThread
{
  float fadeLevel = 1.0;
  NSAutoreleasePool *threadMainPool = [[NSAutoreleasePool alloc] init];

  [mFadeThreadLock lock];

  while ( fadeLevel > 0.0 ) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    fadeLevel -= 0.1;
    [self setAlphaValue:fadeLevel];
    [self flushWindow];
    [NSThread sleepUntilDate:[NSDate dateWithTimeIntervalSinceNow:mFadeDelay]];
    [pool release];
  }

  [mFadeThreadLock unlock];
  [threadMainPool release];
}

@end
