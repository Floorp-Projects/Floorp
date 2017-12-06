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
  tmp->UnlinkModuleRecord();
  tmp->mError.setUndefined();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(ModuleScript)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mLoader)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(ModuleScript)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mModuleRecord)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mError)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(ModuleScript)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ModuleScript)

ModuleScript::ModuleScript(ScriptLoader* aLoader, nsIURI* aBaseURL)
 : mLoader(aLoader),
   mBaseURL(aBaseURL)
{
  MOZ_ASSERT(mLoader);
  MOZ_ASSERT(mBaseURL);
  MOZ_ASSERT(!mModuleRecord);
  MOZ_ASSERT(mError.isUndefined());
}

void
ModuleScript::UnlinkModuleRecord()
{
  // Remove module's back reference to this object request if present.
  if (mModuleRecord) {
    MOZ_ASSERT(JS::GetModuleHostDefinedField(mModuleRecord).toPrivate() ==
               this);
    JS::SetModuleHostDefinedField(mModuleRecord, JS::UndefinedValue());
    mModuleRecord = nullptr;
  }
}

ModuleScript::~ModuleScript()
{
  // The object may be destroyed without being unlinked first.
  UnlinkModuleRecord();
  DropJSObjects(this);
}

void
ModuleScript::SetModuleRecord(JS::Handle<JSObject*> aModuleRecord)
{
  MOZ_ASSERT(!mModuleRecord);
  MOZ_ASSERT(mError.isUndefined());

  mModuleRecord = aModuleRecord;

  // Make module's host defined field point to this module script object.
  // This is cleared in the UnlinkModuleRecord().
  JS::SetModuleHostDefinedField(mModuleRecord, JS::PrivateValue(this));
  HoldJSObjects(this);
}

void
ModuleScript::SetPreInstantiationError(const JS::Value& aError)
{
  MOZ_ASSERT(!aError.isUndefined());

  UnlinkModuleRecord();
  mError = aError;

  HoldJSObjects(this);
}

bool
ModuleScript::IsErrored() const
{
  if (!mModuleRecord) {
    MOZ_ASSERT(!mError.isUndefined());
    return true;
  }

  return JS::IsModuleErrored(mModuleRecord);
}

JS::Value
ModuleScript::Error() const
{
  MOZ_ASSERT(IsErrored());

  if (!mModuleRecord) {
    return mError;
  }

  return JS::GetModuleError(mModuleRecord);
}

} // dom namespace
} // mozilla namespace
