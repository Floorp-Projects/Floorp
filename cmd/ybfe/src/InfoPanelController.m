// InfoPanelController.m

#import "InfoPanelController.h"

@implementation InfoPanelController

- (id)init {
    return [super initWithWindowNibName:@"InfoPanel"];
}

- (IBAction)showWindow:(id)sender {
    [[self window] center];
    [super showWindow:sender];
}

@end
