/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PluginIdentifierParent.h"

#include "nsNPAPIPlugin.h"
#include "nsServiceManagerUtils.h"
#include "PluginScriptableObjectUtils.h"
#include "nsCxPusher.h"
#include "mozilla/unused.h"

using namespace mozilla::plugins::parent;

namespace mozilla {
namespace plugins {

bool
PluginIdentifierParent::RecvRetain()
{
  mTemporaryRefs = 0;

  // Intern the jsid if necessary.
  AutoSafeJSContext cx;
  JS::Rooted<jsid> id(cx, NPIdentifierToJSId(mIdentifier));
  if (JSID_IS_INT(id)) {
    return true;
  }

  // The following is what nsNPAPIPlugin.cpp does. Gross, but the API doesn't
  // give you a NPP to play with.
  JS::RootedString str(cx, JSID_TO_STRING(id));
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
  : mIdentifier(nullptr)
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
