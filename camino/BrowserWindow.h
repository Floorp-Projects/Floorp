/* BrowserWindow */

#import <Cocoa/Cocoa.h>

@interface BrowserWindow : NSWindow
{
  IBOutlet id mAutoCompleteTextField;
}

-(BOOL) makeFirstResponder:(NSResponder*) responder;
@end
