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
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is
 * Crocodile Clips Ltd..
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex@croczilla.com> (original author)
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

#include "nsSVGValue.h"
#include "nsWeakReference.h"

////////////////////////////////////////////////////////////////////////
// nsSVGStringProxyValue implementation
//
// This class is intended to sit between a client and an nsISVGValue object.
// It is used to allow mapped SVG attributes (which store their value in parsed
// form) to assume any string value. This is needed for SVG to work properly with
// XUL templates, where we have XML like this: <svg:circle r="?r"/>.
//
// When nsSVGStringProxyValue::SetValueString() will attempt to call its
// proxied object's SetValueString method. If that call fails, nsSVGStringProxyValue
// will cache the string and return it for subsequent invocations of GetValueString().
// If the call succeeds, however, invocations of GetValueString() will call the
// proxied objects's GetValueString() method.
// If the proxied object's data is changed internally, invocations of GetValueString()
// will also return the proxied object's GetValueString().


class nsSVGStringProxyValue : public nsSVGValue,
                              public nsISVGValueObserver
{
protected:
  friend nsresult
  NS_CreateSVGStringProxyValue(nsISVGValue* proxiedValue, nsISVGValue** aResult);
  
  nsSVGStringProxyValue();
  virtual ~nsSVGStringProxyValue();
  PRBool Init(nsISVGValue* proxiedValue);
  
public:
  NS_DECL_ISUPPORTS

  // nsISVGValue interface: 
  NS_IMETHOD SetValueString(const nsAString& aValue);
  NS_IMETHOD GetValueString(nsAString& aValue);

  // nsISVGValueObserver
  NS_IMETHOD WillModifySVGObservable(nsISVGValue* observable,
                                     modificationType aModType);
  NS_IMETHOD DidModifySVGObservable (nsISVGValue* observable,
                                     modificationType aModType);

  // nsISupportsWeakReference
  // implementation inherited from nsSupportsWeakReference

protected:
  nsString mCachedValue;
  nsCOMPtr<nsISVGValue> mProxiedValue;
  PRPackedBool mUseCachedValue;
};

//----------------------------------------------------------------------

nsresult
NS_CreateSVGStringProxyValue(nsISVGValue* proxiedValue,
                             nsISVGValue** aResult)
{
  *aResult = nsnull;
  
  nsSVGStringProxyValue *sp = new nsSVGStringProxyValue();
  if(!sp) return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(sp);
  if (!sp->Init(proxiedValue)) {
    NS_RELEASE(sp);
    return NS_ERROR_FAILURE;
  }
  
  *aResult = sp;
  return NS_OK;
}

nsSVGStringProxyValue::nsSVGStringProxyValue()
{
#ifdef DEBUG
  printf("nsSVGStringProxyValue CTOR\n");
#endif
}

nsSVGStringProxyValue::~nsSVGStringProxyValue()
{
  mProxiedValue->RemoveObserver(this);
#ifdef DEBUG
  printf("nsSVGStringProxyValue DTOR\n");
#endif
}

PRBool nsSVGStringProxyValue::Init(nsISVGValue* proxiedValue)
{
  mProxiedValue = proxiedValue;
  mProxiedValue->AddObserver(this);
  return PR_TRUE;
}  

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(nsSVGStringProxyValue)
NS_IMPL_RELEASE(nsSVGStringProxyValue)

NS_INTERFACE_MAP_BEGIN(nsSVGStringProxyValue)
  NS_INTERFACE_MAP_ENTRY(nsISVGValue)
  NS_INTERFACE_MAP_ENTRY(nsISVGValueObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISVGValue)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
// nsISVGValue methods:

NS_IMETHODIMP
nsSVGStringProxyValue::SetValueString(const nsAString& aValue)
{
#ifdef DEBUG
  printf("nsSVGStringProxyValue(%p)::SetValueString(%s)\n", this, NS_ConvertUTF16toUTF8(aValue).get());
#endif
  if (NS_FAILED(mProxiedValue->SetValueString(aValue))) {
#ifdef DEBUG
    printf("  -> call failed, now using cached value\n");
#endif
    mUseCachedValue = PR_TRUE; // mUseCachedValue will be reset in DidModifySVGObservable, 
                               // should the inner object ever change state internally
    mCachedValue = aValue;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSVGStringProxyValue::GetValueString(nsAString& aValue)
{
  if (!mUseCachedValue)
    return mProxiedValue->GetValueString(aValue);

  aValue = mCachedValue;
  return NS_OK;
}


//----------------------------------------------------------------------
// nsISVGValueObserver methods

NS_IMETHODIMP
nsSVGStringProxyValue::WillModifySVGObservable(nsISVGValue* observable,
                                               modificationType aModType)
{
  WillModify(aModType);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGStringProxyValue::DidModifySVGObservable (nsISVGValue* observable,
                                               modificationType aModType)
{
  // Our internal proxied object has set its state internally.
  // Make sure its new value takes priority over our cached string:
  mUseCachedValue = PR_FALSE;
  
  DidModify(aModType);
  return NS_OK;
}
