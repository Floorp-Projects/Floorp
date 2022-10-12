/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIXPConnect_h
#define nsIXPConnect_h

/* The core XPConnect public interfaces. */

#include "nsISupports.h"

#include "jspubtd.h"
#include "js/CompileOptions.h"
#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "xptinfo.h"
#include "nsCOMPtr.h"

class XPCWrappedNative;
class nsXPCWrappedJS;
class nsWrapperCache;

// forward declarations...
class nsIPrincipal;
class nsIVariant;

/***************************************************************************/
#define NS_IXPCONNECTJSOBJECTHOLDER_IID_STR \
  "73e6ff4a-ab99-4d99-ac00-ba39ccb8e4d7"
#define NS_IXPCONNECTJSOBJECTHOLDER_IID              \
  {                                                  \
    0x73e6ff4a, 0xab99, 0x4d99, {                    \
      0xac, 0x00, 0xba, 0x39, 0xcc, 0xb8, 0xe4, 0xd7 \
    }                                                \
  }

class NS_NO_VTABLE nsIXPConnectJSObjectHolder : public nsISupports {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IXPCONNECTJSOBJECTHOLDER_IID)

  virtual JSObject* GetJSObject() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIXPConnectJSObjectHolder,
                              NS_IXPCONNECTJSOBJECTHOLDER_IID)

#define NS_IXPCONNECTWRAPPEDNATIVE_IID_STR \
  "e787be29-db5d-4a45-a3d6-1de1d6b85c30"
#define NS_IXPCONNECTWRAPPEDNATIVE_IID               \
  {                                                  \
    0xe787be29, 0xdb5d, 0x4a45, {                    \
      0xa3, 0xd6, 0x1d, 0xe1, 0xd6, 0xb8, 0x5c, 0x30 \
    }                                                \
  }

class nsIXPConnectWrappedNative : public nsIXPConnectJSObjectHolder {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IXPCONNECTWRAPPEDNATIVE_IID)

  nsresult DebugDump(int16_t depth);

  nsISupports* Native() const { return mIdentity; }

 protected:
  nsCOMPtr<nsISupports> mIdentity;

 private:
  XPCWrappedNative* AsXPCWrappedNative();
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIXPConnectWrappedNative,
                              NS_IXPCONNECTWRAPPEDNATIVE_IID)

#define NS_IXPCONNECTWRAPPEDJS_IID_STR "3a01b0d6-074b-49ed-bac3-08c76366cae4"
#define NS_IXPCONNECTWRAPPEDJS_IID                   \
  {                                                  \
    0x3a01b0d6, 0x074b, 0x49ed, {                    \
      0xba, 0xc3, 0x08, 0xc7, 0x63, 0x66, 0xca, 0xe4 \
    }                                                \
  }

class nsIXPConnectWrappedJS : public nsIXPConnectJSObjectHolder {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IXPCONNECTWRAPPEDJS_IID)

  nsresult GetInterfaceIID(nsIID** aInterfaceIID);

  // Returns the global object for our JS object. If this object is a
  // cross-compartment wrapper, returns the compartment's first global.
  // The global we return is guaranteed to be same-compartment with the
  // object.
  // Note: this matches the GetJSObject() signature.
  JSObject* GetJSObjectGlobal();

  nsresult DebugDump(int16_t depth);

  nsresult AggregatedQueryInterface(const nsIID& aIID, void** aInstancePtr);

 private:
  nsXPCWrappedJS* AsXPCWrappedJS();
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIXPConnectWrappedJS, NS_IXPCONNECTWRAPPEDJS_IID)

#define NS_IXPCONNECTWRAPPEDJSUNMARKGRAY_IID_STR \
  "c02a0ce6-275f-4ea1-9c23-08494898b070"
#define NS_IXPCONNECTWRAPPEDJSUNMARKGRAY_IID         \
  {                                                  \
    0xc02a0ce6, 0x275f, 0x4ea1, {                    \
      0x9c, 0x23, 0x08, 0x49, 0x48, 0x98, 0xb0, 0x70 \
    }                                                \
  }

// Special interface to unmark the internal JSObject.
// QIing to nsIXPConnectWrappedJSUnmarkGray does *not* addref, it only unmarks,
// and QIing to nsIXPConnectWrappedJSUnmarkGray is always supposed to fail.
class NS_NO_VTABLE nsIXPConnectWrappedJSUnmarkGray
    : public nsIXPConnectWrappedJS {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IXPCONNECTWRAPPEDJSUNMARKGRAY_IID)
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIXPConnectWrappedJSUnmarkGray,
                              NS_IXPCONNECTWRAPPEDJSUNMARKGRAY_IID)

/***************************************************************************/

#define NS_IXPCONNECT_IID_STR "768507b5-b981-40c7-8276-f6a1da502a24"
#define NS_IXPCONNECT_IID                            \
  {                                                  \
    0x768507b5, 0xb981, 0x40c7, {                    \
      0x82, 0x76, 0xf6, 0xa1, 0xda, 0x50, 0x2a, 0x24 \
    }                                                \
  }

