/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et :
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Plugins.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Turner <bent.mozilla@gmail.com>
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

  JSContext *cx = nsnull;
  stack->GetSafeJSContext(&cx);
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
