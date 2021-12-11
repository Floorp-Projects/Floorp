/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIScriptContext_h__
#define nsIScriptContext_h__

#include "nscore.h"
#include "nsString.h"
#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "jspubtd.h"
#include "js/experimental/JSStencil.h"
#include "mozilla/RefPtr.h"

class nsIScriptGlobalObject;

// Must be kept in sync with xpcom/rust/xpcom/src/interfaces/nonidl.rs
#define NS_ISCRIPTCONTEXT_IID                        \
  {                                                  \
    0x54cbe9cf, 0x7282, 0x421a, {                    \
      0x91, 0x6f, 0xd0, 0x70, 0x73, 0xde, 0xb8, 0xc0 \
    }                                                \
  }

class nsIOffThreadScriptReceiver;

/**
 * It is used by the application to initialize a runtime and run scripts.
 * A script runtime would implement this interface.
 */
class nsIScriptContext : public nsISupports {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISCRIPTCONTEXT_IID)

  /**
   * Return the global object.
   *
   **/
  virtual nsIScriptGlobalObject* GetGlobalObject() = 0;

  // SetProperty is suspect and jst believes should not be needed.  Currenly
  // used only for "arguments".
  virtual nsresult SetProperty(JS::Handle<JSObject*> aTarget,
                               const char* aPropName, nsISupports* aVal) = 0;
  /**
   * Called to set/get information if the script context is
   * currently processing a script tag
   */
  virtual bool GetProcessingScriptTag() = 0;
  virtual void SetProcessingScriptTag(bool aResult) = 0;

  /**
   * Initialize DOM classes on aGlobalObj.
   */
  virtual nsresult InitClasses(JS::Handle<JSObject*> aGlobalObj) = 0;

  /**
   * Access the Window Proxy. The setter should only be called by
   * nsGlobalWindow.
   */
  virtual void SetWindowProxy(JS::Handle<JSObject*> aWindowProxy) = 0;
  virtual JSObject* GetWindowProxy() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIScriptContext, NS_ISCRIPTCONTEXT_IID)

#define NS_IOFFTHREADSCRIPTRECEIVER_IID              \
  {                                                  \
    0x3a980010, 0x878d, 0x46a9, {                    \
      0x93, 0xad, 0xbc, 0xfd, 0xd3, 0x8e, 0xa0, 0xc2 \
    }                                                \
  }

class nsIOffThreadScriptReceiver : public nsISupports {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IOFFTHREADSCRIPTRECEIVER_IID)

  /**
   * Notify this object that a previous Compile call specifying this as
   * aOffThreadReceiver has completed. The script being passed in must be
   * rooted before any call which could trigger GC.
   */
  NS_IMETHOD OnScriptCompileComplete(JS::Stencil* aStencil,
                                     nsresult aStatus) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIOffThreadScriptReceiver,
                              NS_IOFFTHREADSCRIPTRECEIVER_IID)

#endif  // nsIScriptContext_h__
