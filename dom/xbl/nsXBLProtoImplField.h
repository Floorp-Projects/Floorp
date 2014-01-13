/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXBLProtoImplField_h__
#define nsXBLProtoImplField_h__

#include "nsIAtom.h"
#include "nsString.h"
#include "jsapi.h"
#include "nsString.h"
#include "nsXBLProtoImplMember.h"

class nsIObjectInputStream;
class nsIObjectOutputStream;
class nsIScriptContext;
class nsIURI;

class nsXBLProtoImplField
{
public:
  nsXBLProtoImplField(const char16_t* aName, const char16_t* aReadOnly);
  nsXBLProtoImplField(const bool aIsReadOnly);
  ~nsXBLProtoImplField();

  void AppendFieldText(const nsAString& aText);
  void SetLineNumber(uint32_t aLineNumber) {
    mLineNumber = aLineNumber;
  }
  
  nsXBLProtoImplField* GetNext() const { return mNext; }
  void SetNext(nsXBLProtoImplField* aNext) { mNext = aNext; }

  nsresult InstallField(nsIScriptContext* aContext,
                        JS::Handle<JSObject*> aBoundNode,
                        nsIURI* aBindingDocURI,
                        bool* aDidInstall) const;

  nsresult InstallAccessors(JSContext* aCx,
                            JS::Handle<JSObject*> aTargetClassObject);

  nsresult Read(nsIObjectInputStream* aStream);
  nsresult Write(nsIObjectOutputStream* aStream);

  const char16_t* GetName() const { return mName; }

  unsigned AccessorAttributes() const {
    return JSPROP_SHARED | JSPROP_GETTER | JSPROP_SETTER |
           (mJSAttributes & (JSPROP_ENUMERATE | JSPROP_PERMANENT));
  }

  bool IsEmpty() const { return mFieldTextLength == 0; }

protected:
  nsXBLProtoImplField* mNext;
  char16_t* mName;
  char16_t* mFieldText;
  uint32_t mFieldTextLength;
  uint32_t mLineNumber;
  unsigned mJSAttributes;
};

#endif // nsXBLProtoImplField_h__
