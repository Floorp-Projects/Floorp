/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Accessible-inl.h"
#include "HyperTextAccessible-inl.h"
#include "mozilla/a11y/PDocAccessible.h"
#include "nsCocoaUtils.h"
#include "nsIPersistentProperties2.h"
#include "nsObjCExceptions.h"
#include "TextLeafAccessible.h"

#import "mozTextAccessible.h"
#import "GeckoTextMarker.h"

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
    if (Accessible* acc = mGeckoAccessible.AsAccessible()) {
      HyperTextAccessible* text = acc->AsHyperText();
      if (!text || !text->IsTextRole()) {
        // we can't get the attribute, but we should still respect the
        // invalid state flag
        return @"true";
      }
      nsAutoString invalidStr;
      nsCOMPtr<nsIPersistentProperties> attributes =
          text->DefaultTextAttributes();
      nsAccUtils::GetAccAttr(attributes, nsGkAtoms::invalid, invalidStr);
      if (invalidStr.IsEmpty()) {
        // if the attribute had no value, we should still respect the
        // invalid state flag.
        return @"true";
      }
      return nsCocoaUtils::ToNSString(invalidStr);
    } else {
      ProxyAccessible* proxy = mGeckoAccessible.AsProxy();
      // Similar to the acc case above, we iterate through our attributes
      // to find the value for `invalid`.
      AutoTArray<Attribute, 10> attrs;
      proxy->DefaultTextAttributes(&attrs);
      for (size_t i = 0; i < attrs.Length(); i++) {
        if (attrs.ElementAt(i).Name() == "invalid") {
          nsString invalidStr = attrs.ElementAt(i).Value();
          if (invalidStr.IsEmpty()) {
            break;
          }
          return nsCocoaUtils::ToNSString(invalidStr);
        }
      }
      // if we iterated through our attributes and didn't find `invalid`,
      // or if the invalid attribute had no value, we should still respect
      // the invalid flag and return true.
      return @"true";
    }
  }
  // If the flag is not set, we return false.
  return @"false";
}

- (NSNumber*)moxInsertionPointLineNumber {
  MOZ_ASSERT(!mGeckoAccessible.IsNull());

  int32_t lineNumber = -1;
  if (mGeckoAccessible.IsAccessible()) {
    if (HyperTextAccessible* textAcc =
            mGeckoAccessible.AsAccessible()->AsHyperText()) {
      lineNumber = textAcc->CaretLineNumber() - 1;
    }
  } else {
    lineNumber = mGeckoAccessible.AsProxy()->CaretLineNumber() - 1;
  }

  return (lineNumber >= 0) ? [NSNumber numberWithInt:lineNumber] : nil;
}

- (NSString*)moxSubrole {
  MOZ_ASSERT(!mGeckoAccessible.IsNull());

  if (mRole == roles::PASSWORD_TEXT) {
    return NSAccessibilitySecureTextFieldSubrole;
  }

  if (mRole == roles::ENTRY) {
    Accessible* acc = mGeckoAccessible.AsAccessible();
    ProxyAccessible* proxy = mGeckoAccessible.AsProxy();
    if ((acc && acc->IsSearchbox()) || (proxy && proxy->IsSearchbox())) {
      return @"AXSearchField";
    }
  }

  return nil;
}

- (NSNumber*)moxNumberOfCharacters {
  MOZ_ASSERT(!mGeckoAccessible.IsNull());

  Accessible* acc = mGeckoAccessible.AsAccessible();
  ProxyAccessible* proxy = mGeckoAccessible.AsProxy();
  HyperTextAccessible* textAcc = acc ? acc->AsHyperText() : nullptr;
  if (!textAcc && !proxy) {
    return @0;
  }

  return textAcc ? @(textAcc->CharacterCount()) : @(proxy->CharacterCount());
}

- (NSString*)moxSelectedText {
  MOZ_ASSERT(!mGeckoAccessible.IsNull());

  int32_t start = 0, end = 0;
  nsAutoString selText;
  if (mGeckoAccessible.IsAccessible()) {
    if (HyperTextAccessible* textAcc =
            mGeckoAccessible.AsAccessible()->AsHyperText()) {
      textAcc->SelectionBoundsAt(0, &start, &end);
      if (start != end) {
        textAcc->TextSubstring(start, end, selText);
      }
    }
  } else {
    mGeckoAccessible.AsProxy()->SelectionBoundsAt(0, selText, &start, &end);
  }

  return nsCocoaUtils::ToNSString(selText);
}

- (NSValue*)moxSelectedTextRange {
  MOZ_ASSERT(!mGeckoAccessible.IsNull());

  int32_t start = 0;
  int32_t end = 0;
  if (mGeckoAccessible.IsAccessible()) {
    if (HyperTextAccessible* textAcc =
            mGeckoAccessible.AsAccessible()->AsHyperText()) {
      if (textAcc->SelectionCount()) {
        textAcc->SelectionBoundsAt(0, &start, &end);
      } else {
        start = textAcc->CaretOffset();
        end = start != -1 ? start : 0;
      }
    }
  } else {
    ProxyAccessible* proxy = mGeckoAccessible.AsProxy();
    if (proxy->SelectionCount()) {
      nsString data;
      proxy->SelectionBoundsAt(0, data, &start, &end);
    } else {
      start = proxy->CaretOffset();
      end = start != -1 ? start : 0;
    }
  }

  return [NSValue valueWithRange:NSMakeRange(start, end - start)];
}

