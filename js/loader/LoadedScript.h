/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_loader_LoadedScript_h
#define js_loader_LoadedScript_h

#include "js/AllocPolicy.h"
#include "js/Transcoding.h"

#include "mozilla/Maybe.h"
#include "mozilla/MaybeOneOf.h"
#include "mozilla/Utf8.h"  // mozilla::Utf8Unit
#include "mozilla/Variant.h"
#include "mozilla/Vector.h"

#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"

#include "jsapi.h"
#include "ScriptKind.h"
#include "ScriptFetchOptions.h"

class nsIURI;

namespace JS::loader {

class ScriptLoadRequest;

using Utf8Unit = mozilla::Utf8Unit;

void HostAddRefTopLevelScript(const JS::Value& aPrivate);
void HostReleaseTopLevelScript(const JS::Value& aPrivate);

class ClassicScript;
class ModuleScript;
class EventScript;
class LoadContextBase;

class LoadedScript : public nsISupports {
  ScriptKind mKind;
  const mozilla::dom::ReferrerPolicy mReferrerPolicy;
  RefPtr<ScriptFetchOptions> mFetchOptions;
  nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsIURI> mBaseURL;

 protected:
  LoadedScript(ScriptKind aKind, mozilla::dom::ReferrerPolicy aReferrerPolicy,
               ScriptFetchOptions* aFetchOptions, nsIURI* aURI);

  virtual ~LoadedScript();

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(LoadedScript)

  bool IsClassicScript() const { return mKind == ScriptKind::eClassic; }
  bool IsModuleScript() const { return mKind == ScriptKind::eModule; }
  bool IsEventScript() const { return mKind == ScriptKind::eEvent; }

  inline ClassicScript* AsClassicScript();
  inline ModuleScript* AsModuleScript();
  inline EventScript* AsEventScript();

  // Used to propagate Fetch Options to child modules
  ScriptFetchOptions* GetFetchOptions() const { return mFetchOptions; }

  mozilla::dom::ReferrerPolicy ReferrerPolicy() const {
    return mReferrerPolicy;
  }

  nsIURI* GetURI() const { return mURI; }
  void SetBaseURL(nsIURI* aBaseURL) {
    MOZ_ASSERT(!mBaseURL);
    mBaseURL = aBaseURL;
  }
  nsIURI* BaseURL() const { return mBaseURL; }

  void AssociateWithScript(JSScript* aScript);

 public:
  // ===========================================================================
  // Encoding of the content provided by the network, or refined by the JS
  // engine.
  template <typename... Ts>
  using Variant = mozilla::Variant<Ts...>;

  template <typename... Ts>
  using VariantType = mozilla::VariantType<Ts...>;

  // Type of data provided by the nsChannel.
  enum class DataType : uint8_t { eUnknown, eTextSource, eBytecode };

  // Use a vector backed by the JS allocator for script text so that contents
  // can be transferred in constant time to the JS engine, not copied in linear
  // time.
  template <typename Unit>
  using ScriptTextBuffer = mozilla::Vector<Unit, 0, js::MallocAllocPolicy>;

  using MaybeSourceText =
      mozilla::MaybeOneOf<JS::SourceText<char16_t>, JS::SourceText<Utf8Unit>>;

  bool IsUnknownDataType() const { return mDataType == DataType::eUnknown; }
  bool IsTextSource() const { return mDataType == DataType::eTextSource; }
  bool IsSource() const { return IsTextSource(); }
  bool IsBytecode() const { return mDataType == DataType::eBytecode; }

  void SetUnknownDataType() {
    mDataType = DataType::eUnknown;
    mScriptData.reset();
  }

  void SetTextSource(LoadContextBase* maybeLoadContext) {
    MOZ_ASSERT(IsUnknownDataType());
    mDataType = DataType::eTextSource;
    mScriptData.emplace(VariantType<ScriptTextBuffer<Utf8Unit>>());
  }

  void SetBytecode() {
    MOZ_ASSERT(IsUnknownDataType());
    mDataType = DataType::eBytecode;
  }

  bool IsUTF16Text() const {
    return mScriptData->is<ScriptTextBuffer<char16_t>>();
  }
  bool IsUTF8Text() const {
    return mScriptData->is<ScriptTextBuffer<Utf8Unit>>();
  }

  template <typename Unit>
  const ScriptTextBuffer<Unit>& ScriptText() const {
    MOZ_ASSERT(IsTextSource());
    return mScriptData->as<ScriptTextBuffer<Unit>>();
  }
  template <typename Unit>
  ScriptTextBuffer<Unit>& ScriptText() {
    MOZ_ASSERT(IsTextSource());
    return mScriptData->as<ScriptTextBuffer<Unit>>();
  }

