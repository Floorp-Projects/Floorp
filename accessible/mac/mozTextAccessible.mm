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

- (NSArray*)accessibilityAttributeNames {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  static NSMutableArray* supportedAttributes = nil;
  if (!supportedAttributes) {
    // text-specific attributes to supplement the standard one
    supportedAttributes = [[NSMutableArray alloc]
        initWithObjects:NSAccessibilitySelectedTextAttribute,           // required
                        NSAccessibilitySelectedTextRangeAttribute,      // required
                        NSAccessibilityNumberOfCharactersAttribute,     // required
                        NSAccessibilityVisibleCharacterRangeAttribute,  // required
                        NSAccessibilityInsertionPointLineNumberAttribute, @"AXRequired",
                        @"AXInvalid", nil];
    [supportedAttributes addObjectsFromArray:[super accessibilityAttributeNames]];
  }
  return supportedAttributes;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (id)accessibilityAttributeValue:(NSString*)attribute {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if ([attribute isEqualToString:NSAccessibilityNumberOfCharactersAttribute]) {
    return [NSNumber numberWithInt:[self textLength]];
  }

  if ([attribute isEqualToString:NSAccessibilityInsertionPointLineNumberAttribute]) {
    return [self caretLineNumber];
  }

  if ([attribute isEqualToString:NSAccessibilitySelectedTextRangeAttribute]) {
    return [self selectedTextRange];
  }

  if ([attribute isEqualToString:NSAccessibilitySelectedTextAttribute]) {
    return [self selectedText];
  }

  if ([attribute isEqualToString:NSAccessibilityTitleAttribute]) {
    return @"";
  }

  if ([attribute isEqualToString:NSAccessibilityValueAttribute]) {
    // Apple's SpeechSynthesisServer expects AXValue to return an AXStaticText
    // object's AXSelectedText attribute. See bug 674612 for details.
    // Also if there is no selected text, we return the full text.
    // See bug 369710 for details.
    if ([[self moxRole] isEqualToString:NSAccessibilityStaticTextRole]) {
      NSString* selectedText = [self selectedText];
      return (selectedText && [selectedText length]) ? selectedText : [self text];
    }

    return [self text];
  }

  if ([attribute isEqualToString:@"AXRequired"]) {
    return [NSNumber numberWithBool:[self stateWithMask:states::REQUIRED] != 0];
  }

  if ([attribute isEqualToString:@"AXInvalid"]) {
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
        nsCOMPtr<nsIPersistentProperties> attributes = text->DefaultTextAttributes();
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

  if ([attribute isEqualToString:NSAccessibilityVisibleCharacterRangeAttribute]) {
    return [self visibleCharacterRange];
  }

  // let mozAccessible handle all other attributes
  return [super accessibilityAttributeValue:attribute];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (NSArray*)accessibilityParameterizedAttributeNames {
  static NSArray* supportedParametrizedAttributes = nil;
  // text specific parametrized attributes
  if (!supportedParametrizedAttributes) {
    supportedParametrizedAttributes = [[NSArray alloc]
        initWithObjects:NSAccessibilityStringForRangeParameterizedAttribute,
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
                        nil];
  }
  return [supportedParametrizedAttributes
      arrayByAddingObject:[super accessibilityParameterizedAttributeNames]];
}

- (id)accessibilityAttributeValue:(NSString*)attribute forParameter:(id)parameter {
  if ([self isExpired]) {
    return nil;
  }

  Accessible* acc = mGeckoAccessible.AsAccessible();
  ProxyAccessible* proxy = mGeckoAccessible.AsProxy();

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
    HyperTextAccessible* textAcc = acc ? acc->AsHyperText() : nullptr;
    if (!textAcc && !proxy) {
      return nil;
    }

    NSRange range;
    if (!ToNSRange(parameter, &range)) {
#if DEBUG
      NSLog(@"%@:no range", attribute);
#endif
      return nil;
    }

    int32_t start = range.location;
    int32_t end = start + range.length;
    DesktopIntRect bounds;
    if (textAcc) {
      bounds = DesktopIntRect::FromUnknownRect(textAcc->TextBounds(start, end));
    } else if (proxy) {
      bounds = DesktopIntRect::FromUnknownRect(proxy->TextBounds(start, end));
    }

    return [NSValue valueWithRect:nsCocoaUtils::GeckoRectToCocoaRect(bounds)];
  }

  return [super accessibilityAttributeValue:attribute forParameter:parameter];
}

- (BOOL)accessibilityIsAttributeSettable:(NSString*)attribute {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  if ([attribute isEqualToString:NSAccessibilityValueAttribute]) return ![self isReadOnly];

  if ([attribute isEqualToString:NSAccessibilitySelectedTextAttribute] ||
      [attribute isEqualToString:NSAccessibilitySelectedTextRangeAttribute] ||
      [attribute isEqualToString:NSAccessibilityVisibleCharacterRangeAttribute])
    return YES;

  return [super accessibilityIsAttributeSettable:attribute];

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NO);
}

- (void)accessibilitySetValue:(id)value forAttribute:(NSString*)attribute {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if ([self isExpired]) {
    return;
  }

  Accessible* acc = mGeckoAccessible.AsAccessible();
  ProxyAccessible* proxy = mGeckoAccessible.AsProxy();

  if ([attribute isEqualToString:NSAccessibilityValueAttribute]) {
    [self setText:ToNSString(value)];

    return;
  }

  if ([attribute isEqualToString:NSAccessibilitySelectedTextAttribute]) {
    HyperTextAccessible* textAcc = acc ? acc->AsHyperText() : nullptr;
    if (!textAcc && !proxy) return;

    NSString* stringValue = ToNSString(value);
    if (!stringValue) return;

    int32_t start = 0, end = 0;
    nsString text;
    if (textAcc) {
      textAcc->SelectionBoundsAt(0, &start, &end);
      textAcc->DeleteText(start, end - start);
      nsCocoaUtils::GetStringForNSString(stringValue, text);
      textAcc->InsertText(text, start);
    } else if (proxy) {
      nsString data;
      proxy->SelectionBoundsAt(0, data, &start, &end);
      proxy->DeleteText(start, end - start);
      nsCocoaUtils::GetStringForNSString(stringValue, text);
      proxy->InsertText(text, start);
    }
  }

  if ([attribute isEqualToString:NSAccessibilitySelectedTextRangeAttribute]) {
    HyperTextAccessible* textAcc = acc ? acc->AsHyperText() : nullptr;
    if (!textAcc && !proxy) return;

    NSRange range;
    if (!ToNSRange(value, &range)) return;

    if (textAcc) {
      textAcc->SetSelectionBoundsAt(0, range.location, range.location + range.length);
    } else if (proxy) {
      proxy->SetSelectionBoundsAt(0, range.location, range.location + range.length);
    }
    return;
  }

  if ([attribute isEqualToString:NSAccessibilityVisibleCharacterRangeAttribute]) {
    HyperTextAccessible* textAcc = acc ? acc->AsHyperText() : nullptr;
    if (!textAcc && !proxy) return;

    NSRange range;
    if (!ToNSRange(value, &range)) return;

    if (textAcc) {
      textAcc->ScrollSubstringTo(range.location, range.location + range.length,
                                 nsIAccessibleScrollType::SCROLL_TYPE_TOP_EDGE);
    } else if (proxy) {
      proxy->ScrollSubstringTo(range.location, range.location + range.length,
                               nsIAccessibleScrollType::SCROLL_TYPE_TOP_EDGE);
    }
    return;
  }

  [super accessibilitySetValue:value forAttribute:attribute];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (NSString*)moxSubrole {
  if (mRole == roles::PASSWORD_TEXT) return NSAccessibilitySecureTextFieldSubrole;

  if (mRole == roles::ENTRY) {
    Accessible* acc = mGeckoAccessible.AsAccessible();
    ProxyAccessible* proxy = mGeckoAccessible.AsProxy();
    if ((acc && acc->IsSearchbox()) || (proxy && proxy->IsSearchbox())) {
      return @"AXSearchField";
    }
  }

  return nil;
}

#pragma mark -

- (BOOL)isReadOnly {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  if ([self isExpired]) {
    return NO;
  }

  if ([[self moxRole] isEqualToString:NSAccessibilityStaticTextRole]) {
    return YES;
  }

  return [self stateWithMask:states::READONLY] != 0;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NO);
}

- (NSNumber*)caretLineNumber {
  Accessible* acc = mGeckoAccessible.AsAccessible();
  HyperTextAccessible* textAcc = acc ? acc->AsHyperText() : nullptr;

  int32_t lineNumber = -1;
  if (textAcc) {
    lineNumber = textAcc->CaretLineNumber() - 1;
  } else if (ProxyAccessible* proxy = mGeckoAccessible.AsProxy()) {
    lineNumber = proxy->CaretLineNumber() - 1;
  }

  return (lineNumber >= 0) ? [NSNumber numberWithInt:lineNumber] : nil;
}

- (void)setText:(NSString*)aNewString {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  Accessible* acc = mGeckoAccessible.AsAccessible();
  HyperTextAccessible* textAcc = acc ? acc->AsHyperText() : nullptr;

  nsString text;
  nsCocoaUtils::GetStringForNSString(aNewString, text);
  if (textAcc) {
    textAcc->ReplaceText(text);
  } else if (ProxyAccessible* proxy = mGeckoAccessible.AsProxy()) {
    proxy->ReplaceText(text);
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (NSString*)text {
  Accessible* acc = mGeckoAccessible.AsAccessible();
  ProxyAccessible* proxy = mGeckoAccessible.AsProxy();
  HyperTextAccessible* textAcc = acc ? acc->AsHyperText() : nullptr;
  if (!textAcc && !proxy) return nil;

  // A password text field returns an empty value
  if (mRole == roles::PASSWORD_TEXT) return @"";

  nsAutoString text;
  if (textAcc) {
    textAcc->TextSubstring(0, nsIAccessibleText::TEXT_OFFSET_END_OF_TEXT, text);
  } else if (proxy) {
    proxy->TextSubstring(0, nsIAccessibleText::TEXT_OFFSET_END_OF_TEXT, text);
  }

  return nsCocoaUtils::ToNSString(text);
}

- (long)textLength {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  Accessible* acc = mGeckoAccessible.AsAccessible();
  ProxyAccessible* proxy = mGeckoAccessible.AsProxy();
  HyperTextAccessible* textAcc = acc ? acc->AsHyperText() : nullptr;
  if (!textAcc && !proxy) return 0;

  return textAcc ? textAcc->CharacterCount() : proxy->CharacterCount();

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(0);
}

- (long)selectedTextLength {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  Accessible* acc = mGeckoAccessible.AsAccessible();
  ProxyAccessible* proxy = mGeckoAccessible.AsProxy();
  HyperTextAccessible* textAcc = acc ? acc->AsHyperText() : nullptr;
  if (!textAcc && !proxy) return 0;

  int32_t start = 0, end = 0;
  if (textAcc) {
    textAcc->SelectionBoundsAt(0, &start, &end);
  } else if (proxy) {
    nsString data;
    proxy->SelectionBoundsAt(0, data, &start, &end);
  }
  return (end - start);

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(0);
}

- (NSString*)selectedText {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  Accessible* acc = mGeckoAccessible.AsAccessible();
  ProxyAccessible* proxy = mGeckoAccessible.AsProxy();
  HyperTextAccessible* textAcc = acc ? acc->AsHyperText() : nullptr;
  if (!textAcc && !proxy) return nil;

  int32_t start = 0, end = 0;
  nsAutoString selText;
  if (textAcc) {
    textAcc->SelectionBoundsAt(0, &start, &end);
    if (start != end) {
      textAcc->TextSubstring(start, end, selText);
    }
  } else if (proxy) {
    proxy->SelectionBoundsAt(0, selText, &start, &end);
  }

  return nsCocoaUtils::ToNSString(selText);

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (NSValue*)selectedTextRange {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  Accessible* acc = mGeckoAccessible.AsAccessible();
  ProxyAccessible* proxy = mGeckoAccessible.AsProxy();
  HyperTextAccessible* textAcc = acc ? acc->AsHyperText() : nullptr;

  int32_t start = 0;
  int32_t end = 0;
  int32_t count = 0;
  if (textAcc) {
    count = textAcc->SelectionCount();
    if (count) {
      textAcc->SelectionBoundsAt(0, &start, &end);
      return [NSValue valueWithRange:NSMakeRange(start, end - start)];
    }

    start = textAcc->CaretOffset();
    return [NSValue valueWithRange:NSMakeRange(start != -1 ? start : 0, 0)];
  }

  if (proxy) {
    count = proxy->SelectionCount();
    if (count) {
      nsString data;
      proxy->SelectionBoundsAt(0, data, &start, &end);
      return [NSValue valueWithRange:NSMakeRange(start, end - start)];
    }

    start = proxy->CaretOffset();
    return [NSValue valueWithRange:NSMakeRange(start != -1 ? start : 0, 0)];
  }

  return [NSValue valueWithRange:NSMakeRange(0, 0)];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (NSValue*)visibleCharacterRange {
  // XXX this won't work with Textarea and such as we actually don't give
  // the visible character range.
  Accessible* acc = mGeckoAccessible.AsAccessible();
  ProxyAccessible* proxy = mGeckoAccessible.AsProxy();
  HyperTextAccessible* textAcc = acc ? acc->AsHyperText() : nullptr;
  if (!textAcc && !proxy) return 0;

  return [NSValue
      valueWithRange:NSMakeRange(0, textAcc ? textAcc->CharacterCount() : proxy->CharacterCount())];
}

- (void)handleAccessibleEvent:(uint32_t)eventType {
  switch (eventType) {
    case nsIAccessibleEvent::EVENT_VALUE_CHANGE:
    case nsIAccessibleEvent::EVENT_TEXT_VALUE_CHANGE:
      [self moxPostNotification:NSAccessibilityValueChangedNotification];
      break;
    case nsIAccessibleEvent::EVENT_TEXT_CARET_MOVED:
    case nsIAccessibleEvent::EVENT_TEXT_SELECTION_CHANGED:
      [self moxPostNotification:NSAccessibilitySelectedTextChangedNotification];
      break;
    default:
      [super handleAccessibleEvent:eventType];
      break;
  }
}

- (NSString*)stringFromRange:(NSRange*)range {
  MOZ_ASSERT(range, "no range");

  Accessible* acc = mGeckoAccessible.AsAccessible();
  ProxyAccessible* proxy = mGeckoAccessible.AsProxy();
  HyperTextAccessible* textAcc = acc ? acc->AsHyperText() : nullptr;
  if (!textAcc && !proxy) return nil;

  nsAutoString text;
  if (textAcc) {
    textAcc->TextSubstring(range->location, range->location + range->length, text);
  } else if (proxy) {
    proxy->TextSubstring(range->location, range->location + range->length, text);
  }

  return nsCocoaUtils::ToNSString(text);
}

@end

@implementation mozTextLeafAccessible

- (NSArray*)accessibilityAttributeNames {
  static NSMutableArray* supportedAttributes = nil;
  if (!supportedAttributes) {
    supportedAttributes = [[super accessibilityAttributeNames] mutableCopy];
    [supportedAttributes removeObject:NSAccessibilityChildrenAttribute];
    // We remove our AXTitleUIElement here to avoid an IPC call in the
    // parent class when locating values for our attributes.
    [supportedAttributes removeObject:NSAccessibilityTitleUIElementAttribute];
  }

  return supportedAttributes;
}

- (id)accessibilityAttributeValue:(NSString*)attribute {
  if ([attribute isEqualToString:NSAccessibilityTitleAttribute]) return nil;

  if ([attribute isEqualToString:NSAccessibilityValueAttribute]) return [self moxTitle];

  return [super accessibilityAttributeValue:attribute];
}

- (NSString*)moxLabel {
  return nil;
}

@end
