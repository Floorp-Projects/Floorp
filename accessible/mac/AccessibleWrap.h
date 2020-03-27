/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* For documentation of the accessibility architecture,
 * see http://lxr.mozilla.org/seamonkey/source/accessible/accessible-docs.html
 */

#ifndef _AccessibleWrap_H_
#define _AccessibleWrap_H_

#include <objc/objc.h>

#include "Accessible.h"
#include "States.h"

#include "nsCOMPtr.h"

#include "nsTArray.h"

#if defined(__OBJC__)
@class mozAccessible;
#endif

namespace mozilla {
namespace a11y {

class AccessibleWrap : public Accessible {
 public:  // construction, destruction
  AccessibleWrap(nsIContent* aContent, DocAccessible* aDoc);
  virtual ~AccessibleWrap();

  /**
   * Get the native Obj-C object (mozAccessible).
   */
  virtual void GetNativeInterface(void** aOutAccessible) override;

  /**
   * The objective-c |Class| type that this accessible's native object
   * should be instantied with.   used on runtime to determine the
   * right type for this accessible's associated native object.
   */
  virtual Class GetNativeType();

  virtual void Shutdown() override;

  virtual bool InsertChildAt(uint32_t aIdx, Accessible* aChild) override;
  virtual bool RemoveChild(Accessible* aAccessible) override;

  virtual nsresult HandleAccEvent(AccEvent* aEvent) override;

 protected:
  friend class xpcAccessibleMacInterface;

  /**
   * Return true if the parent doesn't have children to expose to AT.
   */
  bool AncestorIsFlat();

  /**
   * Get the native object. Create it if needed.
   */
#if defined(__OBJC__)
  mozAccessible* GetNativeObject();
#else
  id GetNativeObject();
#endif

 private:
  /**
   * Our native object. Private because its creation is done lazily.
   * Don't access it directly. Ever. Unless you are GetNativeObject() or
   * Shutdown()
   */
#if defined(__OBJC__)
  // if we are in Objective-C, we use the actual Obj-C class.
  mozAccessible* mNativeObject;
#else
  id mNativeObject;
#endif

  /**
   * We have created our native. This does not mean there is one.
   * This can never go back to false.
   * We need it because checking whether we need a native object cost time.
   */
  bool mNativeInited;
};

#if defined(__OBJC__)
void FireNativeEvent(mozAccessible* aNativeAcc, uint32_t aEventType);
#else
void FireNativeEvent(id aNativeAcc, uint32_t aEventType);
#endif

Class GetTypeFromRole(roles::Role aRole);

}  // namespace a11y
}  // namespace mozilla

#endif
