/* -*- (Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset:) 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "MOXWebAreaAccessible.h"

#include "nsCocoaUtils.h"
#include "DocAccessibleParent.h"
#include "Pivot.h"

using namespace mozilla::a11y;

@implementation MOXWebAreaAccessible

- (NSURL*)moxURL {
  if ([self isExpired]) {
    return nil;
  }

  nsAutoString url;
  if (mGeckoAccessible.IsAccessible()) {
    MOZ_ASSERT(mGeckoAccessible.AsAccessible()->IsDoc());
    DocAccessible* acc = mGeckoAccessible.AsAccessible()->AsDoc();
    acc->URL(url);
  } else {
    ProxyAccessible* proxy = mGeckoAccessible.AsProxy();
    proxy->URL(url);
  }

  if (url.IsEmpty()) {
    return nil;
  }

  return [NSURL URLWithString:nsCocoaUtils::ToNSString(url)];
}

- (NSNumber*)moxLoaded {
  if ([self isExpired]) {
    return nil;
  }
  // We are loaded if we aren't busy or stale
  return @([self stateWithMask:(states::BUSY & states::STALE)] == 0);
}

// overrides
- (NSNumber*)moxLoadingProgress {
  if ([self isExpired]) {
    return nil;
  }

  if ([self stateWithMask:states::STALE] != 0) {
    // We expose stale state until the document is ready (DOM is loaded and tree is
    // constructed) so we indicate load hasn't started while this state is present.
    return @0.0;
  }

  if ([self stateWithMask:states::BUSY] != 0) {
    // We expose state busy until the document and all its subdocuments are completely
    // loaded, so we indicate partial loading here
    return @0.5;
  }

  // if we are not busy and not stale, we are loaded
  return @1.0;
}

- (NSArray*)moxUIElementsForSearchPredicate:(NSDictionary*)searchPredicate {
  // Create our search object and set it up with the searchPredicate
  // params. The init function does additional parsing. We pass a
  // reference to the web area (mGeckoAccessible) to use as
  // a start element if one is not specified.
  MOXSearchInfo* search = [[MOXSearchInfo alloc] initWithParameters:searchPredicate
                                                            andRoot:mGeckoAccessible];

  return [search performSearch];
}

- (NSNumber*)moxUIElementCountForSearchPredicate:(NSDictionary*)searchPredicate {
  return [NSNumber numberWithDouble:[[self moxUIElementsForSearchPredicate:searchPredicate] count]];
}

- (void)handleAccessibleEvent:(uint32_t)eventType {
  switch (eventType) {
    case nsIAccessibleEvent::EVENT_DOCUMENT_LOAD_COMPLETE:
      [self moxPostNotification:NSAccessibilityFocusedUIElementChangedNotification];
      if ((mGeckoAccessible.IsProxy() && mGeckoAccessible.AsProxy()->IsDoc() &&
           mGeckoAccessible.AsProxy()->AsDoc()->IsTopLevel()) ||
          (mGeckoAccessible.IsAccessible() && !mGeckoAccessible.AsAccessible()->IsRoot() &&
           mGeckoAccessible.AsAccessible()->AsDoc()->ParentDocument()->IsRoot())) {
        // we fire an AXLoadComplete event on top-level documents only
        [self moxPostNotification:@"AXLoadComplete"];
      } else {
        // otherwise the doc belongs to an iframe (IsTopLevelInContentProcess)
        // and we fire AXLayoutComplete instead
        [self moxPostNotification:@"AXLayoutComplete"];
      }
      break;
  }

  [super handleAccessibleEvent:eventType];
}

@end

@implementation MOXSearchInfo

- (id)initWithParameters:(NSDictionary*)params andRoot:(AccessibleOrProxy)root {
  if (id searchKeyParam = [params objectForKey:@"AXSearchKey"]) {
    mSearchKeys =
        [searchKeyParam isKindOfClass:[NSString class]] ? @[ searchKeyParam ] : searchKeyParam;
  }

  if (id startElemParam = [params objectForKey:@"AXStartElement"]) {
    mStartElem = [startElemParam geckoAccessible];
  } else {
    mStartElem = root;
  }
  MOZ_ASSERT(!mStartElem.IsNull(), "Performing search with null gecko accessible!");

  mWebArea = root;

  mResultLimit = [[params objectForKey:@"AXResultsLimit"] intValue];

  mSearchForward = [[params objectForKey:@"AXDirection"] isEqualToString:@"AXDirectionNext"];

  mImmediateDescendantsOnly = [[params objectForKey:@"AXImmediateDescendantsOnly"] boolValue];

  return [super init];
}

- (NSMutableArray*)getMatchesForRule:(PivotRule&)rule {
  int resultLimit = mResultLimit;
  NSMutableArray* matches = [[NSMutableArray alloc] init];
  Pivot p = Pivot(mWebArea);
  AccessibleOrProxy match = mSearchForward ? p.Next(mStartElem, rule) : p.Prev(mStartElem, rule);
  while (!match.IsNull() && resultLimit != 0) {
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
  NSMutableArray* matches = [[NSMutableArray alloc] init];

  for (id key in mSearchKeys) {
    if ([key isEqualToString:@"AXAnyTypeSearchKey"]) {
      PivotMatchAllRule rule =
          mImmediateDescendantsOnly ? PivotMatchAllRule(mStartElem) : PivotMatchAllRule();
      [matches addObjectsFromArray:[self getMatchesForRule:rule]];
    }

    if ([key isEqualToString:@"AXHeadingSearchKey"]) {
      PivotRoleRule rule = mImmediateDescendantsOnly ? PivotRoleRule(roles::HEADING, mStartElem)
                                                     : PivotRoleRule(roles::HEADING);
      [matches addObjectsFromArray:[self getMatchesForRule:rule]];
    }

    if ([key isEqualToString:@"AXArticleSearchKey"]) {
      PivotRoleRule rule = mImmediateDescendantsOnly ? PivotRoleRule(roles::ARTICLE, mStartElem)
                                                     : PivotRoleRule(roles::ARTICLE);
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
