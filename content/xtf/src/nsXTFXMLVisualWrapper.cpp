/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
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
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Alex Fritze <alex@croczilla.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#include "nsCOMPtr.h"
#include "nsXTFVisualWrapper.h"
#include "nsIXTFXMLVisual.h"
#include "nsXTFWeakTearoff.h"
#include "nsIXTFXMLVisualWrapper.h"

typedef nsXTFVisualWrapper nsXTFXMLVisualWrapperBase;

////////////////////////////////////////////////////////////////////////
// nsXTFXMLVisualWrapper class
class nsXTFXMLVisualWrapper : public nsXTFXMLVisualWrapperBase,
                              public nsIXTFXMLVisualWrapper
{
protected:
  friend nsresult
  NS_NewXTFXMLVisualWrapper(nsIXTFXMLVisual* xtfElement,
                            nsINodeInfo* ni,
                            nsIContent** aResult);

  nsXTFXMLVisualWrapper(nsINodeInfo* ni, nsIXTFXMLVisual* xtfElement);
  virtual ~nsXTFXMLVisualWrapper();
  nsresult Init();
  
public:
  // nsISupports interface
  NS_DECL_ISUPPORTS_INHERITED

  // nsIXTFElementWrapperPrivate interface
  virtual PRUint32 GetElementType() { return nsIXTFElement::ELEMENT_TYPE_XML_VISUAL; }
  
  // nsIXTFXMLVisualWrapper interface
  NS_DECL_NSIXTFXMLVISUALWRAPPER

  // nsIXTFElementWrapper interface
  NS_FORWARD_NSIXTFELEMENTWRAPPER(nsXTFXMLVisualWrapperBase::)
  
private:
  virtual nsIXTFElement *GetXTFElement() const { return mXTFElement; }
  virtual nsIXTFVisual *GetXTFVisual() const { return mXTFElement; }
  
  nsCOMPtr<nsIXTFXMLVisual> mXTFElement;
};

//----------------------------------------------------------------------
// implementation:

nsXTFXMLVisualWrapper::nsXTFXMLVisualWrapper(nsINodeInfo* aNodeInfo,
                                             nsIXTFXMLVisual* xtfElement)
    : nsXTFXMLVisualWrapperBase(aNodeInfo), mXTFElement(xtfElement)
{
#ifdef DEBUG
//  printf("nsXTFXMLVisualWrapper CTOR\n");
#endif
  NS_ASSERTION(mXTFElement, "xtfElement is null");
}

nsresult
nsXTFXMLVisualWrapper::Init()
{
  nsXTFXMLVisualWrapperBase::Init();

  // pass a weak wrapper (non base object ref-counted), so that
  // our mXTFElement can safely addref/release.
  nsISupports *weakWrapper=nsnull;
  NS_NewXTFWeakTearoff(NS_GET_IID(nsIXTFXMLVisualWrapper),
                       (nsIXTFXMLVisualWrapper*)this,
                       &weakWrapper);
  if (!weakWrapper) {
    NS_ERROR("could not construct weak wrapper");
    return NS_ERROR_FAILURE;
  }

  mXTFElement->OnCreated((nsIXTFXMLVisualWrapper*)weakWrapper);
  weakWrapper->Release();
  
  return NS_OK;
}

nsXTFXMLVisualWrapper::~nsXTFXMLVisualWrapper()
{
  mXTFElement->OnDestroyed();
  mXTFElement = nsnull;
  
#ifdef DEBUG
//  printf("nsXTFXMLVisualWrapper DTOR\n");
#endif
}

nsresult
NS_NewXTFXMLVisualWrapper(nsIXTFXMLVisual* xtfElement,
                          nsINodeInfo* ni,
                          nsIContent** aResult)
{
  *aResult = nsnull;
  
  if (!xtfElement) {
    NS_ERROR("can't construct an xtf wrapper without an xtf element");
    return NS_ERROR_FAILURE;
  }
  
  nsXTFXMLVisualWrapper* result = new nsXTFXMLVisualWrapper(ni, xtfElement);
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


NS_IMPL_ADDREF_INHERITED(nsXTFXMLVisualWrapper,nsXTFXMLVisualWrapperBase)
NS_IMPL_RELEASE_INHERITED(nsXTFXMLVisualWrapper,nsXTFXMLVisualWrapperBase)

NS_INTERFACE_MAP_BEGIN(nsXTFXMLVisualWrapper)
  NS_INTERFACE_MAP_ENTRY(nsIXTFXMLVisualWrapper)
NS_INTERFACE_MAP_END_INHERITING(nsXTFXMLVisualWrapperBase)

//----------------------------------------------------------------------
// nsIXTFXMLVisualWrapper implementation:

// XXX nothing yet

  



