/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_MAI_H__
#define __NS_MAI_H__

#include <atk/atk.h>
#include <glib.h>
#include <glib-object.h>

#include "AccessibleWrap.h"

namespace mozilla {
namespace a11y {
class ProxyAccessible;
}
}

#define MAI_TYPE_ATK_OBJECT             (mai_atk_object_get_type ())
#define MAI_ATK_OBJECT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                                         MAI_TYPE_ATK_OBJECT, MaiAtkObject))
#define MAI_ATK_OBJECT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), \
                                         MAI_TYPE_ATK_OBJECT, \
                                         MaiAtkObjectClass))
#define IS_MAI_OBJECT(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                         MAI_TYPE_ATK_OBJECT))
#define IS_MAI_OBJECT_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                                         MAI_TYPE_ATK_OBJECT))
#define MAI_ATK_OBJECT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                                         MAI_TYPE_ATK_OBJECT, \
                                         MaiAtkObjectClass))
GType mai_atk_object_get_type(void);
GType mai_util_get_type();
mozilla::a11y::AccessibleWrap* GetAccessibleWrap(AtkObject* aAtkObj);
mozilla::a11y::ProxyAccessible* GetProxy(AtkObject* aAtkObj);
AtkObject* GetWrapperFor(mozilla::a11y::ProxyAccessible* aProxy);

extern int atkMajorVersion, atkMinorVersion;

/**
 * Return true if the loaded version of libatk-1.0.so is at least
 * aMajor.aMinor.0.
 */
static inline bool
IsAtkVersionAtLeast(int aMajor, int aMinor)
{
  return aMajor < atkMajorVersion ||
         (aMajor == atkMajorVersion && aMinor <= atkMinorVersion);
}

// This is or'd with the pointer in MaiAtkObject::accWrap if the wrap-ee is a
// proxy.
static const uintptr_t IS_PROXY = 1;

/**
 * This MaiAtkObject is a thin wrapper, in the MAI namespace, for AtkObject
 */
struct MaiAtkObject
{
  AtkObject parent;
  /*
   * The AccessibleWrap whose properties and features are exported
   * via this object instance.
   */
  uintptr_t accWrap;

  /*
   * Get the AtkHyperlink for this atk object.
   */
  AtkHyperlink* GetAtkHyperlink();

  /*
   * Shutdown this AtkObject.
   */
  void Shutdown();

  /*
   * Notify atk of a state change on this AtkObject.
   */
  void FireStateChangeEvent(uint64_t aState, bool aEnabled);
};

#endif /* __NS_MAI_H__ */
