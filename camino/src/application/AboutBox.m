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
 *   Matt Judy
 *   David Hyatt  <hyatt@netscape.com>
 *   Simon Fraser <sfraser@netscape.com>
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

#import "AboutBox.h"

#include "nsBuildID.h"

@interface AboutBox(Private)
- (void)loadWindow;
@end

@implementation AboutBox

static AboutBox *sharedInstance = nil;

+ (AboutBox *)sharedInstance
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
    [self loadWindow];
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
  scrollTimer = [NSTimer scheduledTimerWithTimeInterval:0.03
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
  if ([NSDate timeIntervalSinceReferenceDate] >= startTime)
  {
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
      currentPosition += 1;
    }
  }
}

- (void)loadWindow
{
  NSString *creditsPath;
  NSAttributedString *creditsString;
  float fieldHeight;
  float containerHeight;

  if (![NSBundle loadNibNamed:@"AboutBox" owner:self]) {
    NSLog( @"Failed to load AboutBox.nib" );
    NSBeep();
    return;
  }

  [window setTitle:[NSString stringWithFormat: NSLocalizedString(@"AboutWindowTitleFormat", @""),
        NSLocalizedStringFromTable(@"CFBundleName", @"InfoPlist", nil)]];

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
  
  [buildNumberField setStringValue: [NSString stringWithFormat: NSLocalizedString(@"BuildID", @""), NS_BUILD_ID]];  
  [window setExcludedFromWindowsMenu:YES];
  [window setMenu:nil];
  [window center];
}

@end
