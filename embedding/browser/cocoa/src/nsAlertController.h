#import <Cocoa/Cocoa.h>

@interface nsAlertController : NSObject
{
    IBOutlet id alertCheckPanel;
    IBOutlet id alertCheckPanelCheck;
    IBOutlet id alertCheckPanelText;
    IBOutlet id alertPanel;
    IBOutlet id alertPanelText;
    IBOutlet id confirmCheckPanel;
    IBOutlet id confirmCheckPanelCheck;
    IBOutlet id confirmCheckPanelText;
    IBOutlet id confirmPanel;
    IBOutlet id confirmPanelText;
    IBOutlet id promptPanel;
    IBOutlet id promptPanelCheck;
    IBOutlet id promptPanelText;
    IBOutlet id promptPanelInput;
    IBOutlet id passwordPanel;
    IBOutlet id passwordPanelCheck;
    IBOutlet id passwordPanelText;
    IBOutlet id passwordPanelInput;    
    IBOutlet id usernamePanel;
    IBOutlet id usernamePanelCheck;
    IBOutlet id usernamePanelText;
    IBOutlet id usernamePanelPassword;    
    IBOutlet id usernamePanelUserName;    
    IBOutlet id owner;
}
- (IBAction)hitButton1:(id)sender;
- (IBAction)hitButton2:(id)sender;
- (IBAction)hitButton3:(id)sender;

- (void)awakeFromNib;
- (void)alert:(NSWindow*)parent title:(NSString*)title text:(NSString*)text;
- (void)alertCheck:(NSWindow*)parent title:(NSString*)title text:(NSString*)text checkMsg:(NSString*)checkMsg checkValue:(BOOL*)checkValue;
- (BOOL)confirm:(NSWindow*)parent title:(NSString*)title text:(NSString*)text;
- (BOOL)confirmCheck:(NSWindow*)parent title:(NSString*)title text:(NSString*)text checkMsg:(NSString*)checkMsg checkValue:(BOOL*)checkValue;
- (BOOL)prompt:(NSWindow*)parent title:(NSString*)title text:(NSString*)text promptText:(NSMutableString*)promptText checkMsg:(NSString*)checkMsg checkValue:(BOOL*)checkValue doCheck:(BOOL)doCheck;
- (BOOL)promptUserNameAndPassword:(NSWindow*)parent title:(NSString*)title text:(NSString*)text userNameText:(NSMutableString*)userNameText passwordText:(NSMutableString*)passwordText checkMsg:(NSString*)checkMsg checkValue:(BOOL*)checkValue doCheck:(BOOL)doCheck;
- (BOOL)promptPassword:(NSWindow*)parent title:(NSString*)title text:(NSString*)text passwordText:(NSMutableString*)passwordText checkMsg:(NSString*)checkMsg checkValue:(BOOL*)checkValue doCheck:(BOOL)doCheck;
@end
