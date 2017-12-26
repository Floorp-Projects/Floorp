/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TypeInState_h
#define TypeInState_h

#include "mozilla/UniquePtr.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISelectionListener.h"
#include "nsISupportsImpl.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nscore.h"

// Workaround for windows headers
#ifdef SetProp
#undef SetProp
#endif

class nsAtom;
class nsIDOMNode;

namespace mozilla {

class HTMLEditRules;
namespace dom {
class Selection;
} // namespace dom

struct PropItem
{
  nsAtom* tag;
  nsAtom* attr;
  nsString value;

  PropItem();
  PropItem(nsAtom* aTag, nsAtom* aAttr, const nsAString& aValue);
  ~PropItem();
};

class TypeInState final : public nsISelectionListener
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(TypeInState)

  TypeInState();
  void Reset();

  nsresult UpdateSelState(dom::Selection* aSelection);

  // nsISelectionListener
  NS_DECL_NSISELECTIONLISTENER

  void SetProp(nsAtom* aProp, nsAtom* aAttr, const nsAString& aValue);

  void ClearAllProps();
  void ClearProp(nsAtom* aProp, nsAtom* aAttr);

  /**
   * TakeClearProperty() hands back next property item on the clear list.
   * Caller assumes ownership of PropItem and must delete it.
   */
  UniquePtr<PropItem> TakeClearProperty();

  /**
   * TakeSetProperty() hands back next property item on the set list.
   * Caller assumes ownership of PropItem and must delete it.
   */
  UniquePtr<PropItem> TakeSetProperty();

  /**
   * TakeRelativeFontSize() hands back relative font value, which is then
   * cleared out.
   */
  int32_t TakeRelativeFontSize();

  void GetTypingState(bool& isSet, bool& theSetting, nsAtom* aProp,
                      nsAtom* aAttr = nullptr, nsString* outValue = nullptr);

  static bool FindPropInList(nsAtom* aProp, nsAtom* aAttr,
                             nsAString* outValue, nsTArray<PropItem*>& aList,
                             int32_t& outIndex);

protected:
  virtual ~TypeInState();

  void RemovePropFromSetList(nsAtom* aProp, nsAtom* aAttr);
  void RemovePropFromClearedList(nsAtom* aProp, nsAtom* aAttr);
  bool IsPropSet(nsAtom* aProp, nsAtom* aAttr, nsAString* outValue);
  bool IsPropSet(nsAtom* aProp, nsAtom* aAttr, nsAString* outValue,
                 int32_t& outIndex);
  bool IsPropCleared(nsAtom* aProp, nsAtom* aAttr);
  bool IsPropCleared(nsAtom* aProp, nsAtom* aAttr, int32_t& outIndex);

  nsTArray<PropItem*> mSetArray;
  nsTArray<PropItem*> mClearedArray;
  int32_t mRelativeFontSize;
  nsCOMPtr<nsIDOMNode> mLastSelectionContainer;
  int32_t mLastSelectionOffset;

  friend class HTMLEditRules;
};

} // namespace mozilla

#endif  // #ifndef TypeInState_h

