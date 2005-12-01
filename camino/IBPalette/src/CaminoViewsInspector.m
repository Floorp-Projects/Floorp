//
//  CaminoViewsInspector.m
//  CaminoViewsPalette
//
//  Created by Simon Fraser on 21/11/05.
//  Copyright __MyCompanyName__ 2005 . All rights reserved.
//

#import "CaminoViewsInspector.h"
#import "CaminoViewsPalette.h"

@implementation CaminoViewsInspector

- (id)init
{
    self = [super init];
    [NSBundle loadNibNamed:@"CaminoViewsInspector" owner:self];
    return self;
}

- (void)ok:(id)sender
{
	/* Your code Here */
    [super ok:sender];
}

- (void)revert:(id)sender
{
	/* Your code Here */
    [super revert:sender];
}

@end
