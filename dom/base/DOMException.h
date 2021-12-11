/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DOMException_h__
#define mozilla_dom_DOMException_h__

// We intentionally shadow non-virtual methods, but gcc gets confused.
#ifdef __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Woverloaded-virtual"
#endif

#include <stdint.h>
#include "js/Value.h"
#include "jspubtd.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsID.h"
#include "nsWrapperCache.h"
#include "nsIException.h"
#include "nsString.h"
#include "mozilla/dom/BindingDeclarations.h"

class nsIStackFrame;

nsresult NS_GetNameAndMessageForDOMNSResult(nsresult aNSResult,
                                            nsACString& aName,
                                            nsACString& aMessage,
                                            uint16_t* aCode = nullptr);

namespace mozilla {
class ErrorResult;

namespace dom {

class GlobalObject;

#define MOZILLA_EXCEPTION_IID                        \
  {                                                  \
    0x55eda557, 0xeba0, 0x4fe3, {                    \
      0xae, 0x2e, 0xf3, 0x94, 0x49, 0x23, 0x62, 0xd6 \
    }                                                \
  }

class Exception : public nsIException, public nsWrapperCache {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(MOZILLA_EXCEPTION_IID)

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Exception)

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIEXCEPTION

  const nsCString& GetMessageMoz() const { return mMessage; }
  nsresult GetResult() const { return mResult; }
  // DOMException wants different ToString behavior, so allow it to override.
  virtual void ToString(JSContext* aCx, nsACString& aReturn);

  // Cruft used by XPConnect for exceptions originating in JS implemented
  // components.
  bool StealJSVal(JS::Value* aVp);
  void StowJSVal(JS::Value& aVp);

  // WebIDL API
  virtual JSObject* WrapObject(JSContext* cx,
                               JS::Handle<JSObject*> aGivenProto) override;

  nsISupports* GetParentObject() const { return nullptr; }

  void GetMessageMoz(nsString& retval);

  uint32_t Result() const;

  void GetName(nsAString& retval);

  virtual void GetErrorMessage(nsAString& aRetVal) {
    // Since GetName is non non-virtual and deals with different
    // member variables in Exception vs. DOMException, have a virtual
    // method to ensure the right error message creation.
    nsAutoString name;
    GetName(name);
    CreateErrorMessage(name, aRetVal);
  }

  void GetFilename(JSContext* aCx, nsAString& aFilename);

  uint32_t SourceId(JSContext* aCx) const;

  uint32_t LineNumber(JSContext* aCx) const;

  uint32_t ColumnNumber() const;

  already_AddRefed<nsIStackFrame> GetLocation() const;

  nsISupports* GetData() const;

  void GetStack(JSContext* aCx, nsAString& aStack) const;

  void Stringify(JSContext* aCx, nsString& retval);

  Exception(const nsACString& aMessage, nsresult aResult,
            const nsACString& aName, nsIStackFrame* aLocation,
            nsISupports* aData);

 protected:
  virtual ~Exception();

  void CreateErrorMessage(const nsAString& aName, nsAString& aRetVal) {
    // Create similar error message as what ErrorReport::init does in jsexn.cpp.
    if (!aName.IsEmpty() && !mMessage.IsEmpty()) {
      aRetVal.Assign(aName);
      aRetVal.AppendLiteral(": ");
      AppendUTF8toUTF16(mMessage, aRetVal);
    } else if (!aName.IsEmpty()) {
      aRetVal.Assign(aName);
    } else if (!mMessage.IsEmpty()) {
      CopyUTF8toUTF16(mMessage, aRetVal);
    } else {
      aRetVal.Truncate();
    }
  }

  nsCString mMessage;
  nsresult mResult;
  nsCString mName;
  nsCOMPtr<nsIStackFrame> mLocation;
  nsCOMPtr<nsISupports> mData;

  bool mHoldingJSVal;
  JS::Heap<JS::Value> mThrownJSVal;
};

NS_DEFINE_STATIC_IID_ACCESSOR(Exception, MOZILLA_EXCEPTION_IID)

class DOMException : public Exception {
 public:
  DOMException(nsresult aRv, const nsACString& aMessage,
               const nsACString& aName, uint16_t aCode,
               nsIStackFrame* aLocation = nullptr);

  NS_INLINE_DECL_REFCOUNTING_INHERITED(DOMException, Exception)

  // nsWrapperCache overrides
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<DOMException> Constructor(
      GlobalObject& /* unused */, const nsAString& aMessage,
      const Optional<nsAString>& aName);

  uint16_t Code() const { return mCode; }

  // Intentionally shadow the Exception version.
  void GetName(nsString& retval);

  // Exception overrides
  void ToString(JSContext* aCx, nsACString& aReturn) override;

  virtual void GetErrorMessage(nsAString& aRetVal) override {
    // See the comment in Exception::GetErrorMessage.
    nsAutoString name;
    GetName(name);
    CreateErrorMessage(name, aRetVal);
  }

  static already_AddRefed<DOMException> Create(nsresult aRv);

  static already_AddRefed<DOMException> Create(nsresult aRv,
                                               const nsACString& aMessage);

 protected:
  virtual ~DOMException() = default;

  uint16_t mCode;
};

}  // namespace dom
}  // namespace mozilla

#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif

#endif
