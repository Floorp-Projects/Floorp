/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
 
#import "nsAlertController.h"
#import "CHBrowserService.h"

enum { kOKButton = 0, kCancelButton = 1, kOtherButton = 2 };

const int kMinDialogWidth = 500;
const int kMaxDialogHeight = 400;
const int kMinDialogHeight = 130;
const int kIconSize = 64;
const int kWindowBorder = 20;
const int kIconMargin = 16;						//  space between the icon and text content
const int kButtonMinWidth = 82;
const int kButtonMaxWidth = 200;
const int kButtonHeight = 32;
const int kButtonRightMargin = 14;		//  space between right edge of button and window
const int kButtonBottomMargin = 12;		//  space between bottom edge of button and window
const int kGeneralViewSpace = 9;			//  space between one view and another (vertically)
const int kViewButtonSpace = 16;			//  space between the bottom view and the buttons
const int kMaxFieldHeight = 100;
const int kCheckBoxWidth = 20;
const int kCheckBoxHeight = 18;
const int kTextFieldHeight = 19;
const int kStaticTextFieldHeight = 14;	//  height of non-editable non-bordered text fields
const int kFieldLabelSpacer = 4;				//  space between a static label and an editabe text field (horizontal)
const int kOtherAltButtonSpace = 25;		//  minimum space between the 'other' and 'alternate' buttons
const int kButtonEndCapWidth = 14;
const int kLabelCheckboxAdjustment = 2; // # pixels the label must be pushed down to line up with checkbox


//
// QDCoordsView
//
// A view that uses the QD coordinate space (top left is (0,0))
//

@interface QDCoordsView : NSView
{
}
@end

@implementation QDCoordsView

- (BOOL) isFlipped
{
  return YES;
}

@end

#pragma mark -

@interface nsAlertController (nsAlertControllerPrivateMethods)

- (NSPanel*)getAlertPanelWithTitle:(NSString*)title
        message:(NSString*)message
        defaultButton:(NSString*)defaultLabel
        altButton:(NSString*)altLabel
        otherButton:(NSString*)otherLabel
        extraView:(NSView*)extraView
        lastResponder:(NSView*)lastResponder;
        
- (NSButton*)makeButtonWithTitle:(NSString*)title;

- (float)getContentWidthWithDefaultButton:(NSString*)defStr alternateButton:(NSString*)altStr otherButton:(NSString*)otherStr;
- (NSTextField*)getTitleView:(NSString*)title withWidth:(float)width;
- (NSTextField*)getLabelView:(NSString*)title withWidth:(float)width;
- (NSView*)getLoginField:(NSTextField**)field withWidth:(float)width;
- (NSView*)getPasswordField:(NSSecureTextField**)field withWidth:(float)width;
- (int)getLoginTextLabelSize;
- (NSView*)getMessageView:(NSString*)message withWidth:(float)width maxHeight:(float)maxHeight smallFont:(BOOL)useSmallFont;
- (NSView*)getCheckboxView:(NSButton**)checkBox withLabel:(NSString*)label andWidth:(float)width;

-(void)tester:(id)sender;

@end

#pragma mark -

@implementation nsAlertController

- (IBAction)hitButton1:(id)sender
{
  [NSApp stopModalWithCode:kOKButton];
}

- (IBAction)hitButton2:(id)sender
{
  [NSApp stopModalWithCode:kCancelButton];
}

- (IBAction)hitButton3:(id)sender
{
  [NSApp stopModalWithCode:kOtherButton];
}

- (void)alert:(NSWindow*)parent title:(NSString*)title text:(NSString*)text
{ 
  NSPanel* panel = [self getAlertPanelWithTitle: title message: text defaultButton: NSLocalizedString(@"OKButtonText", @"") altButton: nil otherButton: nil extraView: nil lastResponder:nil];
  [NSApp runModalForWindow: panel relativeToWindow:parent];
  [panel close];
}

- (void)alertCheck:(NSWindow*)parent title:(NSString*)title text:(NSString*)text checkMsg:(NSString*)checkMsg checkValue:(BOOL*)checkValue
{
  //  set up check box view
  NSButton* checkBox = nil;
  float width = [self getContentWidthWithDefaultButton: NSLocalizedString(@"OKButtonText", @"") alternateButton: nil otherButton: nil];
  NSView* checkboxView = [self getCheckboxView: &checkBox withLabel: checkMsg andWidth: width];
  int state = (*checkValue ? NSOnState : NSOffState);
  [checkBox setState:state];
  
  //  get panel and display it
  NSPanel* panel = [self getAlertPanelWithTitle: title message: text defaultButton: NSLocalizedString(@"OKButtonText", @"") altButton: nil otherButton: nil extraView: checkboxView lastResponder: checkBox];
  [panel setInitialFirstResponder: checkBox];

  [NSApp runModalForWindow: panel relativeToWindow:parent];
  *checkValue = ([checkBox state] == NSOnState);  
  [panel close];
}

