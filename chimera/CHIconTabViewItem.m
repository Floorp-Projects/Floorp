//
//  IconTabViewItem.m
//  Chimera
//
//  Created by Matt L.  Judy on Sun Mar 10 2002.
//  Copyright (c) 2001 __MyCompanyName__. All rights reserved.
//

#import "CHIconTabViewItem.h"

@implementation CHIconTabViewItem

-(id)initWithIdentifier:(id)identifier withTabIcon:(NSImage *)tabIcon
{
    if ( (self = [super initWithIdentifier:identifier]) ) {
        [self setTabIcon:tabIcon];
    }
    return self;
}

- (NSSize)sizeOfLabel:(BOOL)computeMin
{
    return( NSMakeSize(15,15) );
}

-(void)drawLabel:(BOOL)shouldTruncateLabel inRect:(NSRect)tabRect
{
    NSPoint	drawPoint = NSMakePoint( (tabRect.origin.x), (tabRect.origin.y + 15) );
    [[self tabIcon] compositeToPoint:drawPoint operation:NSCompositeSourceOver];    
}

-(NSImage *)tabIcon { return _tabIcon; }
-(void)setTabIcon:(NSImage *)newIcon
{
    [_tabIcon autorelease];
    _tabIcon = [newIcon copy];
}


@end
