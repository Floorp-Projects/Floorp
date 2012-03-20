/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Novell, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brad Taylor <brad@getcoded.net> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <atk/atk.h>
#include "AtkSocketAccessible.h"

#include "InterfaceInitFuncs.h"
#include "nsMai.h"

AtkSocketEmbedType AtkSocketAccessible::g_atk_socket_embed = NULL;
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

  nsAccessibleWrap* accWrap;
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
mai_atk_socket_new(nsAccessibleWrap* aAccWrap)
{
  NS_ENSURE_TRUE(aAccWrap, NULL);

  MaiAtkSocket* acc = nsnull;
  acc = static_cast<MaiAtkSocket*>(g_object_new(MAI_TYPE_ATK_SOCKET, NULL));
  NS_ENSURE_TRUE(acc, NULL);

  acc->accWrap = aAccWrap;
  return ATK_OBJECT(acc);
}

extern "C" {
static AtkObject*
RefAccessibleAtPoint(AtkComponent* aComponent, gint aX, gint aY,
                     AtkCoordType aCoordType)
{
  NS_ENSURE_TRUE(MAI_IS_ATK_SOCKET(aComponent), nsnull);

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
  if (NS_UNLIKELY(!aIface))
    return;

  aIface->ref_accessible_at_point = RefAccessibleAtPoint;
  aIface->get_extents = GetExtents;
}

AtkSocketAccessible::AtkSocketAccessible(nsIContent* aContent,
                                         nsDocAccessible* aDoc,
                                         const nsCString& aPlugId) :
  nsAccessibleWrap(aContent, aDoc)
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
      MAI_ATK_SOCKET(mAtkObject)->accWrap = nsnull;
    g_object_unref(mAtkObject);
    mAtkObject = nsnull;
  }
  nsAccessibleWrap::Shutdown();
}
