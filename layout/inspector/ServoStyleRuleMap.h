/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ServoStyleRuleMap_h
#define mozilla_ServoStyleRuleMap_h

#include "mozilla/ServoStyleRule.h"

#include "nsDataHashtable.h"
#include "nsICSSLoaderObserver.h"
#include "nsStubDocumentObserver.h"

struct RawServoStyleRule;

namespace mozilla {
class ServoCSSRuleList;
class ServoStyleSet;
namespace css {
class Rule;
} // namespace css

class ServoStyleRuleMap final : public nsStubDocumentObserver
                              , public nsICSSLoaderObserver
{
public:
  NS_DECL_ISUPPORTS

  explicit ServoStyleRuleMap(ServoStyleSet* aStyleSet);

  void EnsureTable();
  ServoStyleRule* Lookup(const RawServoStyleRule* aRawRule) const {
    return mTable.Get(aRawRule);
  }

  // nsIDocumentObserver methods
  void StyleSheetAdded(StyleSheet* aStyleSheet, bool aDocumentSheet) final;
  void StyleSheetRemoved(StyleSheet* aStyleSheet, bool aDocumentSheet) final;
  void StyleSheetApplicableStateChanged(StyleSheet* aStyleSheet) final;
  void StyleRuleAdded(StyleSheet* aStyleSheet, css::Rule* aStyleRule) final;
  void StyleRuleRemoved(StyleSheet* aStyleSheet, css::Rule* aStyleRule) final;

  // nsICSSLoaderObserver
  NS_IMETHOD StyleSheetLoaded(StyleSheet* aSheet,
                              bool aWasAlternate, nsresult aStatus) final;

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const;

private:
  ~ServoStyleRuleMap();

  // Since we would never have a document which contains no style rule,
  // we use IsEmpty as an indication whether we need to walk through
  // all stylesheets to fill the table.
  bool IsEmpty() const { return mTable.Count() == 0; }

  void FillTableFromRule(css::Rule* aRule);
  void FillTableFromRuleList(ServoCSSRuleList* aRuleList);
  void FillTableFromStyleSheet(ServoStyleSheet* aSheet);

  typedef nsDataHashtable<nsPtrHashKey<const RawServoStyleRule>,
                          WeakPtr<ServoStyleRule>> Hashtable;
  Hashtable mTable;
  ServoStyleSet* mStyleSet;
};

} // namespace mozilla

#endif // mozilla_ServoStyleRuleMap_h
