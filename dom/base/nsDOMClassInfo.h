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

namespace mozilla {
class DOMSVGLengthList;
class DOMSVGNumberList;
class DOMSVGPathSegList;
class DOMSVGPointList;
class DOMSVGStringList;
class DOMSVGTransformList;
}

class nsContentList;
class nsGlobalWindow;
class nsICanvasRenderingContextInternal;
class nsIDOMHTMLOptionsCollection;
class nsIDOMSVGLength;
class nsIDOMSVGLengthList;
class nsIDOMSVGNumber;
class nsIDOMSVGNumberList;
class nsIDOMSVGPathSeg;
class nsIDOMSVGPathSegList;
class nsIDOMSVGPoint;
class nsIDOMSVGPointList;
class nsIDOMSVGStringList;
class nsIDOMSVGTests;
class nsIDOMSVGTransform;
class nsIDOMSVGTransformList;
class nsIDOMWindow;
class nsIForm;
class nsIHTMLDocument;
class nsNPAPIPluginInstance;

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
  PRUint32 mScriptableFlags : 31; // flags must not use more than 31 bits!
  PRUint32 mHasClassInterface : 1;
  PRUint32 mInterfacesBitmap;
  bool mChromeOnly;
  bool mDisabled;
#ifdef DEBUG
  PRUint32 mDebugID;
#endif
};

struct nsExternalDOMClassInfoData : public nsDOMClassInfoData
{
  const nsCID *mConstructorCID;
};


typedef PRUptrdiff PtrBits;

// To be used with the nsDOMClassInfoData::mCachedClassInfo pointer.
// The low bit is set when we created a generic helper for an external
// (which holds on to the nsDOMClassInfoData).
#define GET_CLEAN_CI_PTR(_ptr) (nsIClassInfo*)(PtrBits(_ptr) & ~0x1)
#define MARK_EXTERNAL(_ptr) (nsIClassInfo*)(PtrBits(_ptr) | 0x1)
#define IS_EXTERNAL(_ptr) (PtrBits(_ptr) & 0x1)


class nsDOMClassInfo : public nsXPCClassInfo
{
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

  virtual PRUint32 GetInterfacesBitmap()
  {
    return mData->mInterfacesBitmap;
  }

  static nsresult Init();
  static nsresult RegisterClassName(PRInt32 aDOMClassInfoID);
  static nsresult RegisterClassProtos(PRInt32 aDOMClassInfoID);
  static nsresult RegisterExternalClasses();
  nsresult ResolveConstructor(JSContext *cx, JSObject *obj,
                              JSObject **objp);

  // Checks if id is a number and returns the number, if aIsNumber is
  // non-null it's set to true if the id is a number and false if it's
  // not a number. If id is not a number this method returns -1
  static PRInt32 GetArrayIndexFromId(JSContext *cx, jsid id,
                                     bool *aIsNumber = nsnull);

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

  static inline bool IsWritableReplaceable(jsid id)
  {
    return (id == sInnerHeight_id  ||
            id == sInnerWidth_id   ||
            id == sOpener_id       ||
            id == sOuterHeight_id  ||
            id == sOuterWidth_id   ||
            id == sScreenX_id      ||
            id == sScreenY_id      ||
            id == sStatus_id       ||
            id == sName_id);
  }

  static nsIXPConnect *sXPConnect;
  static nsIScriptSecurityManager *sSecMan;

  // nsIXPCScriptable code
  static nsresult DefineStaticJSVals(JSContext *cx);

