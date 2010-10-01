/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Johnny Stenback <jst@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsDOMClassInfo_h___
#define nsDOMClassInfo_h___

#include "nsIDOMClassInfo.h"
#include "nsIXPCScriptable.h"
#include "jsapi.h"
#include "nsIScriptSecurityManager.h"
#include "nsIScriptContext.h"
#include "nsDOMJSUtils.h" // for GetScriptContextFromJSContext
#include "nsIScriptGlobalObject.h"
#include "nsContentUtils.h"

class nsIDOMWindow;
class nsIDOMNSHTMLOptionCollection;
class nsIPluginInstance;
class nsIForm;
class nsIDOMNodeList;
class nsIDOMDocument;
class nsIHTMLDocument;
class nsGlobalWindow;

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
  PRBool mChromeOnly;
#ifdef NS_DEBUG
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

  static nsresult WrapNative(JSContext *cx, JSObject *scope,
                             nsISupports *native, const nsIID* aIID,
                             PRBool aAllowWrapping, jsval *vp,
                             // If non-null aHolder will keep the jsval alive
                             // while there's a ref to it
                             nsIXPConnectJSObjectHolder** aHolder = nsnull)
  {
    return nsContentUtils::WrapNative(cx, scope, native, aIID, vp, aHolder,
                                      aAllowWrapping);
  }

  // Used for cases where PreCreate needs to wrap the native parent, and the
  // native parent is likely to have been wrapped already.  |native| must
  // implement nsWrapperCache, and nativeWrapperCache must be |native|'s
  // nsWrapperCache.
  static inline nsresult WrapNativeParent(JSContext *cx, JSObject *scope,
                                          nsISupports *native,
                                          nsWrapperCache *nativeWrapperCache,
                                          JSObject **parentObj);

  // Same as the WrapNative above, but use these if aIID is nsISupports' IID.
  static nsresult WrapNative(JSContext *cx, JSObject *scope,
                             nsISupports *native, PRBool aAllowWrapping,
                             jsval *vp,
                             // If non-null aHolder will keep the jsval alive
                             // while there's a ref to it
                             nsIXPConnectJSObjectHolder** aHolder = nsnull)
  {
    return nsContentUtils::WrapNative(cx, scope, native, vp, aHolder,
                                      aAllowWrapping);
  }
  static nsresult WrapNative(JSContext *cx, JSObject *scope,
                             nsISupports *native, nsWrapperCache *cache,
                             PRBool aAllowWrapping, jsval *vp,
                             // If non-null aHolder will keep the jsval alive
                             // while there's a ref to it
                             nsIXPConnectJSObjectHolder** aHolder = nsnull)
  {
    return nsContentUtils::WrapNative(cx, scope, native, cache, vp, aHolder,
                                      aAllowWrapping);
  }

  static nsresult ThrowJSException(JSContext *cx, nsresult aResult);

  static JSPropertyOp GetXPCNativeWrapperGetPropertyOp() {
    return sXPCNativeWrapperGetPropertyOp;
  }

  static void SetXPCNativeWrapperGetPropertyOp(JSPropertyOp getPropertyOp) {
    NS_ASSERTION(!sXPCNativeWrapperGetPropertyOp,
                 "Double set of sXPCNativeWrapperGetPropertyOp");
    sXPCNativeWrapperGetPropertyOp = getPropertyOp;
  }

  static JSPropertyOp GetXrayWrapperPropertyHolderGetPropertyOp() {
    return sXrayWrapperPropertyHolderGetPropertyOp;
  }

  static void SetXrayWrapperPropertyHolderGetPropertyOp(JSPropertyOp getPropertyOp) {
    sXrayWrapperPropertyHolderGetPropertyOp = getPropertyOp;
  }

  static PRBool ObjectIsNativeWrapper(JSContext* cx, JSObject* obj);

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
                                     PRBool *aIsNumber = nsnull);

  static inline PRBool IsReadonlyReplaceable(jsid id)
  {
    return (id == sTop_id          ||
            id == sParent_id       ||
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
            id == sSelf_id);
  }

  static inline PRBool IsWritableReplaceable(jsid id)
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

  static PRBool sIsInitialized;
  static PRBool sDisableDocumentAllSupport;
  static PRBool sDisableGlobalScopePollutionSupport;

  static jsid sTop_id;
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
  static jsid sOnmousedown_id;
  static jsid sOnmouseup_id;
  static jsid sOnclick_id;
  static jsid sOndblclick_id;
  static jsid sOncontextmenu_id;
  static jsid sOnmouseover_id;
  static jsid sOnmouseout_id;
  static jsid sOnkeydown_id;
  static jsid sOnkeyup_id;
  static jsid sOnkeypress_id;
  static jsid sOnmousemove_id;
  static jsid sOnfocus_id;
  static jsid sOnblur_id;
  static jsid sOnsubmit_id;
  static jsid sOnreset_id;
  static jsid sOnchange_id;
  static jsid sOnselect_id;
  static jsid sOnload_id;
  static jsid sOnpopstate_id;
  static jsid sOnbeforeunload_id;
  static jsid sOnunload_id;
  static jsid sOnhashchange_id;
  static jsid sOnreadystatechange_id;
  static jsid sOnpageshow_id;
  static jsid sOnpagehide_id;
  static jsid sOnabort_id;
  static jsid sOnerror_id;
  static jsid sOnpaint_id;
  static jsid sOnresize_id;
  static jsid sOnscroll_id;
  static jsid sOndrag_id;
  static jsid sOndragend_id;
  static jsid sOndragenter_id;
  static jsid sOndragleave_id;
  static jsid sOndragover_id;
  static jsid sOndragstart_id;
  static jsid sOndrop_id;
  static jsid sScrollX_id;
  static jsid sScrollY_id;
  static jsid sScrollMaxX_id;
  static jsid sScrollMaxY_id;
  static jsid sOpen_id;
  static jsid sItem_id;
  static jsid sNamedItem_id;
  static jsid sEnumerate_id;
  static jsid sNavigator_id;
  static jsid sDocument_id;
  static jsid sWindow_id;
  static jsid sFrames_id;
  static jsid sSelf_id;
  static jsid sOpener_id;
  static jsid sAll_id;
  static jsid sTags_id;
  static jsid sAddEventListener_id;
  static jsid sBaseURIObject_id;
  static jsid sNodePrincipal_id;
  static jsid sDocumentURIObject_id;
  static jsid sOncopy_id;
  static jsid sOncut_id;
  static jsid sOnpaste_id;
  static jsid sJava_id;
  static jsid sPackages_id;
  static jsid sOnloadstart_id;
  static jsid sOnprogress_id;
  static jsid sOnsuspend_id;
  static jsid sOnemptied_id;
  static jsid sOnstalled_id;
  static jsid sOnplay_id;
  static jsid sOnpause_id;
  static jsid sOnloadedmetadata_id;
  static jsid sOnloadeddata_id;
  static jsid sOnwaiting_id;
  static jsid sOnplaying_id;
  static jsid sOncanplay_id;
  static jsid sOncanplaythrough_id;
  static jsid sOnseeking_id;
  static jsid sOnseeked_id;
  static jsid sOntimeupdate_id;
  static jsid sOnended_id;
  static jsid sOnratechange_id;
  static jsid sOndurationchange_id;
  static jsid sOnvolumechange_id;
  static jsid sOnmessage_id;
  static jsid sOnbeforescriptexecute_id;
  static jsid sOnafterscriptexecute_id;

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

