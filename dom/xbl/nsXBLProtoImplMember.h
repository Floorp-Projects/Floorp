/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXBLProtoImplMember_h__
#define nsXBLProtoImplMember_h__

#include "nsIAtom.h"
#include "nsString.h"
#include "nsString.h"
#include "nsIServiceManager.h"
#include "nsContentUtils.h" // For NS_CONTENT_DELETE_LIST_MEMBER.
#include "nsCycleCollectionParticipant.h"

class nsIContent;
class nsIObjectOutputStream;

struct nsXBLTextWithLineNumber
{
  char16_t* mText;
  uint32_t mLineNumber;

  nsXBLTextWithLineNumber() :
    mText(nullptr),
    mLineNumber(0)
  {
    MOZ_COUNT_CTOR(nsXBLTextWithLineNumber);
  }

  ~nsXBLTextWithLineNumber() {
    MOZ_COUNT_DTOR(nsXBLTextWithLineNumber);
    if (mText) {
      nsMemory::Free(mText);
    }
  }

  void AppendText(const nsAString& aText) {
    if (mText) {
      char16_t* temp = mText;
      mText = ToNewUnicode(nsDependentString(temp) + aText);
      nsMemory::Free(temp);
    } else {
      mText = ToNewUnicode(aText);
    }
  }

  char16_t* GetText() {
    return mText;
  }

  void SetLineNumber(uint32_t aLineNumber) {
    mLineNumber = aLineNumber;
  }

  uint32_t GetLineNumber() {
    return mLineNumber;
  }
};

class nsXBLProtoImplMember
{
public:
  explicit nsXBLProtoImplMember(const char16_t* aName)
    : mNext(nullptr)
    , mExposeToUntrustedContent(false)
  {
    mName = ToNewUnicode(nsDependentString(aName));
  }
  virtual ~nsXBLProtoImplMember() {
    nsMemory::Free(mName);
    NS_CONTENT_DELETE_LIST_MEMBER(nsXBLProtoImplMember, this, mNext);
  }

  nsXBLProtoImplMember* GetNext() { return mNext; }
  void SetNext(nsXBLProtoImplMember* aNext) { mNext = aNext; }
  bool ShouldExposeToUntrustedContent() { return mExposeToUntrustedContent; }
  void SetExposeToUntrustedContent(bool aExpose) { mExposeToUntrustedContent = aExpose; }
  const char16_t* GetName() { return mName; }

  virtual nsresult InstallMember(JSContext* aCx,
                                 JS::Handle<JSObject*> aTargetClassObject) = 0;
  virtual nsresult CompileMember(const nsCString& aClassStr,
                                 JS::Handle<JSObject*> aClassObject) = 0;

  virtual void Trace(const TraceCallbacks& aCallbacks, void *aClosure) = 0;

  virtual nsresult Write(nsIObjectOutputStream* aStream)
  {
    return NS_OK;
  }

protected:
  nsXBLProtoImplMember* mNext;  // The members of an implementation are chained.
  char16_t* mName;               // The name of the field, method, or property.

  bool mExposeToUntrustedContent; // If this binding is installed on an element
                                  // in an untrusted scope, should this
                                  // implementation member be accessible to the
                                  // content?
};

#endif // nsXBLProtoImplMember_h__
