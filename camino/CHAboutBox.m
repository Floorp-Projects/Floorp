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
* Contributor(s):
*   Matt Judy (Original Author)
*   David Hyatt <hyatt@netscape.com>
*/

#import "CHAboutBox.h"

@implementation CHAboutBox

static CHAboutBox *sharedInstance = nil;

+ (CHAboutBox *)sharedInstance
{
    return sharedInstance ? sharedInstance : [[self alloc] init];
}

- (id)init
{
    if (sharedInstance) {
        [self dealloc];
    } else {
        sharedInstance = [super init];
    }
        return sharedInstance;
}

- (IBAction)showPanel:(id)sender
{
    if (!creditsField) {
        NSString *creditsPath;
        NSAttributedString *creditsString;
        float fieldHeight;
        float containerHeight;

        if (![NSBundle loadNibNamed:@"AboutBox" owner:self]) {
            NSLog( @"Failed to load AboutBox.nib" );
            NSBeep();
            return;
        }

        [window setTitle:[NSString stringWithFormat: @"About %@", NSLocalizedStringFromTable(@"CFBundleName", @"InfoPlist", nil)]];

        creditsPath = [[NSBundle mainBundle] pathForResource:@"Credits" ofType:@"rtf"];
        creditsString = [[NSAttributedString alloc] initWithPath:creditsPath documentAttributes:nil];
        [creditsField replaceCharactersInRange:NSMakeRange( 0, 0 )
                                       withRTF:[creditsString RTFFromRange:
                                           NSMakeRange( 0, [creditsString length] )
                                                        documentAttributes:nil]];

        fieldHeight = [creditsField frame].size.height;
        
        (void)[[creditsField layoutManager] glyphRangeForTextContainer:[creditsField textContainer]];
        containerHeight = [[creditsField layoutManager] usedRectForTextContainer:
                                                          [creditsField textContainer]].size.height;
        maxScrollHeight = containerHeight - fieldHeight;
        
        [window setExcludedFromWindowsMenu:YES];
        [window setMenu:nil];
        [window center];
    }
    
    if (![window isVisible]) {
                currentPosition = 0;
                restartAtTop = NO;
                startTime = [NSDate timeIntervalSinceReferenceDate] + 3.0;
                [creditsField scrollPoint:NSMakePoint( 0, 0 )];
    }

    [window makeKeyAndOrderFront:nil];
}

- (void)windowDidBecomeKey:(NSNotification *)notification
{
    scrollTimer = [NSTimer scheduledTimerWithTimeInterval:1/4
                                                   target:self
                                                 selector:@selector(scrollCredits:)
                                                 userInfo:nil
                                                  repeats:YES];
}

- (void)windowDidResignKey:(NSNotification *)notification
{
    [scrollTimer invalidate];
}

- (void)scrollCredits:(NSTimer *)timer
{
    if ([NSDate timeIntervalSinceReferenceDate] >= startTime) {

        if (restartAtTop) {
            startTime = [NSDate timeIntervalSinceReferenceDate] + 3.0;
            restartAtTop = NO;
            [creditsField scrollPoint:NSMakePoint( 0, 0 )];
            return;
        }

        if (currentPosition >= maxScrollHeight) {
            startTime = [NSDate timeIntervalSinceReferenceDate] + 3.0;
            currentPosition = 0;
            restartAtTop = YES;
        } else {
            [creditsField scrollPoint:NSMakePoint( 0, currentPosition )];
            currentPosition += 0.01;
        }
    }
}

@end