// EventProp scriptable helper, this class should be the base class of
// all objects that should support things like
// obj.onclick=function{...}

class nsEventReceiverSH : public nsDOMGenericSH
{
protected:
  nsEventReceiverSH(nsDOMClassInfoData* aData) : nsDOMGenericSH(aData)
  {
  }

  virtual ~nsEventReceiverSH()
  {
  }

  static PRBool ReallyIsEventName(jsid id, jschar aFirstChar);

  static inline PRBool IsEventName(jsid id)
  {
    NS_ASSERTION(JSID_IS_STRING(id), "Don't pass non-string jsid's here!");

    jschar *str = ::JS_GetStringChars(JSID_TO_STRING(id));

    if (str[0] == 'o' && str[1] == 'n') {
      return ReallyIsEventName(id, str[2]);
    }

    return PR_FALSE;
  }

  nsresult RegisterCompileHandler(nsIXPConnectWrappedNative *wrapper,
                                  JSContext *cx, JSObject *obj, jsid id,
                                  PRBool compile, PRBool remove,
                                  PRBool *did_define);

public:
  NS_IMETHOD NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, jsid id, PRUint32 flags,
                        JSObject **objp, PRBool *_retval);
  NS_IMETHOD SetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp,
                         PRBool *_retval);
  NS_IMETHOD AddProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp, PRBool *_retval);
};

