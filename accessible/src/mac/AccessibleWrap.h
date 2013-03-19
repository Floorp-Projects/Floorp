/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "nsRect.h"

#include "nsTArray.h"
#include "nsAutoPtr.h"

#if defined(__OBJC__)
@class mozAccessible;
#endif

namespace mozilla {
namespace a11y {

class AccessibleWrap : public Accessible
{
public: // construction, destruction
  AccessibleWrap(nsIContent* aContent, DocAccessible* aDoc);
  virtual ~AccessibleWrap();
    
  /**
   * Get the native Obj-C object (mozAccessible).
   */
  NS_IMETHOD GetNativeInterface (void** aOutAccessible);
  
  /**
   * The objective-c |Class| type that this accessible's native object
   * should be instantied with.   used on runtime to determine the
   * right type for this accessible's associated native object.
   */
  virtual Class GetNativeType ();

  virtual void Shutdown ();
  virtual void InvalidateChildren();

  virtual bool InsertChildAt(uint32_t aIdx, Accessible* aChild) MOZ_OVERRIDE;
  virtual bool RemoveChild(Accessible* aAccessible);

  virtual nsresult HandleAccEvent(AccEvent* aEvent);

  /**
   * Ignored means that the accessible might still have children, but is not
   * displayed to the user. it also has no native accessible object represented
   * for it.
   */
  bool IsIgnored();
  
  inline bool HasPopup () 
    { return (NativeState() & mozilla::a11y::states::HASPOPUP); }
  
  /**
   * Returns this accessible's all children, adhering to "flat" accessibles by 
   * not returning their children.
   */
  void GetUnignoredChildren(nsTArray<Accessible*>* aChildrenArray);
  Accessible* GetUnignoredParent() const;

protected:

  virtual nsresult FirePlatformEvent(AccEvent* aEvent);

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

} // namespace a11y
} // namespace mozilla

#endif
