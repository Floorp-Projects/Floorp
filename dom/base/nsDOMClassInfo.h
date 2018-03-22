/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMClassInfo_h___
#define nsDOMClassInfo_h___

#include "mozilla/Attributes.h"
#include "nsDOMClassInfoID.h"
#include "nsIXPCScriptable.h"
#include "nsIScriptGlobalObject.h"
#include "js/Class.h"
#include "js/Id.h"
#include "nsIXPConnect.h"

struct nsGlobalNameStruct;
class nsGlobalWindowInner;
class nsGlobalWindowOuter;

class nsWindowSH;

class nsDOMClassInfo
{
  friend class nsWindowSH;

public:
  static nsresult Init();
  static void ShutDown();

protected:
  static bool sIsInitialized;
};

// A place to hang some static methods that we should really consider
// moving to be nsGlobalWindow member methods.  See bug 1062418.
class nsWindowSH
{
protected:
  static nsresult GlobalResolve(nsGlobalWindowInner *aWin, JSContext *cx,
                                JS::Handle<JSObject*> obj, JS::Handle<jsid> id,
                                JS::MutableHandle<JS::PropertyDescriptor> desc);

  friend class nsGlobalWindowInner;
  friend class nsGlobalWindowOuter;
public:
  static bool NameStructEnabled(JSContext* aCx, nsGlobalWindowInner *aWin,
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

#endif /* nsDOMClassInfo_h___ */