// Simpler than nsEventReceiverSH
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
                         JSObject *obj, jsid id, jsval *vp, PRBool *_retval);

  virtual void PreserveWrapper(nsISupports *aNative);

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsEventTargetSH(aData);
  }
};

// Window scriptable helper

class nsCommonWindowSH : public nsEventReceiverSH
{
protected:
  nsCommonWindowSH(nsDOMClassInfoData *aData) : nsEventReceiverSH(aData)
  {
  }

  virtual ~nsCommonWindowSH()
  {
  }

  static nsresult GlobalResolve(nsGlobalWindow *aWin, JSContext *cx,
                                JSObject *obj, JSString *str,
                                PRBool *did_resolve);

public:
#ifdef DEBUG
  NS_IMETHOD PostCreate(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj)
  {
    nsCOMPtr<nsIScriptGlobalObject> sgo(do_QueryWrappedNative(wrapper));

    NS_ASSERTION(!sgo || sgo->GetGlobalJSObject() == nsnull,
                 "Multiple wrappers created for global object!");

    return NS_OK;
  }
  NS_IMETHOD GetScriptableFlags(PRUint32 *aFlags)
  {
    PRUint32 flags;
    nsresult rv = nsEventReceiverSH::GetScriptableFlags(&flags);
    if (NS_SUCCEEDED(rv)) {
      *aFlags = flags | nsIXPCScriptable::WANT_POSTCREATE;
    }

    return rv;
  }
#endif
  NS_IMETHOD GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp, PRBool *_retval);
  NS_IMETHOD SetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp, PRBool *_retval);
  NS_IMETHOD DelProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp, PRBool *_retval);
  NS_IMETHOD NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, jsid id, PRUint32 flags,
                        JSObject **objp, PRBool *_retval);
  NS_IMETHOD Finalize(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                      JSObject *obj);
  NS_IMETHOD Equality(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                      JSObject * obj, const jsval &val, PRBool *bp);

  static JSBool GlobalScopePolluterNewResolve(JSContext *cx, JSObject *obj,
                                              jsid id, uintN flags,
                                              JSObject **objp);
  static JSBool GlobalScopePolluterGetProperty(JSContext *cx, JSObject *obj,
                                               jsid id, jsval *vp);
  static JSBool SecurityCheckOnSetProp(JSContext *cx, JSObject *obj, jsid id,
                                       jsval *vp);
  static void InvalidateGlobalScopePolluter(JSContext *cx, JSObject *obj);
  static nsresult InstallGlobalScopePolluter(JSContext *cx, JSObject *obj,
                                             nsIHTMLDocument *doc);
};

class nsOuterWindowSH : public nsCommonWindowSH
{
protected:
  nsOuterWindowSH(nsDOMClassInfoData* aData) : nsCommonWindowSH(aData)
  {
  }

  virtual ~nsOuterWindowSH()
  {
  }

  static PRBool sResolving;

public:
  NS_IMETHOD PreCreate(nsISupports *nativeObj, JSContext *cx,
                       JSObject *globalObj, JSObject **parentObj);
  NS_IMETHOD AddProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp, PRBool *_retval);
  NS_IMETHOD NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, jsid id, PRUint32 flags,
                        JSObject **objp, PRBool *_retval);
  NS_IMETHOD NewEnumerate(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                          JSObject *obj, PRUint32 enum_op, jsval *statep,
                          jsid *idp, PRBool *_retval);
  NS_IMETHOD InnerObject(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                         JSObject * obj, JSObject * *_retval);

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsOuterWindowSH(aData);
  }
};

class nsInnerWindowSH : public nsCommonWindowSH
{
protected:
  nsInnerWindowSH(nsDOMClassInfoData* aData) : nsCommonWindowSH(aData)
  {
  }

  virtual ~nsInnerWindowSH()
  {
  }

public:
  NS_IMETHOD PreCreate(nsISupports *nativeObj, JSContext *cx,
                       JSObject *globalObj, JSObject **parentObj);
  // We WANT_ADDPROPERTY, but are content to inherit it from nsEventReceiverSH.
  NS_IMETHOD OuterObject(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                         JSObject * obj, JSObject * *_retval);

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsInnerWindowSH(aData);
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
                         jsval *vp, PRBool *_retval);

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

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsNavigatorSH(aData);
  }
};


// DOM Node helper, this class deals with setting the parent for the
// wrappers

class nsNodeSH : public nsEventReceiverSH
{
protected:
  nsNodeSH(nsDOMClassInfoData* aData) : nsEventReceiverSH(aData)
  {
  }

