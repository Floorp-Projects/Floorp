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

#import "CHSplashScreenWindow.h"

@interface CHSplashScreenWindow (Private)
-(void)fadeIn;
-(void)fadeInThread;
-(void)fadeOut;
-(void)fadeOutThread;
@end

@implementation CHSplashScreenWindow

-(id)splashImage:(NSImage *)splashImage withFade:(BOOL)shouldFade withStatusRect:(NSRect)statusRect
{
    NSRect		 splashRect;
    NSRect		 statusFieldRect;
    NSImageView *contentImageView;

    _fadeDelay = (0.5 / 60.0);

    if ( ! splashImage ) {
        if ( [NSImage imageNamed:@"splash"] ) {
            splashImage = [NSImage imageNamed:@"splash"];
        } else {
            splashImage = [NSImage imageNamed:@"NSApplicationIcon"];
        }
    }

    splashRect		  = NSMakeRect(0.0, 0.0, [splashImage size].width, [splashImage size].height);
    contentImageView  = [[[NSImageView alloc] initWithFrame:splashRect] autorelease];
#if USE_STATUS_FIELD
    statusFieldRect   = NSMakeRect(0.0, 170.0, (splashRect.size.width - 5.0), 16.0);
    _statusField      = [[[NSTextField alloc] initWithFrame:statusFieldRect];
#endif

    if ( (self = [super initWithContentRect:splashRect
                            styleMask:NSBorderlessWindowMask
                              backing:NSBackingStoreBuffered
                                defer:NO]) ) {

        [contentImageView setImage:splashImage];
        _fadeThreadLock = nil;

#if USE_STATUS_FIELD
        [_statusField setDrawsBackground:NO];
        [_statusField setEditable:NO];
        [_statusField setSelectable:NO];
        [_statusField setBezeled:NO];
        [_statusField setBordered:NO];
        [_statusField setFont:[NSFont fontWithName:@"Monaco" size:10.0]];
        [_statusField setTextColor:[NSColor whiteColor]];
        [_statusField setAlignment:NSRightTextAlignment];
        [_statusField setStringValue:@"Loading..."];
#endif

        [[self contentView] addSubview:contentImageView];
#if USE_STATUS_FIELD
        [[self contentView] addSubview:_statusField];
#endif
        [self setOpaque:NO];
        [self setHasShadow:YES];
        [self setReleasedWhenClosed:YES];
        [self center];

        if ( shouldFade ) {
            [self fadeIn];
        } else {
            [self makeKeyAndOrderFront:self];
        }
        __didFadeIn = shouldFade;
    }
    return self;
}

-(NSString *)statusText
{
    return [_statusField stringValue];
}

-(void)setStatusText:(NSString *)newText
{
#if USE_STATUS_FIELD
    [_statusField setStringValue:newText];
    [_statusField display];
#endif
}

-(void)fadeIn
{
    [self setAlphaValue:0.0];
    [self makeKeyAndOrderFront:self];
    
    if (_fadeThreadLock == nil) {
        _fadeThreadLock = [[NSLock allocWithZone:[self zone]] init];
    }
    
    [NSThread detachNewThreadSelector:@selector(fadeInThread) toTarget:self withObject:nil];
}

-(void)fadeInThread
{
    float fadeLevel = 0.0;
    NSAutoreleasePool *threadMainPool = [[NSAutoreleasePool alloc] init];

    [_fadeThreadLock lock];

    while ( fadeLevel < 1.0 ) {
        NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
        fadeLevel += 0.20;
        [self setAlphaValue:fadeLevel];
        [self flushWindow];
        [NSThread sleepUntilDate:[NSDate dateWithTimeIntervalSinceNow:_fadeDelay]];
        [pool release];
    }

    [_fadeThreadLock unlock];
    [threadMainPool release];
}

-(void)fadeOut
{
    if (_fadeThreadLock == nil) {
        _fadeThreadLock = [[NSLock allocWithZone:[self zone]] init];
    }

    [NSThread detachNewThreadSelector:@selector(fadeOutThread) toTarget:self withObject:nil];
}

-(void)fadeOutThread
{
    float fadeLevel = 1.0;
    NSAutoreleasePool *threadMainPool = [[NSAutoreleasePool alloc] init];

    [_fadeThreadLock lock];

    while ( fadeLevel > 0.0 ) {
        NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
        fadeLevel -= 0.1;
        [self setAlphaValue:fadeLevel];
        [self flushWindow];
        [NSThread sleepUntilDate:[NSDate dateWithTimeIntervalSinceNow:_fadeDelay]];
        [pool release];
    }

    [_fadeThreadLock unlock];
    [threadMainPool release];
}

-(void)close
{
#if USE_STATUS_FIELD
    [_statusField release];
#endif

//  if ( __didFadeIn ) {
    if (      NO     ) { //Fade out is still problematic...
        [self fadeOut];
    }
    [super close];
}

-(void)dealloc
{
    [_fadeThreadLock release];

    [super dealloc];
}

@end
