#import "MyBrowserView.h"

#define DOCUMENT_DONE_STRING @"Document: Done"
#define LOADING_STRING @"Loading..."

@implementation MyBrowserView

- (IBAction)load:(id)sender
{
  NSString* str = [urlbar stringValue];
  [browserView loadURI:str referrer:nil flags:NSLoadFlagsNone];
}

- (void)awakeFromNib 
{
  NSRect bounds = [self bounds];
  browserView = [[CHBrowserView alloc] initWithFrame:bounds];
  [self addSubview:browserView];
  [browserView setContainer:self];
  [browserView addListener:self];

  defaultStatus = NULL;
  loadingStatus = DOCUMENT_DONE_STRING;
  [status setStringValue:loadingStatus];

  [progress retain];
  [progress removeFromSuperview];
}

- (void)setFrame:(NSRect)frameRect
{
  [super setFrame:frameRect];
  NSRect bounds = [self bounds];
  [browserView setFrame:bounds];
}

- (void)onLoadingStarted 
{
  if (defaultStatus) {
    [defaultStatus release];
    defaultStatus = NULL;
  }

  [progressSuper addSubview:progress];
  [progress release];
  [progress setIndeterminate:YES];
  [progress startAnimation:self];

  loadingStatus = LOADING_STRING;
  [status setStringValue:loadingStatus];

#ifdef DEBUG_vidur
  printf("Starting to load\n");
#endif
}

- (void)onLoadingCompleted:(BOOL)succeeded
{
  [progress setIndeterminate:YES];
  [progress stopAnimation:self];
  [progress retain];
  [progress removeFromSuperview];

  loadingStatus = DOCUMENT_DONE_STRING;
  if (defaultStatus) {
    [status setStringValue:defaultStatus];
  }
  else {
    [status setStringValue:loadingStatus];
  }

  [browserView setActive:YES];
#ifdef DEBUG_vidur
  printf("Loading completed\n");
#endif
}

- (void)onProgressChange:(int)currentBytes outOf:(int)maxBytes 
{
  if (maxBytes > 0) {
    BOOL isIndeterminate = [progress isIndeterminate];
    if (isIndeterminate) {
      [progress setIndeterminate:NO];
    }
    double val = ((double)currentBytes / (double)maxBytes) * 100.0;
    [progress setDoubleValue:val];
#ifdef DEBUG_vidur
    printf("Progress notification: %f%%\n", val);
#endif
  }
}

- (void)onLocationChange:(NSString*)url 
{
  [urlbar setStringValue:url];
  
#ifdef DEBUG_vidur
  const char* str = [spec cString];
  printf("Location changed to: %s\n", str);
#endif
}

- (void)setStatus:(NSString *)statusString ofType:(NSStatusType)type 
{
  if (type == NSStatusTypeScriptDefault) {
    if (defaultStatus) {
      [defaultStatus release];
    }
    defaultStatus = statusString;
    if (defaultStatus) {
      [defaultStatus retain];
    }
  }
  else if (!statusString) {
    if (defaultStatus) {
      [status setStringValue:defaultStatus];
    }
    else {
      [status setStringValue:loadingStatus];
    }      
  }
  else {
    [status setStringValue:statusString];
  }
}

- (void)onStatusChange:(NSString*)aMessage
{
  NSLog(@"Status is %@", aMessage);
}

- (void)onSecurityStateChange:(unsigned long)newState
{
}

- (void)onShowContextMenu:(int)flags domEvent:(nsIDOMEvent*)aEvent domNode:(nsIDOMNode*)aNode
{
  NSLog(@"Showing context menu");
}

- (void)onShowTooltip:(NSPoint)where withText:(NSString*)text
{
  NSLog(@"Showing tooltip %@", text);
}

- (void)onHideTooltip
{
}

- (NSString *)title 
{
  NSWindow* window = [self window];
  NSString* str = [window title];
  return str;
}

- (void)setTitle:(NSString *)title
{
  NSWindow* window = [self window];
  [window setTitle:title];
}

- (void)sizeBrowserTo:(NSSize)dimensions
{
  NSRect bounds = [self bounds];
  float dx = dimensions.width - bounds.size.width;
  float dy = dimensions.height - bounds.size.height;

  NSWindow* window = [self window];
  NSRect frame = [window frame];
  frame.size.width += dx;
  frame.size.height += dy;

  [window setFrame:frame display:YES];
}

- (CHBrowserView*)createBrowserWindow:(unsigned int)mask
{
  // XXX not implemented 
  return NULL;
}

- (NSMenu*)getContextMenu
{
  return nil;
}

- (NSWindow*)getNativeWindow
{
  return [self window];
}

- (BOOL)shouldAcceptDragFromSource:(id)dragSource
{
  return NO;
}

@end
