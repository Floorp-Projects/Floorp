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
 * The Original Code is the Mozilla XTF project.
 *
 * The Initial Developer of the Original Code is
 * Alex Fritze.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex@croczilla.com> (original author)
 *   Olli Pettay <Olli.Pettay@helsinki.fi>
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

#include "nsCOMPtr.h"
#include "nsXTFElementWrapper.h"
#include "nsIXTFBindableElement.h"
#include "nsXTFWeakTearoff.h"
#include "nsIXTFBindableElementWrapper.h"
#include "nsXTFInterfaceAggregator.h"

typedef nsXTFStyledElementWrapper nsXTFBindableElementWrapperBase;

////////////////////////////////////////////////////////////////////////
// nsXTFBindableElementWrapper class
class nsXTFBindableElementWrapper : public nsXTFBindableElementWrapperBase,
                                    public nsIXTFBindableElementWrapper
{
protected:
  friend nsresult NS_NewXTFBindableElementWrapper(nsIXTFBindableElement* xtfElement,
                                                  nsINodeInfo* ni,
                                                  nsIContent** aResult);

  nsXTFBindableElementWrapper(nsINodeInfo* aNodeInfo, nsIXTFBindableElement* xtfElement);
  virtual ~nsXTFBindableElementWrapper();
  nsresult Init();
public:  
  // nsISupports interface
  NS_DECL_ISUPPORTS_INHERITED

  // nsIXTFElementWrapperPrivate interface
  virtual PRUint32 GetElementType() { return nsIXTFElement::ELEMENT_TYPE_BINDABLE; }
  
  // nsIXTFBindableElementWrapper interface
  NS_DECL_NSIXTFBINDABLEELEMENTWRAPPER

  // nsIXTFStyledElementWrapper
  NS_FORWARD_NSIXTFSTYLEDELEMENTWRAPPER(nsXTFStyledElementWrapper::)

  // nsIXTFElementWrapper interface
  NS_FORWARD_NSIXTFELEMENTWRAPPER(nsXTFBindableElementWrapperBase::)

  NS_IMETHOD GetInterfaces(PRUint32 *count, nsIID * **array);
private:
  virtual nsIXTFElement *GetXTFElement() const { return mXTFElement; }
  
  nsCOMPtr<nsIXTFBindableElement> mXTFElement;
};

//----------------------------------------------------------------------
// implementation:

nsXTFBindableElementWrapper::nsXTFBindableElementWrapper(nsINodeInfo* aNodeInfo,
                                                         nsIXTFBindableElement* xtfElement)
: nsXTFBindableElementWrapperBase(aNodeInfo), mXTFElement(xtfElement)
{
#ifdef DEBUG
//  printf("nsXTFBindableElementWrapper CTOR\n");
#endif
  NS_ASSERTION(mXTFElement, "xtfElement is null");
}

nsresult
nsXTFBindableElementWrapper::Init()
{
  nsXTFBindableElementWrapperBase::Init();
  
  // pass a weak wrapper (non base object ref-counted), so that
  // our mXTFElement can safely addref/release.
  nsISupports *weakWrapper = nsnull;
  nsresult rv = NS_NewXTFWeakTearoff(NS_GET_IID(nsIXTFBindableElementWrapper),
                                     (nsIXTFBindableElementWrapper*)this,
                                     &weakWrapper);
  if (!weakWrapper) {
    NS_ERROR("could not construct weak wrapper");
    return rv;
  }
  
  mXTFElement->OnCreated((nsIXTFBindableElementWrapper*)weakWrapper);
  NS_RELEASE(weakWrapper);
  
  return NS_OK;
}

NS_IMETHODIMP 
nsXTFBindableElementWrapper::GetInterfaces(PRUint32 *count, nsIID * **array)
{
  *count = 0;
  *array = nsnull;
  return NS_OK;
}

nsXTFBindableElementWrapper::~nsXTFBindableElementWrapper()
{
  mXTFElement->OnDestroyed();
  mXTFElement = nsnull;
  
#ifdef DEBUG
//  printf("nsXTFBindableElementWrapper DTOR\n");
#endif
}

nsresult
NS_NewXTFBindableElementWrapper(nsIXTFBindableElement* xtfElement,
                               nsINodeInfo* ni,
                               nsIContent** aResult)
{
  *aResult = nsnull;
  
  if (!xtfElement) {
    NS_ERROR("can't construct an xtf wrapper without an xtf element");
    return NS_ERROR_INVALID_ARG;
  }
  
  nsXTFBindableElementWrapper* result = new nsXTFBindableElementWrapper(ni, xtfElement);
  if (!result)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(result);

  nsresult rv = result->Init();

  if (NS_FAILED(rv)) {
    NS_RELEASE(result);
    return rv;
  }
  
  *aResult = result;
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISupports implementation

NS_IMPL_ADDREF_INHERITED(nsXTFBindableElementWrapper, nsXTFBindableElementWrapperBase)
NS_IMPL_RELEASE_INHERITED(nsXTFBindableElementWrapper, nsXTFBindableElementWrapperBase)

NS_IMETHODIMP
nsXTFBindableElementWrapper::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  nsresult rv;
  
  if (aIID.Equals(NS_GET_IID(nsIXTFElementWrapperPrivate))) {
    *aInstancePtr = NS_STATIC_CAST(nsIXTFElementWrapperPrivate*, this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  if(aIID.Equals(NS_GET_IID(nsIXTFElementWrapper))) {
    *aInstancePtr =
      NS_STATIC_CAST(nsIXTFElementWrapper*, 
                     NS_STATIC_CAST(nsXTFBindableElementWrapperBase*, this));

    NS_ADDREF_THIS();
    return NS_OK;
  }

  if (aIID.Equals(NS_GET_IID(nsIXTFBindableElementWrapper))) {
    *aInstancePtr = NS_STATIC_CAST(nsIXTFBindableElementWrapper*, this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  if (aIID.Equals(NS_GET_IID(nsIXTFStyledElementWrapper))) {
    *aInstancePtr = NS_STATIC_CAST(nsIXTFStyledElementWrapper*, this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  // Note, using the nsIClassInfo from nsXMLElement.
  if (NS_SUCCEEDED(rv = nsXTFElementWrapperBase::QueryInterface(aIID, aInstancePtr))) {
    return rv;
  }

  // try to get get the interface from our wrapped element:
  void *innerPtr = nsnull;
  QueryInterfaceInner(aIID, &innerPtr);

  if (innerPtr)
    return NS_NewXTFInterfaceAggregator(aIID,
                                        NS_STATIC_CAST(nsISupports*, innerPtr),
                                        NS_STATIC_CAST(nsIContent*, this),
                                        aInstancePtr);

  return NS_ERROR_NO_INTERFACE;
}

//----------------------------------------------------------------------
// nsIXTFBindableElementWrapper implementation:

// XXX nothing yet

