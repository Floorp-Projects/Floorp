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
*   David Hyatt <hyatt@mozilla.org> (Original Author)
*/

#import <AppKit/AppKit.h>
#import "AutoCompleteTextField.h"
#include "nsIAutoCompleteResults.h"

@class AutoCompleteTextField;

@interface AutoCompleteDataSource : NSObject
{
  NSImage *mIconImage;
  
  NSString* mErrorMessage;
  nsIAutoCompleteResults *mResults;
}

- (id) init;

- (int) rowCount;
- (id) resultString:(int)aRow column:(NSString *)aColumn;

- (void) setErrorMessage: (NSString*) error;
- (NSString*) errorMessage;

- (void) setResults: (nsIAutoCompleteResults*) results;
- (nsIAutoCompleteResults*) results;

@end
