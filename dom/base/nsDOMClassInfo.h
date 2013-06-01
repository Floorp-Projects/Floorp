/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMClassInfo_h___
#define nsDOMClassInfo_h___

#include "nsIDOMClassInfo.h"
#include "nsIXPCScriptable.h"
#include "jsapi.h"
#include "nsIScriptSecurityManager.h"
#include "nsIScriptContext.h"
#include "nsDOMJSUtils.h" // for GetScriptContextFromJSContext
#include "nsIScriptGlobalObject.h"
#include "xpcpublic.h"
#include "nsIRunnable.h"

#ifdef XP_WIN
#undef GetClassName
#endif

class nsContentList;
class nsGlobalWindow;
class nsIDOMWindow;
class nsIForm;
class nsNPAPIPluginInstance;
class nsObjectLoadingContent;
class nsIObjectLoadingContent;

class nsIDOMCrypto;
#ifndef MOZ_DISABLE_CRYPTOLEGACY
class nsIDOMCRMFObject;
#endif

struct nsDOMClassInfoData;

typedef nsIClassInfo* (*nsDOMClassInfoConstructorFnc)
  (nsDOMClassInfoData* aData);

typedef nsresult (*nsDOMConstructorFunc)(nsISupports** aNewObject);

struct nsDOMClassInfoData
{
  const char *mName;
  const PRUnichar *mNameUTF16;
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
  uint32_t mInterfacesBitmap;
  bool mChromeOnly;
  bool mDisabled;
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


class nsDOMClassInfo : public nsXPCClassInfo
{
  friend class nsHTMLDocumentSH;
public:
  nsDOMClassInfo(nsDOMClassInfoData* aData);
  virtual ~nsDOMClassInfo();

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
   * wrapper this part is not visible" while combined with
   * || xpc::WrapperFactory::XrayWrapperNotShadowing(obj) it means "through
   * xray wrapper it is visible only if it does not hide any native property."
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

  virtual void PreserveWrapper(nsISupports *aNative)
  {
  }

  virtual uint32_t GetInterfacesBitmap()
  {
    return mData->mInterfacesBitmap;
  }

  static nsresult Init();
  static nsresult RegisterClassProtos(int32_t aDOMClassInfoID);
  static nsresult RegisterExternalClasses();
  nsresult ResolveConstructor(JSContext *cx, JSObject *obj,
                              JSObject **objp);

  // Checks if id is a number and returns the number, if aIsNumber is
  // non-null it's set to true if the id is a number and false if it's
  // not a number. If id is not a number this method returns -1
  static int32_t GetArrayIndexFromId(JSContext *cx, JS::Handle<jsid> id,
                                     bool *aIsNumber = nullptr);

  static inline bool IsReadonlyReplaceable(jsid id)
  {
    return (id == sParent_id       ||
            id == sScrollbars_id   ||
            id == sContent_id      ||
            id == sMenubar_id      ||
            id == sToolbar_id      ||
            id == sLocationbar_id  ||
            id == sPersonalbar_id  ||
            id == sStatusbar_id    ||
            id == sControllers_id  ||
            id == sScrollX_id      ||
            id == sScrollY_id      ||
            id == sScrollMaxX_id   ||
            id == sScrollMaxY_id   ||
            id == sLength_id       ||
            id == sFrames_id       ||
            id == sSelf_id         ||
            id == sURL_id);
  }

  static nsIXPConnect *sXPConnect;
  static nsIScriptSecurityManager *sSecMan;

  // nsIXPCScriptable code
  static nsresult DefineStaticJSVals(JSContext *cx);

