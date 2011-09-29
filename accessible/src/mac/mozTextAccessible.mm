#include "nsAccessibleWrap.h"
#include "nsObjCExceptions.h"

#import "mozTextAccessible.h"

using namespace mozilla::a11y;

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
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if ((self = [super initWithAccessible:accessible])) {
    CallQueryInterface(accessible, &mGeckoTextAccessible);
    CallQueryInterface(accessible, &mGeckoEditableTextAccessible);
  }
  return self;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (BOOL)accessibilityIsIgnored
{
  return mIsExpired;
}

- (NSArray*)accessibilityAttributeNames
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

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

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (id)accessibilityAttributeValue:(NSString*)attribute
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if ([attribute isEqualToString:NSAccessibilityNumberOfCharactersAttribute])
    return [NSNumber numberWithInt:[self textLength]];
  if ([attribute isEqualToString:NSAccessibilitySelectedTextRangeAttribute])
    return [self selectedTextRange];
  if ([attribute isEqualToString:NSAccessibilitySelectedTextAttribute])
    return [self selectedText];
  // Apple's SpeechSynthesisServer expects AXValue to return an AXStaticText
  // object's AXSelectedText attribute.  See bug 674612.
  if ([attribute isEqualToString:NSAccessibilityValueAttribute])
    return [self selectedText];

  // let mozAccessible handle all other attributes
  return [super accessibilityAttributeValue:attribute];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (BOOL)accessibilityIsAttributeSettable:(NSString*)attribute
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  if ([attribute isEqualToString:NSAccessibilityValueAttribute])
    return [self isReadOnly];
  
  return [super accessibilityIsAttributeSettable:attribute];

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NO);
}

- (void)accessibilitySetValue:(id)value forAttribute:(NSString *)attribute
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if ([attribute isEqualToString:NSAccessibilityValueAttribute]) {
    if ([value isKindOfClass:[NSString class]])
      [self setText:(NSString*)value];
  } else
    [super accessibilitySetValue:value forAttribute:attribute];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (NSString*)subrole
{
  // TODO: text accessibles have two different subroles in Cocoa: secure textfield (passwords) and search field
  return nil;
}

- (void)expire
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NS_IF_RELEASE(mGeckoTextAccessible);
  NS_IF_RELEASE(mGeckoEditableTextAccessible);
  [super expire];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

#pragma mark -

- (BOOL)isReadOnly
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  if ([[self role] isEqualToString:NSAccessibilityStaticTextRole])
    return YES;
    
  if (mGeckoEditableTextAccessible)
    return (mGeckoAccessible->State() & states::READONLY) == 0;

  return NO;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NO);
}

- (void)setText:(NSString*)newString
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (mGeckoEditableTextAccessible) {
    mGeckoEditableTextAccessible->SetTextContents(NS_ConvertUTF8toUTF16([newString UTF8String]));
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (long)textLength
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  return mGeckoTextAccessible ? mGeckoTextAccessible->CharacterCount() : 0;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(0);
}

- (long)selectedTextLength
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  if (mGeckoTextAccessible) {
    PRInt32 start, end;
    start = end = 0;
    mGeckoTextAccessible->GetSelectionBounds(0, &start, &end);
    return (end - start);
  }
  return 0;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(0);
}

- (NSString*)selectedText
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

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

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (NSValue*)selectedTextRange
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if (mGeckoTextAccessible) {
    PRInt32 start, end;
    start = end = 0;
    mGeckoTextAccessible->GetSelectionBounds(0, &start, &end);
    return [NSValue valueWithRange:NSMakeRange(start, start-end)];
  }
  return nil;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

#pragma mark -

- (void)valueDidChange
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NSAccessibilityPostNotification([self hasRepresentedView] ? [self representedView] : self, 
                                  NSAccessibilityValueChangedNotification);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

@end

@implementation mozComboboxAccessible

- (NSArray*)accessibilityAttributeNames
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

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

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (NSArray *)accessibilityActionNames
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if ([self isEnabled]) {
    return [NSArray arrayWithObjects:NSAccessibilityConfirmAction,
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_4
                                     NSAccessibilityShowMenuAction,
#endif
                                     nil];
  }
  return nil;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (NSString *)accessibilityActionDescription:(NSString *)action
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_4
  if ([action isEqualToString:NSAccessibilityShowMenuAction])
    return @"show menu";
#endif
  if ([action isEqualToString:NSAccessibilityConfirmAction])
    return @"confirm";
    
  return [super accessibilityActionDescription:action];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (void)accessibilityPerformAction:(NSString *)action
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // both the ShowMenu and Click action do the same thing.
  if ([self isEnabled]) {
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_4
    if ([action isEqualToString:NSAccessibilityShowMenuAction])
      [self showMenu];
#endif
    if ([action isEqualToString:NSAccessibilityConfirmAction])
      [self confirm];
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
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
