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
 * The Initial Developer of the Original Code is Robert Longson.
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsSVGBoolean.h"

NS_IMPL_ADDREF(nsSVGBoolean::DOMAnimatedBoolean)
NS_IMPL_RELEASE(nsSVGBoolean::DOMAnimatedBoolean)

NS_INTERFACE_MAP_BEGIN(nsSVGBoolean::DOMAnimatedBoolean)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGAnimatedBoolean)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGAnimatedBoolean)
NS_INTERFACE_MAP_END

/* Implementation */

nsresult
nsSVGBoolean::SetBaseValueString(const nsAString &aValueAsString,
                                 nsSVGElement *aSVGElement,
                                 PRBool aDoSetAttr)
{
  PRBool val;

  if (aValueAsString.EqualsLiteral("true"))
    val = PR_TRUE;
  else if (aValueAsString.EqualsLiteral("false"))
    val = PR_FALSE;
  else
    return NS_ERROR_FAILURE;

  mBaseVal = mAnimVal = val;
  return NS_OK;
}

void
nsSVGBoolean::GetBaseValueString(nsAString & aValueAsString)
{
  aValueAsString.Assign(mBaseVal
                        ? NS_LITERAL_STRING("true")
                        : NS_LITERAL_STRING("false"));
}

void
nsSVGBoolean::SetBaseValue(PRBool aValue,
                           nsSVGElement *aSVGElement)
{
  NS_PRECONDITION(aValue == PR_TRUE || aValue == PR_FALSE, "Boolean out of range");

  mAnimVal = mBaseVal = aValue;
  aSVGElement->DidChangeBoolean(mAttrEnum, PR_TRUE);
}

nsresult
nsSVGBoolean::ToDOMAnimatedBoolean(nsIDOMSVGAnimatedBoolean **aResult,
                                   nsSVGElement *aSVGElement)
{
  *aResult = new DOMAnimatedBoolean(this, aSVGElement);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}