  static bool sIsInitialized;
  static bool sDisableDocumentAllSupport;

public:
  static jsid sParent_id;
  static jsid sScrollbars_id;
  static jsid sLocation_id;
  static jsid sConstructor_id;
  static jsid s_content_id;
  static jsid sContent_id;
  static jsid sMenubar_id;
  static jsid sToolbar_id;
  static jsid sLocationbar_id;
  static jsid sPersonalbar_id;
  static jsid sStatusbar_id;
  static jsid sDialogArguments_id;
  static jsid sControllers_id;
  static jsid sLength_id;
  static jsid sScrollX_id;
  static jsid sScrollY_id;
  static jsid sScrollMaxX_id;
  static jsid sScrollMaxY_id;
  static jsid sItem_id;
  static jsid sNamedItem_id;
  static jsid sEnumerate_id;
  static jsid sNavigator_id;
  static jsid sTop_id;
  static jsid sDocument_id;
  static jsid sFrames_id;
  static jsid sSelf_id;
  static jsid sAll_id;
  static jsid sJava_id;
  static jsid sPackages_id;
  static jsid sWrappedJSObject_id;
  static jsid sURL_id;
  static jsid sOnload_id;
  static jsid sOnerror_id;

protected:
  static JSPropertyOp sXPCNativeWrapperGetPropertyOp;
  static JSPropertyOp sXrayWrapperPropertyHolderGetPropertyOp;
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
  nsEventTargetSH(nsDOMClassInfoData* aData) : nsDOMGenericSH(aData)
  {
  }

  virtual ~nsEventTargetSH()
  {
  }
public:
  NS_IMETHOD PreCreate(nsISupports *nativeObj, JSContext *cx,
                       JSObject *globalObj, JSObject **parentObj);
  NS_IMETHOD AddProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp, bool *_retval);

  virtual void PreserveWrapper(nsISupports *aNative);

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsEventTargetSH(aData);
  }
};

// Makes sure that the wrapper is preserved if new properties are added.
class nsEventSH : public nsDOMGenericSH
{
protected:
  nsEventSH(nsDOMClassInfoData* aData) : nsDOMGenericSH(aData)
  {
  }

  virtual ~nsEventSH()
  {
  }
public:
  NS_IMETHOD PreCreate(nsISupports* aNativeObj, JSContext* aCx,
                       JSObject* aGlobalObj, JSObject** aParentObj);
  NS_IMETHOD AddProperty(nsIXPConnectWrappedNative* aWrapper, JSContext* aCx,
                         JSObject* aObj, jsid Id, jsval* aVp, bool* aRetval);

  virtual void PreserveWrapper(nsISupports *aNative);

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsEventSH(aData);
  }
};

// Window scriptable helper

class nsWindowSH : public nsDOMGenericSH
{
protected:
  nsWindowSH(nsDOMClassInfoData *aData) : nsDOMGenericSH(aData)
  {
  }

  virtual ~nsWindowSH()
  {
  }

  static nsresult GlobalResolve(nsGlobalWindow *aWin, JSContext *cx,
                                JS::Handle<JSObject*> obj, JS::Handle<jsid> id,
                                bool *did_resolve);

public:
  NS_IMETHOD PreCreate(nsISupports *nativeObj, JSContext *cx,
                       JSObject *globalObj, JSObject **parentObj);
#ifdef DEBUG
  NS_IMETHOD PostCreate(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj)
  {
    nsCOMPtr<nsIScriptGlobalObject> sgo(do_QueryWrappedNative(wrapper));

    NS_ASSERTION(!sgo || sgo->GetGlobalJSObject() == nullptr,
                 "Multiple wrappers created for global object!");

    return NS_OK;
  }
  virtual uint32_t GetScriptableFlags()
  {
    return nsDOMGenericSH::GetScriptableFlags() |
           nsIXPCScriptable::WANT_POSTCREATE;
  }
#endif
  NS_IMETHOD Enumerate(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                       JSObject *obj, bool *_retval);
  NS_IMETHOD NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, jsid id, uint32_t flags,
                        JSObject **objp, bool *_retval);
  NS_IMETHOD Finalize(nsIXPConnectWrappedNative *wrapper, JSFreeOp *fop,
                      JSObject *obj);
  NS_IMETHOD OuterObject(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                         JSObject * obj, JSObject * *_retval);

  static JSBool GlobalScopePolluterNewResolve(JSContext *cx, JSHandleObject obj,
                                              JSHandleId id, unsigned flags,
                                              JS::MutableHandle<JSObject*> objp);
  static JSBool GlobalScopePolluterGetProperty(JSContext *cx, JSHandleObject obj,
                                               JSHandleId id, JSMutableHandleValue vp);
  static JSBool InvalidateGlobalScopePolluter(JSContext *cx,
                                              JS::Handle<JSObject*> obj);
  static nsresult InstallGlobalScopePolluter(JSContext *cx,
                                             JS::Handle<JSObject*> obj);
  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsWindowSH(aData);
  }
};

