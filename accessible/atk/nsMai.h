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

#include "AccessibleOrProxy.h"
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
extern "C" GType mai_atk_socket_get_type(void);

/* MaiAtkSocket */

#define MAI_TYPE_ATK_SOCKET              (mai_atk_socket_get_type ())
#define MAI_ATK_SOCKET(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj),\
                                          MAI_TYPE_ATK_SOCKET, MaiAtkSocket))
#define MAI_IS_ATK_SOCKET(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj),\
                                          MAI_TYPE_ATK_SOCKET))
#define MAI_ATK_SOCKET_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass),\
                                          MAI_TYPE_ATK_SOCKET,\
                                          MaiAtkSocketClass))
#define MAI_IS_ATK_SOCKET_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass),\
                                          MAI_TYPE_ATK_SOCKET))
#define MAI_ATK_SOCKET_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj),\
                                          MAI_TYPE_ATK_SOCKET,\
                                          MaiAtkSocketClass))

typedef struct _MaiAtkSocket
{
  AtkSocket parent;

  mozilla::a11y::AccessibleWrap* accWrap;
} MaiAtkSocket;

typedef struct _MaiAtkSocketClass
{
  AtkSocketClass parent_class;
} MaiAtkSocketClass;

mozilla::a11y::AccessibleWrap* GetAccessibleWrap(AtkObject* aAtkObj);
mozilla::a11y::ProxyAccessible* GetProxy(AtkObject* aAtkObj);
mozilla::a11y::AccessibleOrProxy GetInternalObj(AtkObject* aObj);
AtkObject* GetWrapperFor(mozilla::a11y::ProxyAccessible* aProxy);
AtkObject* GetWrapperFor(mozilla::a11y::AccessibleOrProxy aObj);

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
  mozilla::a11y::AccessibleOrProxy accWrap;

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

  /*
   * Notify ATK of a text change within this ATK object.
   */
  void FireTextChangeEvent(const nsString& aStr, int32_t aStart, uint32_t aLen,
                           bool aIsInsert, bool aIsFromUser);

private:
  /*
   * do we have text-remove and text-insert signals if not we need to use
   * text-changed see AccessibleWrap::FireAtkTextChangedEvent() and
   * bug 619002
   */
  enum EAvailableAtkSignals {
    eUnknown,
    eHaveNewAtkTextSignals,
    eNoNewAtkSignals
  };

  static EAvailableAtkSignals gAvailableAtkSignals;
};

#endif /* __NS_MAI_H__ */