  static bool sIsInitialized;
  static bool sDisableDocumentAllSupport;
  static bool sDisableGlobalScopePollutionSupport;

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
  static jsid sInnerHeight_id;
  static jsid sInnerWidth_id;
  static jsid sOuterHeight_id;
  static jsid sOuterWidth_id;
  static jsid sScreenX_id;
  static jsid sScreenY_id;
  static jsid sStatus_id;
  static jsid sName_id;
  static jsid sScrollX_id;
  static jsid sScrollY_id;
  static jsid sScrollMaxX_id;
  static jsid sScrollMaxY_id;
  static jsid sItem_id;
  static jsid sNamedItem_id;
  static jsid sEnumerate_id;
  static jsid sNavigator_id;
  static jsid sDocument_id;
  static jsid sFrames_id;
  static jsid sSelf_id;
  static jsid sOpener_id;
  static jsid sAll_id;
  static jsid sTags_id;
  static jsid sAddEventListener_id;
  static jsid sBaseURIObject_id;
  static jsid sNodePrincipal_id;
  static jsid sDocumentURIObject_id;
  static jsid sJava_id;
  static jsid sPackages_id;
  static jsid sWrappedJSObject_id;
  static jsid sURL_id;
  static jsid sKeyPath_id;
  static jsid sAutoIncrement_id;
  static jsid sUnique_id;
  static jsid sMultiEntry_id;
  static jsid sOnload_id;
  static jsid sOnerror_id;

protected:
  static JSPropertyOp sXPCNativeWrapperGetPropertyOp;
  static JSPropertyOp sXrayWrapperPropertyHolderGetPropertyOp;
};


inline
const nsQueryInterface
do_QueryWrappedNative(nsIXPConnectWrappedNative *wrapper, JSObject *obj)
{
  return nsQueryInterface(nsDOMClassInfo::GetNative(wrapper, obj));
}

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
                                JSObject *obj, jsid id, bool *did_resolve);

public:
  NS_IMETHOD PreCreate(nsISupports *nativeObj, JSContext *cx,
                       JSObject *globalObj, JSObject **parentObj);
#ifdef DEBUG
  NS_IMETHOD PostCreate(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj)
  {
    nsCOMPtr<nsIScriptGlobalObject> sgo(do_QueryWrappedNative(wrapper));

    NS_ASSERTION(!sgo || sgo->GetGlobalJSObject() == nsnull,
                 "Multiple wrappers created for global object!");

    return NS_OK;
  }
  virtual PRUint32 GetScriptableFlags()
  {
    return nsDOMGenericSH::GetScriptableFlags() |
           nsIXPCScriptable::WANT_POSTCREATE;
  }
#endif
  NS_IMETHOD GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp, bool *_retval);
  NS_IMETHOD Enumerate(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                       JSObject *obj, bool *_retval);
  NS_IMETHOD NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, jsid id, PRUint32 flags,
                        JSObject **objp, bool *_retval);
  NS_IMETHOD Finalize(nsIXPConnectWrappedNative *wrapper, JSFreeOp *fop,
                      JSObject *obj);
  NS_IMETHOD OuterObject(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                         JSObject * obj, JSObject * *_retval);

  static JSBool GlobalScopePolluterNewResolve(JSContext *cx, JSHandleObject obj,
                                              JSHandleId id, unsigned flags,
                                              JSMutableHandleObject objp);
  static JSBool GlobalScopePolluterGetProperty(JSContext *cx, JSHandleObject obj,
                                               JSHandleId id, jsval *vp);
  static JSBool SecurityCheckOnAddDelProp(JSContext *cx, JSHandleObject obj, JSHandleId id,
                                          jsval *vp);
  static JSBool SecurityCheckOnSetProp(JSContext *cx, JSHandleObject obj, JSHandleId id,
                                       JSBool strict, jsval *vp);
  static void InvalidateGlobalScopePolluter(JSContext *cx, JSObject *obj);
  static nsresult InstallGlobalScopePolluter(JSContext *cx, JSObject *obj,
                                             nsIHTMLDocument *doc);
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
                         JSObject *obj, jsid id, PRUint32 mode,
                         jsval *vp, bool *_retval);

  NS_IMETHOD PreCreate(nsISupports *nativeObj, JSContext *cx,
                       JSObject *globalObj, JSObject **parentObj);

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
                        JSObject *obj, jsid id, PRUint32 flags,
                        JSObject **objp, bool *_retval);

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsNavigatorSH(aData);
  }
};

// WebGLExtension scriptable helper