// Location scriptable helper

class nsLocationSH : public nsDOMGenericSH
{
protected:
  nsLocationSH(nsDOMClassInfoData* aData) : nsDOMGenericSH(aData)
  {
  }

  virtual ~nsLocationSH()
  {
  }

public:
  NS_IMETHOD CheckAccess(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, uint32_t mode,
                         jsval *vp, bool *_retval);

  NS_IMETHOD PreCreate(nsISupports *nativeObj, JSContext *cx,
                       JSObject *globalObj, JSObject **parentObj);
  NS_IMETHODIMP AddProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                            JSObject *obj, jsid id, jsval *vp, bool *_retval);

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsLocationSH(aData);
  }
};


// Navigator scriptable helper

class nsNavigatorSH : public nsDOMGenericSH
{
protected:
  nsNavigatorSH(nsDOMClassInfoData* aData) : nsDOMGenericSH(aData)
  {
  }

  virtual ~nsNavigatorSH()
  {
  }

public:
  NS_IMETHOD PreCreate(nsISupports *nativeObj, JSContext *cx,
                       JSObject *globalObj, JSObject **parentObj);
  NS_IMETHOD NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, jsid id, uint32_t flags,
                        JSObject **objp, bool *_retval);

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsNavigatorSH(aData);
  }
};

// DOM Node helper, this class deals with setting the parent for the
// wrappers

class nsNodeSH : public nsDOMGenericSH
{
protected:
  nsNodeSH(nsDOMClassInfoData* aData) : nsDOMGenericSH(aData)
  {
  }

  virtual ~nsNodeSH()
  {
  }

public:
  NS_IMETHOD PreCreate(nsISupports *nativeObj, JSContext *cx,
                       JSObject *globalObj, JSObject **parentObj);
  NS_IMETHOD AddProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp, bool *_retval);
  NS_IMETHOD NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, jsid id, uint32_t flags,
                        JSObject **objp, bool *_retval);
  NS_IMETHOD GetFlags(uint32_t *aFlags);

  virtual void PreserveWrapper(nsISupports *aNative);

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsNodeSH(aData);
  }
};


// Element helper

class nsElementSH : public nsNodeSH
{
protected:
  nsElementSH(nsDOMClassInfoData* aData) : nsNodeSH(aData)
  {
  }

  virtual ~nsElementSH()
  {
  }

public:
  NS_IMETHOD PreCreate(nsISupports *nativeObj, JSContext *cx,
                       JSObject *globalObj, JSObject **parentObj);
  NS_IMETHOD PostCreate(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj);
  NS_IMETHOD PostTransplant(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                            JSObject *obj);

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsElementSH(aData);
  }
};


// Generic array scriptable helper

class nsGenericArraySH : public nsDOMClassInfo
{
protected:
  nsGenericArraySH(nsDOMClassInfoData* aData) : nsDOMClassInfo(aData)
  {
  }

  virtual ~nsGenericArraySH()
  {
  }
  
public:
  NS_IMETHOD NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, jsid id, uint32_t flags,
                        JSObject **objp, bool *_retval);
  NS_IMETHOD Enumerate(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                       JSObject *obj, bool *_retval);
  
  virtual nsresult GetLength(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                             JSObject *obj, uint32_t *length);

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsGenericArraySH(aData);
  }
};


// Array scriptable helper

class nsArraySH : public nsGenericArraySH
{
protected:
  nsArraySH(nsDOMClassInfoData* aData) : nsGenericArraySH(aData)
  {
  }