- (BOOL)confirm:(NSWindow*)parent title:(NSString*)title text:(NSString*)text
{
  NSPanel* panel = [self getAlertPanelWithTitle: title message: text defaultButton: NSLocalizedString(@"OKButtonText", @"") altButton: NSLocalizedString(@"CancelButtonText", @"") otherButton: nil extraView: nil lastResponder: nil];
  int result = [NSApp runModalForWindow: panel relativeToWindow:parent];
  [panel close];

  return (result == kOKButton);
}

- (BOOL)confirmCheck:(NSWindow*)parent title:(NSString*)title text:(NSString*)text checkMsg:(NSString*)checkMsg checkValue:(BOOL*)checkValue
{
  //  get button titles
  NSString* okButton = NSLocalizedString(@"OKButtonText", @"");
  NSString* cancelButton = NSLocalizedString(@"CancelButtonText", @"");

  //  set up check box view
  NSButton* checkBox = nil;
  float width = [self getContentWidthWithDefaultButton: okButton alternateButton: cancelButton otherButton: nil];
  NSView* checkboxView = [self getCheckboxView: &checkBox withLabel: checkMsg andWidth: width];
  int state = (*checkValue ? NSOnState : NSOffState);
  [checkBox setState:state];
  
  //  get panel and display it
  NSPanel* panel = [self getAlertPanelWithTitle: title message: text defaultButton: okButton altButton: cancelButton otherButton: nil extraView: checkboxView lastResponder: checkBox];
  [panel setInitialFirstResponder: checkBox];

  int result = [NSApp runModalForWindow: panel relativeToWindow:parent];
  *checkValue = ([checkBox state] == NSOnState);  
  [panel close];

  return (result == kOKButton);
}

- (int)confirmEx:(NSWindow*)parent title:(NSString*)title text:(NSString*)text button1:(NSString*)btn1 button2:(NSString*)btn2 button3:(NSString*)btn3
{
   NSPanel* panel = [self getAlertPanelWithTitle: title message: text defaultButton: btn1 altButton: btn2 otherButton: btn3 extraView: nil lastResponder: nil];
   int result = [NSApp runModalForWindow: panel relativeToWindow:parent];
   [panel close];
   return result;
}

- (int)confirmCheckEx:(NSWindow*)parent title:(NSString*)title text:(NSString*)text button1:(NSString*)btn1 button2:(NSString*)btn2 button3:(NSString*)btn3 checkMsg:(NSString*)checkMsg checkValue:(BOOL*)checkValue
{
  //  set up check box view
  NSButton* checkBox = nil;
  float width = [self getContentWidthWithDefaultButton: btn1 alternateButton: btn2 otherButton: btn3];
  NSView* checkboxView = [self getCheckboxView: &checkBox withLabel: checkMsg andWidth: width];
  int state = (*checkValue ? NSOnState : NSOffState);
	[checkBox setState:state];
  
  //  get panel and display it
  NSPanel* panel = [self getAlertPanelWithTitle: title message: text defaultButton: btn1 altButton: btn2 otherButton: btn3 extraView: checkboxView lastResponder: checkBox];
  [panel setInitialFirstResponder: checkBox];
  int result = [NSApp runModalForWindow: panel relativeToWindow:parent];
  *checkValue = ([checkBox state] == NSOnState);  
  [panel close];

  return result;
  
}


