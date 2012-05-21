/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXBLProtoImplField_h__
#define nsXBLProtoImplField_h__

#include "nsIAtom.h"
#include "nsString.h"
#include "jsapi.h"
#include "nsIContent.h"
#include "nsString.h"
#include "nsXBLProtoImplMember.h"

class nsIURI;

class nsXBLProtoImplField
{
public:
  nsXBLProtoImplField(const PRUnichar* aName, const PRUnichar* aReadOnly);
  nsXBLProtoImplField(const bool aIsReadOnly);
  ~nsXBLProtoImplField();

  void AppendFieldText(const nsAString& aText);
  void SetLineNumber(PRUint32 aLineNumber) {
    mLineNumber = aLineNumber;
  }
  
  nsXBLProtoImplField* GetNext() const { return mNext; }
  void SetNext(nsXBLProtoImplField* aNext) { mNext = aNext; }

  nsresult InstallField(nsIScriptContext* aContext,
                        JSObject* aBoundNode,
                        nsIPrincipal* aPrincipal,
                        nsIURI* aBindingDocURI,
                        bool* aDidInstall) const;

  nsresult Read(nsIScriptContext* aContext, nsIObjectInputStream* aStream);
  nsresult Write(nsIScriptContext* aContext, nsIObjectOutputStream* aStream);

  const PRUnichar* GetName() const { return mName; }

protected:
  nsXBLProtoImplField* mNext;
  PRUnichar* mName;
  PRUnichar* mFieldText;
  PRUint32 mFieldTextLength;
  PRUint32 mLineNumber;
  unsigned mJSAttributes;
};

#endif // nsXBLProtoImplField_h__
