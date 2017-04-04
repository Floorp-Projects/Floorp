/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_AccessibleHandler_h
#define mozilla_a11y_AccessibleHandler_h

#define NEWEST_IA2_BASENAME Accessible2_3

#define __QUOTE(idl) #idl
#define __GENIDL(base) __QUOTE(base##.idl)
#define IDLFOR(base) __GENIDL(base)
#define NEWEST_IA2_IDL IDLFOR(NEWEST_IA2_BASENAME)

#define __GENIFACE(base) I##base
#define INTERFACEFOR(base) __GENIFACE(base)
#define NEWEST_IA2_INTERFACE INTERFACEFOR(NEWEST_IA2_BASENAME)

#define __GENIID(iface) IID_##iface
#define IIDFOR(iface) __GENIID(iface)
#define NEWEST_IA2_IID IIDFOR(NEWEST_IA2_INTERFACE)

#if defined(__midl)

import NEWEST_IA2_IDL;

#else

#include "HandlerData.h"

#include <windows.h>

#endif // defined(__midl)

#endif // mozilla_a11y_AccessibleHandler_h
