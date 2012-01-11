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
 * The Original Code is Mozilla SVG Project code.
 *
 * The Initial Developer of the Original Code is
 * Robert Longson <longsonr@gmail.com>
 * Portions created by the Initial Developer are Copyright (C) 2011
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

#ifndef MOZILLA_DOMSVGSTRINGLIST_H__
#define MOZILLA_DOMSVGSTRINGLIST_H__

#include "nsIDOMSVGStringList.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCOMArray.h"
#include "nsAutoPtr.h"

class nsSVGElement;

namespace mozilla {

class SVGStringList;

/**
 * Class DOMSVGStringList
 *
 * This class is used to create the DOM tearoff objects that wrap internal
 * SVGPathData objects.
 *
 * See the architecture comment in DOMSVGAnimatedLengthList.h first (that's
 * LENGTH list), then continue reading the remainder of this comment.
 *
 * The architecture of this class is similar to that of DOMSVGLengthList
 * except for two important aspects:
 *
 * First, since there is no nsIDOMSVGAnimatedStringList interface in SVG, we
 * have no parent DOMSVGAnimatedStringList (unlike DOMSVGLengthList which has
 * a parent DOMSVGAnimatedLengthList class). As a consequence, much of the
 * logic that would otherwise be in DOMSVGAnimatedStringList (and is in
 * DOMSVGAnimatedLengthList) is contained in this class.
 *
 * Second, since there is no nsIDOMSVGString interface in SVG, we have no
 * DOMSVGString items to maintain. As far as script is concerned, objects
 * of this class contain a list of strings, not a list of mutable objects
 * like the other SVG list types. As a result, unlike the other SVG list
 * types, this class does not create its items lazily on demand and store
 * them so it can return the same objects each time. It simply returns a new
 * string each time any given item is requested.
 */
class DOMSVGStringList : public nsIDOMSVGStringList
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(DOMSVGStringList)
  NS_DECL_NSIDOMSVGSTRINGLIST

  /**
   * Factory method to create and return a DOMSVGStringList wrapper
   * for a given internal SVGStringList object. The factory takes care
   * of caching the object that it returns so that the same object can be
   * returned for the given SVGStringList each time it is requested.
   * The cached object is only removed from the cache when it is destroyed due
   * to there being no more references to it. If that happens, any subsequent
   * call requesting the DOM wrapper for the SVGStringList will naturally
   * result in a new DOMSVGStringList being returned.
   */
  static already_AddRefed<DOMSVGStringList>
    GetDOMWrapper(SVGStringList *aList,
                  nsSVGElement *aElement,
                  bool aIsConditionalProcessingAttribute,
                  PRUint8 aAttrEnum);

private:
  /**
   * Only our static GetDOMWrapper() factory method may create objects of our
   * type.
   */
  DOMSVGStringList(nsSVGElement *aElement,
                   bool aIsConditionalProcessingAttribute, PRUint8 aAttrEnum)
    : mElement(aElement)
    , mAttrEnum(aAttrEnum)
    , mIsConditionalProcessingAttribute(aIsConditionalProcessingAttribute)
  {}

  ~DOMSVGStringList();

  void DidChangeStringList(PRUint8 aAttrEnum, bool aDoSetAttr);

  SVGStringList &InternalList();

  // Strong ref to our element to keep it alive.
  nsRefPtr<nsSVGElement> mElement;

  PRUint8 mAttrEnum;

  bool    mIsConditionalProcessingAttribute;
};

} // namespace mozilla

#endif // MOZILLA_DOMSVGSTRINGLIST_H__
