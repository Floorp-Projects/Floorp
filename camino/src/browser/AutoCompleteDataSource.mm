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
*   
*/

#import "NSString+Utils.h"

#import <AppKit/AppKit.h>
#import "AutoCompleteTextField.h"
#include "nsString.h"
#include "nsCRT.h"

@implementation AutoCompleteDataSource

-(id)init
{
  if ((self = [super init])) {
    mResults = nil;
    mIconImage = [NSImage imageNamed:@"globe_ico"]; 
  }
  return self;
}

-(void)dealloc
{
  NS_IF_RELEASE(mResults);
  [super dealloc];
}

- (void) setErrorMessage: (NSString*) error
{
  [self setResults:nsnull];
  mErrorMessage = error;
}

- (NSString*) errorMessage
{
    return mErrorMessage;
}

- (void) setResults:(nsIAutoCompleteResults*)aResults
{
  NS_IF_RELEASE(mResults);
  
  mErrorMessage = nil;
  mResults = aResults;
  NS_IF_ADDREF(mResults);
}

- (nsIAutoCompleteResults *) results
{
  return mResults;
}

- (int) rowCount
{
  if (!mResults)
    return 0;
    
  nsCOMPtr<nsISupportsArray> items;
  mResults->GetItems(getter_AddRefs(items));
  PRUint32 count;
  items->Count(&count);

  return count;
}

- (id) resultString:(int)aRow column:(NSString *)aColumn
{
  NSString *result = @"";
  
  if (!mResults)
    return result;

  nsCOMPtr<nsISupportsArray> items;
  mResults->GetItems(getter_AddRefs(items));
  
  nsCOMPtr<nsISupports> itemSupports = dont_AddRef(items->ElementAt(aRow));
  nsCOMPtr<nsIAutoCompleteItem> item = do_QueryInterface(itemSupports);
  if (!item)
    return result;

  if ([aColumn isEqualToString:@"icon"]) {
    return mIconImage;
  } else if ([aColumn isEqualToString:@"col1"]) {
    nsAutoString value;
    item->GetValue(value);
    result = [NSString stringWith_nsAString:value];
  } else if ([aColumn isEqualToString:@"col2"]) {
    nsXPIDLString commentStr;
    item->GetComment(getter_Copies(commentStr));
    result = [NSString stringWith_nsAString:commentStr];
  }

  return result;
}

-(int) numberOfRowsInTableView:(NSTableView*)aTableView
{
  return [self rowCount];
}

-(id)tableView:(NSTableView*)aTableView objectValueForTableColumn:(NSTableColumn*)aTableColumn row:(int)aRowIndex
{
  return [self resultString:aRowIndex column:[aTableColumn identifier]];
}

@end
