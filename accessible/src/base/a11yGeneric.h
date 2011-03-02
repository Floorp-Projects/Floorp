/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
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

#ifndef _a11yGeneric_H_
#define _a11yGeneric_H_

#include "nsThreadUtils.h"

// What we want is: NS_INTERFACE_MAP_ENTRY(self) for static IID accessors,
// but some of our classes have an ambiguous base class of nsISupports which
// prevents this from working (the default macro converts it to nsISupports,
// then addrefs it, then returns it). Therefore, we expand the macro here and
// change it so that it works. Yuck.
#define NS_INTERFACE_MAP_STATIC_AMBIGUOUS(_class)                              \
  if (aIID.Equals(NS_GET_IID(_class))) {                                       \
    NS_ADDREF(this);                                                           \
    *aInstancePtr = this;                                                      \
    return NS_OK;                                                              \
  } else

#define NS_OK_DEFUNCT_OBJECT                                                   \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_GENERAL, 0x22)

#define NS_ENSURE_A11Y_SUCCESS(res, ret)                                       \
  PR_BEGIN_MACRO                                                               \
    nsresult __rv = res; /* Don't evaluate |res| more than once */             \
    if (NS_FAILED(__rv)) {                                                     \
      NS_ENSURE_SUCCESS_BODY(res, ret)                                         \
      return ret;                                                              \
    }                                                                          \
    if (__rv == NS_OK_DEFUNCT_OBJECT)                                          \
      return ret;                                                              \
  PR_END_MACRO


////////////////////////////////////////////////////////////////////////////////
// nsRunnable helpers
////////////////////////////////////////////////////////////////////////////////

/**
 * Use NS_DECL_RUNNABLEMETHOD_ macros to declare a runnable class for the given
 * method of the given class. There are three macros:
 *  NS_DECL_RUNNABLEMETHOD(Class, Method)
 *  NS_DECL_RUNNABLEMETHOD_ARG1(Class, Method, Arg1Type)
 *  NS_DECL_RUNNABLEMETHOD_ARG2(Class, Method, Arg1Type, Arg2Type)
 * Note Arg1Type and Arg2Type must be types which keeps the objects alive.
 *
 * Use NS_DISPATCH_RUNNABLEMETHOD_ macros to create an instance of declared
 * runnable class and dispatch it to main thread. Availabe macros are:
 *  NS_DISPATCH_RUNNABLEMETHOD(Method, Object)
 *  NS_DISPATCH_RUNNABLEMETHOD_ARG1(Method, Object, Arg1)
 *  NS_DISPATCH_RUNNABLEMETHOD_ARG2(Method, Object, Arg1, Arg2)
 */

#define NS_DECL_RUNNABLEMETHOD_HELPER(ClassType, Method)                       \
  void Revoke()                                                                \
  {                                                                            \
    NS_IF_RELEASE(mObj);                                                       \
  }                                                                            \
                                                                               \
protected:                                                                     \
  virtual ~nsRunnableMethod_##Method()                                         \
  {                                                                            \
    NS_IF_RELEASE(mObj);                                                       \
  }                                                                            \
                                                                               \
private:                                                                       \
  ClassType *mObj;                                                             \


#define NS_DECL_RUNNABLEMETHOD(ClassType, Method)                              \
class nsRunnableMethod_##Method : public nsRunnable                            \
{                                                                              \
public:                                                                        \
  nsRunnableMethod_##Method(ClassType *aObj) : mObj(aObj)                      \
  {                                                                            \
    NS_IF_ADDREF(mObj);                                                        \
  }                                                                            \
                                                                               \
  NS_IMETHODIMP Run()                                                          \
  {                                                                            \
    if (!mObj)                                                                 \
      return NS_OK;                                                            \
    (mObj-> Method)();                                                         \
    return NS_OK;                                                              \
  }                                                                            \
                                                                               \
  NS_DECL_RUNNABLEMETHOD_HELPER(ClassType, Method)                             \
                                                                               \
};

#define NS_DECL_RUNNABLEMETHOD_ARG1(ClassType, Method, Arg1Type)               \
class nsRunnableMethod_##Method : public nsRunnable                            \
{                                                                              \
public:                                                                        \
  nsRunnableMethod_##Method(ClassType *aObj, Arg1Type aArg1) :                 \
    mObj(aObj), mArg1(aArg1)                                                   \
  {                                                                            \
    NS_IF_ADDREF(mObj);                                                        \
  }                                                                            \
                                                                               \
  NS_IMETHODIMP Run()                                                          \
  {                                                                            \
    if (!mObj)                                                                 \
      return NS_OK;                                                            \
    (mObj-> Method)(mArg1);                                                    \
    return NS_OK;                                                              \
  }                                                                            \
                                                                               \
  NS_DECL_RUNNABLEMETHOD_HELPER(ClassType, Method)                             \
  Arg1Type mArg1;                                                              \
};

#define NS_DECL_RUNNABLEMETHOD_ARG2(ClassType, Method, Arg1Type, Arg2Type)     \
class nsRunnableMethod_##Method : public nsRunnable                            \
{                                                                              \
public:                                                                        \
                                                                               \
  nsRunnableMethod_##Method(ClassType *aObj,                                   \
                            Arg1Type aArg1, Arg2Type aArg2) :                  \
    mObj(aObj), mArg1(aArg1), mArg2(aArg2)                                     \
  {                                                                            \
    NS_IF_ADDREF(mObj);                                                        \
  }                                                                            \
                                                                               \
  NS_IMETHODIMP Run()                                                          \
  {                                                                            \
    if (!mObj)                                                                 \
      return NS_OK;                                                            \
    (mObj-> Method)(mArg1, mArg2);                                             \
    return NS_OK;                                                              \
  }                                                                            \
                                                                               \
  NS_DECL_RUNNABLEMETHOD_HELPER(ClassType, Method)                             \
  Arg1Type mArg1;                                                              \
  Arg2Type mArg2;                                                              \
};

#define NS_DISPATCH_RUNNABLEMETHOD(Method, Obj)                                \
{                                                                              \
  nsCOMPtr<nsIRunnable> runnable =                                             \
    new nsRunnableMethod_##Method(Obj);                                        \
  if (runnable)                                                                \
    NS_DispatchToMainThread(runnable);                                         \
}

#define NS_DISPATCH_RUNNABLEMETHOD_ARG1(Method, Obj, Arg1)                     \
{                                                                              \
  nsCOMPtr<nsIRunnable> runnable =                                             \
    new nsRunnableMethod_##Method(Obj, Arg1);                                  \
  if (runnable)                                                                \
    NS_DispatchToMainThread(runnable);                                         \
}

#define NS_DISPATCH_RUNNABLEMETHOD_ARG2(Method, Obj, Arg1, Arg2)               \
{                                                                              \
  nsCOMPtr<nsIRunnable> runnable =                                             \
    new nsRunnableMethod_##Method(Obj, Arg1, Arg2);                            \
  if (runnable)                                                                \
    NS_DispatchToMainThread(runnable);                                         \
}

#endif
