/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccAttributes.h"
#include "HyperTextAccessible-inl.h"
#include "LocalAccessible-inl.h"
#include "mozilla/a11y/PDocAccessible.h"
#include "nsCocoaUtils.h"
#include "nsObjCExceptions.h"
#include "TextLeafAccessible.h"

#import "mozTextAccessible.h"
#import "GeckoTextMarker.h"
#import "MOXTextMarkerDelegate.h"

using namespace mozilla;
using namespace mozilla::a11y;

inline bool ToNSRange(id aValue, NSRange* aRange) {
  MOZ_ASSERT(aRange, "aRange is nil");

  if ([aValue isKindOfClass:[NSValue class]] &&
      strcmp([(NSValue*)aValue objCType], @encode(NSRange)) == 0) {
    *aRange = [aValue rangeValue];
    return true;
  }

  return false;
}

inline NSString* ToNSString(id aValue) {
  if ([aValue isKindOfClass:[NSString class]]) {
    return aValue;
  }

  return nil;
}

@interface mozTextAccessible ()
- (long)textLength;
- (BOOL)isReadOnly;
- (NSString*)text;
- (GeckoTextMarkerRange)selection;
- (GeckoTextMarkerRange)textMarkerRangeFromRange:(NSValue*)range;
@end

@implementation mozTextAccessible

- (NSString*)moxTitle {
  return @"";
}

- (id)moxValue {
  // Apple's SpeechSynthesisServer expects AXValue to return an AXStaticText
  // object's AXSelectedText attribute. See bug 674612 for details.
  // Also if there is no selected text, we return the full text.
  // See bug 369710 for details.
  if ([[self moxRole] isEqualToString:NSAccessibilityStaticTextRole]) {
    NSString* selectedText = [self moxSelectedText];
    return (selectedText && [selectedText length]) ? selectedText : [self text];
  }

  return [self text];
}

- (id)moxRequired {
  return @([self stateWithMask:states::REQUIRED] != 0);
}

- (NSString*)moxInvalid {
  if ([self stateWithMask:states::INVALID] != 0) {
    // If the attribute exists, it has one of four values: true, false,
    // grammar, or spelling. We query the attribute value here in order
    // to find the correct string to return.
    RefPtr<AccAttributes> attributes;
    HyperTextAccessibleBase* text = mGeckoAccessible->AsHyperTextBase();
    if (text && mGeckoAccessible->IsTextRole()) {
      attributes = text->DefaultTextAttributes();
    }

    nsAutoString invalidStr;
    if (!attributes ||
        !attributes->GetAttribute(nsGkAtoms::invalid, invalidStr)) {
      return @"true";
    }
    return nsCocoaUtils::ToNSString(invalidStr);
  }

  // If the flag is not set, we return false.
  return @"false";
}

- (NSNumber*)moxInsertionPointLineNumber {
  MOZ_ASSERT(mGeckoAccessible);

  int32_t lineNumber = -1;
  if (HyperTextAccessibleBase* textAcc = mGeckoAccessible->AsHyperTextBase()) {
    lineNumber = textAcc->CaretLineNumber() - 1;
  }

  return (lineNumber >= 0) ? [NSNumber numberWithInt:lineNumber] : nil;
}

- (NSString*)moxRole {
  if ([self stateWithMask:states::MULTI_LINE]) {
    return NSAccessibilityTextAreaRole;
  }

  return [super moxRole];
}

- (NSString*)moxSubrole {
  MOZ_ASSERT(mGeckoAccessible);

  if (mRole == roles::PASSWORD_TEXT) {
    return NSAccessibilitySecureTextFieldSubrole;
  }

  if (mRole == roles::ENTRY && mGeckoAccessible->IsSearchbox()) {
    return @"AXSearchField";
  }

  return nil;
}

- (NSNumber*)moxNumberOfCharacters {
  return @([self textLength]);
}

- (NSString*)moxSelectedText {
  GeckoTextMarkerRange selection = [self selection];
  if (!selection.IsValid()) {
    return nil;
  }

  return selection.Text();
}

- (NSValue*)moxSelectedTextRange {
  GeckoTextMarkerRange selection = [self selection];
  if (!selection.IsValid()) {
    return nil;
  }

  GeckoTextMarkerRange fromStartToSelection(
      GeckoTextMarker(mGeckoAccessible, 0), selection.Start());

  return [NSValue valueWithRange:NSMakeRange(fromStartToSelection.Length(),
                                             selection.Length())];
}

