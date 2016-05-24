/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AnimationUtils.h"

#include "nsDebug.h"
#include "nsIAtom.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsGlobalWindow.h"
#include "nsString.h"
#include "xpcpublic.h" // For xpc::NativeGlobal
#include "mozilla/Preferences.h"

namespace mozilla {

/* static */ void
AnimationUtils::LogAsyncAnimationFailure(nsCString& aMessage,
                                         const nsIContent* aContent)
{
  if (aContent) {
    aMessage.AppendLiteral(" [");
    aMessage.Append(nsAtomCString(aContent->NodeInfo()->NameAtom()));

    nsIAtom* id = aContent->GetID();
    if (id) {
      aMessage.AppendLiteral(" with id '");
      aMessage.Append(nsAtomCString(aContent->GetID()));
      aMessage.Append('\'');
    }
    aMessage.Append(']');
  }
  aMessage.Append('\n');
  printf_stderr("%s", aMessage.get());
}

/* static */ nsIDocument*
AnimationUtils::GetCurrentRealmDocument(JSContext* aCx)
{
  nsGlobalWindow* win = xpc::CurrentWindowOrNull(aCx);
  if (!win) {
    return nullptr;
  }
  return win->GetDoc();
}

/* static */ bool
AnimationUtils::IsOffscreenThrottlingEnabled()
{
  static bool sOffscreenThrottlingEnabled;
  static bool sPrefCached = false;

  if (!sPrefCached) {
    sPrefCached = true;
    Preferences::AddBoolVarCache(&sOffscreenThrottlingEnabled,
                                 "dom.animations.offscreen-throttling");
  }

  return sOffscreenThrottlingEnabled;
}

} // namespace mozilla
