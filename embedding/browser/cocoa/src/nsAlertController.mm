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

- (BOOL)prompt:(NSWindow*)parent title:(NSString*)title text:(NSString*)text promptText:(NSMutableString*)promptText checkMsg:(NSString*)checkMsg checkValue:(BOOL*)checkValue
{
  [promptPanelText setStringValue:text];
  [promptPanel setTitle:title];
  int state = (*checkValue ? NSOnState : NSOffState);
  [promptPanelCheck setState:state];
  [promptPanelCheck setTitle:checkMsg];
  [promptPanelInput setTitle:promptText];

  int result = [NSApp runModalForWindow:promptPanel relativeToWindow:parent];

  *checkValue = ([promptPanelCheck state] == NSOnState);

  NSString* value = [promptPanelInput title];
  PRUint32 length = [promptText length];
  if (length) {
    NSRange all;
    all.location = 0;
    all.length = [promptText length];
    [promptText deleteCharactersInRange:all];
  }
  [promptText appendString:value];

  [promptPanel close];

  return (result == 1);	
}

- (BOOL)promptUserNameAndPassword:(NSWindow*)parent title:(NSString*)title text:(NSString*)text userNameText:(NSMutableString*)userNameText passwordText:(NSMutableString*)passwordText checkMsg:(NSString*)checkMsg checkValue:(BOOL*)checkValue 
{
  [usernamePanelText setStringValue:text];
  [usernamePanel setTitle:title];
  int state = (*checkValue ? NSOnState : NSOffState);
  [usernamePanelCheck setState:state];
  [usernamePanelCheck setTitle:checkMsg];
  [usernamePanelPassword setTitle:passwordText];
  [usernamePanelUserName setTitle:userNameText];

  int result = [NSApp runModalForWindow:usernamePanel relativeToWindow:parent];

  *checkValue = ([usernamePanelCheck state] == NSOnState);

  NSString* value = [usernamePanelUserName title];
  PRUint32 length = [userNameText length];
  if (length) {
    NSRange all;
    all.location = 0;
    all.length = [userNameText length];
    [userNameText deleteCharactersInRange:all];
  }
  [userNameText appendString:value];

  value = [usernamePanelPassword title];
  length = [passwordText length];
  if (length) {
    NSRange all;
    all.location = 0;
    all.length = [passwordText length];
    [passwordText deleteCharactersInRange:all];
  }
  [passwordText appendString:value];

  [usernamePanel close];

  return (result == 1);	
}

- (BOOL)promptPassword:(NSWindow*)parent title:(NSString*)title text:(NSString*)text passwordText:(NSMutableString*)passwordText checkMsg:(NSString*)checkMsg checkValue:(BOOL*)checkValue 
{
  [passwordPanelText setStringValue:text];
  [passwordPanel setTitle:title];
  int state = (*checkValue ? NSOnState : NSOffState);
  [passwordPanelCheck setState:state];
  [passwordPanelCheck setTitle:checkMsg];
  [passwordPanelInput setTitle:passwordText];

  int result = [NSApp runModalForWindow:passwordPanel relativeToWindow:parent];

  *checkValue = ([passwordPanelCheck state] == NSOnState);

  NSString* value = [passwordPanelInput title];
  PRUint32 length = [passwordText length];
  if (length) {
    NSRange all;
    all.location = 0;
    all.length = [passwordText length];
    [passwordText deleteCharactersInRange:all];
  }
  [passwordText appendString:value];

  [passwordPanel close];

  return (result == 1);	
}
@end
