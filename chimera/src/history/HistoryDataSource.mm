//
//  CHHistoryDataSource.mm
//  Chimera
//
//  Created by Ben Goodger on Sun Apr 28 2002.
//  Copyright (c) 2001 __MyCompanyName__. All rights reserved.
//

#import "CHHistoryDataSource.h"

#include "nsIRDFService.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFResource.h"

#include "nsComponentManagerUtils.h"

@implementation CHHistoryDataSource

- (void) ensureDataSourceLoaded
{
    [super ensureDataSourceLoaded];
    
    // Get the Global History DataSource
    mRDFService->GetDataSource("rdf:history", &mDataSource);
    // Get the Date Folder Root
    mRDFService->GetResource("NC:HistoryByDate", &mRootResource);

    [mOutlineView setTarget: self];
    [mOutlineView reloadData];
}

@end
