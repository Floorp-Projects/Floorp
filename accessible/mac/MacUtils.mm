/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "MacUtils.h"
#include "mozAccessible.h"

#include "LocalAccessible.h"
#include "DocAccessible.h"
#include "DocAccessibleParent.h"
#include "nsCocoaUtils.h"
#include "mozilla/a11y/PDocAccessible.h"

namespace mozilla {
namespace a11y {
namespace utils {

/**
 * Get a localized string from the a11y string bundle.
 * Return nil if not found.
 */
NSString* LocalizedString(const nsString& aString) {
  nsString text;

  Accessible::TranslateString(aString, text);

  return text.IsEmpty() ? nil : nsCocoaUtils::ToNSString(text);
}

NSString* GetAccAttr(mozAccessible* aNativeAccessible, nsAtom* aAttrName) {
  nsAutoString result;
  Accessible* acc = [aNativeAccessible geckoAccessible];
  RefPtr<AccAttributes> attributes = acc->Attributes();

  if (!attributes) {
    return nil;
  }

  attributes->GetAttribute(aAttrName, result);

  if (!result.IsEmpty()) {
    return nsCocoaUtils::ToNSString(result);
  }

  return nil;
}

bool DocumentExists(Accessible* aDoc, uintptr_t aDocPtr) {
  if (reinterpret_cast<uintptr_t>(aDoc) == aDocPtr) {
    return true;
  }

  if (aDoc->IsLocal()) {
    DocAccessible* docAcc = aDoc->AsLocal()->AsDoc();
    uint32_t docCount = docAcc->ChildDocumentCount();
    for (uint32_t i = 0; i < docCount; i++) {
      if (DocumentExists(docAcc->GetChildDocumentAt(i), aDocPtr)) {
        return true;
      }
    }
  } else {
    DocAccessibleParent* docProxy = aDoc->AsRemote()->AsDoc();
    size_t docCount = docProxy->ChildDocCount();
    for (uint32_t i = 0; i < docCount; i++) {
      if (DocumentExists(docProxy->ChildDocAt(i), aDocPtr)) {
        return true;
      }
    }
  }

  return false;
}

static NSColor* ColorFromColor(const Color& aColor) {
  return [NSColor colorWithCalibratedRed:NS_GET_R(aColor.mValue) / 255.0
                                   green:NS_GET_G(aColor.mValue) / 255.0
                                    blue:NS_GET_B(aColor.mValue) / 255.0
                                   alpha:1.0];
}

NSDictionary* StringAttributesFromAccAttributes(AccAttributes* aAttributes,
                                                Accessible* aContainer) {
  if (!aAttributes) {
    if (mozAccessible* mozAcc = GetNativeFromGeckoAccessible(aContainer)) {
      // If we don't have attributes provided this is probably a control like
      // a button or empty entry. Just provide the accessible as an
      // AXAttachment.
      return @{@"AXAttachment" : mozAcc};
    }
    return @{};
  }

  NSMutableDictionary* attrDict =
      [NSMutableDictionary dictionaryWithCapacity:aAttributes->Count()];
  NSMutableDictionary* fontAttrDict = [[NSMutableDictionary alloc] init];
  [attrDict setObject:fontAttrDict forKey:@"AXFont"];
  for (auto iter : *aAttributes) {
    if (iter.Name() == nsGkAtoms::backgroundColor) {
      if (Maybe<Color> value = iter.Value<Color>()) {
        NSColor* color = ColorFromColor(*value);
        [attrDict setObject:(__bridge id)color.CGColor
                     forKey:@"AXBackgroundColor"];
      }
    } else if (iter.Name() == nsGkAtoms::color) {
      if (Maybe<Color> value = iter.Value<Color>()) {
        NSColor* color = ColorFromColor(*value);
        [attrDict setObject:(__bridge id)color.CGColor
                     forKey:@"AXForegroundColor"];
      }
    } else if (iter.Name() == nsGkAtoms::font_size) {
      if (Maybe<FontSize> pointSize = iter.Value<FontSize>()) {
        int32_t fontPixelSize = static_cast<int32_t>(pointSize->mValue * 4 / 3);
        [fontAttrDict setObject:@(fontPixelSize) forKey:@"AXFontSize"];
      }
    } else if (iter.Name() == nsGkAtoms::font_family) {
      nsAutoString fontFamily;
      iter.ValueAsString(fontFamily);
      [fontAttrDict setObject:nsCocoaUtils::ToNSString(fontFamily)
                       forKey:@"AXFontFamily"];
    } else if (iter.Name() == nsGkAtoms::textUnderlineColor) {
      [attrDict setObject:@1 forKey:@"AXUnderline"];
      if (Maybe<Color> value = iter.Value<Color>()) {
        NSColor* color = ColorFromColor(*value);
        [attrDict setObject:(__bridge id)color.CGColor
                     forKey:@"AXUnderlineColor"];
      }
    } else if (iter.Name() == nsGkAtoms::invalid) {
      // XXX: There is currently no attribute for grammar
      if (auto value = iter.Value<RefPtr<nsAtom>>()) {
        if (*value == nsGkAtoms::spelling) {
          [attrDict setObject:@YES
                       forKey:NSAccessibilityMarkedMisspelledTextAttribute];
        }
      }
    } else {
      nsAutoString valueStr;
      iter.ValueAsString(valueStr);
      nsAutoString keyStr;
      iter.NameAsString(keyStr);
      [attrDict setObject:nsCocoaUtils::ToNSString(valueStr)
                   forKey:nsCocoaUtils::ToNSString(keyStr)];
    }
  }

  mozAccessible* container = GetNativeFromGeckoAccessible(aContainer);
  id<MOXAccessible> link =
      [container moxFindAncestor:^BOOL(id<MOXAccessible> moxAcc, BOOL* stop) {
        return [[moxAcc moxRole] isEqualToString:NSAccessibilityLinkRole];
      }];
  if (link) {
    [attrDict setObject:link forKey:@"AXLink"];
  }

  id<MOXAccessible> heading =
      [container moxFindAncestor:^BOOL(id<MOXAccessible> moxAcc, BOOL* stop) {
        return [[moxAcc moxRole] isEqualToString:@"AXHeading"];
      }];
  if (heading) {
    [attrDict setObject:[heading moxValue] forKey:@"AXHeadingLevel"];
  }

  return attrDict;
}
}  // namespace utils
}  // namespace a11y
}  // namespace mozilla