- (BOOL)prompt:(NSWindow*)parent title:(NSString*)title text:(NSString*)text promptText:(NSMutableString*)promptText checkMsg:(NSString*)checkMsg checkValue:(BOOL*)checkValue doCheck:(BOOL)doCheck
{
  NSString* okButton = NSLocalizedString(@"OKButtonText", @"");
  NSString* cancelButton = NSLocalizedString(@"CancelButtonText", @"");
  float width = [self getContentWidthWithDefaultButton: okButton alternateButton: cancelButton otherButton: nil];
  NSView* extraView = [[[NSView alloc] initWithFrame: NSMakeRect(0, 0, width, kTextFieldHeight)] autorelease];;

  //  set up input field
  NSTextField* field = [[[NSTextField alloc] initWithFrame: NSMakeRect(0, 0, width, kTextFieldHeight)] autorelease];
  NSTextFieldCell* cell = [field cell];
  [cell setControlSize: NSSmallControlSize];
  [cell setScrollable: YES];
  [field setStringValue: promptText];
  [field selectText: nil];
  [field setFont: [NSFont systemFontOfSize: [NSFont smallSystemFontSize]]];
  [field setAutoresizingMask: NSViewWidthSizable | NSViewMinYMargin];
  [extraView addSubview: field];
  [extraView setNextKeyView: field];

  //  set up check box view, and combine the checkbox and field into the entire extra view
  NSButton* checkBox = nil;  
  if (doCheck) {
    int state = (*checkValue ? NSOnState : NSOffState);
    NSView* checkboxView = [self getCheckboxView: &checkBox withLabel: checkMsg andWidth: width];
    [checkBox setState:state];
    [checkboxView setFrameOrigin: NSMakePoint(0, 0)];
    [extraView setFrameSize: NSMakeSize(width, kTextFieldHeight + kGeneralViewSpace + NSHeight([checkboxView frame]))];
    [extraView addSubview: checkboxView];
    [field setNextKeyView: checkBox];
  }
    
  //  get panel and display it
  NSView* lastResponder = (doCheck ? (NSView*)checkBox : (NSView*)field);
  NSPanel* panel = [self getAlertPanelWithTitle: title message: text defaultButton: okButton altButton: cancelButton otherButton: nil extraView: extraView lastResponder:lastResponder];
  [panel setInitialFirstResponder: field];

  int result = [NSApp runModalForWindow: panel relativeToWindow:parent];
  [panel close];
  
  [promptText setString: [field stringValue]];
  
  if (doCheck)
    *checkValue = ([checkBox state] == NSOnState);  
 
  return (result == kOKButton);
}


- (BOOL)promptUserNameAndPassword:(NSWindow*)parent title:(NSString*)title text:(NSString*)text userNameText:(NSMutableString*)userNameText passwordText:(NSMutableString*)passwordText checkMsg:(NSString*)checkMsg checkValue:(BOOL*)checkValue doCheck:(BOOL)doCheck
{
  NSString* okButton = NSLocalizedString(@"OKButtonText", @"");
  NSString* cancelButton = NSLocalizedString(@"CancelButtonText", @"");
  float width = [self getContentWidthWithDefaultButton: okButton alternateButton: cancelButton otherButton: nil];
  NSView* extraView = [[[NSView alloc] initWithFrame: NSMakeRect(0, 0, width, 2*kTextFieldHeight + kGeneralViewSpace)] autorelease];

  //  set up username field
  NSTextField* userField = nil;
  NSView* userView = [self getLoginField: &userField withWidth: width];
  [userView setAutoresizingMask: NSViewMinYMargin];
  [userView setFrameOrigin: NSMakePoint(0, kTextFieldHeight + kGeneralViewSpace)];
  [userField setStringValue: userNameText];
  [extraView addSubview: userView];
  [extraView setNextKeyView: userField];
  
  //  set up password field
  NSSecureTextField* passField = nil;
  NSView* passView = [self getPasswordField: &passField withWidth: width];
  [passView setAutoresizingMask: NSViewMinYMargin];
  [passView setFrameOrigin: NSMakePoint(0, 0)];
  [passField setStringValue: passwordText];
  [extraView addSubview: passView];
  [userField setNextKeyView: passField];

  //  set up check box view, and combine the checkbox and field into the entire extra view
  NSButton* checkBox = nil;  
  if (doCheck) {
    int state = (*checkValue ? NSOnState : NSOffState);
    NSView* checkboxView = [self getCheckboxView: &checkBox withLabel: checkMsg andWidth: width];
    [checkBox setState:state];
    [checkboxView setFrameOrigin: NSMakePoint(0, 0)];
    [extraView setFrameSize: NSMakeSize(width,  2*kTextFieldHeight + 2*kGeneralViewSpace + NSHeight([checkboxView frame]))];
    [extraView addSubview: checkboxView];
    [passField setNextKeyView: checkBox];
  }
    
  //  get panel and display it
  NSView* lastResponder = (doCheck ? (NSView*)checkBox : (NSView*)passField);
  NSPanel* panel = [self getAlertPanelWithTitle: title message: text defaultButton: okButton altButton: cancelButton otherButton: nil extraView: extraView lastResponder:lastResponder];
  [panel setInitialFirstResponder: userField];

  int result = [NSApp runModalForWindow: panel relativeToWindow:parent];
  [panel close];
  
  [userNameText setString: [userField stringValue]];
  [passwordText setString: [passField stringValue]];
  
  if (doCheck)
    *checkValue = ([checkBox state] == NSOnState);  
 
  return (result == kOKButton);
}

