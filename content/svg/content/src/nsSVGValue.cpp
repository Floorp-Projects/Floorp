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
 * Crocodile Clips Ltd..
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
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

#include "nsSVGValue.h"
#include "nsIWeakReference.h"

nsSVGValue::nsSVGValue()
    : mModifyNestCount(0)
{
}

nsSVGValue::~nsSVGValue()
{
  ReleaseObservers();
}

void
nsSVGValue::ReleaseObservers()
{
  PRInt32 count = mObservers.Count();
  PRInt32 i;
  for (i = 0; i < count; ++i) {
    nsIWeakReference* wr = NS_STATIC_CAST(nsIWeakReference*,mObservers.ElementAt(i));
    NS_RELEASE(wr);
  }
  while (i)
    mObservers.RemoveElementAt(--i);
}

void
nsSVGValue::NotifyObservers(SVGObserverNotifyFunction f)
{
  PRInt32 count = mObservers.Count();
  for (PRInt32 i = 0; i < count; ++i) {
    nsIWeakReference* wr = NS_STATIC_CAST(nsIWeakReference*,mObservers.ElementAt(i));
    nsCOMPtr<nsISVGValueObserver> observer = do_QueryReferent(wr);
    if(observer)
       (NS_STATIC_CAST(nsISVGValueObserver*,observer)->*f)(this);
  }
}

void
nsSVGValue::WillModify()
{
  if (++mModifyNestCount == 1)
    NotifyObservers(&nsISVGValueObserver::WillModifySVGObservable);
}

void
nsSVGValue::DidModify()
{
  NS_ASSERTION(mModifyNestCount>0, "unbalanced Will/DidModify calls");
  if (--mModifyNestCount == 0) {
    OnDidModify();
    NotifyObservers(&nsISVGValueObserver::DidModifySVGObservable);
  }
}


NS_IMETHODIMP
nsSVGValue::AddObserver(nsISVGValueObserver* observer)
{
  nsIWeakReference* wr = NS_GetWeakReference(observer);
  if (!wr) return NS_ERROR_FAILURE;
  mObservers.AppendElement((void*)wr);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGValue::RemoveObserver(nsISVGValueObserver* observer)
{
  nsCOMPtr<nsIWeakReference> wr = do_GetWeakReference(observer);
  if (!wr) return NS_ERROR_FAILURE;
  PRInt32 i = mObservers.IndexOf((void*)wr);
  if (i<0) return NS_ERROR_FAILURE;
  nsIWeakReference* wr2 = NS_STATIC_CAST(nsIWeakReference*,mObservers.ElementAt(i));
  NS_RELEASE(wr2);
  mObservers.RemoveElementAt(i);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGValue::BeginBatchUpdate()
{
  WillModify();
  return NS_OK;
}

NS_IMETHODIMP
nsSVGValue::EndBatchUpdate()
{
  DidModify();
  return NS_OK;
}

  