- (NSValue*)moxVisibleCharacterRange {
  // XXX this won't work with Textarea and such as we actually don't give
  // the visible character range.
  return [NSValue valueWithRange:NSMakeRange(0, [self textLength])];
}

- (BOOL)moxBlockSelector:(SEL)selector {
  if (selector == @selector(moxSetValue:) && [self isReadOnly]) {
    return YES;
  }

  return [super moxBlockSelector:selector];
}

- (void)moxSetValue:(id)value {
  MOZ_ASSERT(mGeckoAccessible);

  nsString text;
  nsCocoaUtils::GetStringForNSString(value, text);
  if (HyperTextAccessibleBase* textAcc = mGeckoAccessible->AsHyperTextBase()) {
    textAcc->ReplaceText(text);
  }
}

- (void)moxSetSelectedText:(NSString*)selectedText {
  MOZ_ASSERT(mGeckoAccessible);

  NSString* stringValue = ToNSString(selectedText);
  if (!stringValue) {
    return;
  }

  HyperTextAccessibleBase* textAcc = mGeckoAccessible->AsHyperTextBase();
  if (!textAcc) {
    return;
  }
  int32_t start = 0, end = 0;
  textAcc->SelectionBoundsAt(0, &start, &end);
  nsString text;
  nsCocoaUtils::GetStringForNSString(stringValue, text);
  textAcc->SelectionBoundsAt(0, &start, &end);
  textAcc->DeleteText(start, end - start);
  textAcc->InsertText(text, start);
}

- (void)moxSetSelectedTextRange:(NSValue*)selectedTextRange {
  GeckoTextMarkerRange markerRange =
      [self textMarkerRangeFromRange:selectedTextRange];

  if (markerRange.IsValid()) {
    markerRange.Select();
  }
}

- (void)moxSetVisibleCharacterRange:(NSValue*)visibleCharacterRange {
  MOZ_ASSERT(mGeckoAccessible);

  NSRange range;
  if (!ToNSRange(visibleCharacterRange, &range)) {
    return;
  }

  if (HyperTextAccessibleBase* textAcc = mGeckoAccessible->AsHyperTextBase()) {
    textAcc->ScrollSubstringTo(range.location, range.location + range.length,
                               nsIAccessibleScrollType::SCROLL_TYPE_TOP_EDGE);
  }
}

- (NSString*)moxStringForRange:(NSValue*)range {
  GeckoTextMarkerRange markerRange = [self textMarkerRangeFromRange:range];

  if (!markerRange.IsValid()) {
    return nil;
  }

  return markerRange.Text();
}

- (NSAttributedString*)moxAttributedStringForRange:(NSValue*)range {
  GeckoTextMarkerRange markerRange = [self textMarkerRangeFromRange:range];

  if (!markerRange.IsValid()) {
    return nil;
  }

  return markerRange.AttributedText();
}

- (NSValue*)moxRangeForLine:(NSNumber*)line {
  // XXX: actually get the integer value for the line #
  return [NSValue valueWithRange:NSMakeRange(0, [self textLength])];
}

- (NSNumber*)moxLineForIndex:(NSNumber*)index {
  // XXX: actually return the line #
  return @0;
}

- (NSValue*)moxBoundsForRange:(NSValue*)range {
  GeckoTextMarkerRange markerRange = [self textMarkerRangeFromRange:range];

  if (!markerRange.IsValid()) {
    return nil;
  }

  return markerRange.Bounds();
}

#pragma mark - mozAccessible

- (void)handleAccessibleTextChangeEvent:(NSString*)change
                               inserted:(BOOL)isInserted
                            inContainer:(Accessible*)container
                                     at:(int32_t)start {
  GeckoTextMarker startMarker(container, start);
  NSDictionary* userInfo = @{
    @"AXTextChangeElement" : self,
    @"AXTextStateChangeType" : @(AXTextStateChangeTypeEdit),
    @"AXTextChangeValues" : @[ @{
      @"AXTextChangeValue" : (change ? change : @""),
      @"AXTextChangeValueStartMarker" :
          (__bridge id)startMarker.CreateAXTextMarker(),
      @"AXTextEditType" : isInserted ? @(AXTextEditTypeTyping)
                                     : @(AXTextEditTypeDelete)
    } ]
  };

  mozAccessible* webArea = [self topWebArea];
  [webArea moxPostNotification:NSAccessibilityValueChangedNotification
                  withUserInfo:userInfo];
  [self moxPostNotification:NSAccessibilityValueChangedNotification
               withUserInfo:userInfo];

  [self moxPostNotification:NSAccessibilityValueChangedNotification];
}

