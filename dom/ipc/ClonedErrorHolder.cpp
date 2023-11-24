/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ClonedErrorHolder.h"

#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/ClonedErrorHolderBinding.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/DOMExceptionBinding.h"
#include "mozilla/dom/Exceptions.h"
#include "mozilla/dom/StructuredCloneTags.h"
#include "mozilla/dom/ToJSValue.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/StructuredClone.h"
#include "nsReadableUtils.h"
#include "xpcpublic.h"

using namespace mozilla;
using namespace mozilla::dom;

// static
UniquePtr<ClonedErrorHolder> ClonedErrorHolder::Constructor(
    const GlobalObject& aGlobal, JS::Handle<JSObject*> aError,
    ErrorResult& aRv) {
  return Create(aGlobal.Context(), aError, aRv);
}

// static
UniquePtr<ClonedErrorHolder> ClonedErrorHolder::Create(
    JSContext* aCx, JS::Handle<JSObject*> aError, ErrorResult& aRv) {
  UniquePtr<ClonedErrorHolder> ceh(new ClonedErrorHolder());
  ceh->Init(aCx, aError, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  return ceh;
}

ClonedErrorHolder::ClonedErrorHolder()
    : mName(VoidCString()),
      mMessage(VoidCString()),
      mFilename(VoidCString()),
      mSourceLine(VoidCString()) {}

void ClonedErrorHolder::Init(JSContext* aCx, JS::Handle<JSObject*> aError,
                             ErrorResult& aRv) {
  JS::Rooted<JSObject*> stack(aCx);

  if (JSErrorReport* err = JS_ErrorFromException(aCx, aError)) {
    mType = Type::JSError;
    if (err->message()) {
      mMessage = err->message().c_str();
    }
    if (err->filename) {
      mFilename = err->filename.c_str();
    }
    if (err->linebuf()) {
      AppendUTF16toUTF8(
          nsDependentSubstring(err->linebuf(), err->linebufLength()),
          mSourceLine);
      mTokenOffset = err->tokenOffset();
    }
    mLineNumber = err->lineno;
    mColumn = err->column;
    mErrorNumber = err->errorNumber;
    mExnType = JSExnType(err->exnType);

    // Note: We don't save the souce ID here, since this object is cross-process
    // clonable, and the source ID won't be valid in other processes.
    // We don't store the source notes either, though for no other reason that
    // it isn't clear that it's worth the complexity.

    stack = JS::ExceptionStackOrNull(aError);
  } else {
    RefPtr<DOMException> domExn;
    RefPtr<Exception> exn;
    if (NS_SUCCEEDED(UNWRAP_OBJECT(DOMException, aError, domExn))) {
      mType = Type::DOMException;
      mCode = domExn->Code();
      exn = std::move(domExn);
    } else if (NS_SUCCEEDED(UNWRAP_OBJECT(Exception, aError, exn))) {
      mType = Type::Exception;
    } else {
      aRv.ThrowNotSupportedError(
          "We can only clone DOM Exceptions and native JS Error objects");
      return;
    }

    nsAutoString str;

    exn->GetName(str);
    CopyUTF16toUTF8(str, mName);

    exn->GetMessageMoz(str);
    CopyUTF16toUTF8(str, mMessage);

    // Note: In DOM exceptions, filename, line number, and column number come
    // from the stack frame, and don't need to be stored separately. mFilename,
    // mLineNumber, and mColumn are only used for JS exceptions.
    //
    // We also don't serialized Exception's mThrownJSVal or mData fields, since
    // they generally won't be serializable.

    mResult = nsresult(exn->Result());

    if (nsCOMPtr<nsIStackFrame> frame = exn->GetLocation()) {
      JS::Rooted<JS::Value> value(aCx);
      frame->GetNativeSavedFrame(&value);
      if (value.isObject()) {
        stack = &value.toObject();
      }
    }
  }

  Maybe<JSAutoRealm> ar;
  if (stack) {
    ar.emplace(aCx, stack);
  }
  JS::Rooted<JS::Value> stackValue(aCx, JS::ObjectOrNullValue(stack));
  mStack.Write(aCx, stackValue, aRv);
}

bool ClonedErrorHolder::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto,
                                   JS::MutableHandle<JSObject*> aReflector) {
  return ClonedErrorHolder_Binding::Wrap(aCx, this, aGivenProto, aReflector);
}

static constexpr uint32_t kVoidStringLength = ~0;