- (NSValue*)moxVisibleCharacterRange {
  // XXX this won't work with Textarea and such as we actually don't give
  // the visible character range.
  Accessible* acc = mGeckoAccessible.AsAccessible();
  ProxyAccessible* proxy = mGeckoAccessible.AsProxy();
  HyperTextAccessible* textAcc = acc ? acc->AsHyperText() : nullptr;
  if (!textAcc && !proxy) {
    return nil;
  }

  return [NSValue
      valueWithRange:NSMakeRange(0, textAcc ? textAcc->CharacterCount()
                                            : proxy->CharacterCount())];
}

- (BOOL)moxBlockSelector:(SEL)selector {
  if (selector == @selector(moxSetValue:) && [self isReadOnly]) {
    return YES;
  }

  return [super moxBlockSelector:selector];
}

- (void)moxSetValue:(id)value {
  MOZ_ASSERT(!mGeckoAccessible.IsNull());

  nsString text;
  nsCocoaUtils::GetStringForNSString(value, text);
  if (mGeckoAccessible.IsAccessible()) {
    if (HyperTextAccessible* textAcc =
            mGeckoAccessible.AsAccessible()->AsHyperText()) {
      textAcc->ReplaceText(text);
    }
  } else {
    mGeckoAccessible.AsProxy()->ReplaceText(text);
  }
}

- (void)moxSetSelectedText:(NSString*)selectedText {
  MOZ_ASSERT(!mGeckoAccessible.IsNull());

  NSString* stringValue = ToNSString(selectedText);
  if (!stringValue) {
    return;
  }

  int32_t start = 0, end = 0;
  nsString text;
  if (mGeckoAccessible.IsAccessible()) {
    if (HyperTextAccessible* textAcc =
            mGeckoAccessible.AsAccessible()->AsHyperText()) {
      textAcc->SelectionBoundsAt(0, &start, &end);
      textAcc->DeleteText(start, end - start);
      nsCocoaUtils::GetStringForNSString(stringValue, text);
      textAcc->InsertText(text, start);
    }
  } else {
    ProxyAccessible* proxy = mGeckoAccessible.AsProxy();
    nsString data;
    proxy->SelectionBoundsAt(0, data, &start, &end);
    proxy->DeleteText(start, end - start);
    nsCocoaUtils::GetStringForNSString(stringValue, text);
    proxy->InsertText(text, start);
  }
}

- (void)moxSetSelectedTextRange:(NSValue*)selectedTextRange {
  MOZ_ASSERT(!mGeckoAccessible.IsNull());

  NSRange range;
  if (!ToNSRange(selectedTextRange, &range)) {
    return;
  }

  if (mGeckoAccessible.IsAccessible()) {
    if (HyperTextAccessible* textAcc =
            mGeckoAccessible.AsAccessible()->AsHyperText()) {
      textAcc->SetSelectionBoundsAt(0, range.location,
                                    range.location + range.length);
    }
  } else {
    mGeckoAccessible.AsProxy()->SetSelectionBoundsAt(
        0, range.location, range.location + range.length);
  }
}

- (void)moxSetVisibleCharacterRange:(NSValue*)visibleCharacterRange {
  MOZ_ASSERT(!mGeckoAccessible.IsNull());

  NSRange range;
  if (!ToNSRange(visibleCharacterRange, &range)) {
    return;
  }

  if (mGeckoAccessible.IsAccessible()) {
    if (HyperTextAccessible* textAcc =
            mGeckoAccessible.AsAccessible()->AsHyperText()) {
      textAcc->ScrollSubstringTo(range.location, range.location + range.length,
                                 nsIAccessibleScrollType::SCROLL_TYPE_TOP_EDGE);
    }
  } else {
    mGeckoAccessible.AsProxy()->ScrollSubstringTo(
        range.location, range.location + range.length,
        nsIAccessibleScrollType::SCROLL_TYPE_TOP_EDGE);
  }
}

- (NSString*)moxStringForRange:(NSValue*)range {
  MOZ_ASSERT(!mGeckoAccessible.IsNull());

  NSRange r = [range rangeValue];
  nsAutoString text;
  if (mGeckoAccessible.IsAccessible()) {
    if (HyperTextAccessible* textAcc =
            mGeckoAccessible.AsAccessible()->AsHyperText()) {
      textAcc->TextSubstring(r.location, r.location + r.length, text);
    }
  } else {
    mGeckoAccessible.AsProxy()->TextSubstring(r.location, r.location + r.length,
                                              text);
  }

  return nsCocoaUtils::ToNSString(text);
}

