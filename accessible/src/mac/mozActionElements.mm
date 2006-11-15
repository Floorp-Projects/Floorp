#import "mozActionElements.h"
#import "nsIAccessible.h"

extern const NSString *kInstanceDescriptionAttribute; // NSAccessibilityDescriptionAttribute
extern const NSString *kTopLevelUIElementAttribute;   // NSAccessibilityTopLevelUIElementAttribute

@implementation mozButtonAccessible

- (NSArray*)accessibilityAttributeNames
{
  static NSArray *attributes = nil;
  if (!attributes) {
    attributes = [[NSArray alloc] initWithObjects:NSAccessibilityParentAttribute, // required
                                                  NSAccessibilityRoleAttribute, // required
                                                  NSAccessibilityPositionAttribute, // required
                                                  NSAccessibilitySizeAttribute, // required
                                                  NSAccessibilityWindowAttribute, // required
                                                  NSAccessibilityPositionAttribute, // required
                                                  kTopLevelUIElementAttribute, // required
                                                  NSAccessibilityHelpAttribute,
                                                  NSAccessibilityEnabledAttribute, // required
                                                  NSAccessibilityFocusedAttribute, // required
                                                  NSAccessibilityTitleAttribute, // required
                                                  kInstanceDescriptionAttribute,
                                                  nil];
  }
  return attributes;
}

- (BOOL)accessibilityIsIgnored
{
  return mIsExpired;
}

- (NSArray*)accessibilityActionNames
{
  if ([self isEnabled])
    return [NSArray arrayWithObject:NSAccessibilityPressAction];
    
  return nil;
}

- (NSString*)accessibilityActionDescription:(NSString*)action 
{
  if ([action isEqualToString:NSAccessibilityPressAction])
    return @"press button"; // XXX: localize this later?
    
  return nil;
}

- (void)accessibilityPerformAction:(NSString*)action 
{
  if ([action isEqualToString:NSAccessibilityPressAction])
    [self click];
}

- (void)click
{
  // both buttons and checkboxes have only one action. we should really stop using arbitrary
  // arrays with actions, and define constants for these actions.
  mGeckoAccessible->DoAction(0);
}

- (NSArray*)children
{
  return [NSArray array];
}

@end

@implementation mozCheckboxAccessible

- (NSString*)accessibilityActionDescription:(NSString*)action 
{
  if ([action isEqualToString:NSAccessibilityPressAction]) {
    if ([self isChecked])
      return @"uncheck checkbox"; // XXX: localize this later?
    
    return @"check checkbox"; // XXX: localize this later?
  }
  
  return nil;
}

- (BOOL)isChecked
{
  PRUint32 state = 0;
  mGeckoAccessible->GetState(&state);
  return ((state & nsIAccessible::STATE_CHECKED) != 0);
}

- (id)value
{
  // need to handle mixed state here too (return 2)
  return [NSNumber numberWithInt:([self isChecked] ? 1 : 0)];
}

@end
