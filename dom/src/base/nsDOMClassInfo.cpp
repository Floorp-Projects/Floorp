/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsDOMClassInfo.h"


nsDOMClassInfo::nsDOMClassInfo()
{
  NS_INIT_REFCNT();
}

nsDOMClassInfo::~nsDOMClassInfo()
{
}

NS_IMPL_ADDREF(nsDOMClassInfo);
NS_IMPL_RELEASE(nsDOMClassInfo);

NS_INTERFACE_MAP_BEGIN(nsDOMClassInfo)
  NS_INTERFACE_MAP_ENTRY(nsIClassInfo)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
nsDOMClassInfo::GetInterfaces(PRUint32 *aCount, nsIID * **aArray)
{
  nsAutoVoidArray void_array;

  GetIIDs(void_array);

  PRInt32 i, count = void_array.Count();

  if (!count) {
    *aCount = 0;
    *aArray = nsnull;

    return NS_OK;
  }

  *aArray = NS_STATIC_CAST(nsIID **, nsMemory::Alloc(count * sizeof(nsIID *)));
  NS_ENSURE_TRUE(*aArray, NS_ERROR_OUT_OF_MEMORY);

  for (i = 0; i < count; i++) {
    *aArray[i] = NS_STATIC_CAST(nsIID *, nsMemory::Alloc(sizeof(nsIID)));

    if (!*aArray[i])
      break;

    *(*aArray[i]) = *NS_STATIC_CAST(nsIID *, void_array.ElementAt(i));
  }

  if (i != count) {
    NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(i, *aArray);

    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMClassInfo::GetHelperForLanguage(PRUint32 language, nsISupports **_retval)
{
  *_retval = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMClassInfo::GetContractID(char * *aContractID)
{
  *aContractID = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMClassInfo::GetClassID(nsCID * *aClassID)
{
  *aClassID = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMClassInfo::GetImplementationLanguage(PRUint32 *aImplLanguage)
{
  *aImplLanguage = LANGUAGE_CPP;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMClassInfo::GetFlags(PRUint32 *aFlags)
{
  *aFlags = 0;

  return NS_OK;
}
