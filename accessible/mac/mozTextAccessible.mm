/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Accessible-inl.h"
#include "HyperTextAccessible-inl.h"
#include "TextLeafAccessible.h"

#include "nsCocoaUtils.h"
#include "nsObjCExceptions.h"

#import "mozTextAccessible.h"

using namespace mozilla::a11y;

inline bool
ToNSRange(id aValue, NSRange* aRange)
{
  NS_PRECONDITION(aRange, "aRange is nil");

  if ([aValue isKindOfClass:[NSValue class]] && 
      strcmp([(NSValue*)aValue objCType], @encode(NSRange)) == 0) {
    *aRange = [aValue rangeValue];
    return true;
  }

  return false;
}

inline NSString*
ToNSString(id aValue)
{
  if ([aValue isKindOfClass:[NSString class]]) {
    return aValue;
  }

  return nil;
}

@interface mozTextAccessible ()
- (NSString*)subrole;
- (NSString*)selectedText;
- (NSValue*)selectedTextRange;
- (NSValue*)visibleCharacterRange;
- (long)textLength;
- (BOOL)isReadOnly;
- (NSNumber*)caretLineNumber;
- (void)setText:(NSString*)newText;
- (NSString*)text;
- (NSString*)stringFromRange:(NSRange*)range;
@end

@implementation mozTextAccessible

- (id)initWithAccessible:(AccessibleWrap*)accessible
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if ((self = [super initWithAccessible:accessible])) {
    mGeckoTextAccessible = accessible->AsHyperText();
  }
  return self;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (BOOL)accessibilityIsIgnored
{
  return !mGeckoAccessible;
}

- (NSArray*)accessibilityAttributeNames
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  static NSMutableArray* supportedAttributes = nil;
  if (!supportedAttributes) {
    // text-specific attributes to supplement the standard one
    supportedAttributes = [[NSMutableArray alloc] initWithObjects:
      NSAccessibilitySelectedTextAttribute, // required
      NSAccessibilitySelectedTextRangeAttribute, // required
      NSAccessibilityNumberOfCharactersAttribute, // required
      NSAccessibilityVisibleCharacterRangeAttribute, // required
      NSAccessibilityInsertionPointLineNumberAttribute,
      @"AXRequired",
      @"AXInvalid",
      nil
    ];
    [supportedAttributes addObjectsFromArray:[super accessibilityAttributeNames]];
  }
  return supportedAttributes;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (id)accessibilityAttributeValue:(NSString*)attribute
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if ([attribute isEqualToString:NSAccessibilityNumberOfCharactersAttribute])
    return [NSNumber numberWithInt:[self textLength]];

  if ([attribute isEqualToString:NSAccessibilityInsertionPointLineNumberAttribute])
    return [self caretLineNumber];

  if ([attribute isEqualToString:NSAccessibilitySelectedTextRangeAttribute])
    return [self selectedTextRange];

  if ([attribute isEqualToString:NSAccessibilitySelectedTextAttribute])
    return [self selectedText];

  if ([attribute isEqualToString:NSAccessibilityTitleAttribute])
    return @"";

  if ([attribute isEqualToString:NSAccessibilityValueAttribute]) {
    // Apple's SpeechSynthesisServer expects AXValue to return an AXStaticText
    // object's AXSelectedText attribute. See bug 674612 for details.
    // Also if there is no selected text, we return the full text. 
    // See bug 369710 for details.
    if ([[self role] isEqualToString:NSAccessibilityStaticTextRole]) {
      NSString* selectedText = [self selectedText];
      return (selectedText && [selectedText length]) ? selectedText : [self text];
    }

    return [self text];
  }

  if ([attribute isEqualToString:@"AXRequired"])
    return [NSNumber numberWithBool:!!(mGeckoAccessible->State() & states::REQUIRED)];

  if ([attribute isEqualToString:@"AXInvalid"])
    return [NSNumber numberWithBool:!!(mGeckoAccessible->State() & states::INVALID)];

  if ([attribute isEqualToString:NSAccessibilityVisibleCharacterRangeAttribute])
    return [self visibleCharacterRange];

  // let mozAccessible handle all other attributes
  return [super accessibilityAttributeValue:attribute];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (NSArray*)accessibilityParameterizedAttributeNames
{
  static NSArray* supportedParametrizedAttributes = nil;
  // text specific parametrized attributes
  if (!supportedParametrizedAttributes) {
    supportedParametrizedAttributes = [[NSArray alloc] initWithObjects:
      NSAccessibilityStringForRangeParameterizedAttribute,
      NSAccessibilityLineForIndexParameterizedAttribute,
      NSAccessibilityRangeForLineParameterizedAttribute,
      NSAccessibilityAttributedStringForRangeParameterizedAttribute,
      NSAccessibilityBoundsForRangeParameterizedAttribute,
#if DEBUG
      NSAccessibilityRangeForPositionParameterizedAttribute,
      NSAccessibilityRangeForIndexParameterizedAttribute,
      NSAccessibilityRTFForRangeParameterizedAttribute,
      NSAccessibilityStyleRangeForIndexParameterizedAttribute,
#endif
      nil
    ];
  }
  return supportedParametrizedAttributes;
}

