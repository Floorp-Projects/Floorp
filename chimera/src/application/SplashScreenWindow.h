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
*/

#import <Cocoa/Cocoa.h>

@interface SplashScreenWindow : NSWindow
{
    BOOL            mFades;
    BOOL            mDidFadeIn;
    int             mFadeIndex;
    NSTimeInterval  mFadeDelay;
    id              mFadeThreadLock;
    NSTextField    *mStatusField;
}

// This method inits the window and displays it, slightly proud of center,
// and at the size of the image it displays.
//
// The splash method should be used in your main controller's init method
// in this fashion:  splashWindow = [[SplashScreenWindow alloc] splashImage:nil withFade:NO withStatusRect:someRect];
//
// Passing nil to splashImage will attempt to load [NSImage imageNamed:@"splash"] instead.
// If that fails, the app icon will be displayed.
//
// The window will release itself whenever you send it the close message.
- (id)initWithImage:(NSImage*)splashImage withFade:(BOOL)shouldFade;

-(NSString *)statusText;
-(void)setStatusText:(NSString *)newText;


@end
