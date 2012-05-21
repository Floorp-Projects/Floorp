/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PluginIdentifierParent.h"

#include "nsServiceManagerUtils.h"
#include "nsNPAPIPlugin.h"
#include "nsIJSContextStack.h"
#include "PluginScriptableObjectUtils.h"
#include "mozilla/unused.h"

using namespace mozilla::plugins::parent;

namespace mozilla {
namespace plugins {

bool
PluginIdentifierParent::RecvRetain()
{
  mTemporaryRefs = 0;

  // Intern the jsid if necessary.
  jsid id = NPIdentifierToJSId(mIdentifier);
  if (JSID_IS_INT(id)) {
    return true;
  }

  // The following is what nsNPAPIPlugin.cpp does. Gross, but the API doesn't
  // give you a NPP to play with.
  nsCOMPtr<nsIThreadJSContextStack> stack =
    do_GetService("@mozilla.org/js/xpc/ContextStack;1");
  if (!stack) {
    return false;
  }

  JSContext* cx = stack->GetSafeJSContext();
  if (!cx) {
    return false;
  }

  JSAutoRequest ar(cx);
  JSString* str = JSID_TO_STRING(id);
  JSString* str2 = JS_InternJSString(cx, str);
  if (!str2) {
    return false;
  }

  NS_ASSERTION(str == str2, "Interning a string in a JSID should always return the same string.");

  return true;
}

PluginIdentifierParent::StackIdentifier::StackIdentifier
    (PluginInstanceParent* inst, NPIdentifier aIdentifier)
  : mIdentifier(inst->Module()->GetIdentifierForNPIdentifier(inst->GetNPP(), aIdentifier))
{
}

PluginIdentifierParent::StackIdentifier::StackIdentifier
    (NPObject* aObject, NPIdentifier aIdentifier)
  : mIdentifier(NULL)
{
  PluginInstanceParent* inst = GetInstance(aObject);
  mIdentifier = inst->Module()->GetIdentifierForNPIdentifier(inst->GetNPP(), aIdentifier);
}

PluginIdentifierParent::StackIdentifier::~StackIdentifier()
{
  if (!mIdentifier) {
    return;
  }

  if (!mIdentifier->IsTemporary()) {
    return;
  }

  if (mIdentifier->RemoveTemporaryRef()) {
    unused << PPluginIdentifierParent::Send__delete__(mIdentifier);
  }
}

} // namespace mozilla::plugins
} // namespace mozilla
