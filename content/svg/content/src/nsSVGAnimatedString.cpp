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
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is 
 * Crocodile Clips Ltd..
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
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

#include "nsSVGValue.h"
#include "nsWeakReference.h"
#include "nsSVGAnimatedString.h"


////////////////////////////////////////////////////////////////////////
// nsSVGAnimatedString

class nsSVGAnimatedString : public nsIDOMSVGAnimatedString,
                            public nsSVGValue
{
protected:
  friend nsresult NS_NewSVGAnimatedString(nsIDOMSVGAnimatedString** result);
  nsSVGAnimatedString();
  ~nsSVGAnimatedString();
  void Init();
  
public:
  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsIDOMSVGAnimatedString interface:
  NS_DECL_NSIDOMSVGANIMATEDSTRING

  // remainder of nsISVGValue interface:
  NS_IMETHOD SetValueString(const nsAString& aValue);
  NS_IMETHOD GetValueString(nsAString& aValue);

protected:
  nsString mBaseVal;
};


//----------------------------------------------------------------------
// Implementation

nsSVGAnimatedString::nsSVGAnimatedString()
{
}

nsSVGAnimatedString::~nsSVGAnimatedString()
{
}

void
nsSVGAnimatedString::Init()
{
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(nsSVGAnimatedString)
NS_IMPL_RELEASE(nsSVGAnimatedString)


NS_INTERFACE_MAP_BEGIN(nsSVGAnimatedString)
  NS_INTERFACE_MAP_ENTRY(nsISVGValue)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGAnimatedString)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGAnimatedString)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISVGValue)
NS_INTERFACE_MAP_END
  
//----------------------------------------------------------------------
// nsISVGValue methods:

NS_IMETHODIMP
nsSVGAnimatedString::SetValueString(const nsAString& aValue)
{
  WillModify();
  mBaseVal = aValue;
  DidModify();
  return NS_OK;
}

NS_IMETHODIMP
nsSVGAnimatedString::GetValueString(nsAString& aValue)
{
  aValue = mBaseVal;
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGAnimatedString methods:


/* attribute DOMString baseVal; */
NS_IMETHODIMP
nsSVGAnimatedString::GetBaseVal(nsAString & aBaseVal)
{
  aBaseVal = mBaseVal;
  return NS_OK;
}
NS_IMETHODIMP
nsSVGAnimatedString::SetBaseVal(const nsAString & aBaseVal)
{
  SetValueString(aBaseVal);
  return NS_OK;
}

/* readonly attribute DOMString animVal; */
NS_IMETHODIMP
nsSVGAnimatedString::GetAnimVal(nsAString & aAnimVal)
{
  aAnimVal = mBaseVal;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// Exported creation functions

nsresult
NS_NewSVGAnimatedString(nsIDOMSVGAnimatedString** aResult)
{
  *aResult = nsnull;
  
  nsSVGAnimatedString* animatedString = new nsSVGAnimatedString();
  if(!animatedString) return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(animatedString);

  animatedString->Init();
  
  *aResult = (nsIDOMSVGAnimatedString*) animatedString;
  
  return NS_OK;
}
