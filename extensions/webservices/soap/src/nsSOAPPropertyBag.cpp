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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsString.h"
#include "nsSOAPPropertyBag.h"
#include "nsIXPConnect.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsSupportsArray.h"
#include "nsHashtable.h"
#include "jsapi.h"
#include "nsIXPCScriptable.h"

class nsSOAPPropertyBagEnumerator;
class nsSOAPPropertyBag:public nsIPropertyBag, public nsIXPCScriptable {
public:
  nsSOAPPropertyBag();
  virtual ~ nsSOAPPropertyBag();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROPERTYBAG
  NS_DECL_NSIXPCSCRIPTABLE
  NS_IMETHOD SetProperty(const nsAString & aName, nsIVariant * aValue);

protected:
   nsSupportsHashtable * mProperties;

  friend class nsSOAPPropertyBagEnumerator;
};


class nsSOAPProperty:public nsIProperty {
public:
  nsSOAPProperty(const nsAString & aName, nsIVariant * aValue);
  virtual ~ nsSOAPProperty();
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROPERTY

protected:
  nsString mName;
  nsCOMPtr < nsIVariant > mValue;
};

class nsSOAPPropertyBagEnumerator:public nsISimpleEnumerator {
public:
  NS_DECL_ISUPPORTS
      // nsISimpleEnumerator methods:
  NS_DECL_NSISIMPLEENUMERATOR
      // nsSOAPPropertyBagEnumerator methods:
  nsSOAPPropertyBagEnumerator(nsSOAPPropertyBag * aPropertyBag);
  virtual ~nsSOAPPropertyBagEnumerator();

protected:
  nsCOMPtr < nsSupportsArray > mProperties;
  PRUint32 mCurrent;
};
NS_IMPL_ISUPPORTS2_CI(nsSOAPPropertyBag, nsIPropertyBag, nsIXPCScriptable) 
nsSOAPPropertyBag::nsSOAPPropertyBag():mProperties(new nsSupportsHashtable)
{
  NS_INIT_ISUPPORTS();

  /* property initializers and constructor code */
}

nsSOAPPropertyBag::~nsSOAPPropertyBag()
{
  /* destructor code */
  delete mProperties;
}