class nsIXPConnect : public nsISupports {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IXPCONNECT_IID)
  // This gets a non-addref'd pointer.
  static nsIXPConnect* XPConnect();

  /**
   * wrapNative will create a new JSObject or return an existing one.
   *
   * This method now correctly deals with cases where the passed in xpcom
   * object already has an associated JSObject for the cases:
   *  1) The xpcom object has already been wrapped for use in the same scope
   *     as an nsIXPConnectWrappedNative.
   *  2) The xpcom object is in fact a nsIXPConnectWrappedJS and thus already
   *     has an underlying JSObject.
   *
   * It *might* be possible to QueryInterface the nsIXPConnectJSObjectHolder
   * returned by the method into a nsIXPConnectWrappedNative or a
   * nsIXPConnectWrappedJS.
   *
   * This method will never wrap the JSObject involved in an
   * XPCNativeWrapper before returning.
   *
   * Returns:
   *    success:
   *       NS_OK
   *    failure:
   *       NS_ERROR_XPC_BAD_CONVERT_NATIVE
   *       NS_ERROR_FAILURE
   */
  nsresult WrapNative(JSContext* aJSContext, JSObject* aScopeArg,
                      nsISupports* aCOMObj, const nsIID& aIID,
                      JSObject** aRetVal);

  /**
   * Same as wrapNative, but it returns the JSObject in aVal. C++ callers
   * must ensure that aVal is rooted.
   * aIID may be null, it means the same as passing in
   * &NS_GET_IID(nsISupports) but when passing in null certain shortcuts
   * can be taken because we know without comparing IIDs that the caller is
   * asking for an nsISupports wrapper.
   * If aAllowWrapper, then the returned value will be wrapped in the proper
   * type of security wrapper on top of the XPCWrappedNative (if needed).
   * This method doesn't push aJSContext on the context stack, so the caller
   * is required to push it if the top of the context stack is not equal to
   * aJSContext.
   */
  nsresult WrapNativeToJSVal(JSContext* aJSContext, JSObject* aScopeArg,
                             nsISupports* aCOMObj, nsWrapperCache* aCache,
                             const nsIID* aIID, bool aAllowWrapping,
                             JS::MutableHandle<JS::Value> aVal);

  /**
   * wrapJS will yield a new or previously existing xpcom interface pointer
   * to represent the JSObject passed in.
   *
   * This method now correctly deals with cases where the passed in JSObject
   * already has an associated xpcom interface for the cases:
   *  1) The JSObject has already been wrapped as a nsIXPConnectWrappedJS.
   *  2) The JSObject is in fact a nsIXPConnectWrappedNative and thus already
   *     has an underlying xpcom object.
   *  3) The JSObject is of a jsclass which supports getting the nsISupports
   *     from the JSObject directly. This is used for idlc style objects
   *     (e.g. DOM objects).
   *
   * It *might* be possible to QueryInterface the resulting interface pointer
   * to nsIXPConnectWrappedJS.
   *
   * Returns:
   *   success:
   *     NS_OK
   *    failure:
   *       NS_ERROR_XPC_BAD_CONVERT_JS
   *       NS_ERROR_FAILURE
   */
  nsresult WrapJS(JSContext* aJSContext, JSObject* aJSObj, const nsIID& aIID,
                  void** result);

  /**
   * Wraps the given jsval in a nsIVariant and returns the new variant.
   */
  nsresult JSValToVariant(JSContext* cx, JS::Handle<JS::Value> aJSVal,
                          nsIVariant** aResult);

  /**
   * This only succeeds if the JSObject is a nsIXPConnectWrappedNative.
   * A new wrapper is *never* constructed.
   */
  nsresult GetWrappedNativeOfJSObject(JSContext* aJSContext, JSObject* aJSObj,
                                      nsIXPConnectWrappedNative** _retval);

  nsresult DebugDump(int16_t depth);
  nsresult DebugDumpObject(nsISupports* aCOMObj, int16_t depth);
  nsresult DebugDumpJSStack(bool showArgs, bool showLocals, bool showThisProps);

  /**
   * wrapJSAggregatedToNative is just like wrapJS except it is used in cases
   * where the JSObject is also aggregated to some native xpcom Object.
   * At present XBL is the only system that might want to do this.
   *
   * XXX write more!
   *
   * Returns:
   *   success:
   *     NS_OK
   *    failure:
   *       NS_ERROR_XPC_BAD_CONVERT_JS
   *       NS_ERROR_FAILURE
   */
  nsresult WrapJSAggregatedToNative(nsISupports* aOuter, JSContext* aJSContext,
                                    JSObject* aJSObj, const nsIID& aIID,
                                    void** result);

  // Methods added since mozilla 0.6....

  nsresult VariantToJS(JSContext* ctx, JSObject* scope, nsIVariant* value,
                       JS::MutableHandle<JS::Value> _retval);
  nsresult JSToVariant(JSContext* ctx, JS::Handle<JS::Value> value,
                       nsIVariant** _retval);

  /**
   * Create a sandbox for evaluating code in isolation using
   * evalInSandboxObject().
   *
   * @param cx A context to use when creating the sandbox object.
   * @param principal The principal (or NULL to use the null principal)
   *                  to use when evaluating code in this sandbox.
   */
  nsresult CreateSandbox(JSContext* cx, nsIPrincipal* principal,
                         JSObject** _retval);

  /**
   * Evaluate script in a sandbox, completely isolated from all
   * other running scripts.
   *
   * @param source The source of the script to evaluate.
   * @param filename The filename of the script. May be null.
   * @param cx The context to use when setting up the evaluation of
   *           the script. The actual evaluation will happen on a new
   *           temporary context.
   * @param sandbox The sandbox object to evaluate the script in.
   * @return The result of the evaluation as a jsval. If the caller
   *         intends to use the return value from this call the caller
   *         is responsible for rooting the jsval before making a call
   *         to this method.
   */
  nsresult EvalInSandboxObject(const nsAString& source, const char* filename,
                               JSContext* cx, JSObject* sandboxArg,
                               JS::MutableHandle<JS::Value> rval);
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIXPConnect, NS_IXPCONNECT_IID)

#endif  // defined nsIXPConnect_h
