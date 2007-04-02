#include "nsAccessibleWrap.h"

#import "mozTextAccessible.h"

extern const NSString *kInstanceDescriptionAttribute; // NSAccessibilityDescriptionAttribute
extern const NSString *kTopLevelUIElementAttribute;   // NSAccessibilityTopLevelUIElementAttribute

@interface mozTextAccessible (Private)
- (NSString*)subrole;
- (NSString*)selectedText;
- (NSValue*)selectedTextRange;
- (long)textLength;
- (BOOL)isReadOnly;
- (void)setText:(NSString*)newText;
@end

@implementation mozTextAccessible

- (id)initWithAccessible:(nsAccessibleWrap*)accessible
{
  if ((self = [super initWithAccessible:accessible])) {
    CallQueryInterface(accessible, &mGeckoTextAccessible);
    CallQueryInterface(accessible, &mGeckoEditableTextAccessible);
  }
  return self;
}

- (BOOL)accessibilityIsIgnored
{
  return mIsExpired;
}

- (NSArray*)accessibilityAttributeNames
{
  static NSArray *supportedAttributes = nil;
  if (!supportedAttributes) {
    // standard attributes that are shared and supported by all generic elements.
    supportedAttributes = [[NSArray alloc] initWithObjects:NSAccessibilityParentAttribute, // required
                                                           NSAccessibilityRoleAttribute,   // required
                                                           NSAccessibilityTitleAttribute,
                                                           NSAccessibilityValueAttribute, // required
                                                           NSAccessibilitySubroleAttribute,
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_4
                                                           NSAccessibilityRoleDescriptionAttribute,
#endif
                                                           NSAccessibilityPositionAttribute, // required
                                                           NSAccessibilitySizeAttribute, // required
                                                           NSAccessibilityWindowAttribute, // required
                                                           NSAccessibilityFocusedAttribute, // required
                                                           NSAccessibilityEnabledAttribute, // required
                                                           kTopLevelUIElementAttribute, // required (on OS X 10.4+)
                                                           kInstanceDescriptionAttribute, // required (on OS X 10.4+)
                                                           /* text-specific attributes */
                                                           NSAccessibilitySelectedTextAttribute, // required
                                                           NSAccessibilitySelectedTextRangeAttribute, // required
                                                           NSAccessibilityNumberOfCharactersAttribute, // required
                                                           // TODO: NSAccessibilityVisibleCharacterRangeAttribute, // required
                                                           // TODO: NSAccessibilityInsertionPointLineNumberAttribute
                                                           nil];
  }
  return supportedAttributes;
}

- (id)accessibilityAttributeValue:(NSString*)attribute
{
  if ([attribute isEqualToString:NSAccessibilityNumberOfCharactersAttribute])
    return [NSNumber numberWithInt:[self textLength]];
  if ([attribute isEqualToString:NSAccessibilitySelectedTextRangeAttribute])
    return [self selectedTextRange];
  if ([attribute isEqualToString:NSAccessibilitySelectedTextAttribute])
    return [self selectedText];
  
  // let mozAccessible handle all other attributes
  return [super accessibilityAttributeValue:attribute];
}

- (BOOL)accessibilityIsAttributeSettable:(NSString*)attribute
{
  if ([attribute isEqualToString:NSAccessibilityValueAttribute])
    return [self isReadOnly];
  
  return [super accessibilityIsAttributeSettable:attribute];
}

- (void)accessibilitySetValue:(id)value forAttribute:(NSString *)attribute
{
  if ([attribute isEqualToString:NSAccessibilityValueAttribute]) {
    if ([value isKindOfClass:[NSString class]])
      [self setText:(NSString*)value];
  } else
    [super accessibilitySetValue:value forAttribute:attribute];
}

- (NSString*)subrole
{
  // TODO: text accessibles have two different subroles in Cocoa: secure textfield (passwords) and search field
  return nil;
}

- (void)expire
{
  NS_IF_RELEASE(mGeckoTextAccessible);
  NS_IF_RELEASE(mGeckoEditableTextAccessible);
  [super expire];
}

#pragma mark -

- (BOOL)isReadOnly
{
  if ([[self role] isEqualToString:NSAccessibilityStaticTextRole])
    return YES;
    
  if (mGeckoEditableTextAccessible) {
    PRUint32 state = 0;
    mGeckoAccessible->GetFinalState(&state, nsnull);
    return (state & nsIAccessibleStates::STATE_READONLY) == 0;
  }

  return NO;
}