  virtual ~nsNodeSH()
  {
  }

  // Helper to check whether a capability is enabled
  PRBool IsCapabilityEnabled(const char* aCapability);

  inline PRBool IsPrivilegedScript() {
    return IsCapabilityEnabled("UniversalXPConnect");
  }

  // Helper to define a void property with JSPROP_SHARED; this can do all the
  // work so it's safe to just return whatever it returns.  |obj| is the object
  // we're defining on, |id| is the name of the prop.  This must be a string
  // jsval.  |objp| is the out param if we define successfully.
  nsresult DefineVoidProp(JSContext* cx, JSObject* obj, jsid id,
                          JSObject** objp);

public:
  NS_IMETHOD PreCreate(nsISupports *nativeObj, JSContext *cx,
                       JSObject *globalObj, JSObject **parentObj);
  NS_IMETHOD AddProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp, PRBool *_retval);
  NS_IMETHOD NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, jsid id, PRUint32 flags,
                        JSObject **objp, PRBool *_retval);
  NS_IMETHOD GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp, PRBool *_retval);
  NS_IMETHOD SetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp, PRBool *_retval);
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
  NS_IMETHOD Enumerate(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                       JSObject *obj, PRBool *_retval);

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
                        JSObject **objp, PRBool *_retval);
  NS_IMETHOD Enumerate(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                       JSObject *obj, PRBool *_retval);
  
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
                         JSObject *obj, jsid id, jsval *vp, PRBool *_retval);

private:
  // Not implemented, nothing should create an instance of this class.
  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData);
};


// NodeList scriptable helper
 
class nsNodeListSH : public nsArraySH
{
protected:
  nsNodeListSH(nsDOMClassInfoData* aData) : nsArraySH(aData)
  {
  }

public:
  NS_IMETHOD PreCreate(nsISupports *nativeObj, JSContext *cx,
                       JSObject *globalObj, JSObject **parentObj);

  virtual nsresult GetLength(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                             JSObject *obj, PRUint32 *length);
  virtual nsISupports* GetItemAt(nsISupports *aNative, PRUint32 aIndex,
                                 nsWrapperCache **aCache, nsresult *aResult);
 
  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsNodeListSH(aData);
  }
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

  virtual nsISupports* GetNamedItem(nsISupports *aNative,
                                    const nsAString& aName,
                                    nsWrapperCache **cache,
                                    nsresult *aResult) = 0;

public:
  NS_IMETHOD GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp, PRBool *_retval);

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


// HTMLCollection helper

class nsHTMLCollectionSH : public nsNamedArraySH
{
protected:
  nsHTMLCollectionSH(nsDOMClassInfoData* aData) : nsNamedArraySH(aData)
  {
  }

  virtual ~nsHTMLCollectionSH()
  {
  }

  virtual nsresult GetLength(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                             JSObject *obj, PRUint32 *length);
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
    return new nsHTMLCollectionSH(aData);
  }
};


// ContentList helper

class nsContentListSH : public nsNamedArraySH
{
protected:
  nsContentListSH(nsDOMClassInfoData* aData) : nsNamedArraySH(aData)
  {
  }

public:
  NS_IMETHOD PreCreate(nsISupports *nativeObj, JSContext *cx,
                       JSObject *globalObj, JSObject **parentObj);

  virtual nsresult GetLength(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                             JSObject *obj, PRUint32 *length);
  virtual nsISupports* GetItemAt(nsISupports *aNative, PRUint32 aIndex,
                                 nsWrapperCache **aCache, nsresult *aResult);
  virtual nsISupports* GetNamedItem(nsISupports *aNative,
                                    const nsAString& aName,
                                    nsWrapperCache **cache,
                                    nsresult *aResult);

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsContentListSH(aData);
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
  NS_IMETHOD NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, jsid id, PRUint32 flags,
                        JSObject **objp, PRBool *_retval);
  NS_IMETHOD GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp, PRBool *_retval);
  NS_IMETHOD SetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp, PRBool *_retval);
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

  static JSBool DocumentOpen(JSContext *cx, uintN argc, jsval *vp);
  static JSBool GetDocumentAllNodeList(JSContext *cx, JSObject *obj,
                                       nsDocument *doc,
                                       nsContentList **nodeList);