class WebGLExtensionSH : public nsDOMGenericSH
{
protected:
  WebGLExtensionSH(nsDOMClassInfoData* aData) : nsDOMGenericSH(aData)
  {
  }

  virtual ~WebGLExtensionSH()
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
    return new WebGLExtensionSH(aData);
  }
};

// scriptable helper for new-binding objects without wrapper caches

class nsNewDOMBindingNoWrapperCacheSH : public nsDOMGenericSH
{
protected:
  nsNewDOMBindingNoWrapperCacheSH(nsDOMClassInfoData* aData) : nsDOMGenericSH(aData)
  {
  }

  virtual ~nsNewDOMBindingNoWrapperCacheSH()
  {
  }

public:
  NS_IMETHOD PreCreate(nsISupports *nativeObj, JSContext *cx,
                       JSObject *globalObj, JSObject **parentObj);

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsNewDOMBindingNoWrapperCacheSH(aData);
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

  // Helper to check whether a capability is enabled
  bool IsCapabilityEnabled(const char* aCapability);

  inline bool IsPrivilegedScript() {
    return IsCapabilityEnabled("UniversalXPConnect");
  }

public:
  NS_IMETHOD PreCreate(nsISupports *nativeObj, JSContext *cx,
                       JSObject *globalObj, JSObject **parentObj);
  NS_IMETHOD PostCreatePrototype(JSContext * cx, JSObject * proto);
  NS_IMETHOD AddProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp, bool *_retval);
  NS_IMETHOD NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, jsid id, PRUint32 flags,
                        JSObject **objp, bool *_retval);
  NS_IMETHOD GetFlags(PRUint32 *aFlags);

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
                        JSObject *obj, jsid id, PRUint32 flags,
                        JSObject **objp, bool *_retval);
  NS_IMETHOD Enumerate(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                       JSObject *obj, bool *_retval);
  
  virtual nsresult GetLength(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                             JSObject *obj, PRUint32 *length);

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
  virtual nsISupports* GetItemAt(nsISupports *aNative, PRUint32 aIndex,
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
                        JSObject *obj, jsid id, PRUint32 flags,
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


// NamedNodeMap helper

class nsNamedNodeMapSH : public nsNamedArraySH
{
protected:
  nsNamedNodeMapSH(nsDOMClassInfoData* aData) : nsNamedArraySH(aData)
  {
  }

  virtual ~nsNamedNodeMapSH()
  {
  }

  virtual nsISupports* GetItemAt(nsISupports *aNative, PRUint32 aIndex,
                                 nsWrapperCache **aCache, nsresult *aResult);

  // Override nsNamedArraySH::GetNamedItem()
  virtual nsISupports* GetNamedItem(nsISupports *aNative,
                                    const nsAString& aName,
                                    nsWrapperCache **cache,
                                    nsresult *aResult);

public:
  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsNamedNodeMapSH(aData);
  }
};


// DOMStringMap helper for .dataset property on elements.

class nsDOMStringMapSH : public nsDOMGenericSH
{
public:
  nsDOMStringMapSH(nsDOMClassInfoData* aData) : nsDOMGenericSH(aData)
  {
  }

  virtual ~nsDOMStringMapSH()
  {
  }

public:
  NS_IMETHOD NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, jsid id, PRUint32 flags,
                        JSObject **objp, bool *_retval);
  NS_IMETHOD Enumerate(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                       JSObject *obj, bool *_retval);
  NS_IMETHOD PreCreate(nsISupports *nativeObj, JSContext *cx,
                       JSObject *globalObj, JSObject **parentObj);
  NS_IMETHOD DelProperty(nsIXPConnectWrappedNative *wrapper,
                         JSContext *cx, JSObject *obj, jsid id,
                         jsval *vp, bool *_retval);
  NS_IMETHOD GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp, bool *_retval);
  NS_IMETHOD SetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp, bool *_retval);

  bool JSIDToProp(const jsid& aId, nsAString& aResult);

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsDOMStringMapSH(aData);
  }
};


// Document helper, for document.location and document.on*

