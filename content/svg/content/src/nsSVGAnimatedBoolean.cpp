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
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
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

#include "nsSVGAnimatedBoolean.h"
#include "nsSVGValue.h"
#include "nsISVGValueUtils.h"
#include "nsDOMError.h"
#include "nsContentUtils.h"


////////////////////////////////////////////////////////////////////////
// nsSVGAnimatedBoolean

class nsSVGAnimatedBoolean : public nsIDOMSVGAnimatedBoolean,
                             public nsSVGValue
{
protected:
  friend nsresult NS_NewSVGAnimatedBoolean(nsIDOMSVGAnimatedBoolean** result,
                                           PRBool aBaseVal);
  void Init(PRBool aBaseVal);

public:
  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsIDOMSVGAnimatedBoolean interface:
  NS_DECL_NSIDOMSVGANIMATEDBOOLEAN

  // remainder of nsISVGValue interface:
  NS_IMETHOD SetValueString(const nsAString& aValue);
  NS_IMETHOD GetValueString(nsAString& aValue);

protected:
  PRPackedBool mBaseVal;
};


//----------------------------------------------------------------------
// Implementation

void
nsSVGAnimatedBoolean::Init(PRBool aBaseVal)
{
  mBaseVal = aBaseVal;
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(nsSVGAnimatedBoolean)
NS_IMPL_RELEASE(nsSVGAnimatedBoolean)

NS_INTERFACE_MAP_BEGIN(nsSVGAnimatedBoolean)
  NS_INTERFACE_MAP_ENTRY(nsISVGValue)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGAnimatedBoolean)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGAnimatedBoolean)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISVGValue)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
// nsISVGValue methods:

NS_IMETHODIMP
nsSVGAnimatedBoolean::SetValueString(const nsAString& aValue)
{
  nsresult rv = NS_OK;
  WillModify();

  if (aValue.EqualsLiteral("true"))
    mBaseVal = PR_TRUE;
  else if (aValue.EqualsLiteral("false"))
    mBaseVal = PR_FALSE;
  else
    rv = NS_ERROR_FAILURE;

  DidModify();
  return rv;
}

NS_IMETHODIMP
nsSVGAnimatedBoolean::GetValueString(nsAString& aValue)
{
  aValue.Assign(mBaseVal
                ? NS_LITERAL_STRING("true")
                : NS_LITERAL_STRING("false"));

  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGAnimatedBoolean methods:

/* attribute nsIDOMSVGNumber baseVal; */
NS_IMETHODIMP
nsSVGAnimatedBoolean::GetBaseVal(PRBool *aBaseVal)
{
  *aBaseVal = mBaseVal;
  return NS_OK;
}

/* attribute nsIDOMSVGNumber baseVal; */
NS_IMETHODIMP
nsSVGAnimatedBoolean::SetBaseVal(PRBool aBaseVal)
{
  if (mBaseVal != aBaseVal &&
      (aBaseVal == PR_TRUE || aBaseVal == PR_FALSE)) {
    WillModify();
    mBaseVal = aBaseVal;
    DidModify();
  }
  return NS_OK;
}

/* readonly attribute nsIDOMSVGNumber animVal; */
NS_IMETHODIMP
nsSVGAnimatedBoolean::GetAnimVal(PRBool *aAnimVal)
{
  *aAnimVal = mBaseVal;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// Exported creation functions

nsresult
NS_NewSVGAnimatedBoolean(nsIDOMSVGAnimatedBoolean** aResult,
                         PRBool aBaseVal)
{
  *aResult = nsnull;

  nsSVGAnimatedBoolean* animatedBoolean = new nsSVGAnimatedBoolean();
  if (!animatedBoolean) return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(animatedBoolean);

  animatedBoolean->Init(aBaseVal);

  *aResult = (nsIDOMSVGAnimatedBoolean*) animatedBoolean;

  return NS_OK;
}
