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
 * Scooter Morris
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Scooter Morris <scootermorris@comcast.net> (original author)
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

#include "nsSVGAnimatedNumber.h"
#include "nsTextFormatter.h"
#include "prdtoa.h"
#include "nsSVGValue.h"
#include "nsISVGValueUtils.h"
#include "nsDOMError.h"


////////////////////////////////////////////////////////////////////////
// nsSVGAnimatedNumber

class nsSVGAnimatedNumber : public nsIDOMSVGAnimatedNumber,
                            public nsSVGValue
{
protected:
  friend nsresult NS_NewSVGAnimatedNumber(nsIDOMSVGAnimatedNumber** result,
                                          float aBaseVal);
  nsSVGAnimatedNumber();
  ~nsSVGAnimatedNumber();
  void Init(float aBaseVal);
  
public:
  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsIDOMSVGAnimatedNumber interface:
  NS_DECL_NSIDOMSVGANIMATEDNUMBER

  // remainder of nsISVGValue interface:
  NS_IMETHOD SetValueString(const nsAString& aValue);
  NS_IMETHOD GetValueString(nsAString& aValue);

protected:
  float mBaseVal;
};



//----------------------------------------------------------------------
// Implementation

nsSVGAnimatedNumber::nsSVGAnimatedNumber()
{
}

nsSVGAnimatedNumber::~nsSVGAnimatedNumber()
{
}

void
nsSVGAnimatedNumber::Init(float aBaseVal)
{
  mBaseVal = aBaseVal;
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(nsSVGAnimatedNumber)
NS_IMPL_RELEASE(nsSVGAnimatedNumber)


NS_INTERFACE_MAP_BEGIN(nsSVGAnimatedNumber)
  NS_INTERFACE_MAP_ENTRY(nsISVGValue)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGAnimatedNumber)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGAnimatedNumber)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISVGValue)
NS_INTERFACE_MAP_END
  
//----------------------------------------------------------------------
// nsISVGValue methods:

NS_IMETHODIMP
nsSVGAnimatedNumber::SetValueString(const nsAString& aValue)
{
  nsresult rv = NS_OK;
  WillModify();
  
  char *str = ToNewCString(aValue);

  if (*str) {
    char *rest;
    double value = PR_strtod(str, &rest);
    if (rest && rest!=str) {
      if (*rest=='%') {
        mBaseVal = (float)(value/100.0);
        rest++;
      } else {
        mBaseVal = (float)(value);
      }
      // skip trailing spaces
      while (*rest && isspace(*rest))
        ++rest;

      // check to see if there is trailing stuff...
      if (*rest != '\0') {
        rv = NS_ERROR_FAILURE;
        NS_ERROR("trailing data in number value");
      }
    } else {
      rv = NS_ERROR_FAILURE;
      // no number
    }
  }
  nsMemory::Free(str);
  DidModify();
  return rv;
}

NS_IMETHODIMP
nsSVGAnimatedNumber::GetValueString(nsAString& aValue)
{
  aValue.Truncate();

  PRUnichar buf[24];
  nsTextFormatter::snprintf(buf, sizeof(buf)/sizeof(PRUnichar),
                            NS_LITERAL_STRING("%g").get(),
                            (double)mBaseVal);
  aValue.Append(buf);
  
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGAnimatedNumber methods:

/* attribute nsIDOMSVGNumber baseVal; */
NS_IMETHODIMP
nsSVGAnimatedNumber::GetBaseVal(float *aBaseVal)
{
  *aBaseVal = mBaseVal;
  return NS_OK;
}

/* attribute nsIDOMSVGNumber baseVal; */
NS_IMETHODIMP
nsSVGAnimatedNumber::SetBaseVal(float aBaseVal)
{
  WillModify();
  mBaseVal = aBaseVal;
  DidModify();
  return NS_OK;
}

/* readonly attribute nsIDOMSVGNumber animVal; */
NS_IMETHODIMP
nsSVGAnimatedNumber::GetAnimVal(float *aAnimVal)
{
  *aAnimVal = mBaseVal;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// Exported creation functions

nsresult
NS_NewSVGAnimatedNumber(nsIDOMSVGAnimatedNumber** aResult,
                        float aBaseVal)
{
  *aResult = nsnull;
  
  nsSVGAnimatedNumber* animatedNumber = new nsSVGAnimatedNumber();
  if (!animatedNumber) return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(animatedNumber);

  animatedNumber->Init(aBaseVal);
  
  *aResult = (nsIDOMSVGAnimatedNumber*) animatedNumber;
  
  return NS_OK;
}


