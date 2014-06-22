/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <atk/atk.h>
#include "AtkSocketAccessible.h"

#include "InterfaceInitFuncs.h"
#include "nsMai.h"
#include "mozilla/Likely.h"

using namespace mozilla::a11y;

AtkSocketEmbedType AtkSocketAccessible::g_atk_socket_embed = nullptr;
GType AtkSocketAccessible::g_atk_socket_type = G_TYPE_INVALID;
const char* AtkSocketAccessible::sATKSocketEmbedSymbol = "atk_socket_embed";
const char* AtkSocketAccessible::sATKSocketGetTypeSymbol = "atk_socket_get_type";

bool AtkSocketAccessible::gCanEmbed = FALSE;

extern "C" void mai_atk_component_iface_init(AtkComponentIface* aIface);
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

  AccessibleWrap* accWrap;
} MaiAtkSocket;

typedef struct _MaiAtkSocketClass
{
  AtkSocketClass parent_class;
} MaiAtkSocketClass;

G_DEFINE_TYPE_EXTENDED(MaiAtkSocket, mai_atk_socket,
                       AtkSocketAccessible::g_atk_socket_type, 0,
                       G_IMPLEMENT_INTERFACE(ATK_TYPE_COMPONENT,
                                             mai_atk_component_iface_init))

void
mai_atk_socket_class_init(MaiAtkSocketClass* aAcc)
{
}

void
mai_atk_socket_init(MaiAtkSocket* aAcc)
{
}

static AtkObject*
mai_atk_socket_new(AccessibleWrap* aAccWrap)
{
  NS_ENSURE_TRUE(aAccWrap, nullptr);

  MaiAtkSocket* acc = nullptr;
  acc = static_cast<MaiAtkSocket*>(g_object_new(MAI_TYPE_ATK_SOCKET, nullptr));
  NS_ENSURE_TRUE(acc, nullptr);

  acc->accWrap = aAccWrap;
  return ATK_OBJECT(acc);
}

extern "C" {
static AtkObject*
RefAccessibleAtPoint(AtkComponent* aComponent, gint aX, gint aY,
                     AtkCoordType aCoordType)
{
  NS_ENSURE_TRUE(MAI_IS_ATK_SOCKET(aComponent), nullptr);

  return refAccessibleAtPointHelper(MAI_ATK_SOCKET(aComponent)->accWrap,
                                    aX, aY, aCoordType);
}

static void
GetExtents(AtkComponent* aComponent, gint* aX, gint* aY, gint* aWidth,
           gint* aHeight, AtkCoordType aCoordType)
{
  *aX = *aY = *aWidth = *aHeight = 0;

  if (!MAI_IS_ATK_SOCKET(aComponent))
    return;

  getExtentsHelper(MAI_ATK_SOCKET(aComponent)->accWrap,
                   aX, aY, aWidth, aHeight, aCoordType);
}
}

void
mai_atk_component_iface_init(AtkComponentIface* aIface)
{
  NS_ASSERTION(aIface, "Invalid Interface");
  if (MOZ_UNLIKELY(!aIface))
    return;

  aIface->ref_accessible_at_point = RefAccessibleAtPoint;
  aIface->get_extents = GetExtents;
}

AtkSocketAccessible::AtkSocketAccessible(nsIContent* aContent,
                                         DocAccessible* aDoc,
                                         const nsCString& aPlugId) :
  AccessibleWrap(aContent, aDoc)
{
  mAtkObject = mai_atk_socket_new(this);
  if (!mAtkObject)
    return;

  // Embeds the children of an AtkPlug, specified by plugId, as the children of
  // this socket.
  // Using G_TYPE macros instead of ATK_SOCKET macros to avoid undefined
  // symbols.
  if (gCanEmbed && G_TYPE_CHECK_INSTANCE_TYPE(mAtkObject, g_atk_socket_type) &&
      !aPlugId.IsVoid()) {
    AtkSocket* accSocket =
      G_TYPE_CHECK_INSTANCE_CAST(mAtkObject, g_atk_socket_type, AtkSocket);
    g_atk_socket_embed(accSocket, (gchar*)aPlugId.get());
  }
}

NS_IMETHODIMP
AtkSocketAccessible::GetNativeInterface(void** aOutAccessible)
{
  *aOutAccessible = mAtkObject;
  return NS_OK;
}

void
AtkSocketAccessible::Shutdown()
{
  if (mAtkObject) {
    if (MAI_IS_ATK_SOCKET(mAtkObject))
      MAI_ATK_SOCKET(mAtkObject)->accWrap = nullptr;
    g_object_unref(mAtkObject);
    mAtkObject = nullptr;
  }
  AccessibleWrap::Shutdown();
}