- (id)accessibilityAttributeValue:(NSString*)attribute forParameter:(id)parameter
{
  if (!mGeckoTextAccessible)
    return nil;

  if ([attribute isEqualToString:NSAccessibilityStringForRangeParameterizedAttribute]) {
    NSRange range;
    if (!ToNSRange(parameter, &range)) {
#if DEBUG
      NSLog(@"%@: range not set", attribute);
#endif
      return @"";
    }

    return [self stringFromRange:&range];
  }

  if ([attribute isEqualToString:NSAccessibilityRangeForLineParameterizedAttribute]) {
    // XXX: actually get the integer value for the line #
    return [NSValue valueWithRange:NSMakeRange(0, [self textLength])];
  }

  if ([attribute isEqualToString:NSAccessibilityAttributedStringForRangeParameterizedAttribute]) {
    NSRange range;
    if (!ToNSRange(parameter, &range)) {
#if DEBUG
      NSLog(@"%@: range not set", attribute);
#endif
      return @"";
    }

    return [[[NSAttributedString alloc] initWithString:[self stringFromRange:&range]] autorelease];
  }

  if ([attribute isEqualToString:NSAccessibilityLineForIndexParameterizedAttribute]) {
    // XXX: actually return the line #
    return [NSNumber numberWithInt:0];
  }

  if ([attribute isEqualToString:NSAccessibilityBoundsForRangeParameterizedAttribute]) {
    NSRange range;
    if (!ToNSRange(parameter, &range)) {
#if DEBUG
      NSLog(@"%@:no range", attribute);
#endif
      return nil;
    }

    int32_t start = range.location;
    int32_t end = start + range.length;
    nsIntRect bounds = mGeckoTextAccessible->TextBounds(start, end);

    return [NSValue valueWithRect:nsCocoaUtils::GeckoRectToCocoaRect(bounds)];
  }

#if DEBUG
  NSLog(@"unhandled attribute:%@ forParameter:%@", attribute, parameter);
#endif

  return nil;
}

- (BOOL)accessibilityIsAttributeSettable:(NSString*)attribute
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  if ([attribute isEqualToString:NSAccessibilityValueAttribute])
    return ![self isReadOnly];
  
  if ([attribute isEqualToString:NSAccessibilitySelectedTextAttribute] ||
      [attribute isEqualToString:NSAccessibilitySelectedTextRangeAttribute] ||
      [attribute isEqualToString:NSAccessibilityVisibleCharacterRangeAttribute])
    return YES;

  return [super accessibilityIsAttributeSettable:attribute];

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NO);
}

- (void)accessibilitySetValue:(id)value forAttribute:(NSString *)attribute
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mGeckoTextAccessible)
    return;

  if ([attribute isEqualToString:NSAccessibilityValueAttribute]) {
    [self setText:ToNSString(value)];
    
    return;
  }

  if ([attribute isEqualToString:NSAccessibilitySelectedTextAttribute]) {
    NSString* stringValue = ToNSString(value);
    if (!stringValue)
      return;

    int32_t start = 0, end = 0;
    mGeckoTextAccessible->SelectionBoundsAt(0, &start, &end);
    mGeckoTextAccessible->DeleteText(start, end - start);

    nsString text;
    nsCocoaUtils::GetStringForNSString(stringValue, text);
    mGeckoTextAccessible->InsertText(text, start);
  }

  if ([attribute isEqualToString:NSAccessibilitySelectedTextRangeAttribute]) {
    NSRange range;
    if (!ToNSRange(value, &range))
      return;

    mGeckoTextAccessible->SetSelectionBoundsAt(0, range.location,
                                               range.location + range.length);
    return;
  }

  if ([attribute isEqualToString:NSAccessibilityVisibleCharacterRangeAttribute]) {
    NSRange range;
    if (!ToNSRange(value, &range))
      return;

    mGeckoTextAccessible->ScrollSubstringTo(range.location, range.location + range.length,
                                            nsIAccessibleScrollType::SCROLL_TYPE_TOP_EDGE);
    return;
  } 

  [super accessibilitySetValue:value forAttribute:attribute];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (NSString*)subrole
{
  if(mRole == roles::PASSWORD_TEXT)
    return NSAccessibilitySecureTextFieldSubrole;

  return nil;
}

- (void)expire
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  mGeckoTextAccessible = nullptr;
  [super expire];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

#pragma mark -