  virtual ~nsArraySH()
  {
  }

  // Subclasses need to override this, if the implementation can't fail it's
  // allowed to not set *aResult.
  virtual nsISupports* GetItemAt(nsISupports *aNative, uint32_t aIndex,
                                 nsWrapperCache **aCache, nsresult *aResult) = 0;

public:
  NS_IMETHOD GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp, bool *_retval);

private:
  // Not implemented, nothing should create an instance of this class.
  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData);
};


// NamedArray helper

class nsNamedArraySH : public nsArraySH
{
protected:
  nsNamedArraySH(nsDOMClassInfoData* aData) : nsArraySH(aData)
  {
  }

  virtual ~nsNamedArraySH()
  {
  }

  NS_IMETHOD NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, jsid id, uint32_t flags,
                        JSObject **objp, bool *_retval);

  virtual nsISupports* GetNamedItem(nsISupports *aNative,
                                    const nsAString& aName,
                                    nsWrapperCache **cache,
                                    nsresult *aResult) = 0;

public:
  NS_IMETHOD GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp, bool *_retval);

private:
  // Not implemented, nothing should create an instance of this class.
  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData);
};


// HTMLDocument helper

class nsHTMLDocumentSH
{
protected:
  static JSBool GetDocumentAllNodeList(JSContext *cx, JS::Handle<JSObject*> obj,
                                       nsDocument *doc,
                                       nsContentList **nodeList);
public:
  static JSBool DocumentAllGetProperty(JSContext *cx, JSHandleObject obj, JSHandleId id,
                                       JSMutableHandleValue vp);
  static JSBool DocumentAllNewResolve(JSContext *cx, JSHandleObject obj, JSHandleId id,
                                      unsigned flags, JS::MutableHandle<JSObject*> objp);
  static void ReleaseDocument(JSFreeOp *fop, JSObject *obj);
  static JSBool CallToGetPropMapper(JSContext *cx, unsigned argc, jsval *vp);
  static JSBool DocumentAllHelperGetProperty(JSContext *cx, JSHandleObject obj,
                                             JSHandleId id, JSMutableHandleValue vp);
  static JSBool DocumentAllHelperNewResolve(JSContext *cx, JSHandleObject obj,
                                            JSHandleId id, unsigned flags,
                                            JS::MutableHandle<JSObject*> objp);

  static nsresult TryResolveAll(JSContext* cx, nsHTMLDocument* doc,
                                JS::Handle<JSObject*> obj);
};


// HTMLFormElement helper

class nsHTMLFormElementSH : public nsElementSH
{
protected:
  nsHTMLFormElementSH(nsDOMClassInfoData* aData) : nsElementSH(aData)
  {
  }

  virtual ~nsHTMLFormElementSH()
  {
  }

public:
  NS_IMETHOD NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, jsid id, uint32_t flags,
                        JSObject **objp, bool *_retval);
  NS_IMETHOD GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp,
                         bool *_retval);

  NS_IMETHOD NewEnumerate(nsIXPConnectWrappedNative *wrapper,
                          JSContext *cx, JSObject *obj,
                          uint32_t enum_op, jsval *statep,
                          jsid *idp, bool *_retval);

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsHTMLFormElementSH(aData);
  }
};


// Plugin helper

class nsPluginSH : public nsNamedArraySH
{
protected:
  nsPluginSH(nsDOMClassInfoData* aData) : nsNamedArraySH(aData)
  {
  }

  virtual ~nsPluginSH()
  {
  }

  virtual nsISupports* GetItemAt(nsISupports *aNative, uint32_t aIndex,
                                 nsWrapperCache **aCache, nsresult *aResult);

  // Override nsNamedArraySH::GetNamedItem()
  virtual nsISupports* GetNamedItem(nsISupports *aNative,
                                    const nsAString& aName,
                                    nsWrapperCache **cache,
                                    nsresult *aResult);

public:
  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsPluginSH(aData);
  }
};


// PluginArray helper

class nsPluginArraySH : public nsNamedArraySH
{
protected:
  nsPluginArraySH(nsDOMClassInfoData* aData) : nsNamedArraySH(aData)
  {
  }