- (void)setText:(NSString*)newString
{
  if (mGeckoEditableTextAccessible) {
    mGeckoEditableTextAccessible->SetTextContents(NS_ConvertUTF8toUTF16([newString UTF8String]));
  }
}

- (long)textLength
{
  if (mGeckoTextAccessible) {
    PRInt32 charCount = 0;
    mGeckoTextAccessible->GetCharacterCount(&charCount);
    return charCount;
  }
  return 0;
}

- (long)selectedTextLength
{
  if (mGeckoTextAccessible) {
    PRInt32 start, end;
    start = end = 0;
    mGeckoTextAccessible->GetSelectionBounds(0, &start, &end);
    return (end - start);
  }
  return 0;
}

- (NSString*)selectedText
{
  if (mGeckoTextAccessible) {
    PRInt32 start, end;
    start = end = 0;
    mGeckoTextAccessible->GetSelectionBounds(0, &start, &end);
    if (start != end) {
      nsAutoString selText;
      mGeckoTextAccessible->GetText(start, end, selText);
      return selText.IsEmpty() ? nil : [NSString stringWithCharacters:selText.BeginReading() length:selText.Length()];
    }
  }
  return nil;
}

- (NSValue*)selectedTextRange
{
  if (mGeckoTextAccessible) {
    PRInt32 start, end;
    start = end = 0;
    mGeckoTextAccessible->GetSelectionBounds(0, &start, &end);
    return [NSValue valueWithRange:NSMakeRange(start, start-end)];
  }
  return nil;
}

#pragma mark -

- (void)valueDidChange
{
  NSAccessibilityPostNotification([self hasRepresentedView] ? [self representedView] : self, 
                                  NSAccessibilityValueChangedNotification);
}

@end

@implementation mozComboboxAccessible

- (NSArray*)accessibilityAttributeNames
{
  static NSArray *supportedAttributes = nil;
  if (!supportedAttributes) {
    // standard attributes that are shared and supported by all generic elements.
    supportedAttributes = [[NSArray alloc] initWithObjects:NSAccessibilityParentAttribute, // required
                                                           NSAccessibilityRoleAttribute,   // required
                                                           NSAccessibilityTitleAttribute,
                                                           NSAccessibilityValueAttribute, // required
                                                           NSAccessibilityHelpAttribute,
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_4
                                                           NSAccessibilityRoleDescriptionAttribute,
#endif
                                                           NSAccessibilityPositionAttribute, // required
                                                           NSAccessibilitySizeAttribute, // required
                                                           NSAccessibilityWindowAttribute, // required
                                                           NSAccessibilityFocusedAttribute, // required
                                                           NSAccessibilityEnabledAttribute, // required
                                                           NSAccessibilityChildrenAttribute, // required
                                                           NSAccessibilityHelpAttribute,
                                                           // NSAccessibilityExpandedAttribute, // required
                                                           kTopLevelUIElementAttribute, // required (on OS X 10.4+)
                                                           kInstanceDescriptionAttribute, // required (on OS X 10.4+)
                                                           /* text-specific attributes */
                                                           NSAccessibilitySelectedTextAttribute, // required
                                                           NSAccessibilitySelectedTextRangeAttribute, // required
                                                           NSAccessibilityNumberOfCharactersAttribute, // required
                                                           // TODO: NSAccessibilityVisibleCharacterRangeAttribute, // required
                                                           // TODO: NSAccessibilityInsertionPointLineNumberAttribute
                                                           nil];
  }
  return supportedAttributes;
}

- (NSArray *)accessibilityActionNames
{
  if ([self isEnabled]) {
    return [NSArray arrayWithObjects:NSAccessibilityConfirmAction,
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_4
                                     NSAccessibilityShowMenuAction,
#endif
                                     nil];
  }
  return nil;
}

- (NSString *)accessibilityActionDescription:(NSString *)action
{
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_4
  if ([action isEqualToString:NSAccessibilityShowMenuAction])
    return @"show menu";
#endif
  if ([action isEqualToString:NSAccessibilityConfirmAction])
    return @"confirm";
    
  return [super accessibilityActionDescription:action];
}

- (void)accessibilityPerformAction:(NSString *)action
{
  // both the ShowMenu and Click action do the same thing.
  if ([self isEnabled]) {
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_4
    if ([action isEqualToString:NSAccessibilityShowMenuAction])
      [self showMenu];
#endif
    if ([action isEqualToString:NSAccessibilityConfirmAction])
      [self confirm];
  }
}

- (void)showMenu
{
  // currently unimplemented. waiting for support in bug 363697
}

- (void)confirm
{
  // should be the same as pressing enter/return in this textfield.
  // not yet implemented
}

@end
