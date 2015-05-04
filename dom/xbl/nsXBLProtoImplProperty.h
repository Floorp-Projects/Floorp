/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXBLProtoImplProperty_h__
#define nsXBLProtoImplProperty_h__

#include "mozilla/Attributes.h"
#include "nsIAtom.h"
#include "nsString.h"
#include "nsString.h"
#include "nsXBLSerialize.h"
#include "nsXBLMaybeCompiled.h"
#include "nsXBLProtoImplMember.h"

class nsXBLProtoImplProperty: public nsXBLProtoImplMember
{
public:
  nsXBLProtoImplProperty(const char16_t* aName,
                         const char16_t* aGetter, 
                         const char16_t* aSetter,
                         const char16_t* aReadOnly,
                         uint32_t aLineNumber);

  nsXBLProtoImplProperty(const char16_t* aName, const bool aIsReadOnly);
 
  virtual ~nsXBLProtoImplProperty();

  void AppendGetterText(const nsAString& aGetter);
  void AppendSetterText(const nsAString& aSetter);

  void SetGetterLineNumber(uint32_t aLineNumber);
  void SetSetterLineNumber(uint32_t aLineNumber);

  virtual nsresult InstallMember(JSContext* aCx,
                                 JS::Handle<JSObject*> aTargetClassObject) override;
  virtual nsresult CompileMember(mozilla::dom::AutoJSAPI& jsapi, const nsString& aClassStr,
                                 JS::Handle<JSObject*> aClassObject) override;

  virtual void Trace(const TraceCallbacks& aCallback, void *aClosure) override;

  nsresult Read(nsIObjectInputStream* aStream,
                XBLBindingSerializeDetails aType);
  virtual nsresult Write(nsIObjectOutputStream* aStream) override;

protected:
  typedef JS::Heap<nsXBLMaybeCompiled<nsXBLTextWithLineNumber> > PropertyOp;

  void EnsureUncompiledText(PropertyOp& aPropertyOp);

  // The raw text for the getter, or the JS object (after compilation).
  PropertyOp mGetter;

  // The raw text for the setter, or the JS object (after compilation).
  PropertyOp mSetter;
  
  unsigned mJSAttributes;  // A flag for all our JS properties (getter/setter/readonly/shared/enum)

#ifdef DEBUG
  bool mIsCompiled;
#endif
};

#endif // nsXBLProtoImplProperty_h__
