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
 * IBM Corporation
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Tim Rowley <tor@cs.brown.edu> (original author)
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

#include "nsSVGEnum.h"
#include "nsSVGValue.h"
#include "nsISVGValueUtils.h"
#include "nsWeakReference.h"
#include "nsIAtom.h"

////////////////////////////////////////////////////////////////////////
// nsSVGEnum class

class nsSVGEnum : public nsISVGEnum,
                  public nsSVGValue
{
protected:
  friend nsresult NS_NewSVGEnum(nsISVGEnum** result,
                                PRUint16 value,
                                nsSVGEnumMapping *mapping);
    
  friend nsresult NS_NewSVGEnum(nsISVGEnum** result,
                                const nsAString &value,
                                nsSVGEnumMapping *mapping);
  
  nsSVGEnum(PRUint16 value, nsSVGEnumMapping *mapping);
  nsSVGEnum(nsSVGEnumMapping *mapping);
  virtual ~nsSVGEnum();

public:
  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsISVGEnum interface:
  NS_IMETHOD GetIntegerValue(PRUint16 &value);
  NS_IMETHOD SetIntegerValue(PRUint16 value);
  
  // nsISVGValue interface:
  NS_IMETHOD SetValueString(const nsAString& aValue);
  NS_IMETHOD GetValueString(nsAString& aValue);
  
#ifdef DEBUG_scooter
  void Print_mapping();
#endif
  
protected:
  PRUint16 mValue;
  nsSVGEnumMapping *mMapping;
};


//----------------------------------------------------------------------
// Implementation

#ifdef DEBUG_scooter
void nsSVGEnum::Print_mapping()
{
  nsSVGEnumMapping *tmp = mMapping;
  nsAutoString aStr;
  printf("Print_mapping: mMapping = 0x%x\n", tmp);
  while (tmp->key) {
    (*tmp->key)->ToString(aStr);
    printf ("Print_mapping: %s (%d)\n", NS_ConvertUTF16toUTF8(aStr).get(), tmp->val);
    tmp++;
  }
}
#endif

nsresult
NS_NewSVGEnum(nsISVGEnum** result,
              PRUint16 value,
              nsSVGEnumMapping *mapping)
{
  NS_ASSERTION(mapping, "no mapping");
  nsSVGEnum *pe = new nsSVGEnum(value, mapping);
  if (!pe) return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(pe);
  *result = pe;
  return NS_OK;
}

nsresult
NS_NewSVGEnum(nsISVGEnum** result,
              const nsAString &value,
              nsSVGEnumMapping *mapping)
{
  NS_ASSERTION(mapping, "no mapping");
  *result = nsnull;
  nsSVGEnum *pe = new nsSVGEnum(0, mapping);
  if (!pe) return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(pe);
  if (NS_FAILED(pe->SetValueString(value))) {
    NS_RELEASE(pe);
    return NS_ERROR_FAILURE;
  }
  *result = pe;
  return NS_OK;
}  


nsSVGEnum::nsSVGEnum(PRUint16 value,
                     nsSVGEnumMapping *mapping)
    : mValue(value), mMapping(mapping)
{
}

nsSVGEnum::~nsSVGEnum()
{
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(nsSVGEnum)
NS_IMPL_RELEASE(nsSVGEnum)

NS_INTERFACE_MAP_BEGIN(nsSVGEnum)
  NS_INTERFACE_MAP_ENTRY(nsISVGValue)
  NS_INTERFACE_MAP_ENTRY(nsISVGEnum)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISVGValue)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
// nsISVGValue methods:
NS_IMETHODIMP
nsSVGEnum::SetValueString(const nsAString& aValue)
{
  nsCOMPtr<nsIAtom> valAtom = do_GetAtom(aValue);

  nsSVGEnumMapping *tmp = mMapping;

  while (tmp->key) {
    if (valAtom == *(tmp->key)) {
      WillModify();
      mValue = tmp->val;
      DidModify();
      return NS_OK;
    }
    tmp++;
  }

  NS_ERROR("unknown enumeration key");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSVGEnum::GetValueString(nsAString& aValue)
{
  nsSVGEnumMapping *tmp = mMapping;

  while (tmp->key) {
    if (mValue == tmp->val) {
      (*tmp->key)->ToString(aValue);
      return NS_OK;
    }
    tmp++;
  }
  NS_ERROR("unknown enumeration value");
  return NS_ERROR_FAILURE;
}

//----------------------------------------------------------------------
// nsISVGEnum methods:

NS_IMETHODIMP
nsSVGEnum::GetIntegerValue(PRUint16& aValue)
{
  aValue = mValue;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGEnum::SetIntegerValue(PRUint16 aValue)
{
  WillModify();
  mValue = aValue;
  DidModify();
  return NS_OK;
}


