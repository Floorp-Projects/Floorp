#import "BrowserWindow.h"
#import "BrowserWindowController.h"

@implementation BrowserWindow

- (BOOL)makeFirstResponder:(NSResponder*)responder
{
  NSResponder* oldResponder = [self firstResponder];
  BOOL madeFirstResponder = [super makeFirstResponder:responder];
  if (madeFirstResponder && oldResponder != [self firstResponder])
    [(BrowserWindowController*)[self delegate] focusChangedFrom:oldResponder to:[self firstResponder]];
  return madeFirstResponder;
}

@end