- (BOOL)promptPassword:(NSWindow*)parent title:(NSString*)title text:(NSString*)text passwordText:(NSMutableString*)passwordText
        checkMsg:(NSString*)checkMsg checkValue:(BOOL*)checkValue doCheck:(BOOL)doCheck
{
  NSString* okButton = NSLocalizedString(@"OKButtonText", @"");
  NSString* cancelButton = NSLocalizedString(@"CancelButtonText", @"");
  float width = [self getContentWidthWithDefaultButton: okButton alternateButton: cancelButton otherButton: nil];
  NSView* extraView = [[[NSView alloc] initWithFrame: NSMakeRect(0, 0, width, kTextFieldHeight)] autorelease];;

  //  set up input field
  NSSecureTextField* passField = nil;
  NSView* passView = [self getPasswordField: &passField withWidth: width];
  [passView setAutoresizingMask: NSViewMinYMargin];
  [passView setFrameOrigin: NSMakePoint(0, 0)];
  [passField setStringValue: passwordText];
  [extraView addSubview: passView];
  [extraView setNextKeyView: passField];

  //  set up check box view, and combine the checkbox and field into the entire extra view
  NSButton* checkBox = nil;  
  if (doCheck) {
    int state = (*checkValue ? NSOnState : NSOffState);
    NSView* checkboxView = [self getCheckboxView: &checkBox withLabel: checkMsg andWidth: width];
    [checkBox setState:state];
    [checkboxView setFrameOrigin: NSMakePoint(0, 0)];
    [extraView setFrameSize: NSMakeSize(width, kTextFieldHeight + kGeneralViewSpace + NSHeight([checkboxView frame]))];
    [extraView addSubview: checkboxView];
    [passField setNextKeyView: checkBox];
  }
    
  //  get panel and display it
  NSView* lastResponder = (doCheck ? (NSView*)checkBox : (NSView*)passField);
  NSPanel* panel = [self getAlertPanelWithTitle: title message: text defaultButton: okButton altButton: cancelButton otherButton: nil extraView: extraView lastResponder:lastResponder];
  [panel setInitialFirstResponder: passField];

  int result = [NSApp runModalForWindow: panel relativeToWindow:parent];
  [panel close];
  
  [passwordText setString: [passField stringValue]];
  
  if (doCheck)
    *checkValue = ([checkBox state] == NSOnState);  
 
  return (result == kOKButton);
}


- (BOOL)postToInsecureFromSecure:(NSWindow*)parent
{
  NSString* title = NSLocalizedString(@"Security Warning", @"");
  NSString* message = NSLocalizedString(@"Post To Insecure", @"");
  NSString* continueButton = NSLocalizedString(@"ContinueButton", @"");
  NSString* stopButton = NSLocalizedString(@"StopButton", @"");
  
  id panel = NSGetAlertPanel(title, message, continueButton, stopButton, nil);
  int result = [NSApp runModalForWindow: panel relativeToWindow:parent];
  [panel close];
  NSReleaseAlertPanel(panel);
  
  return (result == NSAlertDefaultReturn);
}


- (BOOL)badCert:(NSWindow*)parent
{
  NSString* title = NSLocalizedString(@"Security Warning", @"");
  NSString* message = NSLocalizedString(@"Security Mismatch", @"");
  NSString* continueButton = NSLocalizedString(@"ContinueButton", @"");
  NSString* stopButton = NSLocalizedString(@"StopButton", @"");
  
  id panel = NSGetAlertPanel(title, message, continueButton, stopButton, nil);
  int result = [NSApp runModalForWindow: panel relativeToWindow:parent];
  [panel close];
  NSReleaseAlertPanel(panel);
  
  return (result == NSAlertDefaultReturn);
}

- (BOOL)expiredCert:(NSWindow*)parent
{
  NSString* title = NSLocalizedString(@"Security Warning", @"");
  NSString* message = NSLocalizedString(@"Expired Certification", @"");
  NSString* continueButton = NSLocalizedString(@"ContinueButton", @"");
  NSString* stopButton = NSLocalizedString(@"StopButton", @"");
  
  id panel = NSGetAlertPanel(title, message, continueButton, stopButton, nil);
  int result = [NSApp runModalForWindow: panel relativeToWindow:parent];
  [panel close];
  NSReleaseAlertPanel(panel);
  
  return (result == NSAlertDefaultReturn);
}


- (int)unknownCert:(NSWindow*)parent
{
  // this dialog is a little backward, with "Stop" returning 0, and the default returning
  // 1. That's just how nsIBadCertListener defines its constants. *shrug*
  NSString* title = NSLocalizedString(@"Security Warning", @"");
  NSString* message = NSLocalizedString(@"Unknown Certification", @"");
  NSString* alwaysAcceptButton = NSLocalizedString(@"Accept Always", @"");
  NSString* oneAcceptButton = NSLocalizedString(@"Accept Once", @"");
  NSString* stopButton = NSLocalizedString(@"StopButton", @"");
  
  id panel = NSGetAlertPanel(title, message, oneAcceptButton, stopButton, alwaysAcceptButton);

  int result = [NSApp runModalForWindow: panel relativeToWindow:parent];
  [panel close];
  NSReleaseAlertPanel(panel);
  
  int returnValue;
  if (result == NSAlertDefaultReturn)
    returnValue = 1;
  else if (result == NSAlertOtherReturn)
    returnValue = 2;
  else
    returnValue = 0;
  
  return returnValue;
}


