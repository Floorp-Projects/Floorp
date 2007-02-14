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
 
#import "SecurityPane.h"

#include "nsServiceManagerUtils.h"
#include "nsIPref.h"

// prefs for showing security dialogs
#define LEAVE_SITE_PREF      "security.warn_leaving_secure"
#define MIXEDCONTENT_PREF    "security.warn_viewing_mixed"

const unsigned int kSelectAutomaticallyMatrixRowValue = 0;
const unsigned int kAskEveryTimeMatrixRowValue        = 1;

@implementation OrgMozillaChimeraPreferenceSecurity

- (void)dealloc
{
  [super dealloc];
}

- (void)updateButtons
{
  // Set initial value on Security checkboxes
  PRBool leaveEncrypted = PR_TRUE;
  mPrefService->GetBoolPref(LEAVE_SITE_PREF, &leaveEncrypted);
  [mLeaveEncrypted setState:(leaveEncrypted ? NSOnState : NSOffState)];

  PRBool viewMixed = PR_TRUE;
  mPrefService->GetBoolPref(MIXEDCONTENT_PREF, &viewMixed);
  [mViewMixed setState:(viewMixed ? NSOnState : NSOffState)];

  BOOL gotPref;
  NSString* certificateBehavior = [self getStringPref:"security.default_personal_cert" withSuccess:&gotPref];
  if (gotPref) {
    if ([certificateBehavior isEqual:@"Select Automatically"])
      [mCertificateBehavior selectCellAtRow:kSelectAutomaticallyMatrixRowValue column:0];
    else if ([certificateBehavior isEqual:@"Ask Every Time"])
      [mCertificateBehavior selectCellAtRow:kAskEveryTimeMatrixRowValue column:0];
  }

}


- (void)mainViewDidLoad
{
  if ( !mPrefService )
    return;

  [self updateButtons];
}

- (void)didActivate
{
  [self updateButtons];
}

//
// clickEnableViewMixed:
// clickEnableLeaveEncrypted:
//
// Set prefs for warnings/alerts wrt secure sites
//

- (IBAction)clickEnableViewMixed:(id)sender
{
  [self setPref:MIXEDCONTENT_PREF toBoolean:[sender state] == NSOnState];
}

- (IBAction)clickEnableLeaveEncrypted:(id)sender
{
  [self setPref:LEAVE_SITE_PREF toBoolean:[sender state] == NSOnState];
}

- (IBAction)clickCertificateSelectionBehavior:(id)sender
{
  unsigned int row = [mCertificateBehavior selectedRow];

  if (row == kSelectAutomaticallyMatrixRowValue)
    [self setPref:"security.default_personal_cert" toString:@"Select Automatically"];
  else // row == kAskEveryTimeMatrixRowValue
    [self setPref:"security.default_personal_cert" toString:@"Ask Every Time"];
}

- (IBAction)showCertificates:(id)sender
{
  // we'll just fire off a notification and let the application show the UI
  NSDictionary* userInfoDict = [NSDictionary dictionaryWithObject:[mLeaveEncrypted window]  // any view's window
                                                           forKey:@"parent_window"];
  [[NSNotificationCenter defaultCenter] postNotificationName:@"ShowCertificatesNotification"
                                                      object:nil
                                                    userInfo:userInfoDict];
}

@end
