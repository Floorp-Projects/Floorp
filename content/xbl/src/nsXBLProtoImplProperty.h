/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXBLProtoImplProperty_h__
#define nsXBLProtoImplProperty_h__

#include "nsIAtom.h"
#include "nsString.h"
#include "jsapi.h"
#include "nsIContent.h"
#include "nsString.h"
#include "nsXBLSerialize.h"
#include "nsXBLProtoImplMember.h"

class nsXBLProtoImplProperty: public nsXBLProtoImplMember
{
public:
  nsXBLProtoImplProperty(const PRUnichar* aName,
                         const PRUnichar* aGetter, 
                         const PRUnichar* aSetter,
                         const PRUnichar* aReadOnly);

  nsXBLProtoImplProperty(const PRUnichar* aName, const bool aIsReadOnly);
 
  virtual ~nsXBLProtoImplProperty();

  void AppendGetterText(const nsAString& aGetter);
  void AppendSetterText(const nsAString& aSetter);

  void SetGetterLineNumber(PRUint32 aLineNumber);
  void SetSetterLineNumber(PRUint32 aLineNumber);

  virtual nsresult InstallMember(nsIScriptContext* aContext,
                                 nsIContent* aBoundElement, 
                                 JSObject* aScriptObject,
                                 JSObject* aTargetClassObject,
                                 const nsCString& aClassStr);
  virtual nsresult CompileMember(nsIScriptContext* aContext,
                                 const nsCString& aClassStr,
                                 JSObject* aClassObject);

  virtual void Trace(TraceCallback aCallback, void *aClosure) const;

  nsresult Read(nsIScriptContext* aContext,
                nsIObjectInputStream* aStream,
                XBLBindingSerializeDetails aType);
  virtual nsresult Write(nsIScriptContext* aContext,
                         nsIObjectOutputStream* aStream);

protected:
  union {
    // The raw text for the getter (prior to compilation).
    nsXBLTextWithLineNumber* mGetterText;
    // The JS object for the getter (after compilation)
    JSObject *               mJSGetterObject;
  };

  union {
    // The raw text for the setter (prior to compilation).
    nsXBLTextWithLineNumber* mSetterText;
    // The JS object for the setter (after compilation)
    JSObject *               mJSSetterObject;
  };
  
  unsigned mJSAttributes;          // A flag for all our JS properties (getter/setter/readonly/shared/enum)

#ifdef DEBUG
  bool mIsCompiled;
#endif
};

#endif // nsXBLProtoImplProperty_h__