public:
  static JSBool DocumentAllGetProperty(JSContext *cx, JSObject *obj, jsid id,
                                       jsval *vp);
  static JSBool DocumentAllNewResolve(JSContext *cx, JSObject *obj, jsid id,
                                      uintN flags, JSObject **objp);
  static void ReleaseDocument(JSContext *cx, JSObject *obj);
  static JSBool CallToGetPropMapper(JSContext *cx, uintN argc, jsval *vp);
  static JSBool DocumentAllHelperGetProperty(JSContext *cx, JSObject *obj,
                                             jsid id, jsval *vp);
  static JSBool DocumentAllHelperNewResolve(JSContext *cx, JSObject *obj,
                                            jsid id, uintN flags,
                                            JSObject **objp);
  static JSBool DocumentAllTagsNewResolve(JSContext *cx, JSObject *obj,
                                          jsid id, uintN flags,
                                          JSObject **objp);

  NS_IMETHOD NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, jsid id, PRUint32 flags,
                        JSObject **objp, PRBool *_retval);
  NS_IMETHOD GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp, PRBool *_retval);

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsHTMLDocumentSH(aData);
  }
};


// HTMLBodyElement helper

class nsHTMLBodyElementSH : public nsElementSH
{
protected:
  nsHTMLBodyElementSH(nsDOMClassInfoData* aData) : nsElementSH(aData)
  {
  }

  virtual ~nsHTMLBodyElementSH()
  {
  }

public:
  NS_IMETHOD NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, jsid id, PRUint32 flags,
                        JSObject **objp, PRBool *_retval);

  NS_IMETHOD GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp,
                         PRBool *_retval);

  NS_IMETHOD SetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp, PRBool *_retval);

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsHTMLBodyElementSH(aData);
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

  static nsresult FindNamedItem(nsIForm *aForm, JSString *str,
                                nsISupports **aResult, nsWrapperCache **aCache);

public:
  NS_IMETHOD NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, jsid id, PRUint32 flags,
                        JSObject **objp, PRBool *_retval);
  NS_IMETHOD GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp,
                         PRBool *_retval);

  NS_IMETHOD NewEnumerate(nsIXPConnectWrappedNative *wrapper,
                          JSContext *cx, JSObject *obj,
                          PRUint32 enum_op, jsval *statep,
                          jsid *idp, PRBool *_retval);

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
  NS_IMETHOD GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp,
                         PRBool *_retval);
  NS_IMETHOD SetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp, PRBool *_retval);

  static nsresult SetOption(JSContext *cx, jsval *vp, PRUint32 aIndex,
                            nsIDOMNSHTMLOptionCollection *aOptCollection);

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
                                          nsIPluginInstance **aResult);

  static nsresult GetPluginJSObject(JSContext *cx, JSObject *obj,
                                    nsIPluginInstance *plugin_inst,
                                    JSObject **plugin_obj,
                                    JSObject **plugin_proto);

public:
  NS_IMETHOD NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, jsid id, PRUint32 flags,
                        JSObject **objp, PRBool *_retval);
  NS_IMETHOD PreCreate(nsISupports *nativeObj, JSContext *cx,
                       JSObject *globalObj, JSObject **parentObj);
  NS_IMETHOD PostCreate(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj);
  NS_IMETHOD GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp, PRBool *_retval);
  NS_IMETHOD SetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp, PRBool *_retval);
  NS_IMETHOD Call(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                  JSObject *obj, PRUint32 argc, jsval *argv, jsval *vp,
                  PRBool *_retval);


  static nsresult SetupProtoChain(nsIXPConnectWrappedNative *wrapper,
                                  JSContext *cx, JSObject *obj);

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsHTMLPluginObjElementSH(aData);
  }
};


// HTMLOptionsCollection helper

class nsHTMLOptionsCollectionSH : public nsHTMLCollectionSH
{
protected:
  nsHTMLOptionsCollectionSH(nsDOMClassInfoData* aData)
    : nsHTMLCollectionSH(aData)
  {
  }

  virtual ~nsHTMLOptionsCollectionSH()
  {
  }

public:
  NS_IMETHOD SetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp, PRBool *_retval);
  
  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsHTMLOptionsCollectionSH(aData);
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
                         JSObject *obj, jsid id, jsval *vp, PRBool *_retval);
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
  NS_IMETHOD GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp, PRBool *_retval);

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


// DOMTokenList scriptable helper

class nsDOMTokenListSH : public nsStringArraySH
{
protected:
  nsDOMTokenListSH(nsDOMClassInfoData* aData) : nsStringArraySH(aData)
  {
  }

  virtual ~nsDOMTokenListSH()
  {
  }