  size_t ScriptTextLength() const {
    MOZ_ASSERT(IsTextSource());
    return IsUTF16Text() ? ScriptText<char16_t>().length()
                         : ScriptText<Utf8Unit>().length();
  }

  // Get source text.  On success |aMaybeSource| will contain either UTF-8 or
  // UTF-16 source; on failure it will remain in its initial state.
  nsresult GetScriptSource(JSContext* aCx, MaybeSourceText* aMaybeSource,
                           LoadContextBase* aMaybeLoadContext);

  void ClearScriptSource() {
    if (IsTextSource()) {
      ClearScriptText();
    }
  }

  void ClearScriptText() {
    MOZ_ASSERT(IsTextSource());
    return IsUTF16Text() ? ScriptText<char16_t>().clearAndFree()
                         : ScriptText<Utf8Unit>().clearAndFree();
  }

  size_t ReceivedScriptTextLength() const { return mReceivedScriptTextLength; }

  void SetReceivedScriptTextLength(size_t aLength) {
    mReceivedScriptTextLength = aLength;
  }

  JS::TranscodeBuffer& SRIAndBytecode() {
    // Note: SRIAndBytecode might be called even if the IsSource() returns true,
    // as we want to be able to save the bytecode content when we are loading
    // from source.
    MOZ_ASSERT(IsBytecode() || IsSource());
    return mScriptBytecode;
  }
  JS::TranscodeRange Bytecode() const {
    MOZ_ASSERT(IsBytecode());
    const auto& bytecode = mScriptBytecode;
    auto offset = mBytecodeOffset;
    return JS::TranscodeRange(bytecode.begin() + offset,
                              bytecode.length() - offset);
  }

  size_t GetSRILength() const {
    MOZ_ASSERT(IsBytecode() || IsSource());
    return mBytecodeOffset;
  }
  void SetSRILength(size_t sriLength) {
    MOZ_ASSERT(IsBytecode() || IsSource());
    mBytecodeOffset = JS::AlignTranscodingBytecodeOffset(sriLength);
  }

  void DropBytecode() {
    MOZ_ASSERT(IsBytecode() || IsSource());
    mScriptBytecode.clearAndFree();
  }

  // Determine whether the mScriptData or mScriptBytecode is used.
  DataType mDataType;

  // Holds script source data for non-inline scripts.
  mozilla::Maybe<
      Variant<ScriptTextBuffer<char16_t>, ScriptTextBuffer<Utf8Unit>>>
      mScriptData;

  // The length of script source text, set when reading completes. This is used
  // since mScriptData is cleared when the source is passed to the JS engine.
  size_t mReceivedScriptTextLength;

  // Holds the SRI serialized hash and the script bytecode for non-inline
  // scripts. The data is laid out according to ScriptBytecodeDataLayout
  // or, if compression is enabled, ScriptBytecodeCompressedDataLayout.
  JS::TranscodeBuffer mScriptBytecode;
  uint32_t mBytecodeOffset;  // Offset of the bytecode in mScriptBytecode
};

// Provide accessors for any classes `Derived` which is providing the
// `getLoadedScript` function as interface. The accessors are meant to be
// inherited by the `Derived` class.
template <typename Derived>
class LoadedScriptDelegate {
 private:
  // Use a static_cast<Derived> instead of declaring virtual functions. This is
  // meant to avoid relying on virtual table, and improve inlining for non-final
  // classes.
  const LoadedScript* GetLoadedScript() const {
    return static_cast<const Derived*>(this)->getLoadedScript();
  }
  LoadedScript* GetLoadedScript() {
    return static_cast<Derived*>(this)->getLoadedScript();
  }

 public:
  template <typename Unit>
  using ScriptTextBuffer = LoadedScript::ScriptTextBuffer<Unit>;
  using MaybeSourceText = LoadedScript::MaybeSourceText;

  bool IsModuleScript() const { return GetLoadedScript()->IsModuleScript(); }
  bool IsEventScript() const { return GetLoadedScript()->IsEventScript(); }

  bool IsUnknownDataType() const {
    return GetLoadedScript()->IsUnknownDataType();
  }
  bool IsTextSource() const { return GetLoadedScript()->IsTextSource(); }
  bool IsSource() const { return GetLoadedScript()->IsSource(); }
  bool IsBytecode() const { return GetLoadedScript()->IsBytecode(); }

  void SetUnknownDataType() { GetLoadedScript()->SetUnknownDataType(); }

  void SetTextSource(LoadContextBase* maybeLoadContext) {
    GetLoadedScript()->SetTextSource(maybeLoadContext);
  }

