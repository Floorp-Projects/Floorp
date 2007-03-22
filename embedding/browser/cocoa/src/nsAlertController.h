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
    IBOutlet id alertCheckPanel;
    IBOutlet id alertCheckPanelCheck;
    IBOutlet id alertCheckPanelText;
    IBOutlet id alertPanel;
    IBOutlet id alertPanelText;
    IBOutlet id confirmCheckPanel;
    IBOutlet id confirmCheckPanelCheck;
    IBOutlet id confirmCheckPanelText;
    IBOutlet id confirmCheckPanelButton1;
    IBOutlet id confirmCheckPanelButton2;
    IBOutlet id confirmCheckPanelButton3;
    IBOutlet id confirmPanel;
    IBOutlet id confirmPanelText;
    IBOutlet id confirmPanelButton1;
    IBOutlet id confirmPanelButton2;
    IBOutlet id confirmPanelButton3;
    IBOutlet id confirmStorePasswordPanel;
    IBOutlet id promptPanel;
    IBOutlet id promptPanelCheck;
    IBOutlet id promptPanelText;
    IBOutlet id promptPanelInput;
    IBOutlet id passwordPanel;
    IBOutlet id passwordPanelCheck;
    IBOutlet id passwordPanelText;
    IBOutlet id passwordPanelInput;
    IBOutlet id postToInsecureFromSecurePanel;
    IBOutlet id securityMismatchPanel;
    IBOutlet id expiredCertPanel;
    IBOutlet id securityUnknownPanel;
    IBOutlet id usernamePanel;
    IBOutlet id usernamePanelCheck;
    IBOutlet id usernamePanelText;
    IBOutlet id usernamePanelPassword;    
    IBOutlet id usernamePanelUserName;    
    IBOutlet id owner;
}
- (IBAction)hitButton1:(id)sender;
- (IBAction)hitButton2:(id)sender;
- (IBAction)hitButton3:(id)sender;

- (void)awakeFromNib;
- (void)alert:(NSWindow*)parent title:(NSString*)title text:(NSString*)text;
- (void)alertCheck:(NSWindow*)parent title:(NSString*)title text:(NSString*)text checkMsg:(NSString*)checkMsg checkValue:(BOOL*)checkValue;

- (BOOL)confirm:(NSWindow*)parent title:(NSString*)title text:(NSString*)text;
- (BOOL)confirmCheck:(NSWindow*)parent title:(NSString*)title text:(NSString*)text checkMsg:(NSString*)checkMsg checkValue:(BOOL*)checkValue;

- (int)confirmEx:(NSWindow*)parent title:(NSString*)title text:(NSString*)text
   button1:(NSString*)btn1 button2:(NSString*)btn2 button3:(NSString*)btn3;
- (int)confirmCheckEx:(NSWindow*)parent title:(NSString*)title text:(NSString*)text 
  button1:(NSString*)btn1 button2:(NSString*)btn2 button3:(NSString*)btn3
  checkMsg:(NSString*)checkMsg checkValue:(BOOL*)checkValue;
- (BOOL)confirmStorePassword:(NSWindow*)parent;

- (BOOL)prompt:(NSWindow*)parent title:(NSString*)title text:(NSString*)text promptText:(NSMutableString*)promptText checkMsg:(NSString*)checkMsg checkValue:(BOOL*)checkValue doCheck:(BOOL)doCheck;
- (BOOL)promptUserNameAndPassword:(NSWindow*)parent title:(NSString*)title text:(NSString*)text userNameText:(NSMutableString*)userNameText passwordText:(NSMutableString*)passwordText checkMsg:(NSString*)checkMsg checkValue:(BOOL*)checkValue doCheck:(BOOL)doCheck;
- (BOOL)promptPassword:(NSWindow*)parent title:(NSString*)title text:(NSString*)text passwordText:(NSMutableString*)passwordText checkMsg:(NSString*)checkMsg checkValue:(BOOL*)checkValue doCheck:(BOOL)doCheck;

#if PROBABLY_DONT_WANT_THIS
- (BOOL)postToInsecureFromSecure:(NSWindow*)parent;

- (BOOL)badCert:(NSWindow*)parent;
- (BOOL)expiredCert:(NSWindow*)parent;
- (int)unknownCert:(NSWindow*)parent;
#endif

@end
