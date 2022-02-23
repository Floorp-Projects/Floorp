/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_ImageAccessible_h__
#define mozilla_a11y_ImageAccessible_h__

#include "BaseAccessibles.h"
#include "imgINotificationObserver.h"

namespace mozilla {
namespace a11y {

/* LocalAccessible for supporting images
 * supports:
 * - gets name, role
 * - support basic state
 */
class ImageAccessible : public LinkableAccessible,
                        public imgINotificationObserver {
 public:
  ImageAccessible(nsIContent* aContent, DocAccessible* aDoc);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_IMGINOTIFICATIONOBSERVER

  // LocalAccessible
  virtual void Shutdown() override;
  virtual a11y::role NativeRole() const override;
  virtual uint64_t NativeState() const override;
  virtual already_AddRefed<AccAttributes> NativeAttributes() override;

  // ActionAccessible
  virtual uint8_t ActionCount() const override;
  virtual void ActionNameAt(uint8_t aIndex, nsAString& aName) override;
  virtual bool DoAction(uint8_t aIndex) const override;

  // ImageAccessible
  LayoutDeviceIntPoint Position(uint32_t aCoordType);
  LayoutDeviceIntSize Size();

  /**
   * Return whether the element has a longdesc URI.
   */
  bool HasLongDesc() const {
    nsCOMPtr<nsIURI> uri = GetLongDescURI();
    return uri;
  }

 protected:
  virtual ~ImageAccessible();

  // LocalAccessible
  virtual ENameValueFlag NativeName(nsString& aName) const override;

  virtual void DOMAttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                                   int32_t aModType,
                                   const nsAttrValue* aOldValue,
                                   uint64_t aOldState) override;

 private:
  /**
   * Return an URI for showlongdesc action if any.
   */
  already_AddRefed<nsIURI> GetLongDescURI() const;

  /**
   * Used by ActionNameAt and DoAction to ensure the index for opening the
   * longdesc URL is valid.
   * It is always assumed that the highest possible index opens the longdesc.
   * This doesn't check that there is actually a longdesc, just that the index
   * would be correct if there was one.
   *
   * @param aIndex  The 0-based index to be tested.
   *
   * @returns  true if index is valid for longdesc action.
   */
  inline bool IsLongDescIndex(uint8_t aIndex) const;

  uint32_t mImageRequestStatus;
};

////////////////////////////////////////////////////////////////////////////////
// LocalAccessible downcasting method

inline ImageAccessible* LocalAccessible::AsImage() {
  return IsImage() ? static_cast<ImageAccessible*>(this) : nullptr;
}

}  // namespace a11y
}  // namespace mozilla

#endif
