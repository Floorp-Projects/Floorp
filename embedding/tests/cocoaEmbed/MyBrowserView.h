#import <Cocoa/Cocoa.h>
#import "CHBrowserView.h"

@interface MyBrowserView : NSView <CHBrowserListener, CHBrowserContainer>
{
    IBOutlet id urlbar;
    IBOutlet id status;
    IBOutlet id progress;
    IBOutlet id progressSuper;
    CHBrowserView* browserView;
    NSString* defaultStatus;
    NSString* loadingStatus;
}
- (IBAction)load:(id)sender;
- (void)awakeFromNib;
- (void)setFrame:(NSRect)frameRect;

// CHBrowserListener messages
- (void)onLoadingStarted;
- (void)onLoadingCompleted:(BOOL)succeeded;
- (void)onProgressChange:(int)currentBytes outOf:(int)maxBytes;
- (void)onLocationChange:(NSString*)url;
- (void)onStatusChange:(NSString*)aMessage;
- (void)onSecurityStateChange:(unsigned long)newState;
// Called when a context menu should be shown.
- (void)onShowContextMenu:(int)flags domEvent:(nsIDOMEvent*)aEvent domNode:(nsIDOMNode*)aNode;
// Called when a tooltip should be shown or hidden
- (void)onShowTooltip:(NSPoint)where withText:(NSString*)text;
- (void)onHideTooltip;

// CHBrowserContainer messages
- (void)setStatus:(NSString *)statusString ofType:(NSStatusType)type;
- (NSString *)title;
- (void)setTitle:(NSString *)title;
- (void)sizeBrowserTo:(NSSize)dimensions;
- (CHBrowserView*)createBrowserWindow:(unsigned int)mask;
- (NSMenu*)getContextMenu;
- (NSWindow*)getNativeWindow;
- (BOOL)shouldAcceptDragFromSource:(id)dragSource;

@end
