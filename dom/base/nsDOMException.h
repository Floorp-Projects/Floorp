/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// We intentionally shadow non-virtual methods, but gcc gets confused.
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
#endif

#include "mozilla/NullPtr.h"
#include "nsError.h"
#include "nsIDOMDOMException.h"
#include "nsIException.h"
#include "nsCOMPtr.h"
#include "xpcprivate.h"

template<typename> class already_AddRefed;

class nsDOMException : public nsXPCException,
                       public nsIDOMDOMException
{
public:
  nsDOMException(nsresult aRv, const char* aMessage,
                 const char* aName, uint16_t aCode);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMDOMEXCEPTION

  // nsIException overrides
  NS_IMETHOD ToString(char **aReturn) MOZ_OVERRIDE;

  // nsWrapperCache overrides
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
    MOZ_OVERRIDE;

  uint16_t Code() const {
    return mCode;
  }

  // Intentionally shadow the nsXPCException version.
  void GetMessageMoz(nsString& retval);
  void GetName(nsString& retval);

protected:

  virtual ~nsDOMException() {}

  // Intentionally shadow the nsXPCException version.
  const char* mName;
  const char* mMessage;

  uint16_t mCode;
};

nsresult
NS_GetNameAndMessageForDOMNSResult(nsresult aNSResult, const char** aName,
                                   const char** aMessage,
                                   uint16_t* aCode = nullptr);

already_AddRefed<nsDOMException>
NS_NewDOMException(nsresult aNSResult);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
