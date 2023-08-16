/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ClonedErrorHolder_h
#define mozilla_dom_ClonedErrorHolder_h

#include "nsISupportsImpl.h"
#include "js/ColumnNumber.h"  // JS::ColumnNumberOneOrigin
#include "js/ErrorReport.h"
#include "js/TypeDecls.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/StructuredCloneHolder.h"
#include "mozilla/Attributes.h"

class nsIGlobalObject;
class nsQueryActorChild;

namespace mozilla {
class ErrorResult;

namespace dom {

class ClonedErrorHolder final {
  NS_INLINE_DECL_REFCOUNTING(ClonedErrorHolder)

 public:
  static already_AddRefed<ClonedErrorHolder> Constructor(
      const GlobalObject& aGlobal, JS::Handle<JSObject*> aError,
      ErrorResult& aRv);

  static already_AddRefed<ClonedErrorHolder> Create(
      JSContext* aCx, JS::Handle<JSObject*> aError, ErrorResult& aRv);

  enum class Type : uint8_t {
    Uninitialized,
    JSError,
    Exception,
    DOMException,
    Max_,
  };

  bool WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto,
                  JS::MutableHandle<JSObject*> aReflector);

  bool WriteStructuredClone(JSContext* aCx, JSStructuredCloneWriter* aWriter,
                            StructuredCloneHolder* aHolder);

  // Reads the structured clone data for the ClonedErrorHolder and returns the
  // wrapped object (either a JS Error or an Exception/DOMException object)
  // directly. Never returns an actual ClonedErrorHolder object.
  static JSObject* ReadStructuredClone(JSContext* aCx,
                                       JSStructuredCloneReader* aReader,
                                       StructuredCloneHolder* aHolder);

 private:
  ClonedErrorHolder();
  ~ClonedErrorHolder() = default;

  void Init(JSContext* aCx, JS::Handle<JSObject*> aError, ErrorResult& aRv);

  bool Init(JSContext* aCx, JSStructuredCloneReader* aReader);

  // Creates a new JS Error or Exception/DOMException object based on the
  // values stored in the holder. Returns false and sets an exception on aCx
  // if it fails.
  bool ToErrorValue(JSContext* aCx, JS::MutableHandle<JS::Value> aResult);

  class Holder final : public StructuredCloneHolder {
   public:
    using StructuredCloneHolder::StructuredCloneHolder;

    bool ReadStructuredCloneInternal(JSContext* aCx,
                                     JSStructuredCloneReader* aReader);
  };

  // Only a subset of the following fields are used, depending on the mType of
  // the error stored:
  nsCString mName;        // Exception, DOMException
  nsCString mMessage;     // JSError, Exception, DOMException
  nsCString mFilename;    // JSError only
  nsCString mSourceLine;  // JSError only

  uint32_t mLineNumber = 0;           // JSError only
  JS::ColumnNumberOneOrigin mColumn;  // JSError only
  uint32_t mTokenOffset = 0;          // JSError only
  uint32_t mErrorNumber = 0;          // JSError only

  Type mType = Type::Uninitialized;

  uint16_t mCode = 0;                 // DOMException only
  JSExnType mExnType = JSExnType(0);  // JSError only
  nsresult mResult = NS_OK;           // Exception, DOMException

  // JSError, Exception, DOMException
  Holder mStack{Holder::CloningSupported, Holder::TransferringNotSupported,
                Holder::StructuredCloneScope::DifferentProcess};
};

}  // namespace dom
}  // namespace mozilla

#endif  // !defined(mozilla_dom_ClonedErrorHolder_h)
