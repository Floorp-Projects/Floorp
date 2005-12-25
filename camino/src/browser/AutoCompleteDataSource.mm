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
 *   David Hyatt <hyatt@mozilla.org> (Original Author)
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

#import "NSString+Utils.h"

#import <AppKit/AppKit.h>
#import "AutoCompleteDataSource.h"

#include "nsString.h"
#include "nsCRT.h"
#include "nsIAutoCompleteResults.h"


@implementation AutoCompleteDataSource

-(id)init
{
  if ((self = [super init])) {
    mResults = nil;
    mIconImage = [[NSImage imageNamed:@"globe_ico"] retain];
  }
  return self;
}

-(void)dealloc
{
  NS_IF_RELEASE(mResults);
  [mIconImage release];
  [mErrorMessage release];
  [super dealloc];
}

- (void) setErrorMessage: (NSString*) error
{
  [self setResults:nsnull]; // releases mErrorMessage

  mErrorMessage = [error retain];
}

- (NSString*) errorMessage
{
  return mErrorMessage;
}

- (void) setResults:(nsIAutoCompleteResults*)aResults
{
  NS_IF_RELEASE(mResults);
  
  [mErrorMessage release];
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
  NSString* result = @"";
  
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
