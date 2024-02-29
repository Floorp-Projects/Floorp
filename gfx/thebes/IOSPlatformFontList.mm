/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <UIKit/UIKit.h>
#include "IOSPlatformFontList.h"

IOSPlatformFontList::IOSPlatformFontList() : CoreTextFontList() {}

IOSPlatformFontList::~IOSPlatformFontList() {}

namespace {
class nsAutoreleasePool {
 public:
  nsAutoreleasePool() { mLocalPool = [[NSAutoreleasePool alloc] init]; }
  ~nsAutoreleasePool() { [mLocalPool release]; }

 private:
  NSAutoreleasePool* mLocalPool;
};

static void GetUtf8StringFromNSString(NSString* aSrc, nsACString& aDest) {
  nsAutoString temp;
  temp.SetLength([aSrc length]);
  [aSrc getCharacters:reinterpret_cast<unichar*>(temp.BeginWriting())
                range:NSMakeRange(0, [aSrc length])];
  CopyUTF16toUTF8(temp, aDest);
}
}  // namespace

void IOSPlatformFontList::InitSystemFontNames() {
  nsAutoreleasePool localPool;

  UIFontDescriptor* desc = [UIFontDescriptor
      preferredFontDescriptorWithTextStyle:UIFontTextStyleBody];
  desc = [desc fontDescriptorWithDesign:UIFontDescriptorSystemDesignDefault];

  NSString* name =
      [[desc fontAttributes] objectForKey:UIFontDescriptorFamilyAttribute];

  GetUtf8StringFromNSString(name, mSystemFontFamilyName);
}

FontFamily IOSPlatformFontList::GetDefaultFontForPlatform(
    nsPresContext* aPresContext, const gfxFontStyle* aStyle,
    nsAtom* aLanguage) {
  nsAutoreleasePool localPool;

  UIFontDescriptor* desc = [UIFontDescriptor
      preferredFontDescriptorWithTextStyle:UIFontTextStyleBody];
  NSString* name =
      [[desc fontAttributes] objectForKey:UIFontDescriptorFamilyAttribute];

  nsAutoCString familyName;
  GetUtf8StringFromNSString(name, familyName);

  return FindFamily(aPresContext, familyName);
}

void IOSPlatformFontList::LookupSystemFont(
    mozilla::LookAndFeel::FontID aSystemFontID, nsACString& aSystemFontName,
    gfxFontStyle& aFontStyle) {
  MOZ_CRASH("UNIMPLEMENTED");
}
