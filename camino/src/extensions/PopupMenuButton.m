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
 *   Simon Fraser <smfr@smfr.org> (Original Author)
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

#import "PopupMenuButton.h"


NSString* const PopupMenuButtonWillDisplayMenu = @"PopupMenuButtonWillDisplayMenu";


@implementation PopupMenuButton

- (id) initWithCoder:(NSCoder *)coder
{
	if ((self = [super initWithCoder:coder]))
  {
  }
  return self;
}

- (void)keyDown:(NSEvent *)event
{
  unichar showMenuChars[] = {
        0x0020,               // space character
        NSUpArrowFunctionKey,
        NSDownArrowFunctionKey
  };
  
  NSString* showMenuString = [NSString stringWithCharacters:showMenuChars length:sizeof(showMenuChars) / sizeof(unichar)];
  NSCharacterSet *showMenuCharset = [NSCharacterSet characterSetWithCharactersInString:showMenuString];
  
  if ([[event characters] rangeOfCharacterFromSet:showMenuCharset].location == NSNotFound)
  {
    [super keyDown:event];
    return;
  }

  [[NSNotificationCenter defaultCenter] postNotificationName:PopupMenuButtonWillDisplayMenu object:self];

  NSPoint menuLocation = NSMakePoint(0, NSHeight([self frame]) + 4.0);
  // convert to window coords
  menuLocation = [self convertPoint:menuLocation toView:nil];
  
  NSEvent *newEvent = [NSEvent mouseEventWithType:NSLeftMouseDown
                     location: menuLocation
                modifierFlags: [event modifierFlags]
                    timestamp: [event timestamp]
                 windowNumber: [event windowNumber]
                      context: [event context]
                  eventNumber: 0
                   clickCount: 1
                     pressure: 1.0];

  
  [[self cell] setHighlighted:YES];
  [NSMenu popUpContextMenu:[self menu] withEvent:newEvent forView:self];
  [[self cell] setHighlighted:NO];
}

- (void)mouseDown:(NSEvent *)event
{
  [[NSNotificationCenter defaultCenter] postNotificationName:PopupMenuButtonWillDisplayMenu object:self];

  NSPoint menuLocation = NSMakePoint(0, NSHeight([self frame]) + 4.0);
  // convert to window coords
  menuLocation = [self convertPoint:menuLocation toView:nil];

  NSEvent *newEvent = [NSEvent mouseEventWithType: [event type]
                     location: menuLocation
                modifierFlags: [event modifierFlags]
                    timestamp: [event timestamp]
                 windowNumber: [event windowNumber]
                      context: [event context]
                  eventNumber: [event eventNumber]
                   clickCount: [event clickCount]
                     pressure: [event pressure]];

  [[self cell] setHighlighted:YES];
  [NSMenu popUpContextMenu:[self menu] withEvent:newEvent forView:self];
  [[self cell] setHighlighted:NO];
}

@end
