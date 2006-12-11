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
 * Nick Kreeger
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Nick Kreeger <nick.kreeger@park.edu>
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

/*
This is a simple controller class that handles notifications to the user when
dealing with opening feeds from within Camino. 

If the user attempts to open a feed detected in Camino, this class checks for
available applications that are registered for "feed://". If there are no
applications found, display a sheet that warns the user and allows them to 
select a application. If there are feed applications found, run a sheet that 
allows the user to choose a new default feed viewer, and open or cancel the 
opening action.

Note that in 10.3.9, the version of Safari that is shipped registers itself as
an application that can handle feeds, but does not actually have any RSS support.
Due to this issue, we have to check the users OS version to make sure they are
not trying to open a feed with a useless RSS viewer.
*/

@interface FeedServiceController : NSObject 
{
  IBOutlet NSWindow*      mNotifyOpenExternalFeedApp;  // shown when there are RSS viewing apps
  IBOutlet NSWindow*      mNotifyNoFeedAppFound;       // shown when there is not a RSS viewing app registered
  
  IBOutlet NSPopUpButton* mFeedAppsPopUp;              // pop-up that contains the registered RSS viewer apps
  IBOutlet NSButton*      mSelectAppButton;            // button that allows user to select a RSS viewer app
  IBOutlet NSButton*      mDoNotShowDialogCheckbox;    // checkbox to not show the no feed apps found dialog
  
  NSString*               mFeedURI;                    // holds the URL of the feed when the dialog is run
}

+(FeedServiceController*)sharedFeedServiceController;

-(IBAction)cancelOpenFeed:(id)sender;  
-(IBAction)openFeed:(id)sender;          
-(IBAction)closeSheetAndContinue:(id)sender;    
-(IBAction)selectFeedAppFromMenuItem:(id)sender;
-(IBAction)selectFeedAppFromButton:(id)sender;

-(void)showFeedWillOpenDialog:(NSWindow*)parent feedURI:(NSString*)inFeedURI;  
-(BOOL)feedAppsExist;

@end
