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
 
#import <Cocoa/Cocoa.h>

@interface nsAlertController : NSObject
{
  // all dialogs are now created on the fly and sized to fit
}

- (IBAction)hitButton1:(id)sender;
- (IBAction)hitButton2:(id)sender;
- (IBAction)hitButton3:(id)sender;

// 
// This is a version of [NSApp runModalForWindow:(relativeToWindow:)] that does three
// things:
// 
//  1. It verifies that inParentWindow is a valid window to show a sheet on
//     (i.e. that it's not nil, is visible, and doesn't already have a sheet).
//
//  2. It doesn't show inWindow as a sheet if there is already a modal (non-sheet)
//     dialog on the screen, because that fubars AppKit.
// 
//  3. It does some JS context stack magic that pushes a "native code" security principle
//     ("trust label") so that Gecko knows we're running native code, and not calling
//     from JS. This is important, because we can remain on the stack while PLEvents
//     are being handled in the sheet's event loop, and those PLEvents can cause
//     code to run that is sensitive to the security context. See bug 179307 for details.
// 
+ (int)safeRunModalForWindow:(NSWindow*)inWindow relativeToWindow:(NSWindow*)inParentWindow;

// 
// Nota Bene: all of these methods can throw Objective-C exceptions
// if there was an error displaying the dialog.
//
- (void)alert:(NSWindow*)parent title:(NSString*)title text:(NSString*)text;
- (void)alertCheck:(NSWindow*)parent title:(NSString*)title text:(NSString*)text checkMsg:(NSString*)checkMsg checkValue:(BOOL*)checkValue;

- (BOOL)confirm:(NSWindow*)parent title:(NSString*)title text:(NSString*)text;
- (BOOL)confirmCheck:(NSWindow*)parent title:(NSString*)title text:(NSString*)text checkMsg:(NSString*)checkMsg checkValue:(BOOL*)checkValue;

// these return NSAlertDefaultReturn, NSAlertAlternateReturn or NSAlertOtherReturn
- (int)confirmEx:(NSWindow*)parent title:(NSString*)title text:(NSString*)text
   button1:(NSString*)btn1 button2:(NSString*)btn2 button3:(NSString*)btn3;
- (int)confirmCheckEx:(NSWindow*)parent title:(NSString*)title text:(NSString*)text 
  button1:(NSString*)btn1 button2:(NSString*)btn2 button3:(NSString*)btn3
  checkMsg:(NSString*)checkMsg checkValue:(BOOL*)checkValue;

- (BOOL)prompt:(NSWindow*)parent title:(NSString*)title text:(NSString*)text promptText:(NSMutableString*)promptText checkMsg:(NSString*)checkMsg checkValue:(BOOL*)checkValue doCheck:(BOOL)doCheck;
- (BOOL)promptUserNameAndPassword:(NSWindow*)parent title:(NSString*)title text:(NSString*)text userNameText:(NSMutableString*)userNameText passwordText:(NSMutableString*)passwordText checkMsg:(NSString*)checkMsg checkValue:(BOOL*)checkValue doCheck:(BOOL)doCheck;
- (BOOL)promptPassword:(NSWindow*)parent title:(NSString*)title text:(NSString*)text passwordText:(NSMutableString*)passwordText checkMsg:(NSString*)checkMsg checkValue:(BOOL*)checkValue doCheck:(BOOL)doCheck;

- (BOOL)postToInsecureFromSecure:(NSWindow*)parent;

@end