class nsDocumentSH : public nsNodeSH
{
public:
  nsDocumentSH(nsDOMClassInfoData* aData) : nsNodeSH(aData)
  {
  }

  virtual ~nsDocumentSH()
  {
  }

public:
  NS_IMETHOD PostCreatePrototype(JSContext * cx, JSObject * proto);
  NS_IMETHOD NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, jsid id, PRUint32 flags,
                        JSObject **objp, bool *_retval);
  NS_IMETHOD GetFlags(PRUint32* aFlags);
  NS_IMETHOD PostCreate(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj);

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsDocumentSH(aData);
  }
};


// HTMLDocument helper

class nsHTMLDocumentSH : public nsDocumentSH
{
protected:
  nsHTMLDocumentSH(nsDOMClassInfoData* aData) : nsDocumentSH(aData)
  {
  }

  virtual ~nsHTMLDocumentSH()
  {
  }

  static JSBool GetDocumentAllNodeList(JSContext *cx, JSObject *obj,
                                       nsDocument *doc,
                                       nsContentList **nodeList);

public:
  static JSBool DocumentAllGetProperty(JSContext *cx, JSHandleObject obj, JSHandleId id,
                                       jsval *vp);
  static JSBool DocumentAllNewResolve(JSContext *cx, JSHandleObject obj, JSHandleId id,
                                      unsigned flags, JSMutableHandleObject objp);
  static void ReleaseDocument(JSFreeOp *fop, JSObject *obj);
  static JSBool CallToGetPropMapper(JSContext *cx, unsigned argc, jsval *vp);
  static JSBool DocumentAllHelperGetProperty(JSContext *cx, JSHandleObject obj,
                                             JSHandleId id, jsval *vp);
  static JSBool DocumentAllHelperNewResolve(JSContext *cx, JSHandleObject obj,
                                            JSHandleId id, unsigned flags,
                                            JSMutableHandleObject objp);
  static JSBool DocumentAllTagsNewResolve(JSContext *cx, JSHandleObject obj,
                                          JSHandleId id, unsigned flags,
                                          JSMutableHandleObject objp);

  NS_IMETHOD NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, jsid id, PRUint32 flags,
                        JSObject **objp, bool *_retval);
  NS_IMETHOD GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp, bool *_retval);

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsHTMLDocumentSH(aData);
  }
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

  static nsresult FindNamedItem(nsIForm *aForm, jsid id,
                                nsISupports **aResult, nsWrapperCache **aCache);

public:
  NS_IMETHOD NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, jsid id, PRUint32 flags,
                        JSObject **objp, bool *_retval);
  NS_IMETHOD GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp,
                         bool *_retval);

  NS_IMETHOD NewEnumerate(nsIXPConnectWrappedNative *wrapper,
                          JSContext *cx, JSObject *obj,
                          PRUint32 enum_op, jsval *statep,
                          jsid *idp, bool *_retval);

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsHTMLFormElementSH(aData);
  }
};


// HTMLSelectElement helper

class nsHTMLSelectElementSH : public nsElementSH
{
protected:
  nsHTMLSelectElementSH(nsDOMClassInfoData* aData) : nsElementSH(aData)
  {
  }

  virtual ~nsHTMLSelectElementSH()
  {
  }

public:
  NS_IMETHOD NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, jsid id, PRUint32 flags,
                        JSObject **objp, bool *_retval);
  NS_IMETHOD GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp,
                         bool *_retval);
  NS_IMETHOD SetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp, bool *_retval);

  static nsresult SetOption(JSContext *cx, jsval *vp, PRUint32 aIndex,
                            nsIDOMHTMLOptionsCollection *aOptCollection);

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsHTMLSelectElementSH(aData);
  }
};


// HTMLEmbed/Object/AppletElement helper

class nsHTMLPluginObjElementSH : public nsElementSH
{
protected:
  nsHTMLPluginObjElementSH(nsDOMClassInfoData* aData)
    : nsElementSH(aData)
  {
  }

  virtual ~nsHTMLPluginObjElementSH()
  {
  }