  virtual ~nsPluginArraySH()
  {
  }

  virtual nsISupports* GetItemAt(nsISupports *aNative, uint32_t aIndex,
                                 nsWrapperCache **aCache, nsresult *aResult);

  // Override nsNamedArraySH::GetNamedItem()
  virtual nsISupports* GetNamedItem(nsISupports *aNative,
                                    const nsAString& aName,
                                    nsWrapperCache **cache,
                                    nsresult *aResult);

public:
  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsPluginArraySH(aData);
  }
};


// MimeTypeArray helper

class nsMimeTypeArraySH : public nsNamedArraySH
{
protected:
  nsMimeTypeArraySH(nsDOMClassInfoData* aData) : nsNamedArraySH(aData)
  {
  }

  virtual ~nsMimeTypeArraySH()
  {
  }

  virtual nsISupports* GetItemAt(nsISupports *aNative, uint32_t aIndex,
                                 nsWrapperCache **aCache, nsresult *aResult);

  // Override nsNamedArraySH::GetNamedItem()
  virtual nsISupports* GetNamedItem(nsISupports *aNative,
                                    const nsAString& aName,
                                    nsWrapperCache **cache,
                                    nsresult *aResult);

public:
  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsMimeTypeArraySH(aData);
  }
};


// String array helper

class nsStringArraySH : public nsGenericArraySH
{
protected:
  nsStringArraySH(nsDOMClassInfoData* aData) : nsGenericArraySH(aData)
  {
  }

  virtual ~nsStringArraySH()
  {
  }

  virtual nsresult GetStringAt(nsISupports *aNative, int32_t aIndex,
                               nsAString& aResult) = 0;

public:
  NS_IMETHOD GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp, bool *_retval);
};


// History helper

class nsHistorySH : public nsStringArraySH
{
protected:
  nsHistorySH(nsDOMClassInfoData* aData) : nsStringArraySH(aData)
  {
  }

  virtual ~nsHistorySH()
  {
  }

  virtual nsresult GetStringAt(nsISupports *aNative, int32_t aIndex,
                               nsAString& aResult);

public:
  NS_IMETHOD PreCreate(nsISupports *nativeObj, JSContext *cx,
                       JSObject *globalObj, JSObject **parentObj);
  NS_IMETHOD GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp, bool *_retval);

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsHistorySH(aData);
  }
};

// StringList scriptable helper

class nsStringListSH : public nsStringArraySH
{
protected:
  nsStringListSH(nsDOMClassInfoData* aData) : nsStringArraySH(aData)
  {
  }

  virtual ~nsStringListSH()
  {
  }

  virtual nsresult GetStringAt(nsISupports *aNative, int32_t aIndex,
                               nsAString& aResult);

public:
  // Inherit GetProperty, Enumerate from nsStringArraySH
  
  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsStringListSH(aData);
  }
};


// MediaList helper

class nsMediaListSH : public nsStringArraySH
{
protected:
  nsMediaListSH(nsDOMClassInfoData* aData) : nsStringArraySH(aData)
  {
  }

  virtual ~nsMediaListSH()
  {
  }

  virtual nsresult GetStringAt(nsISupports *aNative, int32_t aIndex,
                               nsAString& aResult);

public:
  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsMediaListSH(aData);
  }
};


// StyleSheetList helper

class nsStyleSheetListSH : public nsArraySH
{
protected:
  nsStyleSheetListSH(nsDOMClassInfoData* aData) : nsArraySH(aData)
  {
  }

  virtual ~nsStyleSheetListSH()
  {
  }

  virtual nsISupports* GetItemAt(nsISupports *aNative, uint32_t aIndex,
                                 nsWrapperCache **aCache, nsresult *aResult);

public:
  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsStyleSheetListSH(aData);
  }
};


// CSSRuleList helper

class nsCSSRuleListSH : public nsArraySH
{
protected:
  nsCSSRuleListSH(nsDOMClassInfoData* aData) : nsArraySH(aData)
  {
  }

