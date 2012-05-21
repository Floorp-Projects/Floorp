/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIBaseDOMException_h___
#define nsIBaseDOMException_h___

#include "nsIDOMClassInfo.h"

// {1f13b201-39fa-11d6-a7f2-df501ff820dc}
#define NS_BASE_DOM_EXCEPTION_CID  \
{ 0x1f13b201, 0x39fa, 0x11d6, \
{ 0xa7, 0xf2, 0xdf, 0x50, 0x1f, 0xf8, 0x20, 0xdc } }

// {731d9701-39f8-11d6-a7f2-b39073384c9c}
#define NS_IBASEDOMEXCEPTION_IID  \
{ 0xb33afd76, 0x5531, 0x423b, \
{ 0x99, 0x42, 0x90, 0x69, 0xf0, 0x9a, 0x3f, 0x5c } }

class nsIBaseDOMException : public nsISupports {
public:  
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IBASEDOMEXCEPTION_IID)

  NS_IMETHOD Init(nsresult aNSResult, const char* aName,
                  const char* aMessage, PRUint16 aCode,
                  nsIException* aDefaultException) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIBaseDOMException, NS_IBASEDOMEXCEPTION_IID)

#define IMPL_DOM_EXCEPTION_HEAD(classname, ifname)                           \
class classname : public nsIException,                                       \
                  public ifname                                              \
{                                                                            \
public:                                                                      \
  classname(nsIException* aInner);                                           \
  virtual ~classname();                                                      \
                                                                             \
  NS_DECL_ISUPPORTS                                                          \
  NS_FORWARD_NSIEXCEPTION(mBase->)

// Note: the exception implemented by this macro doesn't free the pointers
//       it gets from the mapping_function and assumes they will be valid
//       as long as the exception object is alive.

#define IMPL_DOM_EXCEPTION_TAIL(classname, ifname, domname, module,          \
                                mapping_function)                            \
private:                                                                     \
  nsCOMPtr<nsIException> mBase;                                              \
};                                                                           \
                                                                             \
classname::classname(nsIException* aInner) : mBase(aInner)                   \
{                                                                            \
}                                                                            \
classname::~classname() {}                                                   \
                                                                             \
NS_IMPL_ADDREF(classname)                                                    \
NS_IMPL_RELEASE(classname)                                                   \
NS_INTERFACE_MAP_BEGIN(classname)                                            \
  NS_INTERFACE_MAP_ENTRY(nsIException)                                       \
  NS_INTERFACE_MAP_ENTRY(ifname)                                             \
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIException)                \
  NS_INTERFACE_MAP_ENTRY_EXTERNAL_DOM_CLASSINFO(domname)                     \
NS_INTERFACE_MAP_END                                                         \
                                                                             \
NS_DEFINE_CID(kBaseDOMException_CID_##domname, NS_BASE_DOM_EXCEPTION_CID);   \
                                                                             \
nsresult                                                                     \
NS_New##domname(nsresult aNSResult, nsIException* aDefaultException,         \
                nsIException** aException)                                   \
{                                                                            \
  if (!(NS_ERROR_GET_MODULE(aNSResult) == module)) {                         \
    NS_WARNING("Trying to create an exception for the wrong error module."); \
    return NS_ERROR_FAILURE;                                                 \
  }                                                                          \
  const char* name;                                                          \
  const char* message;                                                       \
  mapping_function(aNSResult, &name, &message);                              \
  nsCOMPtr<nsIBaseDOMException> baseException =                              \
    do_CreateInstance(kBaseDOMException_CID_##domname);                      \
  NS_ENSURE_TRUE(baseException, NS_ERROR_OUT_OF_MEMORY);                     \
  baseException->Init(aNSResult, name, message, aDefaultException);          \
  nsCOMPtr<nsIException> inner = do_QueryInterface(baseException);           \
  *aException = new classname(inner);                                        \
  NS_ENSURE_TRUE(*aException, NS_ERROR_OUT_OF_MEMORY);                       \
  NS_ADDREF(*aException);                                                    \
  return NS_OK;                                                              \
}

#endif /* nsIBaseDOMException_h___ */
