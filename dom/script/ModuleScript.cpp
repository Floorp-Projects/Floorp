/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ModuleScript.h"
#include "mozilla/HoldDropJSObjects.h"
#include "ScriptLoader.h"

namespace mozilla {
namespace dom {

// A single module script.  May be used to satisfy multiple load requests.

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ModuleScript)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_CLASS(ModuleScript)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(ModuleScript)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mLoader)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mBaseURL)
  tmp->UnlinkScript();
  tmp->mParseError.setUndefined();
  tmp->mErrorToRethrow.setUndefined();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(ModuleScript)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mLoader)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(ModuleScript)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mScript)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mParseError)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mErrorToRethrow)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(ModuleScript)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ModuleScript)

ModuleScript::ModuleScript(ScriptLoader* aLoader, nsIURI* aBaseURL)
 : mLoader(aLoader),
   mBaseURL(aBaseURL),
   mSourceElementAssociated(false)
{
  MOZ_ASSERT(mLoader);
  MOZ_ASSERT(mBaseURL);
  MOZ_ASSERT(!mScript);
  MOZ_ASSERT(!HasParseError());
  MOZ_ASSERT(!HasErrorToRethrow());
}

void
ModuleScript::UnlinkScript()
{
  // Remove module's back reference to this object request if present.
  if (mScript) {
    MOZ_ASSERT(JS::GetTopLevelScriptPrivate(mScript) == this);
    JS::SetTopLevelScriptPrivate(mScript, nullptr);
    mScript = nullptr;
  }
}

ModuleScript::~ModuleScript()
{
  // The object may be destroyed without being unlinked first.
  UnlinkScript();
  DropJSObjects(this);
}

void
ModuleScript::SetScript(JS::Handle<JSScript*> aScript)
{
  MOZ_ASSERT(!mScript);
  MOZ_ASSERT(!HasParseError());
  MOZ_ASSERT(!HasErrorToRethrow());

  mScript = aScript;

  // Make module's host defined field point to this module script object.
  // This is cleared in the UnlinkScript().
  MOZ_ASSERT(!JS::GetTopLevelScriptPrivate(mScript));
  JS::SetTopLevelScriptPrivate(mScript, this);

  HoldJSObjects(this);
}

void
ModuleScript::SetParseError(const JS::Value& aError)
{
  MOZ_ASSERT(!aError.isUndefined());
  MOZ_ASSERT(!HasParseError());
  MOZ_ASSERT(!HasErrorToRethrow());

  UnlinkScript();
  mParseError = aError;
  HoldJSObjects(this);
}

void
ModuleScript::SetErrorToRethrow(const JS::Value& aError)
{
  MOZ_ASSERT(!aError.isUndefined());
  MOZ_ASSERT(!HasErrorToRethrow());

  // This is only called after SetScript() or SetParseError() so we don't
  // need to call HoldJSObjects() here.
  MOZ_ASSERT(mScript || HasParseError());

  mErrorToRethrow = aError;
}

void
ModuleScript::SetSourceElementAssociated()
{
  MOZ_ASSERT(mScript);
  MOZ_ASSERT(!mSourceElementAssociated);

  mSourceElementAssociated = true;
}

} // dom namespace
} // mozilla namespace
