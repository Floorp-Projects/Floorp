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
* The Initial Developer of the Original Code is Netscape
* Communications Corporation. Portions created by Netscape are
* Copyright (C) 2002 Netscape Communications Corporation. All
* Rights Reserved.
*
* Contributor(s):
*   Joe Hewitt <hewitt@netscape.com> (Original Author)
*/

#import <AppKit/AppKit.h>
#import "CHAutoCompleteDataSource.h"
#include "nsIAutoCompleteSession.h"
#include "nsIAutoCompleteResults.h"
#include "nsIAutoCompleteListener.h"

@class CHAutoCompleteDataSource, CHPageProxyIcon;

@interface CHAutoCompleteTextField : NSTextField
{
  IBOutlet CHPageProxyIcon *mProxyIcon;
  NSWindow *mPopupWin;
  NSTableView *mTableView;
  
  CHAutoCompleteDataSource *mDataSource;

  nsIAutoCompleteSession *mSession;
  nsIAutoCompleteResults *mResults;
  nsIAutoCompleteListener *mListener;

  NSString* mSearchString;
  BOOL mBackspaced;
  
  NSTimer *mOpenTimer;
}

- (void) setSession:(NSString *)aSession;
- (NSString *) session;

- (NSTableView *) tableView;
- (int) visibleRows;

- (void) startSearch:(NSString*)aString;
- (void) performSearch;
- (void) dataReady:(nsIAutoCompleteResults*)aResults status:(AutoCompleteStatus)aStatus;
- (void) searchTimer:(NSTimer *)aTimer;

- (void) completeDefaultResult;
- (void) completeResult:(int)aRow;
- (void) enterResult:(int)aRow;

- (void) selectRowAt:(int)aRow;
- (void) selectRowBy:(int)aRows;

- (void) openPopup;
- (void) closePopup;
- (void) resizePopup;
- (BOOL) isOpen;

- (void) onBlur:(id)sender;
- (void) onResize:(id)sender;

@end
