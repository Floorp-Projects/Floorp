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
*   David Hyatt <hyatt@netscape.com> (Original Author)
*/

#import "CHAutoCompleteTableView.h"
#import "CHAutoCompleteDataSource.h"

@implementation CHAutoCompleteTableView

-(id)initWithFrame:(NSRect)aRect
{
  if ((self = [super initWithFrame: aRect])) {
    // Create our data source.
    CHAutoCompleteDataSource* ds = [[[CHAutoCompleteDataSource alloc] init] autorelease];
    [self setDataSource: ds];

    // Create the URL column.
    NSTableColumn* urlColumn = [[[NSTableColumn alloc] initWithIdentifier:@"URL"] autorelease];
    [self addTableColumn: urlColumn];
    NSTableColumn* titleColumn = [[[NSTableColumn alloc] initWithIdentifier:@"Title"] autorelease];
    [self addTableColumn: titleColumn];
  }
  return self;
}

-(void)controlTextDidChange:(NSNotification*)aNotification
{
}

-(void)controlTextDidEndEditing:(NSNotification*)aNotification
{
  printf("Editing finished.\n");
}

@end
