/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
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

#include "CAccessibleImage.h"

#include "AccessibleImage_i.c"

#include "nsIAccessible.h"
#include "nsIAccessibleImage.h"
#include "nsIAccessibleTypes.h"

#include "nsCOMPtr.h"
#include "nsString.h"

// IUnknown

STDMETHODIMP
CAccessibleImage::QueryInterface(REFIID iid, void** ppv)
{
  *ppv = NULL;

  if (IID_IAccessibleImage == iid) {
    nsCOMPtr<nsIAccessibleImage> imageAcc(do_QueryInterface(this));
    if (!imageAcc)
      return E_FAIL;

    *ppv = static_cast<IAccessibleImage*>(this);
    (reinterpret_cast<IUnknown*>(*ppv))->AddRef();
    return S_OK;
  }

  return E_NOINTERFACE;
}

// IAccessibleImage

STDMETHODIMP
CAccessibleImage::get_description(BSTR *aDescription)
{
  nsCOMPtr<nsIAccessible> acc(do_QueryInterface(this));
  if (!acc)
    return E_FAIL;

  nsAutoString description;
  nsresult rv = acc->GetName(description);
  if (NS_FAILED(rv))
    return E_FAIL;

  INT result = ::SysReAllocStringLen(aDescription, description.get(),
                                     description.Length());
  return result ? NS_OK : E_OUTOFMEMORY;
}

STDMETHODIMP
CAccessibleImage::get_imagePosition(enum IA2CoordinateType aCoordType,
                                    long *aX,
                                    long *aY)
{
  *aX = 0;
  *aY = 0;

  PRUint32 geckoCoordType = (aCoordType == IA2_COORDTYPE_SCREEN_RELATIVE) ?
    nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE :
    nsIAccessibleCoordinateType::COORDTYPE_PARENT_RELATIVE;

  nsCOMPtr<nsIAccessibleImage> imageAcc(do_QueryInterface(this));
  if (!imageAcc)
    return E_FAIL;

  PRInt32 x = 0, y = 0;
  nsresult rv = imageAcc->GetImagePosition(geckoCoordType, &x, &y);
  if (NS_FAILED(rv))
    return E_FAIL;

  *aX = x;
  *aY = y;

  return S_OK;
}

STDMETHODIMP
CAccessibleImage::get_imageSize(long *aHeight, long *aWidth)
{
  *aHeight = 0;
  *aWidth = 0;

  nsCOMPtr<nsIAccessibleImage> imageAcc(do_QueryInterface(this));
  if (!imageAcc)
    return E_FAIL;

  PRInt32 x = 0, y = 0, width = 0, height = 0;
  nsresult rv = imageAcc->GetImageSize(&width, &height);
  if (NS_FAILED(rv))
    return E_FAIL;

  *aHeight = width;
  *aWidth = height;

  return S_OK;
}

