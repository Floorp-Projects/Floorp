// Document.m

#import "Document.h"

@implementation Document

// -----------------------------------------------------------------------------
// Override returning the nib file name of the document

- (NSString *)windowNibName {
    // If you need to use a subclass of NSWindowController or if your document supports multiple NSWindowControllers, you should remove this method and override -makeWindowControllers instead.
    return @"Mozilla Window";
}

// -----------------------------------------------------------------------------
// Override

- (void)windowControllerDidLoadNib:(NSWindowController *) aController{
    [super windowControllerDidLoadNib:aController];
    // Add any code here that need to be executed once the windowController has loaded the document's window.
}

// -----------------------------------------------------------------------------
// Override

- (NSData *)dataRepresentationOfType:(NSString *)aType {
    return nil;
}

// -----------------------------------------------------------------------------
// Override

- (BOOL)loadDataRepresentation:(NSData *)data ofType:(NSString *)aType {
    return YES;
}

@end
