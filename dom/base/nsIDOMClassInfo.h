/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIDOMClassInfo_h___
#define nsIDOMClassInfo_h___

#include "nsIXPCScriptable.h"

#define DOM_BASE_SCRIPTABLE_FLAGS                                          \
  (XPC_SCRIPTABLE_USE_JSSTUB_FOR_ADDPROPERTY |                             \
   XPC_SCRIPTABLE_USE_JSSTUB_FOR_DELPROPERTY |                             \
   XPC_SCRIPTABLE_ALLOW_PROP_MODS_TO_PROTOTYPE |                           \
   XPC_SCRIPTABLE_DONT_ASK_INSTANCE_FOR_SCRIPTABLE |                       \
   XPC_SCRIPTABLE_DONT_REFLECT_INTERFACE_NAMES)

#define DEFAULT_SCRIPTABLE_FLAGS                                           \
  (DOM_BASE_SCRIPTABLE_FLAGS |                                             \
   XPC_SCRIPTABLE_WANT_RESOLVE |                                           \
   XPC_SCRIPTABLE_WANT_PRECREATE)

#define DOM_DEFAULT_SCRIPTABLE_FLAGS                                       \
  (DEFAULT_SCRIPTABLE_FLAGS |                                              \
   XPC_SCRIPTABLE_DONT_ENUM_QUERY_INTERFACE |                              \
   XPC_SCRIPTABLE_CLASSINFO_INTERFACES_ONLY)

#endif /* nsIDOMClassInfo_h___ */
