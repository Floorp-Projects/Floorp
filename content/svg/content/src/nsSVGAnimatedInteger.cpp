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
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Parts of this file contain code derived from the following files(s)
 * of the Mozilla SVG project (these parts are Copyright (C) by their
 * respective copyright-holders):
 *    content/svg/content/src/nsSVGAnimatedNumber.cpp
 *
 * Contributor(s):
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

#include "nsSVGAnimatedInteger.h"
#include "nsTextFormatter.h"
#include "prdtoa.h"
#include "nsSVGValue.h"
#include "nsISVGValueUtils.h"
#include "nsDOMError.h"
#include "nsContentUtils.h"

////////////////////////////////////////////////////////////////////////
// nsSVGAnimatedInteger

class nsSVGAnimatedInteger : public nsIDOMSVGAnimatedInteger,
                             public nsSVGValue
{
protected:
  friend nsresult NS_NewSVGAnimatedInteger(nsIDOMSVGAnimatedInteger** result,
                                          PRInt32 aBaseVal);
  nsSVGAnimatedInteger();
  ~nsSVGAnimatedInteger();
  void Init(PRInt32 aBaseVal);
  
public:
  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsIDOMSVGAnimatedInteger interface:
  NS_DECL_NSIDOMSVGANIMATEDINTEGER

  // remainder of nsISVGValue interface:
  NS_IMETHOD SetValueString(const nsAString& aValue);
  NS_IMETHOD GetValueString(nsAString& aValue);

protected:
  PRInt32 mBaseVal;
};



//----------------------------------------------------------------------
// Implementation

nsSVGAnimatedInteger::nsSVGAnimatedInteger()
{
}

nsSVGAnimatedInteger::~nsSVGAnimatedInteger()
{
}

void
nsSVGAnimatedInteger::Init(PRInt32 aBaseVal)
{
  mBaseVal = aBaseVal;
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(nsSVGAnimatedInteger)
NS_IMPL_RELEASE(nsSVGAnimatedInteger)


NS_INTERFACE_MAP_BEGIN(nsSVGAnimatedInteger)
  NS_INTERFACE_MAP_ENTRY(nsISVGValue)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGAnimatedInteger)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGAnimatedInteger)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISVGValue)
NS_INTERFACE_MAP_END
  
//----------------------------------------------------------------------
// nsISVGValue methods:

NS_IMETHODIMP
nsSVGAnimatedInteger::SetValueString(const nsAString& aValue)
{
  nsresult rv = NS_OK;
  WillModify();
  
  char *str = ToNewCString(aValue);

  if (*str) {
    char *tmp = str;

    // check if string is well formed

    if (*tmp != '-' &&
        *tmp != '+' &&
        !isdigit(*tmp))
      rv = NS_ERROR_FAILURE;

    tmp++;

    while (*tmp) {
      if (!isdigit(*str)) {
        rv = NS_ERROR_FAILURE;
        break;
      }
    }

    if (NS_SUCCEEDED(rv))
      sscanf(str, "%d", &mBaseVal);
  }
  nsMemory::Free(str);
  DidModify();
  return rv;
}

NS_IMETHODIMP
nsSVGAnimatedInteger::GetValueString(nsAString& aValue)
{
  aValue.Truncate();

  PRUnichar buf[24];
  nsTextFormatter::snprintf(buf, sizeof(buf)/sizeof(PRUnichar),
                            NS_LITERAL_STRING("%d").get(),
                            mBaseVal);
  aValue.Append(buf);
  
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGAnimatedInteger methods:

/* attribute nsIDOMSVGNumber baseVal; */
NS_IMETHODIMP
nsSVGAnimatedInteger::GetBaseVal(PRInt32 *aBaseVal)
{
  *aBaseVal = mBaseVal;
  return NS_OK;
}

/* attribute nsIDOMSVGNumber baseVal; */
NS_IMETHODIMP
nsSVGAnimatedInteger::SetBaseVal(PRInt32 aBaseVal)
{
  WillModify();
  mBaseVal = aBaseVal;
  DidModify();
  return NS_OK;
}

/* readonly attribute nsIDOMSVGNumber animVal; */
NS_IMETHODIMP
nsSVGAnimatedInteger::GetAnimVal(PRInt32 *aAnimVal)
{
  *aAnimVal = mBaseVal;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// Exported creation functions

nsresult
NS_NewSVGAnimatedInteger(nsIDOMSVGAnimatedInteger** aResult,
                         PRInt32 aBaseVal)
{
  *aResult = nsnull;
  
  nsSVGAnimatedInteger* animatedNumber = new nsSVGAnimatedInteger();
  if (!animatedNumber) return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(animatedNumber);

  animatedNumber->Init(aBaseVal);
  
  *aResult = (nsIDOMSVGAnimatedInteger*) animatedNumber;
  
  return NS_OK;
}


