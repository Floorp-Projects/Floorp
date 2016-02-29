/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIDOMClassInfo_h___
#define nsIDOMClassInfo_h___

#include "nsIXPCScriptable.h"

#define DOM_BASE_SCRIPTABLE_FLAGS                                          \
  (nsIXPCScriptable::USE_JSSTUB_FOR_ADDPROPERTY |                          \
   nsIXPCScriptable::USE_JSSTUB_FOR_DELPROPERTY |                          \
   nsIXPCScriptable::USE_JSSTUB_FOR_SETPROPERTY |                          \
   nsIXPCScriptable::ALLOW_PROP_MODS_TO_PROTOTYPE |                        \
   nsIXPCScriptable::DONT_ASK_INSTANCE_FOR_SCRIPTABLE |                    \
   nsIXPCScriptable::DONT_REFLECT_INTERFACE_NAMES)

#define DEFAULT_SCRIPTABLE_FLAGS                                           \
  (DOM_BASE_SCRIPTABLE_FLAGS |                                             \
   nsIXPCScriptable::WANT_RESOLVE |                                        \
   nsIXPCScriptable::WANT_PRECREATE)

#define DOM_DEFAULT_SCRIPTABLE_FLAGS                                       \
  (DEFAULT_SCRIPTABLE_FLAGS |                                              \
   nsIXPCScriptable::DONT_ENUM_QUERY_INTERFACE |                           \
   nsIXPCScriptable::CLASSINFO_INTERFACES_ONLY)

#endif /* nsIDOMClassInfo_h___ */
