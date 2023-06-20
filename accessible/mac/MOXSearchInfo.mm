/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "MOXSearchInfo.h"
#import "MOXWebAreaAccessible.h"
#import "RotorRules.h"

#include "nsCocoaUtils.h"
#include "DocAccessibleParent.h"
#include "nsAccessibilityService.h"

using namespace mozilla::a11y;

@interface MOXSearchInfo ()
- (NSArray*)getMatchesForRule:(PivotRule&)rule;

- (Accessible*)rootGeckoAccessible;

- (Accessible*)startGeckoAccessible;
@end

@implementation MOXSearchInfo

- (id)initWithParameters:(NSDictionary*)params
                 andRoot:(MOXAccessibleBase*)root {
  if (id searchKeyParam = [params objectForKey:@"AXSearchKey"]) {
    mSearchKeys = [searchKeyParam isKindOfClass:[NSString class]]
                      ? [[NSArray alloc] initWithObjects:searchKeyParam, nil]
                      : [searchKeyParam retain];
  }

  if (id startElemParam = [params objectForKey:@"AXStartElement"]) {
    mStartElem = startElemParam;
  } else {
    mStartElem = root;
  }

  mRoot = root;

  mResultLimit = [[params objectForKey:@"AXResultsLimit"] intValue];

  mSearchForward =
      [[params objectForKey:@"AXDirection"] isEqualToString:@"AXDirectionNext"];

  mImmediateDescendantsOnly =
      [[params objectForKey:@"AXImmediateDescendantsOnly"] boolValue];

  mSearchText = [params objectForKey:@"AXSearchText"];

  return [super init];
}

- (Accessible*)rootGeckoAccessible {
  id root =
      [mRoot isKindOfClass:[mozAccessible class]] ? mRoot : [mRoot moxParent];

  return [static_cast<mozAccessible*>(root) geckoAccessible];
}

- (Accessible*)startGeckoAccessible {
  if ([mStartElem isKindOfClass:[mozAccessible class]]) {
    return [static_cast<mozAccessible*>(mStartElem) geckoAccessible];
  }

  // If it isn't a mozAccessible, it doesn't have a gecko accessible
  // this is most likely the root group. Use the gecko doc as the start
  // accessible.
  return [self rootGeckoAccessible];
}

- (NSArray*)getMatchesForRule:(PivotRule&)rule {
  int resultLimit = mResultLimit;

  NSMutableArray<mozAccessible*>* matches =
      [[[NSMutableArray alloc] init] autorelease];
  Accessible* geckoRootAcc = [self rootGeckoAccessible];
  Accessible* geckoStartAcc = [self startGeckoAccessible];
  Pivot p = Pivot(geckoRootAcc);
  Accessible* match;
  if (mSearchForward) {
    match = p.Next(geckoStartAcc, rule);
  } else {
    // Search backwards
    if (geckoRootAcc == geckoStartAcc) {
      // If we have no explicit start accessible, start from the last match.
      match = p.Last(rule);
    } else {
      match = p.Prev(geckoStartAcc, rule);
    }
  }

  while (match && resultLimit != 0) {
    if (!mSearchForward && match == geckoRootAcc) {
      // If searching backwards, don't include root.
      break;
    }

    // we use mResultLimit != 0 to capture the case where mResultLimit is -1
    // when it is set from the params dictionary. If that's true, we want
    // to return all matches (ie. have no limit)
    mozAccessible* nativeMatch = GetNativeFromGeckoAccessible(match);
    if (nativeMatch) {
      // only add/count results for which there is a matching
      // native accessible
      [matches addObject:nativeMatch];
      resultLimit -= 1;
    }

    match = mSearchForward ? p.Next(match, rule) : p.Prev(match, rule);
  }

  return matches;
}

