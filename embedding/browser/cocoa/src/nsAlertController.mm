#import "nsAlertController.h"
#import "nsCocoaBrowserService.h"

@implementation nsAlertController

- (IBAction)hitButton1:(id)sender
{
  [NSApp stopModalWithCode:1];
}

- (IBAction)hitButton2:(id)sender
{
  [NSApp stopModalWithCode:2];
}

- (IBAction)hitButton3:(id)sender
{
  [NSApp stopModalWithCode:3];
}


- (void)awakeFromNib
{
  nsCocoaBrowserService::SetAlertController(self);
}

- (void)alert:(NSWindow*)parent title:(NSString*)title text:(NSString*)text
{
  [alertPanelText setStringValue:text];
  [alertPanel setTitle:title];

  [NSApp runModalForWindow:alertPanel relativeToWindow:parent];
  
  [alertPanel close];
}

- (void)alertCheck:(NSWindow*)parent title:(NSString*)title text:(NSString*)text checkMsg:(NSString*)checkMsg checkValue:(BOOL*)checkValue
{
  [alertCheckPanelText setStringValue:text];
  [alertCheckPanel setTitle:title];
  int state = (*checkValue ? NSOnState : NSOffState);
  [alertCheckPanelCheck setState:state];
  [alertCheckPanelCheck setTitle:checkMsg];

  [NSApp runModalForWindow:alertCheckPanel relativeToWindow:parent];

  *checkValue = ([alertCheckPanelCheck state] == NSOnState);
  [alertCheckPanel close];
}

- (BOOL)confirm:(NSWindow*)parent title:(NSString*)title text:(NSString*)text
{
  [confirmPanelText setStringValue:text];
  [confirmPanel setTitle:title];

  int result = [NSApp runModalForWindow:confirmPanel relativeToWindow:parent];
  
  [confirmPanel close];

  return (result == 1);
}

- (BOOL)confirmCheck:(NSWindow*)parent title:(NSString*)title text:(NSString*)text checkMsg:(NSString*)checkMsg checkValue:(BOOL*)checkValue
{
  [confirmCheckPanelText setStringValue:text];
  [confirmCheckPanel setTitle:title];
  int state = (*checkValue ? NSOnState : NSOffState);
  [confirmCheckPanelCheck setState:state];
  [confirmCheckPanelCheck setTitle:checkMsg];

  int result = [NSApp runModalForWindow:confirmCheckPanel relativeToWindow:parent];

  *checkValue = ([confirmCheckPanelCheck state] == NSOnState);
  [confirmCheckPanel close];

  return (result == 1);
}

@end