- (BOOL)isReadOnly
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  if ([[self role] isEqualToString:NSAccessibilityStaticTextRole])
    return YES;

  if (mGeckoTextAccessible)
    return (mGeckoAccessible->State() & states::READONLY) == 0;

  return NO;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NO);
}

- (NSNumber*)caretLineNumber
{
  int32_t lineNumber = mGeckoTextAccessible ?
    mGeckoTextAccessible->CaretLineNumber() - 1 : -1;

  return (lineNumber >= 0) ? [NSNumber numberWithInt:lineNumber] : nil;
}

- (void)setText:(NSString*)aNewString
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (mGeckoTextAccessible) {
    nsString text;
    nsCocoaUtils::GetStringForNSString(aNewString, text);
    mGeckoTextAccessible->ReplaceText(text);
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (NSString*)text
{
  if (!mGeckoAccessible || !mGeckoTextAccessible)
    return nil;

  // A password text field returns an empty value
  if (mRole == roles::PASSWORD_TEXT)
    return @"";

  nsAutoString text;
  mGeckoTextAccessible->TextSubstring(0,
                                      nsIAccessibleText::TEXT_OFFSET_END_OF_TEXT,
                                      text);
  return nsCocoaUtils::ToNSString(text);
}

- (long)textLength
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  if (!mGeckoAccessible || !mGeckoTextAccessible)
    return 0;

  return mGeckoTextAccessible ? mGeckoTextAccessible->CharacterCount() : 0;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(0);
}

- (long)selectedTextLength
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  if (mGeckoTextAccessible) {
    int32_t start = 0, end = 0;
    mGeckoTextAccessible->SelectionBoundsAt(0, &start, &end);
    return (end - start);
  }
  return 0;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(0);
}

- (NSString*)selectedText
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if (mGeckoTextAccessible) {
    int32_t start = 0, end = 0;
    mGeckoTextAccessible->SelectionBoundsAt(0, &start, &end);
    if (start != end) {
      nsAutoString selText;
      mGeckoTextAccessible->TextSubstring(start, end, selText);
      return nsCocoaUtils::ToNSString(selText);
    }
  }
  return nil;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (NSValue*)selectedTextRange
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if (mGeckoTextAccessible) {
    int32_t start = 0;
    int32_t end = 0;
    int32_t count = mGeckoTextAccessible->SelectionCount();

    if (count) {
      mGeckoTextAccessible->SelectionBoundsAt(0, &start, &end);
      return [NSValue valueWithRange:NSMakeRange(start, end - start)];
    }

    start = mGeckoTextAccessible->CaretOffset();
    return [NSValue valueWithRange:NSMakeRange(start != -1 ? start : 0, 0)]; 
  }
  return [NSValue valueWithRange:NSMakeRange(0, 0)];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (NSValue*)visibleCharacterRange
{
  // XXX this won't work with Textarea and such as we actually don't give
  // the visible character range.
  return [NSValue valueWithRange:
    NSMakeRange(0, mGeckoTextAccessible ? 
                mGeckoTextAccessible->CharacterCount() : 0)];
}

- (void)valueDidChange
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NSAccessibilityPostNotification(GetObjectOrRepresentedView(self),
                                  NSAccessibilityValueChangedNotification);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)selectedTextDidChange
{
  NSAccessibilityPostNotification(GetObjectOrRepresentedView(self),
                                  NSAccessibilitySelectedTextChangedNotification);
}

- (NSString*)stringFromRange:(NSRange*)range
{
  NS_PRECONDITION(mGeckoTextAccessible && range, "no Gecko text accessible or range");

  nsAutoString text;
  mGeckoTextAccessible->TextSubstring(range->location,
                                      range->location + range->length, text);
  return nsCocoaUtils::ToNSString(text);
}

@end

@implementation mozTextLeafAccessible

- (NSArray*)accessibilityAttributeNames
{
  static NSMutableArray* supportedAttributes = nil;
  if (!supportedAttributes) {
    supportedAttributes = [[super accessibilityAttributeNames] mutableCopy];
    [supportedAttributes removeObject:NSAccessibilityChildrenAttribute];
  }

  return supportedAttributes;
}

- (id)accessibilityAttributeValue:(NSString*)attribute
{
  if ([attribute isEqualToString:NSAccessibilityTitleAttribute])
    return @"";

  if ([attribute isEqualToString:NSAccessibilityValueAttribute])
    return [self text];

  return [super accessibilityAttributeValue:attribute];
}

- (NSString*)text
{
  if (!mGeckoAccessible)
    return nil;

  return nsCocoaUtils::ToNSString(mGeckoAccessible->AsTextLeaf()->Text());
}

- (long)textLength
{
  if (!mGeckoAccessible)
    return 0;

  return mGeckoAccessible->AsTextLeaf()->Text().Length();
}

@end
