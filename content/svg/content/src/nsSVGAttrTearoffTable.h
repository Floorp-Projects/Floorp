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
 * Contributor(s):
 *   Brian Birtles <birtles@gmail.com>
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

#ifndef NS_SVGATTRTEAROFFTABLE_H_
#define NS_SVGATTRTEAROFFTABLE_H_

#include "nsDataHashtable.h"

/**
 * Global hashmap to associate internal SVG data types (e.g. nsSVGLength2) with
 * DOM tear-off objects (e.g. nsIDOMSVGLength). This allows us to always return
 * the same object for subsequent requests for DOM objects.
 *
 * We don't keep an owning reference to the tear-off objects so they are
 * responsible for removing themselves from this table when they die.
 */
template<class SimpleType, class TearoffType>
class nsSVGAttrTearoffTable
{
public:
  ~nsSVGAttrTearoffTable()
  {
    NS_ABORT_IF_FALSE(mTable.Count() == 0,
        "Tear-off objects remain in hashtable at shutdown.");
  }

  TearoffType* GetTearoff(SimpleType* aSimple);

  void AddTearoff(SimpleType* aSimple, TearoffType* aTearoff);

  void RemoveTearoff(SimpleType* aSimple);

private:
  typedef nsPtrHashKey<SimpleType> SimpleTypePtrKey;
  typedef nsDataHashtable<SimpleTypePtrKey, TearoffType* > TearoffTable;

  TearoffTable mTable;
};

template<class SimpleType, class TearoffType>
TearoffType*
nsSVGAttrTearoffTable<SimpleType, TearoffType>::GetTearoff(SimpleType* aSimple)
{
  if (!mTable.IsInitialized())
    return nsnull;

  TearoffType *tearoff = nsnull;

#ifdef DEBUG
  bool found =
#endif
    mTable.Get(aSimple, &tearoff);
  NS_ABORT_IF_FALSE(!found || tearoff,
      "NULL pointer stored in attribute tear-off map");

  return tearoff;
}

template<class SimpleType, class TearoffType>
void
nsSVGAttrTearoffTable<SimpleType, TearoffType>::AddTearoff(SimpleType* aSimple,
                                                          TearoffType* aTearoff)
{
  if (!mTable.IsInitialized()) {
    mTable.Init();
  }

  // We shouldn't be adding a tear-off if there already is one. If that happens,
  // something is wrong.
  if (mTable.Get(aSimple, nsnull)) {
    NS_ABORT_IF_FALSE(PR_FALSE, "There is already a tear-off for this object.");
    return;
  }

#ifdef DEBUG
  bool result =
#endif
    mTable.Put(aSimple, aTearoff);
  NS_ABORT_IF_FALSE(result, "Out of memory.");
}

template<class SimpleType, class TearoffType>
void
nsSVGAttrTearoffTable<SimpleType, TearoffType>::RemoveTearoff(
    SimpleType* aSimple)
{
  if (!mTable.IsInitialized()) {
    // Perhaps something happened in between creating the SimpleType object and
    // registering it
    return;
  }

  mTable.Remove(aSimple);
}

#endif // NS_SVGATTRTEAROFFTABLE_H_