#pragma mark -


//  implementation of private methods

// The content width is determined by how much space is needed to display all 3 buttons,
// since every other view in the dialog can wrap if necessary
- (float)getContentWidthWithDefaultButton:(NSString*)defStr alternateButton:(NSString*)altStr otherButton:(NSString*)otherStr
{
  NSButton* defButton = [self makeButtonWithTitle: defStr];
  NSButton* altButton = [self makeButtonWithTitle: altStr];
  NSButton* otherButton = [self makeButtonWithTitle: otherStr];

  float defWidth = NSWidth([defButton frame]);
  float altWidth = (altButton) ? NSWidth([altButton frame]) : 0;
  float otherWidth = (otherButton) ? NSWidth([otherButton frame]) : 0;
  
  float minContentWidth = kMinDialogWidth - 2*kWindowBorder - kIconMargin - kIconSize;
  float buttonWidth = defWidth + altWidth + otherWidth + kOtherAltButtonSpace;

  //  return the larger of the two
  return (minContentWidth > buttonWidth) ? minContentWidth : buttonWidth;
}

- (NSPanel*)getAlertPanelWithTitle:(NSString*)title message:(NSString*)message
                defaultButton:(NSString*)defaultLabel altButton:(NSString*)altLabel
                otherButton:(NSString*)otherLabel extraView:(NSView*)extraView lastResponder:(NSView*)lastResponder
{
  NSRect rect = NSMakeRect(0, 0, kMinDialogWidth, kMaxDialogHeight);
  NSPanel* panel = [[[NSPanel alloc] initWithContentRect: rect styleMask: NSTitledWindowMask backing: NSBackingStoreBuffered defer: YES] autorelease];
  NSImageView* imageView = [[[NSImageView alloc] initWithFrame: NSMakeRect(kWindowBorder, kMaxDialogHeight - kWindowBorder - kIconSize, kIconSize, kIconSize)] autorelease];
  
  //  app icon
  
  [imageView setImage: [NSImage imageNamed: @"NSApplicationIcon"]];
  [imageView setImageFrameStyle: NSImageFrameNone];
  [imageView setImageScaling: NSScaleProportionally];
  [imageView setAutoresizingMask: NSViewMinYMargin | NSViewMaxXMargin];
  [[panel contentView] addSubview: imageView];
  
  //  create buttons
  
  NSButton* defButton = [self makeButtonWithTitle: defaultLabel];
  [defButton setAction: @selector(hitButton1:)];
  [defButton setAutoresizingMask: NSViewMinXMargin | NSViewMaxYMargin];
  [defButton setKeyEquivalent: @"\r"];		// return
  [[panel contentView] addSubview: defButton];
  [panel setDefaultButtonCell: [defButton cell]];
  
  NSView* firstKeyView = (extraView ? extraView : defButton);

  NSButton* altButton = nil;
  if (altLabel) {
    altButton = [self makeButtonWithTitle: altLabel];
    [altButton setAction: @selector(hitButton2:)];
    [altButton setAutoresizingMask: NSViewMinXMargin | NSViewMaxYMargin];
    [altButton setKeyEquivalent: @"\e"];		// escape
    [[panel contentView] addSubview: altButton];
    [defButton setNextKeyView: altButton];
  }
  
  NSButton* otherButton = nil;
  if (otherLabel) {
    otherButton = [self makeButtonWithTitle: otherLabel];
    [otherButton setAction: @selector(hitButton3:)];
    [otherButton setAutoresizingMask: NSViewMaxXMargin | NSViewMaxYMargin];
    [[panel contentView] addSubview: otherButton];
    [otherButton setNextKeyView: firstKeyView];
    if (altButton)
      [altButton setNextKeyView: otherButton];
    else
      [defButton setNextKeyView: otherButton];
  } else {
    if (altButton)
      [altButton setNextKeyView: firstKeyView];
    else
      [defButton setNextKeyView: firstKeyView];
  }
  
  //  position buttons
  
  float defWidth = NSWidth([defButton frame]);
  float altWidth = (altButton) ? NSWidth([altButton frame]) : 0;
  
  [defButton setFrameOrigin: NSMakePoint(NSWidth([panel frame]) - defWidth - kButtonRightMargin, kButtonBottomMargin)];
  [altButton setFrameOrigin: NSMakePoint(NSMinX([defButton frame]) - altWidth, kButtonBottomMargin)];
  [otherButton setFrameOrigin: NSMakePoint(kIconSize + kWindowBorder + kIconMargin, kButtonBottomMargin)];
  
  //  set the window width
  //  contentWidth is the width of the area with text, buttons, etc
  //  windowWidth is the total window width (contentWidth + margins + icon)
  
  float contentWidth = [self getContentWidthWithDefaultButton: defaultLabel alternateButton: altLabel otherButton: otherLabel];
  float windowWidth = kIconSize + kIconMargin + 2*kWindowBorder + contentWidth;
  if (windowWidth < kMinDialogWidth)
    windowWidth = kMinDialogWidth;

  //  get the height of all elements, and set the window height
  
  NSTextField* titleField = [self getTitleView: title withWidth: contentWidth];
  
  float titleHeight = [title length] ? NSHeight([titleField frame]) : 0;
  float extraViewHeight = (extraView) ? NSHeight([extraView frame]) : 0;
  float totalHeight = kWindowBorder + titleHeight + kGeneralViewSpace + kViewButtonSpace + kButtonHeight + kButtonBottomMargin;
  if (extraViewHeight > 0)
    totalHeight += extraViewHeight + kGeneralViewSpace;
  
  float maxMessageHeight = kMaxDialogHeight - totalHeight;
  NSView* messageView = [self getMessageView: message withWidth: contentWidth maxHeight: maxMessageHeight smallFont:[title length]];
  float messageHeight = NSHeight([messageView frame]);
  totalHeight += messageHeight;
  
  if (totalHeight < kMinDialogHeight)
    totalHeight = kMinDialogHeight;
  
  [panel setContentSize: NSMakeSize(windowWidth, totalHeight)];
  
  //  position the title
  
  float contentLeftEdge = kWindowBorder + kIconSize + kIconMargin;
  NSRect titleRect = NSMakeRect(contentLeftEdge, totalHeight - titleHeight - kWindowBorder, contentWidth, titleHeight);
  [titleField setFrame: titleRect];
  if ( [title length] )
    [[panel contentView] addSubview: titleField];
  
  //  position the message
  
  NSRect messageRect = NSMakeRect(contentLeftEdge, NSMinY([titleField frame]) - kGeneralViewSpace - messageHeight, contentWidth, messageHeight);
  [messageView setFrame: messageRect];
  [[panel contentView] addSubview: messageView];
  
  //  position the extra view

  NSRect extraRect = NSMakeRect(contentLeftEdge, NSMinY([messageView frame]) - kGeneralViewSpace - extraViewHeight, contentWidth, extraViewHeight);
  [extraView setFrame: extraRect];
  [[panel contentView] addSubview: extraView];

  // If a lastResponder was passed in (the last item in the tab order inside
  // extraView), hook it up to cycle to defButton.  If not, assume there is
  // nothing focusable inside extraView and hook up defButton as the first
  // responder.

  if (lastResponder)
    [lastResponder setNextKeyView: defButton];
  else
    [panel setInitialFirstResponder: defButton];

  return panel;
}

