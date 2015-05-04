/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMClassInfo_h___
#define nsDOMClassInfo_h___

#include "mozilla/Attributes.h"
#include "nsIXPCScriptable.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMScriptObjectFactory.h"
#include "js/Id.h"
#include "nsIXPConnect.h"

#ifdef XP_WIN
#undef GetClassName
#endif

struct nsGlobalNameStruct;
class nsGlobalWindow;

struct nsDOMClassInfoData;

typedef nsIClassInfo* (*nsDOMClassInfoConstructorFnc)
  (nsDOMClassInfoData* aData);

typedef nsresult (*nsDOMConstructorFunc)(nsISupports** aNewObject);

struct nsDOMClassInfoData
{
  const char *mName;
  const char16_t *mNameUTF16;
  union {
    nsDOMClassInfoConstructorFnc mConstructorFptr;
    nsDOMClassInfoExternalConstructorFnc mExternalConstructorFptr;
  } u;

  nsIClassInfo *mCachedClassInfo; // low bit is set to 1 if external,
                                  // so be sure to mask if necessary!
  const nsIID *mProtoChainInterface;
  const nsIID **mInterfaces;
  uint32_t mScriptableFlags : 31; // flags must not use more than 31 bits!
  uint32_t mHasClassInterface : 1;
  bool mChromeOnly : 1;
  bool mAllowXBL : 1;
  bool mDisabled : 1;
#ifdef DEBUG
  uint32_t mDebugID;
#endif
};

struct nsExternalDOMClassInfoData : public nsDOMClassInfoData
{
  const nsCID *mConstructorCID;
};


// To be used with the nsDOMClassInfoData::mCachedClassInfo pointer.
// The low bit is set when we created a generic helper for an external
// (which holds on to the nsDOMClassInfoData).
#define GET_CLEAN_CI_PTR(_ptr) (nsIClassInfo*)(uintptr_t(_ptr) & ~0x1)
#define MARK_EXTERNAL(_ptr) (nsIClassInfo*)(uintptr_t(_ptr) | 0x1)
#define IS_EXTERNAL(_ptr) (uintptr_t(_ptr) & 0x1)

class nsWindowSH;

class nsDOMClassInfo : public nsXPCClassInfo
{
  friend class nsWindowSH;

protected:
  virtual ~nsDOMClassInfo();

public:
  explicit nsDOMClassInfo(nsDOMClassInfoData* aData);

  NS_DECL_NSIXPCSCRIPTABLE

  NS_DECL_ISUPPORTS

  NS_DECL_NSICLASSINFO

  // Helper method that returns a *non* refcounted pointer to a
  // helper. So please note, don't release this pointer, if you do,
  // you better make sure you've addreffed before release.
  //
  // Whaaaaa! I wanted to name this method GetClassInfo, but nooo,
  // some of Microsoft devstudio's headers #defines GetClassInfo to
  // GetClassInfoA so I can't, those $%#@^! bastards!!! What gives
  // them the right to do that?

  static nsIClassInfo* GetClassInfoInstance(nsDOMClassInfoData* aData);

  static nsresult Init();
  static void ShutDown();

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsDOMClassInfo(aData);
  }

  /*
   * The following two functions exist because of the way that Xray wrappers
   * work. In order to allow scriptable helpers to define non-IDL defined but
   * still "safe" properties for Xray wrappers, we call into the scriptable
   * helper with |obj| being the wrapper.
   *
   * Ideally, that would be the end of the story, however due to complications
   * dealing with document.domain, it's possible to end up in a scriptable
   * helper with a wrapper, even though we should be treating the lookup as a
   * transparent one.
   *
   * Note: So ObjectIsNativeWrapper(cx, obj) check usually means "through xray
   * wrapper this part is not visible".
   */
  static bool ObjectIsNativeWrapper(JSContext* cx, JSObject* obj);

  static nsISupports *GetNative(nsIXPConnectWrappedNative *wrapper, JSObject *obj);

  static nsIXPConnect *XPConnect()
  {
    return sXPConnect;
  }

protected:
  friend nsIClassInfo* NS_GetDOMClassInfoInstance(nsDOMClassInfoID aID);

  const nsDOMClassInfoData* mData;

  virtual void PreserveWrapper(nsISupports *aNative) override
  {
  }

  static nsresult RegisterClassProtos(int32_t aDOMClassInfoID);
  static nsresult RegisterExternalClasses();

  static nsIXPConnect *sXPConnect;

  // nsIXPCScriptable code
  static nsresult DefineStaticJSVals(JSContext *cx);

  static bool sIsInitialized;

public:
  static jsid sConstructor_id;
  static jsid sWrappedJSObject_id;
};

// THIS ONE ISN'T SAFE!! It assumes that the private of the JSObject is
// an nsISupports.
inline
const nsQueryInterface
do_QueryWrappedNative(nsIXPConnectWrappedNative *wrapper, JSObject *obj)
{
  return nsQueryInterface(nsDOMClassInfo::GetNative(wrapper, obj));
}

