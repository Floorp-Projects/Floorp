/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_AccessibleWrap_h_
#define mozilla_a11y_AccessibleWrap_h_

#include "Accessible.h"
#include "GeneratedJNIWrappers.h"
#include "mozilla/a11y/ProxyAccessible.h"
#include "nsCOMPtr.h"

namespace mozilla {
namespace a11y {

class AccessibleWrap : public Accessible
{
public:
  AccessibleWrap(nsIContent* aContent, DocAccessible* aDoc);
  virtual ~AccessibleWrap();

  virtual nsresult HandleAccEvent(AccEvent* aEvent) override;
  virtual void Shutdown() override;

  int32_t VirtualViewID() const { return mID; }

  virtual void SetTextContents(const nsAString& aText);

  virtual void GetTextContents(nsAString& aText);

  virtual bool GetSelectionBounds(int32_t* aStartOffset, int32_t* aEndOffset);

  virtual mozilla::java::GeckoBundle::LocalRef ToBundle();

  static const int32_t kNoID = -1;

protected:
  mozilla::java::GeckoBundle::LocalRef CreateBundle(
    int32_t aParentID,
    role aRole,
    uint64_t aState,
    const nsString& aName,
    const nsString& aTextValue,
    const nsString& aDOMNodeID,
    const nsIntRect& aBounds,
    double aCurVal,
    double aMinVal,
    double aMaxVal,
    double aStep,
    nsIPersistentProperties* aAttributes,
    const nsTArray<int32_t>& aChildren) const;

  // IDs should be a positive 32bit integer.
  static int32_t AcquireID();
  static void ReleaseID(int32_t aID);

  static int32_t GetAndroidClass(role aRole);

  static int32_t GetInputType(const nsString& aInputTypeAttr);

  int32_t mID;

private:
  void DOMNodeID(nsString& aDOMNodeID);

  static void GetRoleDescription(role aRole,
                                 nsAString& aGeckoRole,
                                 nsAString& aRoleDescription);

  static uint64_t GetFlags(role aRole, uint64_t aState);
};

static inline AccessibleWrap*
WrapperFor(const ProxyAccessible* aProxy)
{
  return reinterpret_cast<AccessibleWrap*>(aProxy->GetWrapper());
}

} // namespace a11y
} // namespace mozilla

#endif