  virtual nsresult GetStringAt(nsISupports *aNative, PRInt32 aIndex,
                               nsAString& aResult);

public:

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsDOMTokenListSH(aData);
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

// ClientRectList helper

class nsClientRectListSH : public nsArraySH
{
protected:
  nsClientRectListSH(nsDOMClassInfoData* aData) : nsArraySH(aData)
  {
  }

  virtual ~nsClientRectListSH()
  {
  }

  virtual nsISupports* GetItemAt(nsISupports *aNative, PRUint32 aIndex,
                                 nsWrapperCache **aCache, nsresult *aResult);

public:
  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsClientRectListSH(aData);
  }
};


// PaintRequestList helper

class nsPaintRequestListSH : public nsArraySH
{
protected:
  nsPaintRequestListSH(nsDOMClassInfoData* aData) : nsArraySH(aData)
  {
  }

  virtual ~nsPaintRequestListSH()
  {
  }

  virtual nsISupports* GetItemAt(nsISupports *aNative, PRUint32 aIndex,
                                 nsWrapperCache **aCache, nsresult *aResult);

public:
  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsPaintRequestListSH(aData);
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

class nsStorageSH : public nsNamedArraySH
{
protected:
  nsStorageSH(nsDOMClassInfoData* aData) : nsNamedArraySH(aData)
  {
  }

  virtual ~nsStorageSH()
  {
  }

  NS_IMETHOD NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, jsid id, PRUint32 flags,
                        JSObject **objp, PRBool *_retval);
  NS_IMETHOD SetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp, PRBool *_retval);
  NS_IMETHOD DelProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp, PRBool *_retval);
  NS_IMETHOD NewEnumerate(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                          JSObject *obj, PRUint32 enum_op, jsval *statep,
                          jsid *idp, PRBool *_retval);

  virtual nsISupports* GetItemAt(nsISupports *aNative, PRUint32 aIndex,
                                 nsWrapperCache **aCache, nsresult *aResult)
  {
    return nsnull;
  }
  // Override nsNamedArraySH::GetNamedItem()
  virtual nsISupports* GetNamedItem(nsISupports *aNative,
                                    const nsAString& aName,
                                    nsWrapperCache **cache,
                                    nsresult *aResult);

public:
  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsStorageSH(aData);
  }
};


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
                        JSObject **objp, PRBool *_retval);
  NS_IMETHOD SetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp, PRBool *_retval);
  NS_IMETHOD GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp, PRBool *_retval);
  NS_IMETHOD DelProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid id, jsval *vp, PRBool *_retval);
  NS_IMETHOD NewEnumerate(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                          JSObject *obj, PRUint32 enum_op, jsval *statep,
                          jsid *idp, PRBool *_retval);

public:
  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsStorage2SH(aData);
  }
};

class nsStorageListSH : public nsNamedArraySH
{
protected:
  nsStorageListSH(nsDOMClassInfoData* aData) : nsNamedArraySH(aData)
  {
  }

  virtual ~nsStorageListSH()
  {
  }

  virtual nsISupports* GetItemAt(nsISupports *aNative, PRUint32 aIndex,
                                 nsWrapperCache **aCache, nsresult *aResult)
  {
    return nsnull;
  }
  // Override nsNamedArraySH::GetNamedItem()
  virtual nsISupports* GetNamedItem(nsISupports *aNative,
                                    const nsAString& aName,
                                    nsWrapperCache **cache,
                                    nsresult *aResult);

public:
  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsStorageListSH(aData);
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
  NS_IMETHOD PostCreatePrototype(JSContext * cx, JSObject * proto)
  {
    return NS_OK;
  }
  NS_IMETHOD Call(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                  JSObject *obj, PRUint32 argc, jsval *argv, jsval *vp,
                  PRBool *_retval);

  NS_IMETHOD Construct(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                       JSObject *obj, PRUint32 argc, jsval *argv,
                       jsval *vp, PRBool *_retval);

  NS_IMETHOD HasInstance(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, const jsval &val, PRBool *bp,
                         PRBool *_retval);

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

class nsFileListSH : public nsArraySH
{
protected:
  nsFileListSH(nsDOMClassInfoData *aData) : nsArraySH(aData)
  {
  }

  virtual ~nsFileListSH()
  {
  }

  virtual nsISupports* GetItemAt(nsISupports *aNative, PRUint32 aIndex,
                                 nsWrapperCache **aCache, nsresult *aResult);

public:
  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsFileListSH(aData);
  }
};

#endif /* nsDOMClassInfo_h___ */
