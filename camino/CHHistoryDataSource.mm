//
//  CHHistoryDataSource.mm
//  Chimera
//
//  Created by Ben Goodger on Sun Apr 28 2002.
//  Copyright (c) 2001 __MyCompanyName__. All rights reserved.
//

#import "NSString+Utils.h"

#import "BrowserWindowController.h"
#import "CHHistoryDataSource.h"
#import "CHBrowserView.h"

#include "nsIRDFService.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFResource.h"

#include "nsXPIDLString.h"
#include "nsString.h"

#include "nsComponentManagerUtils.h"

@implementation CHHistoryDataSource

- (void) ensureDataSourceLoaded
{
    [super ensureDataSourceLoaded];
    
    if (!mDataSource)
    {
      // Get the Global History DataSource
      mRDFService->GetDataSource("rdf:history", &mDataSource);
      // Get the Date Folder Root
      mRDFService->GetResource("NC:HistoryByDate", &mRootResource);
  
      [mOutlineView setTarget: self];
      [mOutlineView setDoubleAction: @selector(openHistoryItem:)];
  
      [mOutlineView reloadData];
    }
}

- (id) outlineView: (NSOutlineView*) aOutlineView objectValueForTableColumn: (NSTableColumn*) aTableColumn
                                                  byItem: (id) aItem
{
    NSMutableAttributedString *cellValue = [[NSMutableAttributedString alloc] init];

    if (!mDataSource || !aItem)
        return nil;
    
    // The table column's identifier is the RDF Resource URI of the property being displayed in
    // that column, e.g. "http://home.netscape.com/NC-rdf#Name"
    NSString* columnPropertyURI = [aTableColumn identifier];
    
    nsCOMPtr<nsIRDFResource> propertyResource;
    mRDFService->GetResource([columnPropertyURI cString], getter_AddRefs(propertyResource));
            
    nsCOMPtr<nsIRDFResource> resource = dont_AddRef([aItem resource]);
            
    nsCOMPtr<nsIRDFNode> valueNode;
    mDataSource->GetTarget(resource, propertyResource, PR_TRUE, getter_AddRefs(valueNode));
    if (!valueNode) {
#if DEBUG
        NSLog(@"ValueNode is null in objectValueForTableColumn");
#endif
        return nil;
    }
    
    nsCOMPtr<nsIRDFLiteral> valueLiteral(do_QueryInterface(valueNode));
    if (!valueLiteral)
        return nil;
    
    nsXPIDLString literalValue;
    valueLiteral->GetValue(getter_Copies(literalValue));

    //Set cell's textual contents
    [cellValue replaceCharactersInRange:NSMakeRange(0, [cellValue length]) withString:[NSString stringWith_nsAString:literalValue]];
    
    if ([columnPropertyURI isEqualToString: @"http://home.netscape.com/NC-rdf#Name"])
    {
      NSMutableAttributedString *attachmentAttrString = nil;
      NSFileWrapper *fileWrapper = [[NSFileWrapper alloc] initRegularFileWithContents:nil];
      NSTextAttachment *textAttachment = [[NSTextAttachment alloc] initWithFileWrapper:fileWrapper];
      NSCell *attachmentAttrStringCell;

      //Create an attributed string to hold the empty attachment, then release the components.
      attachmentAttrString = [[NSMutableAttributedString attributedStringWithAttachment:textAttachment] retain];
      [textAttachment release];
      [fileWrapper release];
  
      //Get the cell of the text attachment.
      attachmentAttrStringCell = (NSCell *)[(NSTextAttachment *)[attachmentAttrString attribute:
                NSAttachmentAttributeName atIndex:0 effectiveRange:nil] attachmentCell];
  
      if ([self outlineView:mOutlineView isItemExpandable:aItem]) {
        [attachmentAttrStringCell setImage:[NSImage imageNamed:@"folder"]];
      }
      else
        [attachmentAttrStringCell setImage:[NSImage imageNamed:@"smallbookmark"]];
  
      //Insert the image
      [cellValue replaceCharactersInRange:NSMakeRange(0, 0) withAttributedString:attachmentAttrString];

      //Tweak the baseline to vertically center the text.
      [cellValue addAttribute:NSBaselineOffsetAttributeName
                        value:[NSNumber numberWithFloat:-5.0]
                        range:NSMakeRange(0, 1)];
    }
    
    return cellValue;
}

-(IBAction)openHistoryItem: (id)aSender
{
    int index = [mOutlineView selectedRow];
    if (index == -1)
        return;
    
    id item = [mOutlineView itemAtRow: index];
    if (!item)
        return;
    
    // expand if collapsed and double click
    if ([mOutlineView isExpandable: item]) {
      if ([mOutlineView isItemExpanded: item])
        [mOutlineView collapseItem: item];
      else
        [mOutlineView expandItem: item];
        
      return;
    }
    
    // get uri
    nsCOMPtr<nsIRDFResource> propertyResource;
    mRDFService->GetResource("http://home.netscape.com/NC-rdf#URL", getter_AddRefs(propertyResource));
    
    nsCOMPtr<nsIRDFResource> resource = dont_AddRef([item resource]);
            
    nsCOMPtr<nsIRDFNode> valueNode;
    mDataSource->GetTarget(resource, propertyResource, PR_TRUE, getter_AddRefs(valueNode));
    if (!valueNode) {
#if DEBUG
        NSLog(@"ValueNode is null in openHistoryItem");
#endif
        return;
    }
    
    nsCOMPtr<nsIRDFLiteral> valueLiteral(do_QueryInterface(valueNode));
    if (!valueLiteral)
        return;
    
    nsXPIDLString literalValue;
    valueLiteral->GetValue(getter_Copies(literalValue));

    NSString* url = [NSString stringWith_nsAString: literalValue];
    [[[mBrowserWindowController getBrowserWrapper] getBrowserView] loadURI: url referrer: nil flags: NSLoadFlagsNone];
    // Focus and activate our content area.
    [[[mBrowserWindowController getBrowserWrapper] getBrowserView] setActive: YES];
}

@end
