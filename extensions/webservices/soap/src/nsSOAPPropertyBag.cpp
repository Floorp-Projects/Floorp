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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Harshal Pradhan <hpradhan@hotpop.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsString.h"
#include "nsSOAPPropertyBag.h"
#include "nsCOMArray.h"
#include "nsInterfaceHashtable.h"
#include "nsHashKeys.h"
#include "jsapi.h"
#include "nsIXPCScriptable.h"

class nsSOAPPropertyBagEnumerator;
class nsSOAPPropertyBag:public nsIPropertyBag, public nsIXPCScriptable {
public:
  nsSOAPPropertyBag();
  virtual ~nsSOAPPropertyBag();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROPERTYBAG
  NS_DECL_NSIXPCSCRIPTABLE

  nsresult Init();
  nsresult SetProperty(const nsAString& aName, nsIVariant *aValue);

protected:
  nsInterfaceHashtable<nsStringHashKey, nsIVariant> mProperties;

  friend class nsSOAPPropertyBagEnumerator;
};


class nsSOAPProperty:public nsIProperty {
public:
  nsSOAPProperty(const nsAString & aName, nsIVariant * aValue);
  virtual ~nsSOAPProperty();
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROPERTY

protected:
  nsString mName;
  nsCOMPtr<nsIVariant> mValue;
};

class nsSOAPPropertyBagEnumerator:public nsISimpleEnumerator {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISIMPLEENUMERATOR

  nsSOAPPropertyBagEnumerator();
  virtual ~nsSOAPPropertyBagEnumerator();

  nsresult Init(nsSOAPPropertyBag*);

protected:
  nsCOMArray<nsISupports> mProperties;
  PRUint32 mCurrent;
};

NS_IMPL_ISUPPORTS2_CI(nsSOAPPropertyBag, nsIPropertyBag, nsIXPCScriptable) 

nsSOAPPropertyBag::nsSOAPPropertyBag()
{
}

nsSOAPPropertyBag::~nsSOAPPropertyBag()
{
}

nsresult
nsSOAPPropertyBag::Init()
{
  return mProperties.Init() ? NS_OK : NS_ERROR_FAILURE;
}

/* nsIVariant getProperty (in AString name); */
NS_IMETHODIMP
nsSOAPPropertyBag::GetProperty(const nsAString & aName,
                               nsIVariant ** _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  return mProperties.Get(aName, _retval) ? NS_OK : NS_ERROR_FAILURE;
}

nsresult
nsSOAPPropertyBag::SetProperty(const nsAString & aName,
                               nsIVariant * aValue)
{
  NS_ENSURE_ARG_POINTER(aValue);
  return mProperties.Put(aName, aValue);
}

// The nsIXPCScriptable map declaration that will generate stubs for us...
#define XPC_MAP_CLASSNAME           nsSOAPPropertyBag
#define XPC_MAP_QUOTED_CLASSNAME   "SOAPPropertyBag"
#define                             XPC_MAP_WANT_GETPROPERTY
#define XPC_MAP_FLAGS               0
#include "xpc_map_end.h"        /* This will #undef the above */

NS_IMETHODIMP
nsSOAPPropertyBag::GetProperty(nsIXPConnectWrappedNative * wrapper,
                               JSContext * cx, JSObject * obj,
                               jsval id, jsval * vp, PRBool * _retval)
{
  if (JSVAL_IS_STRING(id)) {
    JSString *str = JSVAL_TO_STRING(id);
    const PRUnichar *name = NS_REINTERPRET_CAST(const PRUnichar *,
                                                JS_GetStringChars(str));
    nsCOMPtr<nsIVariant> value;
    mProperties.Get(nsDependentString(name), getter_AddRefs(value));
    if (!value)
      return NS_OK;
    void *mark;
    jsval *argv = JS_PushArguments(cx, &mark, "%iv", value.get());
    *vp = *argv;
    JS_PopArguments(cx, mark);
  }
  return NS_OK;
}

