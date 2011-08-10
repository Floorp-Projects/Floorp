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

#ifndef _NS_ACCESSIBLE_RELATION_WRAP_H
#define _NS_ACCESSIBLE_RELATION_WRAP_H

#include "nsAccessible.h"

#include "nsTArray.h"

#include "AccessibleRelation.h"

class ia2AccessibleRelation : public IAccessibleRelation
{
public:
  ia2AccessibleRelation(PRUint32 aType, Relation* aRel);
  virtual ~ia2AccessibleRelation() { }

  // IUnknown
  virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID aIID, void** aOutPtr);
  virtual ULONG STDMETHODCALLTYPE AddRef();
  virtual ULONG STDMETHODCALLTYPE Release();

  // IAccessibleRelation
  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_relationType(
      /* [retval][out] */ BSTR *relationType);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_localizedRelationType(
      /* [retval][out] */ BSTR *localizedRelationType);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_nTargets(
      /* [retval][out] */ long *nTargets);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_target(
      /* [in] */ long targetIndex,
      /* [retval][out] */ IUnknown **target);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_targets(
      /* [in] */ long maxTargets,
      /* [length_is][size_is][out] */ IUnknown **target,
      /* [retval][out] */ long *nTargets);

  inline bool HasTargets() const
    { return mTargets.Length(); }

private:
  ia2AccessibleRelation();
  ia2AccessibleRelation(const ia2AccessibleRelation&);
  ia2AccessibleRelation& operator = (const ia2AccessibleRelation&);

  PRUint32 mType;
  nsTArray<nsRefPtr<nsAccessible> > mTargets;
  ULONG mReferences;
};

#endif