/* nsIVariant getProperty (in AString name); */
NS_IMETHODIMP
    nsSOAPPropertyBag::GetProperty(const nsAString & aName,
				   nsIVariant ** _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  nsStringKey nameKey(aName);
  *_retval = NS_STATIC_CAST(nsIVariant *, mProperties->Get(&nameKey));
  return *_retval ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
    nsSOAPPropertyBag::SetProperty(const nsAString & aName,
				   nsIVariant * aValue)
{
  NS_ENSURE_ARG_POINTER(&aName);
  NS_ENSURE_ARG_POINTER(aValue);
  nsStringKey nameKey(aName);
  return mProperties->Put(&nameKey, aValue);
}

// The nsIXPCScriptable map declaration that will generate stubs for us...
#define XPC_MAP_CLASSNAME           nsSOAPPropertyBag
#define XPC_MAP_QUOTED_CLASSNAME   "SOAPPropertyBag"
#define                             XPC_MAP_WANT_GETPROPERTY
#define XPC_MAP_FLAGS               0
#include "xpc_map_end.h"	/* This will #undef the above */

NS_IMETHODIMP
    nsSOAPPropertyBag::GetProperty(nsIXPConnectWrappedNative * wrapper,
				   JSContext * cx, JSObject * obj,
				   jsval id, jsval * vp, PRBool * _retval)
{
  if (JSVAL_IS_STRING(id)) {
    JSString *str = JSVAL_TO_STRING(id);
    const PRUnichar *name = NS_REINTERPRET_CAST(const PRUnichar *,
						JS_GetStringChars(str));
    nsDependentString namestr(name);
    nsStringKey nameKey(namestr);
    nsCOMPtr < nsIVariant > value =
	dont_AddRef(NS_STATIC_CAST
		    (nsIVariant *, mProperties->Get(&nameKey)));
    if (value == nsnull)
      return NS_OK;
    void *mark;
    jsval *argv = JS_PushArguments(cx, &mark, "%iv", value.get());
    *vp = *argv;
    JS_PopArguments(cx, mark);
  }
  return NS_OK;
}
PRBool PropertyBagEnumFunc(nsHashKey * aKey, void *aData, void *aClosure)
{
  nsISupportsArray *properties =
      NS_STATIC_CAST(nsISupportsArray *, aClosure);
  nsAutoString name(NS_STATIC_CAST(nsStringKey *, aKey)->GetString());
  properties->
      AppendElement(new
		    nsSOAPProperty(name,
				   NS_STATIC_CAST(nsIVariant *, aData)));
  return PR_TRUE;
}

nsSOAPPropertyBagEnumerator::nsSOAPPropertyBagEnumerator(nsSOAPPropertyBag * aPropertyBag):mProperties(new nsSupportsArray()),
    mCurrent
    (0)
{
  NS_INIT_REFCNT();
  aPropertyBag->mProperties->Enumerate(&PropertyBagEnumFunc, mProperties);
}

nsSOAPPropertyBagEnumerator::~nsSOAPPropertyBagEnumerator()
{
}

NS_IMPL_ISUPPORTS1_CI(nsSOAPPropertyBagEnumerator, nsISimpleEnumerator)
NS_IMETHODIMP nsSOAPPropertyBagEnumerator::GetNext(nsISupports ** aItem)
{
  NS_ENSURE_ARG_POINTER(aItem);
  PRUint32 count;
  mProperties->Count(&count);
  if (mCurrent < count) {
    *aItem = mProperties->ElementAt(mCurrent++);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
    nsSOAPPropertyBagEnumerator::HasMoreElements(PRBool * aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  PRUint32 count;
  mProperties->Count(&count);
  *aResult = mCurrent < count;
  return NS_OK;
}

NS_IMPL_ISUPPORTS1_CI(nsSOAPProperty, nsIProperty)

nsSOAPProperty::nsSOAPProperty(const nsAString & aName,
		 nsIVariant * aValue):mName(aName), mValue(aValue) {
  NS_INIT_REFCNT();
}
nsSOAPProperty::~nsSOAPProperty()
{
}

NS_IMETHODIMP nsSOAPProperty::GetName(nsAString & _retval) {
  NS_ENSURE_ARG_POINTER(&_retval);
  _retval.Assign(mName);
  return NS_OK;
}
NS_IMETHODIMP nsSOAPProperty::GetValue(nsIVariant** _retval) {
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = mValue;
  NS_ADDREF(*_retval);
  return NS_OK;
}

/* readonly attribute nsISimpleEnumerator enumerator; */
NS_IMETHODIMP
    nsSOAPPropertyBag::GetEnumerator(nsISimpleEnumerator * *aEnumerator)
{
  NS_ENSURE_ARG_POINTER(aEnumerator);
  *aEnumerator = new nsSOAPPropertyBagEnumerator(this);
  if (aEnumerator) {
    NS_ADDREF(*aEnumerator);
    return NS_OK;
  }
  return NS_ERROR_OUT_OF_MEMORY;
}

nsSOAPPropertyBagMutator::nsSOAPPropertyBagMutator()
{
  NS_INIT_REFCNT();
  mBag = mSOAPBag = new nsSOAPPropertyBag();
}

nsSOAPPropertyBagMutator::~nsSOAPPropertyBagMutator()
{
}

NS_IMPL_ISUPPORTS1_CI(nsSOAPPropertyBagMutator, nsISOAPPropertyBagMutator)

NS_IMETHODIMP nsSOAPPropertyBagMutator::GetPropertyBag(nsIPropertyBag ** aPropertyBag) {
  NS_ENSURE_ARG_POINTER(aPropertyBag);
  *aPropertyBag = mBag;
  NS_ADDREF(*aPropertyBag);
  return NS_OK;
}

NS_IMETHODIMP
    nsSOAPPropertyBagMutator::AddProperty(const nsAString & aName,
				   nsIVariant * aValue)
{
  NS_ENSURE_ARG_POINTER(&aName);
  NS_ENSURE_ARG_POINTER(aValue);
  return mSOAPBag->SetProperty(aName, aValue);
}
