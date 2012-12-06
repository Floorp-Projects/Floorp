/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 *  * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 *   * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ATK_INTERFACE_INIT_FUNCS_H_
#define ATK_INTERFACE_INIT_FUNCS_H_

#include <atk/atk.h>

namespace mozilla {
namespace a11y {

class AccessibleWrap;

} // namespace a11y
} // namespace mozilla

struct MaiUtilClass;

extern "C" {
void actionInterfaceInitCB(AtkActionIface* aIface);
void componentInterfaceInitCB(AtkComponentIface* aIface);
void documentInterfaceInitCB(AtkDocumentIface *aIface);
void editableTextInterfaceInitCB(AtkEditableTextIface* aIface);
void hyperlinkImplInterfaceInitCB(AtkHyperlinkImplIface *aIface);
void hypertextInterfaceInitCB(AtkHypertextIface* aIface);
void imageInterfaceInitCB(AtkImageIface* aIface);
void selectionInterfaceInitCB(AtkSelectionIface* aIface);
void tableInterfaceInitCB(AtkTableIface *aIface);
void textInterfaceInitCB(AtkTextIface* aIface);
void valueInterfaceInitCB(AtkValueIface *aIface);
}

/**
 * XXX these should live in a file of utils for atk.
 */
AtkObject* refAccessibleAtPointHelper(mozilla::a11y::AccessibleWrap* aAccWrap,
                                      gint aX, gint aY, AtkCoordType aCoordType);
void getExtentsHelper(mozilla::a11y::AccessibleWrap* aAccWrap,
                      gint* aX, gint* aY, gint* aWidth, gint* aHeight,
                      AtkCoordType aCoordType);

#endif // ATK_INTERFACE_INIT_FUNCS_H_
