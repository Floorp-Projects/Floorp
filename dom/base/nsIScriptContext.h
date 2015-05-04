/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIScriptContext_h__
#define nsIScriptContext_h__

#include "nscore.h"
#include "nsStringGlue.h"
#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "jspubtd.h"
#include "js/GCAPI.h"

class nsIScriptGlobalObject;

#define NS_ISCRIPTCONTEXT_IID \
{ 0x901f0d5e, 0x217a, 0x45fa, \
  { 0x9a, 0xca, 0x45, 0x0f, 0xe7, 0x2f, 0x10, 0x9a } }

class nsIOffThreadScriptReceiver;

/**
 * It is used by the application to initialize a runtime and run scripts.
 * A script runtime would implement this interface.
 */
class nsIScriptContext : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISCRIPTCONTEXT_IID)

  /**
   * Return the global object.
   *
   **/
  virtual nsIScriptGlobalObject *GetGlobalObject() = 0;

  /**
   * Return the native script context
   *
   **/
  virtual JSContext* GetNativeContext() = 0;

  /**
   * Initialize the context generally. Does not create a global object.
   **/
  virtual nsresult InitContext() = 0;

  /**
   * Check to see if context is as yet intialized. Used to prevent
   * reentrancy issues during the initialization process.
   *
   * @return true if initialized, false if not
   *
   */
  virtual bool IsContextInitialized() = 0;

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
   * Initialize DOM classes on aGlobalObj, always call
   * WillInitializeContext() before calling InitContext(), and always
   * call DidInitializeContext() when a context is fully
   * (successfully) initialized.
   */
  virtual nsresult InitClasses(JS::Handle<JSObject*> aGlobalObj) = 0;

  /**
   * Tell the context we're about to be reinitialize it.
   */
  virtual void WillInitializeContext() = 0;

  /**
   * Tell the context we're done reinitializing it.
   */
  virtual void DidInitializeContext() = 0;

  /**
   * Access the Window Proxy. The setter should only be called by nsGlobalWindow.
   */
  virtual void SetWindowProxy(JS::Handle<JSObject*> aWindowProxy) = 0;
  virtual JSObject* GetWindowProxy() = 0;
  virtual JSObject* GetWindowProxyPreserveColor() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIScriptContext, NS_ISCRIPTCONTEXT_IID)

#define NS_IOFFTHREADSCRIPTRECEIVER_IID \
{0x3a980010, 0x878d, 0x46a9,            \
  {0x93, 0xad, 0xbc, 0xfd, 0xd3, 0x8e, 0xa0, 0xc2}}

class nsIOffThreadScriptReceiver : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IOFFTHREADSCRIPTRECEIVER_IID)

  /**
   * Notify this object that a previous CompileScript call specifying this as
   * aOffThreadReceiver has completed. The script being passed in must be
   * rooted before any call which could trigger GC.
   */
  NS_IMETHOD OnScriptCompileComplete(JSScript* aScript, nsresult aStatus) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIOffThreadScriptReceiver, NS_IOFFTHREADSCRIPTRECEIVER_IID)

#endif // nsIScriptContext_h__