- (void)handleAccessibleEvent:(uint32_t)eventType {
  switch (eventType) {
    default:
      [super handleAccessibleEvent:eventType];
      break;
  }
}

#pragma mark -

- (long)textLength {
  return [[self text] length];
}

- (BOOL)isReadOnly {
  return [self stateWithMask:states::EDITABLE] == 0;
}

- (NSString*)text {
  // A password text field returns an empty value
  if (mRole == roles::PASSWORD_TEXT) {
    return @"";
  }

  id<MOXTextMarkerSupport> delegate = [self moxTextMarkerDelegate];
  return [delegate
      moxStringForTextMarkerRange:[delegate
                                      moxTextMarkerRangeForUIElement:self]];
}

- (GeckoTextMarkerRange)selection {
  MOZ_ASSERT(mGeckoAccessible);

  id<MOXTextMarkerSupport> delegate = [self moxTextMarkerDelegate];
  GeckoTextMarkerRange selection =
      [static_cast<MOXTextMarkerDelegate*>(delegate) selection];

  if (!selection.IsValid() || !selection.Crop(mGeckoAccessible)) {
    // The selection is not in this accessible. Return invalid range.
    return GeckoTextMarkerRange();
  }

  return selection;
}

- (GeckoTextMarkerRange)textMarkerRangeFromRange:(NSValue*)range {
  NSRange r = [range rangeValue];

  GeckoTextMarker startMarker =
      GeckoTextMarker::MarkerFromIndex(mGeckoAccessible, r.location);

  GeckoTextMarker endMarker =
      GeckoTextMarker::MarkerFromIndex(mGeckoAccessible, r.location + r.length);

  return GeckoTextMarkerRange(startMarker, endMarker);
}

@end

@implementation mozTextLeafAccessible

- (BOOL)moxBlockSelector:(SEL)selector {
  if (selector == @selector(moxChildren) || selector == @selector
                                                (moxTitleUIElement)) {
    return YES;
  }

  return [super moxBlockSelector:selector];
}

- (NSString*)moxValue {
  NSString* val = [super moxTitle];
  return [val length] ? val : nil;
}

- (NSString*)moxTitle {
  return nil;
}

- (NSString*)moxLabel {
  return nil;
}

- (BOOL)moxIgnoreWithParent:(mozAccessible*)parent {
  // Don't render text nodes that are completely empty
  // or those that should be ignored based on our
  // standard ignore rules
  return [self moxValue] == nil || [super moxIgnoreWithParent:parent];
}

static GeckoTextMarkerRange TextMarkerSubrange(Accessible* aAccessible,
                                               NSValue* aRange) {
  GeckoTextMarkerRange textMarkerRange(aAccessible);
  GeckoTextMarker start = textMarkerRange.Start();
  GeckoTextMarker end = textMarkerRange.End();

  NSRange r = [aRange rangeValue];
  start.Offset() += r.location;
  end.Offset() = start.Offset() + r.length;

  textMarkerRange = GeckoTextMarkerRange(start, end);
  // Crop range to accessible
  textMarkerRange.Crop(aAccessible);

  return textMarkerRange;
}

- (NSString*)moxStringForRange:(NSValue*)range {
  MOZ_ASSERT(mGeckoAccessible);
  GeckoTextMarkerRange textMarkerRange =
      TextMarkerSubrange(mGeckoAccessible, range);

  return textMarkerRange.Text();
}

- (NSAttributedString*)moxAttributedStringForRange:(NSValue*)range {
  MOZ_ASSERT(mGeckoAccessible);
  GeckoTextMarkerRange textMarkerRange =
      TextMarkerSubrange(mGeckoAccessible, range);

  return textMarkerRange.AttributedText();
}

- (NSValue*)moxBoundsForRange:(NSValue*)range {
  MOZ_ASSERT(mGeckoAccessible);
  GeckoTextMarkerRange textMarkerRange =
      TextMarkerSubrange(mGeckoAccessible, range);

  return textMarkerRange.Bounds();
}

@end