- (NSArray*)performSearch {
  Accessible* geckoRootAcc = [self rootGeckoAccessible];
  Accessible* geckoStartAcc = [self startGeckoAccessible];
  NSMutableArray* matches = [[[NSMutableArray alloc] init] autorelease];
  nsString searchText;
  nsCocoaUtils::GetStringForNSString(mSearchText, searchText);
  for (id key in mSearchKeys) {
    if ([key isEqualToString:@"AXAnyTypeSearchKey"]) {
      RotorRule rule = mImmediateDescendantsOnly
                           ? RotorRule(geckoRootAcc, searchText)
                           : RotorRule(searchText);

      if (searchText.IsEmpty() &&
          [mStartElem isKindOfClass:[MOXWebAreaAccessible class]]) {
        // Don't include the root group when a search text is defined.
        if (id rootGroup =
                [static_cast<MOXWebAreaAccessible*>(mStartElem) rootGroup]) {
          // Moving forward from web area, rootgroup; if it exists, is next.
          [matches addObject:rootGroup];
          if (mResultLimit == 1) {
            // Found one match, continue in search keys for block.
            continue;
          }
        }
      }

      if (mImmediateDescendantsOnly && mStartElem != mRoot &&
          [mStartElem isKindOfClass:[MOXRootGroup class]]) {
        // Moving forward from root group. If we don't match descendants,
        // there is no match. Continue.
        continue;
      }
      [matches addObjectsFromArray:[self getMatchesForRule:rule]];
    }

    if ([key isEqualToString:@"AXHeadingSearchKey"]) {
      RotorRoleRule rule =
          mImmediateDescendantsOnly
              ? RotorRoleRule(roles::HEADING, geckoRootAcc, searchText)
              : RotorRoleRule(roles::HEADING, searchText);
      [matches addObjectsFromArray:[self getMatchesForRule:rule]];
    }

    if ([key isEqualToString:@"AXArticleSearchKey"]) {
      RotorRoleRule rule =
          mImmediateDescendantsOnly
              ? RotorRoleRule(roles::ARTICLE, geckoRootAcc, searchText)
              : RotorRoleRule(roles::ARTICLE, searchText);
      [matches addObjectsFromArray:[self getMatchesForRule:rule]];
    }

    if ([key isEqualToString:@"AXTableSearchKey"]) {
      RotorRoleRule rule =
          mImmediateDescendantsOnly
              ? RotorRoleRule(roles::TABLE, geckoRootAcc, searchText)
              : RotorRoleRule(roles::TABLE, searchText);
      [matches addObjectsFromArray:[self getMatchesForRule:rule]];
    }

    if ([key isEqualToString:@"AXLandmarkSearchKey"]) {
      RotorRoleRule rule =
          mImmediateDescendantsOnly
              ? RotorRoleRule(roles::LANDMARK, geckoRootAcc, searchText)
              : RotorRoleRule(roles::LANDMARK, searchText);
      [matches addObjectsFromArray:[self getMatchesForRule:rule]];
    }

    if ([key isEqualToString:@"AXListSearchKey"]) {
      RotorRoleRule rule =
          mImmediateDescendantsOnly
              ? RotorRoleRule(roles::LIST, geckoRootAcc, searchText)
              : RotorRoleRule(roles::LIST, searchText);
      [matches addObjectsFromArray:[self getMatchesForRule:rule]];
    }

    if ([key isEqualToString:@"AXLinkSearchKey"]) {
      RotorLinkRule rule = mImmediateDescendantsOnly
                               ? RotorLinkRule(geckoRootAcc, searchText)
                               : RotorLinkRule(searchText);
      [matches addObjectsFromArray:[self getMatchesForRule:rule]];
    }

    if ([key isEqualToString:@"AXVisitedLinkSearchKey"]) {
      RotorVisitedLinkRule rule =
          mImmediateDescendantsOnly
              ? RotorVisitedLinkRule(geckoRootAcc, searchText)
              : RotorVisitedLinkRule(searchText);
      [matches addObjectsFromArray:[self getMatchesForRule:rule]];
    }

    if ([key isEqualToString:@"AXUnvisitedLinkSearchKey"]) {
      RotorUnvisitedLinkRule rule =
          mImmediateDescendantsOnly
              ? RotorUnvisitedLinkRule(geckoRootAcc, searchText)
              : RotorUnvisitedLinkRule(searchText);
      [matches addObjectsFromArray:[self getMatchesForRule:rule]];
    }

    if ([key isEqualToString:@"AXButtonSearchKey"]) {
      RotorRoleRule rule =
          mImmediateDescendantsOnly
              ? RotorRoleRule(roles::PUSHBUTTON, geckoRootAcc, searchText)
              : RotorRoleRule(roles::PUSHBUTTON, searchText);
      [matches addObjectsFromArray:[self getMatchesForRule:rule]];
    }

    if ([key isEqualToString:@"AXControlSearchKey"]) {
      RotorControlRule rule = mImmediateDescendantsOnly
                                  ? RotorControlRule(geckoRootAcc, searchText)
                                  : RotorControlRule(searchText);
      [matches addObjectsFromArray:[self getMatchesForRule:rule]];
    }

    if ([key isEqualToString:@"AXSameTypeSearchKey"]) {
      mozAccessible* native = GetNativeFromGeckoAccessible(geckoStartAcc);
      NSString* macRole = [native moxRole];
      RotorMacRoleRule rule =
          mImmediateDescendantsOnly
              ? RotorMacRoleRule(macRole, geckoRootAcc, searchText)
              : RotorMacRoleRule(macRole, searchText);
      [matches addObjectsFromArray:[self getMatchesForRule:rule]];
    }

    if ([key isEqualToString:@"AXDifferentTypeSearchKey"]) {
      mozAccessible* native = GetNativeFromGeckoAccessible(geckoStartAcc);
      NSString* macRole = [native moxRole];
      RotorNotMacRoleRule rule =
          mImmediateDescendantsOnly
              ? RotorNotMacRoleRule(macRole, geckoRootAcc, searchText)
              : RotorNotMacRoleRule(macRole, searchText);
      [matches addObjectsFromArray:[self getMatchesForRule:rule]];
    }

    if ([key isEqualToString:@"AXRadioGroupSearchKey"]) {
      RotorRoleRule rule =
          mImmediateDescendantsOnly
              ? RotorRoleRule(roles::RADIO_GROUP, geckoRootAcc, searchText)
              : RotorRoleRule(roles::RADIO_GROUP, searchText);
      [matches addObjectsFromArray:[self getMatchesForRule:rule]];
    }

    if ([key isEqualToString:@"AXFrameSearchKey"]) {
      RotorRoleRule rule =
          mImmediateDescendantsOnly
              ? RotorRoleRule(roles::DOCUMENT, geckoRootAcc, searchText)
              : RotorRoleRule(roles::DOCUMENT, searchText);
      [matches addObjectsFromArray:[self getMatchesForRule:rule]];
    }

    if ([key isEqualToString:@"AXImageSearchKey"] ||
        [key isEqualToString:@"AXGraphicSearchKey"]) {
      RotorRoleRule rule =
          mImmediateDescendantsOnly
              ? RotorRoleRule(roles::GRAPHIC, geckoRootAcc, searchText)
              : RotorRoleRule(roles::GRAPHIC, searchText);
      [matches addObjectsFromArray:[self getMatchesForRule:rule]];
    }

    if ([key isEqualToString:@"AXCheckBoxSearchKey"]) {
      RotorRoleRule rule =
          mImmediateDescendantsOnly
              ? RotorRoleRule(roles::CHECKBUTTON, geckoRootAcc, searchText)
              : RotorRoleRule(roles::CHECKBUTTON, searchText);
      [matches addObjectsFromArray:[self getMatchesForRule:rule]];
    }

    if ([key isEqualToString:@"AXStaticTextSearchKey"]) {
      RotorStaticTextRule rule =
          mImmediateDescendantsOnly
              ? RotorStaticTextRule(geckoRootAcc, searchText)
              : RotorStaticTextRule(searchText);
      [matches addObjectsFromArray:[self getMatchesForRule:rule]];
    }

    if ([key isEqualToString:@"AXHeadingLevel1SearchKey"]) {
      RotorHeadingLevelRule rule =
          mImmediateDescendantsOnly
              ? RotorHeadingLevelRule(1, geckoRootAcc, searchText)
              : RotorHeadingLevelRule(1, searchText);
      [matches addObjectsFromArray:[self getMatchesForRule:rule]];
    }

    if ([key isEqualToString:@"AXHeadingLevel2SearchKey"]) {
      RotorHeadingLevelRule rule =
          mImmediateDescendantsOnly
              ? RotorHeadingLevelRule(2, geckoRootAcc, searchText)
              : RotorHeadingLevelRule(2, searchText);
      [matches addObjectsFromArray:[self getMatchesForRule:rule]];
    }

    if ([key isEqualToString:@"AXHeadingLevel3SearchKey"]) {
      RotorHeadingLevelRule rule =
          mImmediateDescendantsOnly
              ? RotorHeadingLevelRule(3, geckoRootAcc, searchText)
              : RotorHeadingLevelRule(3, searchText);
      [matches addObjectsFromArray:[self getMatchesForRule:rule]];
    }

    if ([key isEqualToString:@"AXHeadingLevel4SearchKey"]) {
      RotorHeadingLevelRule rule =
          mImmediateDescendantsOnly
              ? RotorHeadingLevelRule(4, geckoRootAcc, searchText)
              : RotorHeadingLevelRule(4, searchText);
      [matches addObjectsFromArray:[self getMatchesForRule:rule]];
    }

    if ([key isEqualToString:@"AXHeadingLevel5SearchKey"]) {
      RotorHeadingLevelRule rule =
          mImmediateDescendantsOnly
              ? RotorHeadingLevelRule(5, geckoRootAcc, searchText)
              : RotorHeadingLevelRule(5, searchText);
      [matches addObjectsFromArray:[self getMatchesForRule:rule]];
    }

    if ([key isEqualToString:@"AXHeadingLevel6SearchKey"]) {
      RotorHeadingLevelRule rule =
          mImmediateDescendantsOnly
              ? RotorHeadingLevelRule(6, geckoRootAcc, searchText)
              : RotorHeadingLevelRule(6, searchText);
      [matches addObjectsFromArray:[self getMatchesForRule:rule]];
    }

    if ([key isEqualToString:@"AXBlockquoteSearchKey"]) {
      RotorRoleRule rule =
          mImmediateDescendantsOnly
              ? RotorRoleRule(roles::BLOCKQUOTE, geckoRootAcc, searchText)
              : RotorRoleRule(roles::BLOCKQUOTE, searchText);
      [matches addObjectsFromArray:[self getMatchesForRule:rule]];
    }

    if ([key isEqualToString:@"AXTextFieldSearchKey"]) {
      RotorTextEntryRule rule =
          mImmediateDescendantsOnly
              ? RotorTextEntryRule(geckoRootAcc, searchText)
              : RotorTextEntryRule(searchText);
      [matches addObjectsFromArray:[self getMatchesForRule:rule]];
    }

    if ([key isEqualToString:@"AXLiveRegionSearchKey"]) {
      RotorLiveRegionRule rule =
          mImmediateDescendantsOnly
              ? RotorLiveRegionRule(geckoRootAcc, searchText)
              : RotorLiveRegionRule(searchText);
      [matches addObjectsFromArray:[self getMatchesForRule:rule]];
    }
  }

  return matches;
}

- (void)dealloc {
  [mSearchKeys release];
  [super dealloc];
}

@end
