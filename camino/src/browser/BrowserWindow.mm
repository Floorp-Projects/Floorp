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

#import "BrowserWindow.h"
#import "BrowserWindowController.h"
#import "AutoCompleteTextField.h"

static const int kEscapeKeyCode = 53;

@implementation BrowserWindow

- (void)dealloc
{
  [super dealloc];
}

- (BOOL)makeFirstResponder:(NSResponder*)responder
{
  NSResponder* oldResponder = [self firstResponder];
  BOOL madeFirstResponder = [super makeFirstResponder:responder];
  if (madeFirstResponder && oldResponder != [self firstResponder])
    [(BrowserWindowController*)[self delegate] focusChangedFrom:oldResponder to:[self firstResponder]];

  //NSLog(@"Old FR %@, new FR %@, responder %@, made %d", oldResponder, [self firstResponder], responder, madeFirstResponder);
  return madeFirstResponder;
}

- (void)sendEvent:(NSEvent *)theEvent
{
  // We need this hack because NSWindow::sendEvent will eat the escape key
  // and won't pass it down to the key handler of responders in the window.
  // We have to override sendEvent for all of our escape key needs.
  if ([theEvent type] == NSKeyDown &&
      [theEvent keyCode] == kEscapeKeyCode && [self firstResponder] == [mAutoCompleteTextField fieldEditor])
    [mAutoCompleteTextField revertText];
  else
    [super sendEvent:theEvent];
}

- (BOOL)suppressMakeKeyFront
{
  return mSuppressMakeKeyFront;
}

- (void)setSuppressMakeKeyFront:(BOOL)inSuppress
{
	mSuppressMakeKeyFront = inSuppress;
}

// pass command-return off to the controller so that locations/searches may be opened in a new tab
- (BOOL)performKeyEquivalent:(NSEvent *)theEvent
{
  BrowserWindowController* windowController = (BrowserWindowController*)[self delegate];
  NSString* keyString = [theEvent charactersIgnoringModifiers];
  unichar keyChar = [keyString characterAtIndex:0];
  BOOL handled = NO;
  if (keyChar == NSCarriageReturnCharacter) {
    handled = [windowController handleCommandReturn];
  }
  return handled ? handled : [super performKeyEquivalent:theEvent];
}

// accessor for the 'URL' Apple Event attribute
- (NSString*)getURL
{
  BrowserWindowController* windowController = (BrowserWindowController*)[self delegate];
  
  NSString* titleString = nil;
  NSString* urlString = nil;

  [[windowController getBrowserWrapper] getTitle:&titleString andHref:&urlString];
  return urlString;
}

@end
