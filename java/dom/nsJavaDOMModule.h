/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 The contents of this file are subject to the Mozilla Public
 License Version 1.1 (the "License"); you may not use this file
 except in compliance with the License. You may obtain a copy of
 the License at http://www.mozilla.org/MPL/

 Software distributed under the License is distributed on an "AS
 IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 implied. See the License for the specific language governing
 rights and limitations under the License.

 The Original Code is mozilla.org code.

 The Initial Developer of the Original Code is Sun Microsystems,
 Inc. Portions created by Sun are
 Copyright (C) 1999 Sun Microsystems, Inc. All
 Rights Reserved.

 Contributor(s): 
*/

#ifndef __nsJavaDOMModule_h__
#define __nsJavaDOMModule_h__

static NS_DEFINE_CID(kJavaDOMCID, NS_JAVADOM_CID);

class nsJavaDOMModule : public nsIModule
{
public:
  nsJavaDOMModule();
  virtual ~nsJavaDOMModule();

  NS_DECL_ISUPPORTS

  NS_DECL_NSIMODULE

protected:
  nsresult Initialize();
  
  void Shutdown();

  PRBool mInitialized;
  nsCOMPtr<nsIGenericFactory> mFactory;
};

extern "C" NS_EXPORT nsresult NSGetModule(nsIComponentManager *servMgr,
                                          nsIFileSpec* location,
                                          nsIModule** return_cobj);

// Functions used to create new instances of a given object by the
// generic factory.

#define MAKE_CTOR(_name)                                             \
static NS_IMETHODIMP                                                 \
CreateNew##_name(nsISupports* aOuter, REFNSIID aIID, void **aResult) \
{                                                                    \
  if (!aResult) {                                                  \
      return NS_ERROR_INVALID_POINTER;                             \
  }                                                                \
  if (aOuter) {                                                    \
      *aResult = nsnull;                                           \
      return NS_ERROR_NO_AGGREGATION;                              \
  }                                                                \
  nsCOMPtr<nsI##_name> inst;                                       \
  nsresult rv = NS_New##_name(getter_AddRefs(inst));               \
  if (NS_FAILED(rv)) {                                             \
      *aResult = nsnull;                                           \
      return rv;                                                   \
  }                                                                \
  rv = inst->QueryInterface(aIID, aResult);                        \
  if (NS_FAILED(rv)) {                                             \
      *aResult = nsnull;                                           \
  }                                                                \
  return rv;                                                       \
}

#endif /* __nsJavaDOMModule_h__ */