- (NSButton*)makeButtonWithTitle:(NSString*)title
{
  if (title == nil)
    return nil;

  NSButton* button = [[[NSButton alloc] initWithFrame: NSMakeRect(0, 0, kButtonMinWidth, kButtonHeight)] autorelease];

  [button setTitle: title];
  [button setFont: [NSFont systemFontOfSize: [NSFont systemFontSize]]];
  [button setBordered: YES];
  [button setButtonType: NSMomentaryPushButton];
  [button setBezelStyle: NSRoundedBezelStyle];
  [button setTarget: self];
  [button sizeToFit];
  
  NSRect buttonFrame = [button frame];
  
  //  sizeToFit doesn't leave large enough endcaps (it goes to the bare minimum size)
  buttonFrame.size.width += 2*kButtonEndCapWidth;
  
  //  make sure the button is within our allowed size range
  if (NSWidth(buttonFrame) < kButtonMinWidth)
    buttonFrame.size.width = kButtonMinWidth;
  else if (NSWidth(buttonFrame) > kButtonMaxWidth)
    buttonFrame.size.width = kButtonMaxWidth;
    
  [button setFrame: buttonFrame];

  return button;
}


- (NSTextField*)getTitleView:(NSString*)title withWidth:(float)width
{
  NSTextView* textView = [[[NSTextView alloc] initWithFrame: NSMakeRect(0, 0, width, 100)] autorelease];		
  [textView setString: title];
  [textView setMinSize: NSMakeSize(width, 0.0)];
  [textView setMaxSize: NSMakeSize(width, kMaxFieldHeight)];
  [textView setVerticallyResizable: YES];
  [textView setHorizontallyResizable: NO];
  [textView setFont: [NSFont boldSystemFontOfSize: [NSFont systemFontSize]]];
  [textView sizeToFit];
  NSSize textSize = NSMakeSize( ceil( NSWidth([textView frame]) ), ceil( NSHeight([textView frame]) ) );
  
  NSTextField* field = [[[NSTextField alloc] initWithFrame: NSMakeRect(0, 0, textSize.width, textSize.height)] autorelease];
  [field setStringValue: title];
  [field setFont: [NSFont boldSystemFontOfSize: [NSFont systemFontSize]]];
  [field setEditable: NO];
  [field setSelectable: NO];
  [field setBezeled: NO];
  [field setBordered: NO];
  [field setDrawsBackground: NO];

  return field;
}

