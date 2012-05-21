/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