// THIS ONE ISN'T SAFE!! It assumes that the private of the JSObject is
// an nsISupports.
inline
const nsQueryInterfaceWithError
do_QueryWrappedNative(nsIXPConnectWrappedNative *wrapper, JSObject *obj,
                      nsresult *aError)

{
  return nsQueryInterfaceWithError(nsDOMClassInfo::GetNative(wrapper, obj),
                                   aError);
}

inline
nsQueryInterface
do_QueryWrapper(JSContext *cx, JSObject *obj)
{
  nsISupports *native =
    nsDOMClassInfo::XPConnect()->GetNativeOfWrapper(cx, obj);
  return nsQueryInterface(native);
}

inline
nsQueryInterfaceWithError
do_QueryWrapper(JSContext *cx, JSObject *obj, nsresult* error)
{
  nsISupports *native =
    nsDOMClassInfo::XPConnect()->GetNativeOfWrapper(cx, obj);
  return nsQueryInterfaceWithError(native, error);
}


typedef nsDOMClassInfo nsDOMGenericSH;

// Makes sure that the wrapper is preserved if new properties are added.
class nsEventTargetSH : public nsDOMGenericSH
{
protected:
  explicit nsEventTargetSH(nsDOMClassInfoData* aData) : nsDOMGenericSH(aData)
  {
  }

  virtual ~nsEventTargetSH()
  {
  }
public:
  NS_IMETHOD PreCreate(nsISupports *nativeObj, JSContext *cx,
                       JSObject *globalObj, JSObject **parentObj) override;
  NS_IMETHOD AddProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, JS::Handle<JS::Value> val,
                         bool *_retval) override;

  virtual void PreserveWrapper(nsISupports *aNative) override;

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsEventTargetSH(aData);
  }
};

// A place to hang some static methods that we should really consider
// moving to be nsGlobalWindow member methods.  See bug 1062418.
class nsWindowSH
{
protected:
  static nsresult GlobalResolve(nsGlobalWindow *aWin, JSContext *cx,
                                JS::Handle<JSObject*> obj, JS::Handle<jsid> id,
                                JS::MutableHandle<JSPropertyDescriptor> desc);

  friend class nsGlobalWindow;
public:
  static bool NameStructEnabled(JSContext* aCx, nsGlobalWindow *aWin,
                                const nsAString& aName,
                                const nsGlobalNameStruct& aNameStruct);
};


// Event handler 'this' translator class, this is called by XPConnect
// when a "function interface" (nsIDOMEventListener) is called, this
// class extracts 'this' fomr the first argument to the called
// function (nsIDOMEventListener::HandleEvent(in nsIDOMEvent)), this
// class will pass back nsIDOMEvent::currentTarget to be used as
// 'this'.

class nsEventListenerThisTranslator : public nsIXPCFunctionThisTranslator
{
  virtual ~nsEventListenerThisTranslator()
  {
  }

public:
  nsEventListenerThisTranslator()
  {
  }

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIXPCFunctionThisTranslator
  NS_DECL_NSIXPCFUNCTIONTHISTRANSLATOR
};

class nsDOMConstructorSH : public nsDOMGenericSH
{
protected:
  explicit nsDOMConstructorSH(nsDOMClassInfoData* aData) : nsDOMGenericSH(aData)
  {
  }

public:
  NS_IMETHOD PreCreate(nsISupports *nativeObj, JSContext *cx,
                       JSObject *globalObj, JSObject **parentObj) override;
  NS_IMETHOD PostCreatePrototype(JSContext * cx, JSObject * proto) override
  {
    return NS_OK;
  }
  NS_IMETHOD Resolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                     JSObject *obj, jsid id, bool *resolvedp,
                     bool *_retval) override;
  NS_IMETHOD Call(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                  JSObject *obj, const JS::CallArgs &args, bool *_retval) override;

  NS_IMETHOD Construct(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                       JSObject *obj, const JS::CallArgs &args, bool *_retval) override;

  NS_IMETHOD HasInstance(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, JS::Handle<JS::Value> val, bool *bp,
                         bool *_retval) override;

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsDOMConstructorSH(aData);
  }
};

class nsNonDOMObjectSH : public nsDOMGenericSH
{
protected:
  explicit nsNonDOMObjectSH(nsDOMClassInfoData* aData) : nsDOMGenericSH(aData)
  {
  }

  virtual ~nsNonDOMObjectSH()
  {
  }

public:
  NS_IMETHOD GetFlags(uint32_t *aFlags) override;

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsNonDOMObjectSH(aData);
  }
};

template<typename Super>
class nsMessageManagerSH : public Super
{
protected:
  explicit nsMessageManagerSH(nsDOMClassInfoData* aData)
    : Super(aData)
  {
  }

  virtual ~nsMessageManagerSH()
  {
  }
public:
  NS_IMETHOD Resolve(nsIXPConnectWrappedNative* wrapper, JSContext* cx,
                     JSObject* obj_, jsid id_, bool* resolvedp,
                     bool* _retval) override;
  NS_IMETHOD Enumerate(nsIXPConnectWrappedNative* wrapper, JSContext* cx,
                       JSObject* obj_, bool* _retval) override;

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsMessageManagerSH(aData);
  }
};

#endif /* nsDOMClassInfo_h___ */
