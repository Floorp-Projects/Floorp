/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "AccessibleWrap.h"
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
    CallQueryInterface(accessible, &mGeckoEditableTextAccessible);
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
    
    PRInt32 start = range.location;
    PRInt32 end = start + range.length;
    nsIntRect bounds = mGeckoTextAccessible->GetTextBounds(start, end);

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

    PRInt32 start = 0;
    PRInt32 end = 0;

    nsresult rv = mGeckoTextAccessible->GetSelectionBounds(0, &start, &end);
    NS_ENSURE_SUCCESS(rv,);
    
    rv = mGeckoTextAccessible->DeleteText(start, end - start);
    NS_ENSURE_SUCCESS(rv,);

    nsString text;
    nsCocoaUtils::GetStringForNSString(stringValue, text);
    rv = mGeckoTextAccessible->InsertText(text, start);
    NS_ENSURE_SUCCESS(rv,);

    return;
  }

  if ([attribute isEqualToString:NSAccessibilitySelectedTextRangeAttribute]) {
    NSRange range;
    if (!ToNSRange(value, &range))
      return;

    nsresult rv = mGeckoTextAccessible->SetSelectionBounds(0, range.location, 
                                                           range.location + range.length);
    NS_ENSURE_SUCCESS(rv,);

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

  mGeckoTextAccessible = nsnull;
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

- (NSNumber*)caretLineNumber
{
  PRInt32 lineNumber = mGeckoTextAccessible ?
    mGeckoTextAccessible->CaretLineNumber() - 1 : -1;

  return (lineNumber >= 0) ? [NSNumber numberWithInt:lineNumber] : nil;
}

- (void)setText:(NSString*)aNewString
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (mGeckoEditableTextAccessible) {
    nsString text;
    nsCocoaUtils::GetStringForNSString(aNewString, text);
    mGeckoEditableTextAccessible->SetTextContents(text);
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
  nsresult rv = mGeckoTextAccessible->
    GetText(0, nsIAccessibleText::TEXT_OFFSET_END_OF_TEXT, text);
  NS_ENSURE_SUCCESS(rv, @"");

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
    PRInt32 start, end;
    start = end = 0;
    nsresult rv = mGeckoTextAccessible->GetSelectionBounds(0, &start, &end);
    NS_ENSURE_SUCCESS(rv, 0);

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
    PRInt32 start = 0;
    PRInt32 end = 0;
    PRInt32 count = 0;

    nsresult rv = mGeckoTextAccessible->GetSelectionCount(&count);
    NS_ENSURE_SUCCESS(rv, nil);

    if (count) {
      rv = mGeckoTextAccessible->GetSelectionBounds(0, &start, &end);
      NS_ENSURE_SUCCESS(rv, nil);

      return [NSValue valueWithRange:NSMakeRange(start, end - start)];
    }

    rv = mGeckoTextAccessible->GetCaretOffset(&start);
    NS_ENSURE_SUCCESS(rv, nil);
    
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
  mGeckoTextAccessible->GetText(range->location, 
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