  static nsresult GetPluginInstanceIfSafe(nsIXPConnectWrappedNative *aWrapper,
                                          JSObject *obj,
                                          nsNPAPIPluginInstance **aResult);

  static nsresult GetPluginJSObject(JSContext *cx, JSObject *obj,
                                    nsNPAPIPluginInstance *plugin_inst,
                                    JSObject **plugin_obj,
                                    JSObject **plugin_proto);

public:
  NS_IMETHOD NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, jsid id, PRUint32 flags,
                        JSObject **objp, bool *_retval);
  NS_IMETHOD PreCreate(nsISupports *nativeObj, JSContext *cx,
                       JSObject *globalObj, JSObject **parentObj);
  NS_IMETHOD PostCreate(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj);
  NS_IMETHOD GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp, bool *_retval);
  NS_IMETHOD SetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp, bool *_retval);
  NS_IMETHOD Call(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                  JSObject *obj, PRUint32 argc, jsval *argv, jsval *vp,
                  bool *_retval);


  static nsresult SetupProtoChain(nsIXPConnectWrappedNative *wrapper,
                                  JSContext *cx, JSObject *obj);

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsHTMLPluginObjElementSH(aData);
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

  virtual nsISupports* GetItemAt(nsISupports *aNative, PRUint32 aIndex,
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

  virtual nsISupports* GetItemAt(nsISupports *aNative, PRUint32 aIndex,
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

  virtual nsISupports* GetItemAt(nsISupports *aNative, PRUint32 aIndex,
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

  virtual nsresult GetStringAt(nsISupports *aNative, PRInt32 aIndex,
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

  virtual nsresult GetStringAt(nsISupports *aNative, PRInt32 aIndex,
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

  virtual nsresult GetStringAt(nsISupports *aNative, PRInt32 aIndex,
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

  virtual nsresult GetStringAt(nsISupports *aNative, PRInt32 aIndex,
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

  virtual nsISupports* GetItemAt(nsISupports *aNative, PRUint32 aIndex,
                                 nsWrapperCache **aCache, nsresult *aResult);

public:
  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsStyleSheetListSH(aData);
  }
};


// CSSValueList helper

class nsCSSValueListSH : public nsArraySH
{
protected:
  nsCSSValueListSH(nsDOMClassInfoData* aData) : nsArraySH(aData)
  {
  }

  virtual ~nsCSSValueListSH()
  {
  }

  virtual nsISupports* GetItemAt(nsISupports *aNative, PRUint32 aIndex,
                                 nsWrapperCache **aCache, nsresult *aResult);

public:
  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsCSSValueListSH(aData);
  }
};


// CSSStyleDeclaration helper

class nsCSSStyleDeclSH : public nsStringArraySH
{
protected:
  nsCSSStyleDeclSH(nsDOMClassInfoData* aData) : nsStringArraySH(aData)
  {
  }

  virtual ~nsCSSStyleDeclSH()
  {
  }

  virtual nsresult GetStringAt(nsISupports *aNative, PRInt32 aIndex,
                               nsAString& aResult);

public:
  NS_IMETHOD PreCreate(nsISupports *nativeObj, JSContext *cx,
                       JSObject *globalObj, JSObject **parentObj);

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsCSSStyleDeclSH(aData);
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

  virtual nsISupports* GetItemAt(nsISupports *aNative, PRUint32 aIndex,
                                 nsWrapperCache **aCache, nsresult *aResult);

public:
  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsCSSRuleListSH(aData);
  }
};

#ifdef MOZ_XUL
// TreeColumns helper

class nsTreeColumnsSH : public nsNamedArraySH
{
protected:
  nsTreeColumnsSH(nsDOMClassInfoData* aData) : nsNamedArraySH(aData)
  {
  }

  virtual ~nsTreeColumnsSH()
  {
  }

  virtual nsISupports* GetItemAt(nsISupports *aNative, PRUint32 aIndex,
                                 nsWrapperCache **aCache, nsresult *aResult);

  // Override nsNamedArraySH::GetNamedItem()
  virtual nsISupports* GetNamedItem(nsISupports *aNative,
                                    const nsAString& aName,
                                    nsWrapperCache **cache,
                                    nsresult *aResult);

public:
  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsTreeColumnsSH(aData);
  }
};
#endif

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
                        JSObject *obj, jsid id, PRUint32 flags,
                        JSObject **objp, bool *_retval);
  NS_IMETHOD SetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp, bool *_retval);
  NS_IMETHOD GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp, bool *_retval);
  NS_IMETHOD DelProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp, bool *_retval);
  NS_IMETHOD NewEnumerate(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                          JSObject *obj, PRUint32 enum_op, jsval *statep,
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

class nsMutationCallbackThisTranslator : public nsIXPCFunctionThisTranslator
{
public:
  nsMutationCallbackThisTranslator()
  {
  }

  virtual ~nsMutationCallbackThisTranslator()
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
                        JSObject *obj, jsid id, PRUint32 flags,
                        JSObject **objp, bool *_retval);
  NS_IMETHOD Call(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                  JSObject *obj, PRUint32 argc, jsval *argv, jsval *vp,
                  bool *_retval);

  NS_IMETHOD Construct(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                       JSObject *obj, PRUint32 argc, jsval *argv,
                       jsval *vp, bool *_retval);

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
  NS_IMETHOD GetFlags(PRUint32 *aFlags);

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsNonDOMObjectSH(aData);
  }
};