static bool WriteStringPair(JSStructuredCloneWriter* aWriter,
                            const nsACString& aString1,
                            const nsACString& aString2) {
  auto StringLength = [](const nsACString& aStr) -> uint32_t {
    auto length = uint32_t(aStr.Length());
    MOZ_DIAGNOSTIC_ASSERT(length != kVoidStringLength,
                          "We should not be serializing a 4GiB string");
    if (aStr.IsVoid()) {
      return kVoidStringLength;
    }
    return length;
  };

  return JS_WriteUint32Pair(aWriter, StringLength(aString1),
                            StringLength(aString2)) &&
         JS_WriteBytes(aWriter, aString1.BeginReading(), aString1.Length()) &&
         JS_WriteBytes(aWriter, aString2.BeginReading(), aString2.Length());
}

static bool ReadStringPair(JSStructuredCloneReader* aReader,
                           nsACString& aString1, nsACString& aString2) {
  auto ReadString = [&](nsACString& aStr, uint32_t aLength) {
    if (aLength == kVoidStringLength) {
      aStr.SetIsVoid(true);
      return true;
    }
    char* data = nullptr;
    return aLength == 0 || (aStr.GetMutableData(&data, aLength, fallible) &&
                            JS_ReadBytes(aReader, data, aLength));
  };

  aString1.Truncate(0);
  aString2.Truncate(0);

  uint32_t length1, length2;
  return JS_ReadUint32Pair(aReader, &length1, &length2) &&
         ReadString(aString1, length1) && ReadString(aString2, length2);
}

bool ClonedErrorHolder::WriteStructuredClone(JSContext* aCx,
                                             JSStructuredCloneWriter* aWriter,
                                             StructuredCloneHolder* aHolder) {
  auto& data = mStack.BufferData();
  return JS_WriteUint32Pair(aWriter, SCTAG_DOM_CLONED_ERROR_OBJECT, 0) &&
         WriteStringPair(aWriter, mName, mMessage) &&
         WriteStringPair(aWriter, mFilename, mSourceLine) &&
         JS_WriteUint32Pair(aWriter, mLineNumber,
                            *mColumn.addressOfValueForTranscode()) &&
         JS_WriteUint32Pair(aWriter, mTokenOffset, mErrorNumber) &&
         JS_WriteUint32Pair(aWriter, uint32_t(mType), uint32_t(mExnType)) &&
         JS_WriteUint32Pair(aWriter, mCode, uint32_t(mResult)) &&
         JS_WriteUint32Pair(aWriter, data.Size(),
                            JS_STRUCTURED_CLONE_VERSION) &&
         data.ForEachDataChunk([&](const char* aData, size_t aSize) {
           return JS_WriteBytes(aWriter, aData, aSize);
         });
}

bool ClonedErrorHolder::Init(JSContext* aCx, JSStructuredCloneReader* aReader) {
  uint32_t type, exnType, result, code;
  if (!(ReadStringPair(aReader, mName, mMessage) &&
        ReadStringPair(aReader, mFilename, mSourceLine) &&
        JS_ReadUint32Pair(aReader, &mLineNumber,
                          mColumn.addressOfValueForTranscode()) &&
        JS_ReadUint32Pair(aReader, &mTokenOffset, &mErrorNumber) &&
        JS_ReadUint32Pair(aReader, &type, &exnType) &&
        JS_ReadUint32Pair(aReader, &code, &result) &&
        mStack.ReadStructuredCloneInternal(aCx, aReader))) {
    return false;
  }

  if (type == uint32_t(Type::Uninitialized) || type >= uint32_t(Type::Max_) ||
      exnType >= uint32_t(JSEXN_ERROR_LIMIT)) {
    return false;
  }

  mType = Type(type);
  mExnType = JSExnType(exnType);
  mResult = nsresult(result);
  mCode = code;

  return true;
}

/* static */
JSObject* ClonedErrorHolder::ReadStructuredClone(
    JSContext* aCx, JSStructuredCloneReader* aReader,
    StructuredCloneHolder* aHolder) {
  // Keep the result object rooted across the call to ClonedErrorHolder::Release
  // to avoid a potential rooting hazard.
  JS::Rooted<JS::Value> errorVal(aCx);
  {
    UniquePtr<ClonedErrorHolder> ceh(new ClonedErrorHolder());
    if (!ceh->Init(aCx, aReader) || !ceh->ToErrorValue(aCx, &errorVal)) {
      return nullptr;
    }
  }
  return &errorVal.toObject();
}

static JS::UniqueTwoByteChars ToNullTerminatedJSStringBuffer(
    JSContext* aCx, const nsString& aStr) {
  // Since nsString is null terminated, we can simply copy + 1 characters.
  size_t nbytes = (aStr.Length() + 1) * sizeof(char16_t);
  JS::UniqueTwoByteChars buffer(static_cast<char16_t*>(JS_malloc(aCx, nbytes)));
  if (buffer) {
    memcpy(buffer.get(), aStr.get(), nbytes);
  }
  return buffer;
}

