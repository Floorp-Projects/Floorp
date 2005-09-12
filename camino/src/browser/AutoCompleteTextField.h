/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com> (Original Author)
 *   David Haas <haasd@cae.wisc.edu>
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

#import <AppKit/AppKit.h>

#import "AutoCompleteDataSource.h"

class nsIAutoCompleteSession;
class nsIAutoCompleteResults;
class nsIAutoCompleteListener;

@class PageProxyIcon;

@interface AutoCompleteTextField : NSTextField
{
  IBOutlet PageProxyIcon*   mProxyIcon;

  NSWindow*                 mPopupWin;
  NSTableView*              mTableView;
  
  NSImageView*              mLock;                  // STRONG, lock that shows when a page is secure, hidden otherwise
  NSColor*                  mSecureBackgroundColor; // STRONG, yellow color for bg when on a secure page, cached for perf
  
  AutoCompleteDataSource*   mDataSource;
  NSString*                 mSearchString;
  NSTimer*                  mOpenTimer;

  nsIAutoCompleteSession*   mSession;   // owned
  nsIAutoCompleteResults*   mResults;   // owned
  nsIAutoCompleteListener*  mListener;  // owned
  
  // used to remember if backspace was pressed in complete: so we can check this in controlTextDidChange
  BOOL mBackspaced;
  // determines if the search currently pending should complete the default result when it is ready
  BOOL mCompleteResult;
  // should the autocomplete fill in the default completion into the text field? The default
  // is no, but this can be set with a user default pref.
  BOOL mCompleteWhileTyping;
}

// get/set the autcomplete session
- (void) setSession:(NSString *)aSession;
- (NSString *) session;

- (void)setPageProxyIcon:(NSImage *)aImage;
- (void)setURI:(NSString*)aURI;

- (BOOL)userHasTyped;
- (void)clearResults;
- (void)revertText;

- (id)fieldEditor;

// Changes the display of the text field to indicate whether the page
// is secure or not.
- (void)setSecureState:(unsigned char)inState;

@end
