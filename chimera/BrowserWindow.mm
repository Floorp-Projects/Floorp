#import "BrowserWindow.h"
#import "CHAutoCompleteTextField.h"
#import "BrowserWindowController.h"

static const int kEscapeKeyCode = 53;

@implementation BrowserWindow

- (void)sendEvent:(NSEvent *)theEvent
{
  // We need this hack because NSWindow::sendEvent will eat the escape key
  // and won't pass it down to the key handler of responders in the window.
  // We have to override sendEvent for all of our escape key needs.
  if ([theEvent keyCode] == kEscapeKeyCode && [theEvent type] == NSKeyDown) {
    NSText *fieldEditor = [self fieldEditor:NO forObject:mAutoCompleteTextField];
    if (fieldEditor && [self firstResponder] == fieldEditor) {
      [mAutoCompleteTextField revertText];
    }
  } else
    [super sendEvent:theEvent];
}

- (BOOL)makeFirstResponder:(NSResponder*)responder
{
  NSResponder* oldResponder = [self firstResponder];
  BOOL madeFirstResponder = [super makeFirstResponder:responder];
  if (madeFirstResponder && oldResponder != [self firstResponder])
    [(BrowserWindowController*)[self delegate] focusChangedFrom:oldResponder to:[self firstResponder]];
  return madeFirstResponder;
}

@end
