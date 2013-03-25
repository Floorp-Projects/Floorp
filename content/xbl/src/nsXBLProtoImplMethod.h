/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXBLProtoImplMethod_h__
#define nsXBLProtoImplMethod_h__

#include "nsIAtom.h"
#include "nsString.h"
#include "jsapi.h"
#include "nsString.h"
#include "nsXBLProtoImplMember.h"
#include "nsXBLSerialize.h"

class nsIContent;

struct nsXBLParameter {
  nsXBLParameter* mNext;
  char* mName;

  nsXBLParameter(const nsAString& aName) {
    MOZ_COUNT_CTOR(nsXBLParameter);
    mName = ToNewCString(aName);
    mNext = nullptr;
  }

  ~nsXBLParameter() {
    MOZ_COUNT_DTOR(nsXBLParameter);
    nsMemory::Free(mName);
    NS_CONTENT_DELETE_LIST_MEMBER(nsXBLParameter, this, mNext);
  }
};

struct nsXBLUncompiledMethod {
  nsXBLParameter* mParameters;
  nsXBLParameter* mLastParameter;
  nsXBLTextWithLineNumber mBodyText;

  nsXBLUncompiledMethod() :
    mParameters(nullptr),
    mLastParameter(nullptr),
    mBodyText()
  {
    MOZ_COUNT_CTOR(nsXBLUncompiledMethod);
  }

  ~nsXBLUncompiledMethod() {
    MOZ_COUNT_DTOR(nsXBLUncompiledMethod);
    delete mParameters;
  }

  int32_t GetParameterCount() {
    int32_t result = 0;
    for (nsXBLParameter* curr = mParameters; curr; curr=curr->mNext)
      result++;
    return result;
  }

  void AppendBodyText(const nsAString& aText) {
    mBodyText.AppendText(aText);
  }

  void AddParameter(const nsAString& aText) {
    nsXBLParameter* param = new nsXBLParameter(aText);
    if (!param)
      return;
    if (!mParameters)
      mParameters = param;
    else
      mLastParameter->mNext = param;
    mLastParameter = param;
  }

  void SetLineNumber(uint32_t aLineNumber) {
    mBodyText.SetLineNumber(aLineNumber);
  }
};

class nsXBLProtoImplMethod: public nsXBLProtoImplMember
{
public:
  nsXBLProtoImplMethod(const PRUnichar* aName);
  virtual ~nsXBLProtoImplMethod();

  void AppendBodyText(const nsAString& aBody);
  void AddParameter(const nsAString& aName);

  void SetLineNumber(uint32_t aLineNumber);
  
  virtual nsresult InstallMember(JSContext* aCx,
                                 JSObject* aTargetClassObject);
  virtual nsresult CompileMember(nsIScriptContext* aContext,
                                 const nsCString& aClassStr,
                                 JSObject* aClassObject);

  virtual void Trace(TraceCallback aCallback, void *aClosure) const;

  nsresult Read(nsIScriptContext* aContext, nsIObjectInputStream* aStream);
  virtual nsresult Write(nsIScriptContext* aContext, nsIObjectOutputStream* aStream);

  bool IsCompiled() const
  {
    return !(mUncompiledMethod & BIT_UNCOMPILED);
  }
  void SetUncompiledMethod(nsXBLUncompiledMethod* aUncompiledMethod)
  {
    mUncompiledMethod = uintptr_t(aUncompiledMethod) | BIT_UNCOMPILED;
  }
  nsXBLUncompiledMethod* GetUncompiledMethod() const
  {
    uintptr_t unmasked = mUncompiledMethod & ~BIT_UNCOMPILED;
    return reinterpret_cast<nsXBLUncompiledMethod*>(unmasked);
  }

protected:
  enum { BIT_UNCOMPILED = 1 << 0 };

  union {
    uintptr_t mUncompiledMethod; // An object that represents the method before being compiled.
    JSObject* mJSMethodObject;    // The JS object for the method (after compilation)
  };

#ifdef DEBUG
  bool mIsCompiled;
#endif
};

class nsXBLProtoImplAnonymousMethod : public nsXBLProtoImplMethod {
public:
  nsXBLProtoImplAnonymousMethod() :
    nsXBLProtoImplMethod(EmptyString().get())
  {}
  
  nsresult Execute(nsIContent* aBoundElement);

  // Override InstallMember; these methods never get installed as members on
  // binding instantiations (though they may hang out in mMembers on the
  // prototype implementation).
  virtual nsresult InstallMember(JSContext* aCx,
                                 JSObject* aTargetClassObject) {
    return NS_OK;
  }

  using nsXBLProtoImplMethod::Write;
  nsresult Write(nsIScriptContext* aContext,
                 nsIObjectOutputStream* aStream,
                 XBLBindingSerializeDetails aType);
};

#endif // nsXBLProtoImplMethod_h__