PLDHashOperator PR_CALLBACK
PropertyBagEnumFunc(const nsAString& aKey, nsIVariant *aData, void *aClosure)
{
  nsCOMArray<nsISupports>* properties =
      NS_STATIC_CAST(nsCOMArray<nsISupports>*, aClosure);
  
  nsSOAPProperty* prop = new nsSOAPProperty(aKey, aData);
  NS_ENSURE_TRUE(prop, PL_DHASH_STOP);

  properties->AppendObject(prop);

  return PL_DHASH_NEXT;
}

nsSOAPPropertyBagEnumerator::nsSOAPPropertyBagEnumerator() 
  : mCurrent(0)
{
}

nsSOAPPropertyBagEnumerator::~nsSOAPPropertyBagEnumerator()
{
}

NS_IMPL_ISUPPORTS1_CI(nsSOAPPropertyBagEnumerator, nsISimpleEnumerator)

nsresult
nsSOAPPropertyBagEnumerator::Init(nsSOAPPropertyBag* aPropertyBag)
{
  PRUint32 count = aPropertyBag->mProperties.EnumerateRead(PropertyBagEnumFunc,
                                                           &mProperties);
  // Check if all the properties got copied to the array.
  if (count != aPropertyBag->mProperties.Count()) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
  
}

NS_IMETHODIMP
nsSOAPPropertyBagEnumerator::GetNext(nsISupports ** aItem)
{
  NS_ENSURE_ARG_POINTER(aItem);
  PRUint32 count = mProperties.Count();
  if (mCurrent < count) {
    NS_ADDREF(*aItem = mProperties[mCurrent++]);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSOAPPropertyBagEnumerator::HasMoreElements(PRBool * aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = mCurrent < (PRUint32) mProperties.Count();
  return NS_OK;
}

NS_IMPL_ISUPPORTS1_CI(nsSOAPProperty, nsIProperty)

nsSOAPProperty::nsSOAPProperty(const nsAString& aName, nsIVariant *aValue) 
  : mName(aName), mValue(aValue) 
{
}

nsSOAPProperty::~nsSOAPProperty()
{
}

NS_IMETHODIMP
nsSOAPProperty::GetName(nsAString& _retval)
{
  _retval.Assign(mName);
  return NS_OK;
}

NS_IMETHODIMP
nsSOAPProperty::GetValue(nsIVariant** _retval)
{
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

  nsRefPtr<nsSOAPPropertyBagEnumerator> enumerator = new nsSOAPPropertyBagEnumerator();
  NS_ENSURE_TRUE(enumerator, NS_ERROR_OUT_OF_MEMORY);

  if (!enumerator->Init(this)) {
    return NS_ERROR_FAILURE;
  }
  
  NS_ADDREF(*aEnumerator = enumerator);

  return NS_OK;
}

nsSOAPPropertyBagMutator::nsSOAPPropertyBagMutator()
{
}

nsSOAPPropertyBagMutator::~nsSOAPPropertyBagMutator()
{
}

NS_IMPL_ISUPPORTS1_CI(nsSOAPPropertyBagMutator, nsISOAPPropertyBagMutator)

nsresult
nsSOAPPropertyBagMutator::Init()
{
  mSOAPBag = new nsSOAPPropertyBag();
  NS_ENSURE_TRUE(mSOAPBag, NS_ERROR_OUT_OF_MEMORY);
  
  return mSOAPBag->Init();
}

NS_IMETHODIMP
nsSOAPPropertyBagMutator::GetPropertyBag(nsIPropertyBag ** aPropertyBag)
{
  NS_ENSURE_ARG_POINTER(aPropertyBag);

  NS_ADDREF(*aPropertyBag = mSOAPBag);
  return NS_OK;
}

NS_IMETHODIMP
nsSOAPPropertyBagMutator::AddProperty(const nsAString& aName,
                                      nsIVariant *aValue)
{
  NS_ENSURE_ARG_POINTER(aValue);

  return mSOAPBag->SetProperty(aName, aValue);
}
