/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
* Version: NPL 1.1/GPL 2.0/LGPL 2.1
*
* The contents of this file are subject to the Netscape Public License
* Version 1.1 (the "License"); you may not use this file except in
* compliance with the License. You may obtain a copy of the License at
* http://www.mozilla.org/NPL/
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
*    David Haas <haasd@cae.wisc.edu>
*
*
* Alternatively, the contents of this file may be used under the terms of
* either the GNU General Public License Version 2 or later (the "GPL"), or
* the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
* in which case the provisions of the GPL or the LGPL are applicable instead
* of those above. If you wish to allow use of your version of this file only
* under the terms of either the GPL or the LGPL, and not to allow others to
* use your version of this file under the terms of the NPL, indicate your
* decision by deleting the provisions above and replace them with the notice
* and other provisions required by the GPL or the LGPL. If you do not delete
* the provisions above, a recipient may use your version of this file under
* the terms of any one of the NPL, the GPL or the LGPL.
*
* ***** END LICENSE BLOCK ***** */

#import "BookmarkItem.h"

//Status Flags
#define kBookmarkOKStatus 0
#define kBookmarkBrokenLinkStatus 1
#define kBookmarkMovedLinkStatus 2
#define kBookmarkServerErrorStatus 3
#define kBookmarkNeverCheckStatus 5
#define kBookmarkSpacerStatus 9

@interface Bookmark : BookmarkItem  //DBME
{
  NSString* mURL; //DBMu
  NSDate* mLastVisit; //DBMv
  NSNumber* mStatus;
  NSNumber* mNumberOfVisits;
}

-(NSString *) url; 
-(NSDate *) lastVisit; 
-(unsigned) numberOfVisits;
-(unsigned)  status;
-(BOOL) isMoved;
-(BOOL) isCheckable;
-(BOOL) isSeparator;
-(BOOL) isSick;     //DHIs

-(void) setUrl:(NSString *)aURL; 
-(void) setLastVisit:(NSDate *)aLastVisit;
-(void) setStatus:(unsigned)aStatus;
-(void) setIsSeparator:(BOOL)aSeparatorFlag;
-(void) setNumberOfVisits:(unsigned)aNumber;


// functions aiding in checking for updates
-(NSURL *)urlAsURL;
-(void) checkForUpdate;

@end