// Need this to override GetFlags() on nsNodeSH
class nsAttributeSH : public nsNodeSH
{
protected:
  nsAttributeSH(nsDOMClassInfoData* aData) : nsNodeSH(aData)
  {
  }

  virtual ~nsAttributeSH()
  {
  }

public:
  NS_IMETHOD GetFlags(PRUint32 *aFlags);

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsAttributeSH(aData);
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

  virtual nsresult GetStringAt(nsISupports *aNative, PRInt32 aIndex,
                               nsAString& aResult);

public:
  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsOfflineResourceListSH(aData);
  }
};

// SVGStringList helper

class nsSVGStringListSH : public nsStringArraySH
{
protected:
  nsSVGStringListSH(nsDOMClassInfoData* aData) : nsStringArraySH(aData)
  {
  }
  
  virtual ~nsSVGStringListSH()
  {
  }
  
  virtual nsresult GetStringAt(nsISupports *aNative, PRInt32 aIndex,
                               nsAString& aResult);
  
public:
  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsSVGStringListSH(aData);
  }
};

template<class T, class BaseType = T>
class nsNewDOMBindingSH : public nsDOMGenericSH
{
protected:
  nsNewDOMBindingSH(nsDOMClassInfoData* aData) : nsDOMGenericSH(aData)
  {
  }

  virtual ~nsNewDOMBindingSH()
  {
  }

public:
  NS_IMETHOD PreCreate(nsISupports *nativeObj, JSContext *cx,
                       JSObject *globalObj, JSObject **parentObj);

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsNewDOMBindingSH<T, BaseType>(aData);
  }
};

class nsWebGLViewportHandlerSH
  : public nsNewDOMBindingSH<nsICanvasRenderingContextInternal>
{
protected:
  nsWebGLViewportHandlerSH(nsDOMClassInfoData *aData)
    : nsNewDOMBindingSH<nsICanvasRenderingContextInternal>(aData)
  {
  }

  virtual ~nsWebGLViewportHandlerSH()
  {
  }

public:
  NS_IMETHOD PostCreatePrototype(JSContext * cx, JSObject * proto) {
    nsresult rv = nsDOMGenericSH::PostCreatePrototype(cx, proto);
    if (NS_SUCCEEDED(rv)) {
      if (!::JS_DefineProperty(cx, proto, "VIEWPORT", INT_TO_JSVAL(0x0BA2),
                               nsnull, nsnull, JSPROP_ENUMERATE))
      {
        return NS_ERROR_UNEXPECTED;
      }
    }
    return rv;
  }

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsWebGLViewportHandlerSH(aData);
  }
};

#endif /* nsDOMClassInfo_h___ */
