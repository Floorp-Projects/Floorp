//
//  HistoryDataSource.h
//  Chimera
//
//  Created by Ben Goodger on Sun Apr 28 2002.
//  Copyright (c) 2001 __MyCompanyName__. All rights reserved.
//

#import <Foundation/Foundation.h>

#import "RDFOutlineViewDataSource.h"

@class BrowserWindowController;

@interface HistoryDataSource : RDFOutlineViewDataSource
{

  IBOutlet BrowserWindowController* mBrowserWindowController;

}

-(IBAction)openHistoryItem: (id)aSender;

@end