- (NSTextField*)getLabelView:(NSString*)title withWidth:(float)width
{
  NSTextView* textView = [[[NSTextView alloc] initWithFrame: NSMakeRect(0, 0, width, 100)] autorelease];		
  [textView setString: title];
  [textView setMinSize: NSMakeSize(width, 0.0)];
  [textView setMaxSize: NSMakeSize(width, kMaxFieldHeight)];
  [textView setVerticallyResizable: YES];
  [textView setHorizontallyResizable: NO];
  [textView setFont: [NSFont systemFontOfSize: [NSFont smallSystemFontSize]]];
  [textView sizeToFit];
  NSSize textSize = NSMakeSize(ceil(NSWidth([textView frame])), ceil(NSHeight([textView frame])) + 5);
  
  NSTextField* field = [[[NSTextField alloc] initWithFrame: NSMakeRect(0, 0, textSize.width, textSize.height)] autorelease];
  [field setStringValue: title];
  [field setFont: [NSFont systemFontOfSize: [NSFont smallSystemFontSize]]];
  [field setEditable: NO];
  [field setSelectable: NO];
  [field setBezeled: NO];
  [field setBordered: NO];
  [field setDrawsBackground: NO];

  return field;
}

- (NSView*)getMessageView:(NSString*)message withWidth:(float)width maxHeight:(float)maxHeight smallFont:(BOOL)useSmallFont
{
  NSTextView* textView = [[[NSTextView alloc] initWithFrame: NSMakeRect(0, 0, width, 100)] autorelease];		
  [textView setString: message];
  [textView setMinSize: NSMakeSize(width, 0.0)];
  [textView setVerticallyResizable: YES];
  [textView setHorizontallyResizable: NO];
  float displayFontSize = useSmallFont ? [NSFont smallSystemFontSize] : [NSFont systemFontSize];
  [textView setFont: [NSFont systemFontOfSize:displayFontSize]];
  [textView sizeToFit];
  NSSize textSize = NSMakeSize(ceil(NSWidth([textView frame])), ceil(NSHeight([textView frame])) + 5);
  
  //  if the text is small enough to fit, then display it
  
  if (textSize.height <= maxHeight) {
    NSTextField* field = [[[NSTextField alloc] initWithFrame: NSMakeRect(0, 0, textSize.width, textSize.height)] autorelease];
    [field setStringValue: message];
    [field setFont: [NSFont systemFontOfSize: displayFontSize]];
    [field setEditable: NO];
    [field setSelectable: YES];
    [field setBezeled: NO];
    [field setBordered: NO];
    [field setDrawsBackground: NO];
    
    return field;
  }
  
  //  if not, create scrollers
  
  NSScrollView* scrollView = [[[NSScrollView alloc] initWithFrame: NSMakeRect(0, 0, width, maxHeight)] autorelease];
  [scrollView setDocumentView: textView];
  [scrollView setDrawsBackground: NO];
  [scrollView setBorderType: NSBezelBorder];
  [scrollView setHasHorizontalScroller: NO];
  [scrollView setHasVerticalScroller: YES];
  
  [textView setSelectable: YES];
  [textView setEditable: NO];
  [textView setDrawsBackground: NO];
  [textView setFrameSize: [scrollView contentSize]];

  return scrollView;
}

- (NSView*)getCheckboxView:(NSButton**)checkBoxPtr withLabel:(NSString*)label andWidth:(float)width
{
  NSTextField* textField = [self getLabelView: label withWidth: width - kCheckBoxWidth];
  float height = NSHeight([textField frame]);
  
  //  one line of text isn't as tall as the checkbox.  make the view taller, or the checkbox will get clipped
  if (height < kCheckBoxHeight)
    height = kCheckBoxHeight;
  
  // use a flipped view so we can more easily align everything at the top/left.
  NSView* view = [[[QDCoordsView alloc] initWithFrame: NSMakeRect(0, 0, width, height)] autorelease];
  
  [view addSubview: textField];
  [view setNextKeyView: textField];
  [textField setFrameOrigin: NSMakePoint(kCheckBoxWidth, kLabelCheckboxAdjustment)];
  
  NSButton* checkBox = [[[NSButton alloc] initWithFrame: NSMakeRect(0, 0, kCheckBoxWidth, kCheckBoxHeight)] autorelease];
  *checkBoxPtr = checkBox;
  [checkBox setButtonType: NSSwitchButton];
  [[checkBox cell] setControlSize: NSSmallControlSize];
  [view addSubview: checkBox];
  [textField setNextKeyView: checkBox];
  
  // overlay the label with a transparent button that will push the checkbox
  // when clicked on
  NSButton* labelOverlay = [[[NSButton alloc] initWithFrame:[textField frame]] autorelease];
  [labelOverlay setTransparent:YES];
  [labelOverlay setTarget:[checkBox cell]];
  [labelOverlay setAction:@selector(performClick:)];
  [view addSubview:labelOverlay positioned:NSWindowAbove relativeTo:checkBox];

  return view;
}