  void SetBytecode() { GetLoadedScript()->SetBytecode(); }

  bool IsUTF16Text() const { return GetLoadedScript()->IsUTF16Text(); }
  bool IsUTF8Text() const { return GetLoadedScript()->IsUTF8Text(); }

  template <typename Unit>
  const ScriptTextBuffer<Unit>& ScriptText() const {
    const LoadedScript* loader = GetLoadedScript();
    return loader->ScriptText<Unit>();
  }
  template <typename Unit>
  ScriptTextBuffer<Unit>& ScriptText() {
    LoadedScript* loader = GetLoadedScript();
    return loader->ScriptText<Unit>();
  }

  size_t ScriptTextLength() const {
    return GetLoadedScript()->ScriptTextLength();
  }

  size_t ReceivedScriptTextLength() const {
    return GetLoadedScript()->ReceivedScriptTextLength();
  }

  void SetReceivedScriptTextLength(size_t aLength) {
    GetLoadedScript()->SetReceivedScriptTextLength(aLength);
  }

  // Get source text.  On success |aMaybeSource| will contain either UTF-8 or
  // UTF-16 source; on failure it will remain in its initial state.
  nsresult GetScriptSource(JSContext* aCx, MaybeSourceText* aMaybeSource,
                           LoadContextBase* aLoadContext) {
    return GetLoadedScript()->GetScriptSource(aCx, aMaybeSource, aLoadContext);
  }

  void ClearScriptSource() { GetLoadedScript()->ClearScriptSource(); }

  void ClearScriptText() { GetLoadedScript()->ClearScriptText(); }

  JS::TranscodeBuffer& SRIAndBytecode() {
    return GetLoadedScript()->SRIAndBytecode();
  }
  JS::TranscodeRange Bytecode() const { return GetLoadedScript()->Bytecode(); }

  size_t GetSRILength() const { return GetLoadedScript()->GetSRILength(); }
  void SetSRILength(size_t sriLength) {
    GetLoadedScript()->SetSRILength(sriLength);
  }

  void DropBytecode() { GetLoadedScript()->DropBytecode(); }
};

class ClassicScript final : public LoadedScript {
  ~ClassicScript() = default;

 private:
  // Scripts can be created only by ScriptLoadRequest::NoCacheEntryFound.
  ClassicScript(mozilla::dom::ReferrerPolicy aReferrerPolicy,
                ScriptFetchOptions* aFetchOptions, nsIURI* aURI);

  friend class ScriptLoadRequest;
};

class EventScript final : public LoadedScript {
  ~EventScript() = default;

 public:
  EventScript(mozilla::dom::ReferrerPolicy aReferrerPolicy,
              ScriptFetchOptions* aFetchOptions, nsIURI* aURI);
};

// A single module script. May be used to satisfy multiple load requests.

class ModuleScript final : public LoadedScript {
  JS::Heap<JSObject*> mModuleRecord;
  JS::Heap<JS::Value> mParseError;
  JS::Heap<JS::Value> mErrorToRethrow;
  bool mDebuggerDataInitialized;

  ~ModuleScript();

 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(ModuleScript,
                                                         LoadedScript)

 private:
  // Scripts can be created only by ScriptLoadRequest::NoCacheEntryFound.
  ModuleScript(mozilla::dom::ReferrerPolicy aReferrerPolicy,
               ScriptFetchOptions* aFetchOptions, nsIURI* aURI);

  friend class ScriptLoadRequest;

 public:
  void SetModuleRecord(JS::Handle<JSObject*> aModuleRecord);
  void SetParseError(const JS::Value& aError);
  void SetErrorToRethrow(const JS::Value& aError);
  void SetDebuggerDataInitialized();

  JSObject* ModuleRecord() const { return mModuleRecord; }

  JS::Value ParseError() const { return mParseError; }
  JS::Value ErrorToRethrow() const { return mErrorToRethrow; }
  bool HasParseError() const { return !mParseError.isUndefined(); }
  bool HasErrorToRethrow() const { return !mErrorToRethrow.isUndefined(); }
  bool DebuggerDataInitialized() const { return mDebuggerDataInitialized; }

  void Shutdown();

  void UnlinkModuleRecord();

  friend void CheckModuleScriptPrivate(LoadedScript*, const JS::Value&);
};

ClassicScript* LoadedScript::AsClassicScript() {
  MOZ_ASSERT(!IsModuleScript());
  return static_cast<ClassicScript*>(this);
}

ModuleScript* LoadedScript::AsModuleScript() {
  MOZ_ASSERT(IsModuleScript());
  return static_cast<ModuleScript*>(this);
}

}  // namespace JS::loader

#endif  // js_loader_LoadedScript_h