  virtual ~nsCSSRuleListSH()
  {
  }

  virtual nsISupports* GetItemAt(nsISupports *aNative, uint32_t aIndex,
                                 nsWrapperCache **aCache, nsresult *aResult);

public:
  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsCSSRuleListSH(aData);
  }
};

class nsDOMTouchListSH : public nsArraySH
{
  protected:
  nsDOMTouchListSH(nsDOMClassInfoData* aData) : nsArraySH(aData)
  {
  }

  virtual ~nsDOMTouchListSH()
  {
  }

  virtual nsISupports* GetItemAt(nsISupports *aNative, uint32_t aIndex,
                                 nsWrapperCache **aCache, nsresult *aResult);

  public:
  static nsIClassInfo* doCreate(nsDOMClassInfoData* aData)
  {
    return new nsDOMTouchListSH(aData);
  }
};

// WebApps Storage helpers

class nsStorage2SH : public nsDOMGenericSH
{
protected:
  nsStorage2SH(nsDOMClassInfoData* aData) : nsDOMGenericSH(aData)
  {
  }

  virtual ~nsStorage2SH()
  {
  }

  NS_IMETHOD NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, jsid id, uint32_t flags,
                        JSObject **objp, bool *_retval);
  NS_IMETHOD SetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp, bool *_retval);
  NS_IMETHOD GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp, bool *_retval);
  NS_IMETHOD DelProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, bool *_retval);
  NS_IMETHOD NewEnumerate(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                          JSObject *obj, uint32_t enum_op, jsval *statep,
                          jsid *idp, bool *_retval);

public:
  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsStorage2SH(aData);
  }
};

// Event handler 'this' translator class, this is called by XPConnect
// when a "function interface" (nsIDOMEventListener) is called, this
// class extracts 'this' fomr the first argument to the called
// function (nsIDOMEventListener::HandleEvent(in nsIDOMEvent)), this
// class will pass back nsIDOMEvent::currentTarget to be used as
// 'this'.

class nsEventListenerThisTranslator : public nsIXPCFunctionThisTranslator
{
public:
  nsEventListenerThisTranslator()
  {
  }

  virtual ~nsEventListenerThisTranslator()
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
  nsDOMConstructorSH(nsDOMClassInfoData* aData) : nsDOMGenericSH(aData)
  {
  }

public:
  NS_IMETHOD PreCreate(nsISupports *nativeObj, JSContext *cx,
                       JSObject *globalObj, JSObject **parentObj);
  NS_IMETHOD PostCreatePrototype(JSContext * cx, JSObject * proto)
  {
    return NS_OK;
  }
  NS_IMETHOD NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, jsid id, uint32_t flags,
                        JSObject **objp, bool *_retval);
  NS_IMETHOD Call(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                  JSObject *obj, const JS::CallArgs &args, bool *_retval);

  NS_IMETHOD Construct(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                       JSObject *obj, const JS::CallArgs &args, bool *_retval);

  NS_IMETHOD HasInstance(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, const jsval &val, bool *bp,
                         bool *_retval);

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsDOMConstructorSH(aData);
  }
};

class nsNonDOMObjectSH : public nsDOMGenericSH
{
protected:
  nsNonDOMObjectSH(nsDOMClassInfoData* aData) : nsDOMGenericSH(aData)
  {
  }

  virtual ~nsNonDOMObjectSH()
  {
  }

public:
  NS_IMETHOD GetFlags(uint32_t *aFlags);

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsNonDOMObjectSH(aData);
  }
};

class nsOfflineResourceListSH : public nsStringArraySH
{
protected:
  nsOfflineResourceListSH(nsDOMClassInfoData* aData) : nsStringArraySH(aData)
  {
  }

  virtual ~nsOfflineResourceListSH()
  {
  }

  virtual nsresult GetStringAt(nsISupports *aNative, int32_t aIndex,
                               nsAString& aResult);

public:
  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsOfflineResourceListSH(aData);
  }
};

#endif /* nsDOMClassInfo_h___ */