- (NSView*)getLoginField:(NSTextField**)fieldPtr withWidth:(float)width
{
  int labelSize = [self getLoginTextLabelSize];
  int fieldLeftEdge = labelSize + kFieldLabelSpacer;

  NSView* view = [[[NSView alloc] initWithFrame: NSMakeRect(0, 0, width, kTextFieldHeight)] autorelease];
  NSTextField* label = [[[NSTextField alloc] initWithFrame: NSMakeRect(0, 3, labelSize, kStaticTextFieldHeight)] autorelease];
  [[label cell] setControlSize: NSSmallControlSize];
  [label setFont: [NSFont systemFontOfSize: [NSFont smallSystemFontSize]]];
  [label setStringValue: NSLocalizedString(@"Username Label", @"")];
  [label setEditable: NO];
  [label setSelectable: NO];
  [label setBordered: NO];
  [label setDrawsBackground: NO];
  [label setAlignment: NSRightTextAlignment];
  [view addSubview: label];
  
  NSTextField* field = [[[NSTextField alloc] initWithFrame: NSMakeRect(fieldLeftEdge, 0, width - fieldLeftEdge, kTextFieldHeight)] autorelease];
  [[field cell] setControlSize: NSSmallControlSize];
  [field setFont: [NSFont systemFontOfSize: [NSFont smallSystemFontSize]]];
  [view addSubview: field];
  [view setNextKeyView: field];
  *fieldPtr = field;
  
  return view;
}

- (NSView*)getPasswordField:(NSSecureTextField**)fieldPtr withWidth:(float)width
{
  int labelSize = [self getLoginTextLabelSize];
  int fieldLeftEdge = labelSize + kFieldLabelSpacer;

  NSView* view = [[[NSView alloc] initWithFrame: NSMakeRect(0, 0, width, kTextFieldHeight)] autorelease];
  NSTextField* label = [[[NSTextField alloc] initWithFrame: NSMakeRect(0, 3, labelSize, kStaticTextFieldHeight)] autorelease];
  [[label cell] setControlSize: NSSmallControlSize];
  [label setFont: [NSFont systemFontOfSize: [NSFont smallSystemFontSize]]];
  [label setStringValue: NSLocalizedString(@"Password Label", @"")];
  [label setEditable: NO];
  [label setSelectable: NO];
  [label setBordered: NO];
  [label setDrawsBackground: NO];
  [label setAlignment: NSRightTextAlignment];
  [view addSubview: label];
  
  NSSecureTextField* field = [[[NSSecureTextField alloc] initWithFrame: NSMakeRect(fieldLeftEdge, 0, width - fieldLeftEdge, kTextFieldHeight)] autorelease];
  [[field cell] setControlSize: NSSmallControlSize];
  [field setFont: [NSFont systemFontOfSize: [NSFont smallSystemFontSize]]];
  [view addSubview: field];
  [view setNextKeyView: field];
  *fieldPtr = field;
  
  return view;
}

- (int)getLoginTextLabelSize
{
  NSTextField* label = [[[NSTextField alloc] initWithFrame: NSMakeRect(0, 0, 200, kStaticTextFieldHeight)] autorelease];
  [[label cell] setControlSize: NSSmallControlSize];
  [label setFont: [NSFont systemFontOfSize: [NSFont smallSystemFontSize]]];
  [label setEditable: NO];
  [label setSelectable: NO];
  [label setBordered: NO];
  [label setDrawsBackground: NO];
  [label setAlignment: NSRightTextAlignment];
  
  [label setStringValue: NSLocalizedString(@"Username Label", @"")];
  [label sizeToFit];
  float userSize = NSWidth([label frame]);
  
  [label setStringValue: NSLocalizedString(@"Password Label", @"")];
  [label sizeToFit];
  float passSize = NSWidth([label frame]);
  
  float largest = (userSize > passSize) ? userSize : passSize;
  return (int)ceil(largest) + 6;
}

@end