- (NSAttributedString*)moxAttributedStringForRange:(NSValue*)range {
  return [[[NSAttributedString alloc]
      initWithString:[self moxStringForRange:range]] autorelease];
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
  MOZ_ASSERT(!mGeckoAccessible.IsNull());

  NSRange r = [range rangeValue];
  int32_t start = r.location;
  int32_t end = start + r.length;
  DesktopIntRect bounds;
  if (mGeckoAccessible.IsAccessible()) {
    if (HyperTextAccessible* textAcc =
            mGeckoAccessible.AsAccessible()->AsHyperText()) {
      bounds = DesktopIntRect::FromUnknownRect(textAcc->TextBounds(start, end));
    }
  } else {
    bounds = DesktopIntRect::FromUnknownRect(
        mGeckoAccessible.AsProxy()->TextBounds(start, end));
  }

  return [NSValue valueWithRect:nsCocoaUtils::GeckoRectToCocoaRect(bounds)];
}

#pragma mark - mozAccessible

enum AXTextEditType {
  AXTextEditTypeUnknown,
  AXTextEditTypeDelete,
  AXTextEditTypeInsert,
  AXTextEditTypeTyping,
  AXTextEditTypeDictation,
  AXTextEditTypeCut,
  AXTextEditTypePaste,
  AXTextEditTypeAttributesChange
};

enum AXTextStateChangeType {
  AXTextStateChangeTypeUnknown,
  AXTextStateChangeTypeEdit,
  AXTextStateChangeTypeSelectionMove,
  AXTextStateChangeTypeSelectionExtend
};

- (void)handleAccessibleTextChangeEvent:(NSString*)change
                               inserted:(BOOL)isInserted
                            inContainer:(const AccessibleOrProxy&)container
                                     at:(int32_t)start {
  GeckoTextMarker startMarker(container, start);
  NSDictionary* userInfo = @{
    @"AXTextChangeElement" : self,
    @"AXTextStateChangeType" : @(AXTextStateChangeTypeEdit),
    @"AXTextChangeValues" : @[ @{
      @"AXTextChangeValue" : (change ? change : @""),
      @"AXTextChangeValueStartMarker" : startMarker.CreateAXTextMarker(),
      @"AXTextEditType" : isInserted ? @(AXTextEditTypeTyping)
                                     : @(AXTextEditTypeDelete)
    } ]
  };

  mozAccessible* webArea = GetNativeFromGeckoAccessible([self geckoDocument]);
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
  MOZ_ASSERT(!mGeckoAccessible.IsNull());

  Accessible* acc = mGeckoAccessible.AsAccessible();
  ProxyAccessible* proxy = mGeckoAccessible.AsProxy();
  HyperTextAccessible* textAcc = acc ? acc->AsHyperText() : nullptr;
  if (!textAcc && !proxy) {
    return 0;
  }

  return textAcc ? textAcc->CharacterCount() : proxy->CharacterCount();
}

- (BOOL)isReadOnly {
  return [self stateWithMask:states::EDITABLE] == 0;
}

- (NSString*)text {
  MOZ_ASSERT(!mGeckoAccessible.IsNull());

  // A password text field returns an empty value
  if (mRole == roles::PASSWORD_TEXT) {
    return @"";
  }

  nsAutoString text;
  if (mGeckoAccessible.IsAccessible()) {
    if (HyperTextAccessible* textAcc =
            mGeckoAccessible.AsAccessible()->AsHyperText()) {
      textAcc->TextSubstring(0, nsIAccessibleText::TEXT_OFFSET_END_OF_TEXT,
                             text);
    }
  } else {
    mGeckoAccessible.AsProxy()->TextSubstring(
        0, nsIAccessibleText::TEXT_OFFSET_END_OF_TEXT, text);
  }

  return nsCocoaUtils::ToNSString(text);
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
  return [super moxTitle];
}

- (NSString*)moxTitle {
  return nil;
}

- (NSString*)moxLabel {
  return nil;
}

- (NSString*)moxStringForRange:(NSValue*)range {
  MOZ_ASSERT(!mGeckoAccessible.IsNull());

  NSRange r = [range rangeValue];
  GeckoTextMarkerRange textMarkerRange(mGeckoAccessible);
  textMarkerRange.mStart.mOffset += r.location;
  textMarkerRange.mEnd.mOffset =
      textMarkerRange.mStart.mOffset + r.location + r.length;

  return textMarkerRange.Text();
}

- (NSAttributedString*)moxAttributedStringForRange:(NSValue*)range {
  return [[[NSAttributedString alloc]
      initWithString:[self moxStringForRange:range]] autorelease];
}

- (NSValue*)moxBoundsForRange:(NSValue*)range {
  MOZ_ASSERT(!mGeckoAccessible.IsNull());

  NSRange r = [range rangeValue];
  GeckoTextMarkerRange textMarkerRange(mGeckoAccessible);

  textMarkerRange.mStart.mOffset += r.location;
  textMarkerRange.mEnd.mOffset = textMarkerRange.mStart.mOffset + r.length;

  return textMarkerRange.Bounds();
}

@end
