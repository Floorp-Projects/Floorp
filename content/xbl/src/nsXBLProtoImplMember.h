/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXBLProtoImplMember_h__
#define nsXBLProtoImplMember_h__

#include "nsIAtom.h"
#include "nsString.h"
#include "jsapi.h"
#include "nsString.h"
#include "nsIServiceManager.h"
#include "nsContentUtils.h"
#include "nsCycleCollectionParticipant.h"

class nsIContent;
class nsIScriptContext;

struct nsXBLTextWithLineNumber
{
  PRUnichar* mText;
  PRUint32 mLineNumber;

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
      PRUnichar* temp = mText;
      mText = ToNewUnicode(nsDependentString(temp) + aText);
      nsMemory::Free(temp);
    } else {
      mText = ToNewUnicode(aText);
    }
  }

  PRUnichar* GetText() {
    return mText;
  }

  void SetLineNumber(PRUint32 aLineNumber) {
    mLineNumber = aLineNumber;
  }

  PRUint32 GetLineNumber() {
    return mLineNumber;
  }
};

class nsXBLProtoImplMember
{
public:
  nsXBLProtoImplMember(const PRUnichar* aName) :mNext(nullptr) { mName = ToNewUnicode(nsDependentString(aName)); }
  virtual ~nsXBLProtoImplMember() {
    nsMemory::Free(mName);
    NS_CONTENT_DELETE_LIST_MEMBER(nsXBLProtoImplMember, this, mNext);
  }

  nsXBLProtoImplMember* GetNext() { return mNext; }
  void SetNext(nsXBLProtoImplMember* aNext) { mNext = aNext; }

  virtual nsresult InstallMember(nsIScriptContext* aContext,
                                 nsIContent* aBoundElement, 
                                 JSObject* aScriptObject, // Unused
                                 JSObject* aTargetClassObject,
                                 const nsCString& aClassStr) = 0;
  virtual nsresult CompileMember(nsIScriptContext* aContext,
                                 const nsCString& aClassStr,
                                 JSObject* aClassObject) = 0;

  virtual void Trace(TraceCallback aCallback, void *aClosure) const = 0;

  virtual nsresult Write(nsIScriptContext* aContext,
                         nsIObjectOutputStream* aStream)
  {
    return NS_OK;
  }

protected:
  nsXBLProtoImplMember* mNext;  // The members of an implementation are chained.
  PRUnichar* mName;               // The name of the field, method, or property.
};

#endif // nsXBLProtoImplMember_h__