static bool ToJSString(JSContext* aCx, const nsACString& aStr,
                       JS::MutableHandle<JSString*> aJSString) {
  if (aStr.IsVoid()) {
    aJSString.set(nullptr);
    return true;
  }
  JS::Rooted<JS::Value> res(aCx);
  if (xpc::NonVoidStringToJsval(aCx, NS_ConvertUTF8toUTF16(aStr), &res)) {
    aJSString.set(res.toString());
    return true;
  }
  return false;
}

bool ClonedErrorHolder::ToErrorValue(JSContext* aCx,
                                     JS::MutableHandle<JS::Value> aResult) {
  JS::Rooted<JS::Value> stackVal(aCx);
  JS::Rooted<JSObject*> stack(aCx);

  IgnoredErrorResult rv;
  mStack.Read(xpc::CurrentNativeGlobal(aCx), aCx, &stackVal, rv);
  // Note: We continue even if reading the stack fails, since we can still
  // produce a useful error object even without a stack. That said, if decoding
  // the stack fails, there's a pretty good chance that the rest of the message
  // is corrupt, and there's no telling how useful the final result will
  // actually be.
  if (!rv.Failed() && stackVal.isObject()) {
    stack = &stackVal.toObject();
    // Make sure that this is really a saved frame. This mainly ensures that
    // malicious code on the child side can't trigger a memory exploit by
    // sending an incompatible data type, but also protects against potential
    // issues like a cross-compartment wrapper being unexpectedly cut.
    if (!js::IsSavedFrame(stack)) {
      stack = nullptr;
    }
  }

  if (mType == Type::JSError) {
    JS::Rooted<JSString*> filename(aCx);
    JS::Rooted<JSString*> message(aCx);

    // For some unknown reason, we can end up with a void string in mFilename,
    // which will cause filename to be null, which causes JS::CreateError() to
    // crash. Make this code against robust against this by treating void
    // strings as the empty string.
    if (mFilename.IsVoid()) {
      mFilename.Assign(""_ns);
    }

    // When fuzzing, we can also end up with the message to be null,
    // so we should handle that case as well.
    if (mMessage.IsVoid()) {
      mMessage.Assign(""_ns);
    }

    if (!ToJSString(aCx, mFilename, &filename) ||
        !ToJSString(aCx, mMessage, &message)) {
      return false;
    }
    if (!JS::CreateError(aCx, mExnType, stack, filename, mLineNumber, mColumn,
                         nullptr, message, JS::NothingHandleValue, aResult)) {
      return false;
    }

    if (!mSourceLine.IsVoid()) {
      JS::Rooted<JSObject*> errObj(aCx, &aResult.toObject());
      if (JSErrorReport* err = JS_ErrorFromException(aCx, errObj)) {
        NS_ConvertUTF8toUTF16 sourceLine(mSourceLine);
        // Because this string ends up being consumed as an nsDependentString
        // in nsXPCComponents_Utils::ReportError, this needs to be a null
        // terminated string.
        //
        // See Bug 1699569.
        if (mTokenOffset >= sourceLine.Length()) {
          // Corrupt data, leave linebuf unset.
        } else if (JS::UniqueTwoByteChars buffer =
                       ToNullTerminatedJSStringBuffer(aCx, sourceLine)) {
          err->initOwnedLinebuf(buffer.release(), sourceLine.Length(),
                                mTokenOffset);
        } else {
          // Just ignore OOM and continue if the string copy failed.
          JS_ClearPendingException(aCx);
        }
      }
    }

    return true;
  }

  nsCOMPtr<nsIStackFrame> frame(exceptions::CreateStack(aCx, stack));

  RefPtr<Exception> exn;
  if (mType == Type::Exception) {
    exn = new Exception(mMessage, mResult, mName, frame, nullptr);
  } else {
    MOZ_ASSERT(mType == Type::DOMException);
    exn = new DOMException(mResult, mMessage, mName, mCode, frame);
  }

  return ToJSValue(aCx, exn, aResult);
}

bool ClonedErrorHolder::Holder::ReadStructuredCloneInternal(
    JSContext* aCx, JSStructuredCloneReader* aReader) {
  uint32_t length;
  uint32_t version;
  if (!JS_ReadUint32Pair(aReader, &length, &version)) {
    return false;
  }
  if (length % 8 != 0) {
    return false;
  }

  JSStructuredCloneData data(mStructuredCloneScope);
  while (length) {
    size_t size;
    char* buffer = data.AllocateBytes(length, &size);
    if (!buffer || !JS_ReadBytes(aReader, buffer, size)) {
      return false;
    }
    length -= size;
  }

  mBuffer = MakeUnique<JSAutoStructuredCloneBuffer>(
      mStructuredCloneScope, &StructuredCloneHolder::sCallbacks, this);
  mBuffer->adopt(std::move(data), version, &StructuredCloneHolder::sCallbacks);
  return true;
}
