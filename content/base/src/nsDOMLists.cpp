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
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peterv@netscape.com>
 *
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

#include "nsDOMLists.h"
#include "nsDOMError.h"
#include "nsIDOMClassInfo.h"
#include "nsContentUtils.h"

nsDOMStringList::nsDOMStringList()
{
}

nsDOMStringList::~nsDOMStringList()
{
}

NS_IMPL_ADDREF(nsDOMStringList)
NS_IMPL_RELEASE(nsDOMStringList)
NS_INTERFACE_MAP_BEGIN(nsDOMStringList)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDOMStringList)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(DOMStringList)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
nsDOMStringList::Item(PRUint32 aIndex, nsAString& aResult)
{
  if (aIndex >= (PRUint32)mNames.Count()) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  mNames.StringAt(aIndex, aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMStringList::GetLength(PRUint32 *aLength)
{
  *aLength = (PRUint32)mNames.Count();

  return NS_OK;
}

NS_IMETHODIMP
nsDOMStringList::Contains(const nsAString& aString, PRBool *aResult)
{
  *aResult = mNames.IndexOf(aString) > -1;

  return NS_OK;
}


nsNameList::nsNameList()
{
}

nsNameList::~nsNameList()
{
}

NS_IMPL_ADDREF(nsNameList)
  NS_IMPL_RELEASE(nsNameList)
  NS_INTERFACE_MAP_BEGIN(nsNameList)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNameList)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(NameList)
  NS_INTERFACE_MAP_END

NS_IMETHODIMP
nsNameList::GetName(PRUint32 aIndex, nsAString& aResult)
{
  if (aIndex >= (PRUint32)mNames.Count()) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  mNames.StringAt(aIndex, aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsNameList::GetNamespaceURI(PRUint32 aIndex, nsAString& aResult)
{
  if (aIndex >= (PRUint32)mNames.Count()) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  mNamespaceURIs.StringAt(aIndex, aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsNameList::GetLength(PRUint32 *aLength)
{
  *aLength = (PRUint32)mNames.Count();

  return NS_OK;
}

PRBool
nsNameList::Add(const nsAString& aNamespaceURI, const nsAString& aName)
{
  PRInt32 count = mNamespaceURIs.Count();
  if (mNamespaceURIs.InsertStringAt(aNamespaceURI, count)) {
    if (mNames.InsertStringAt(aName, count)) {
      return PR_TRUE;
    }
    mNamespaceURIs.RemoveStringAt(count);
  }

  return PR_FALSE;
}

NS_IMETHODIMP
nsNameList::Contains(const nsAString& aName, PRBool *aResult)
{
  *aResult = mNames.IndexOf(aName) > -1;

  return NS_OK;
}

NS_IMETHODIMP
nsNameList::ContainsNS(const nsAString& aNamespaceURI, const nsAString& aName,
                       PRBool *aResult)
{
  PRInt32 index = mNames.IndexOf(aName);
  if (index > -1) {
    nsAutoString ns;
    mNamespaceURIs.StringAt(index, ns);

    *aResult = ns.Equals(aNamespaceURI);
  }
  else {
    *aResult = PR_FALSE;
  }

  return NS_OK;
}
