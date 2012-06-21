/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_DOMSVGANIMATEDLENGTHLIST_H__
#define MOZILLA_DOMSVGANIMATEDLENGTHLIST_H__

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIDOMSVGAnimatedLengthList.h"
#include "nsSVGElement.h"
#include "mozilla/Attributes.h"

namespace mozilla {

class SVGAnimatedLengthList;
class SVGLengthList;
class DOMSVGLengthList;

/**
 * Class DOMSVGAnimatedLengthList
 *
 * This class is used to create the DOM tearoff objects that wrap internal
 * SVGAnimatedLengthList objects. We have this internal-DOM split because DOM
 * classes are relatively heavy-weight objects with non-optimal interfaces for
 * internal code, and they're relatively infrequently used. Having separate
 * internal and DOM classes does add complexity - especially for lists where
 * the internal list and DOM lists (and their items) need to be kept in sync -
 * but it keeps the internal classes light and fast, and in 99% of cases
 * they're all that's used. DOM wrappers are only instantiated when script
 * demands it.
 *
 * Ownership model:
 *
 * The diagram below shows the ownership model between the various DOM objects
 * in the tree of DOM objects that correspond to an SVG length list attribute.
 * The angled brackets ">" and "<" denote a reference from one object to
 * another, where the "!" character denotes a strong reference, and the "~"
 * character denotes a weak reference.
 *
 *      .----<!----.                .----<!----.        .----<!----.
 *      |          |                |          |        |          |
 *   element ~> DOMSVGAnimatedLengthList ~> DOMSVGLengthList ~> DOMSVGLength
 *
 * Rationale:
 *
 * The following three paragraphs explain the main three requirements that must
 * be met by any design. These are followed by an explanation of the rationale
 * behind our particular design.
 *
 * 1: DOMSVGAnimatedLengthList, DOMSVGLengthLists and DOMSVGLength get to their
 * internal counterparts via their element, and they use their element to send
 * out appropriate notifications when they change. Because of this, having
 * their element disappear out from under them would be very bad. To keep their
 * element alive at least as long as themselves, each of these classes must
 * contain a _strong_ reference (directly or indirectly) to their element.
 *
 * 2: Another central requirement of any design is the SVG specification's
 * requirement that script must always be given the exact same objects each
 * time it accesses a given object in a DOM object tree corresponding to an SVG
 * length list attribute. In practice "always" actually means "whenever script
 * has kept a references to a DOM object it previously accessed", since a
 * script will only be able to detect any difference in object identity if it
 * has a previous reference to compare against.
 *
 * 3: The wiggle room in the "same object" requirement leads us to a third
 * (self imposed) requirement: if script no longer has a reference to a given
 * DOM object from an object tree corresponding to an SVG length list
 * attribute, and if that object doesn't currently have any descendants, then
 * that object should be released to free up memory.
 *
 * To help in understanding our current design, consider this BROKEN design:
 *
 *      .-------------------------------<!-------------------------.
 *      |--------------------<!----------------.                   |
 *      |----<!----.                           |                   |
 *      |          |                           |                   |
 *   element ~> DOMSVGAnimatedLengthList !> DOMSVGLengthList !> DOMSVGLength
 *
 * Having all the objects keep a reference directly to their element like this
 * would reduce the number of dereferences that they need to make to get their
 * internal counterpart. Hovewer, this design does not meet the "same object"
 * requirement of the SVG specification. If script keeps a reference to a
 * DOMSVGLength or DOMSVGLengthList object, but not to that object's
 * DOMSVGAnimatedLengthList, then the DOMSVGAnimatedLengthList may be garbage
 * collected. We'd then have no way to return the same DOMSVGLength /
 * DOMSVGLengthList object that the script has a reference to if the script
 * went looking for it via the DOMSVGAnimatedLengthList property on the
 * element - we'd end up creating a fresh DOMSVGAnimatedLengthList, with no
 * knowlegde of the existing DOMSVGLengthList or DOMSVGLength object.
 *
 * The way we solve this problem is by making sure that parent objects cannot
 * die until all their children are dead by having child objects hold a strong
 * reference to their parent object. Note that this design means that the child
 * objects hold a strong reference to their element too, albeit indirectly via
 * the strong reference to their parent object:
 *
 *      .----<!----.                .----<!----.        .----<!----.
 *      |          |                |          |        |          |
 *   element ~> DOMSVGAnimatedLengthList ~> DOMSVGLengthList ~> DOMSVGLength
 *
 * One drawback of this design is that objects must look up their parent
 * chain to find their element, but that overhead is relatively small.
 */
class DOMSVGAnimatedLengthList MOZ_FINAL : public nsIDOMSVGAnimatedLengthList
{
  friend class DOMSVGLengthList;

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(DOMSVGAnimatedLengthList)
  NS_DECL_NSIDOMSVGANIMATEDLENGTHLIST

  /**
   * Factory method to create and return a DOMSVGAnimatedLengthList wrapper
   * for a given internal SVGAnimatedLengthList object. The factory takes care
   * of caching the object that it returns so that the same object can be
   * returned for the given SVGAnimatedLengthList each time it is requested.
   * The cached object is only removed from the cache when it is destroyed due
   * to there being no more references to it or to any of its descendant
   * objects. If that happens, any subsequent call requesting the DOM wrapper
   * for the SVGAnimatedLengthList will naturally result in a new
   * DOMSVGAnimatedLengthList being returned.
   */
  static already_AddRefed<DOMSVGAnimatedLengthList>
    GetDOMWrapper(SVGAnimatedLengthList *aList,
                  nsSVGElement *aElement,
                  PRUint8 aAttrEnum,
                  PRUint8 aAxis);

  /**
   * This method returns the DOMSVGAnimatedLengthList wrapper for an internal
   * SVGAnimatedLengthList object if it currently has a wrapper. If it does
   * not, then nsnull is returned.
   */
  static DOMSVGAnimatedLengthList*
    GetDOMWrapperIfExists(SVGAnimatedLengthList *aList);

  /**
   * Called by internal code to notify us when we need to sync the length of
   * our baseVal DOM list with its internal list. This is called just prior to
   * the length of the internal baseVal list being changed so that any DOM list
   * items that need to be removed from the DOM list can first get their values
   * from their internal counterpart.
   *
   * The only time this method could fail is on OOM when trying to increase the
   * length of the DOM list. If that happens then this method simply clears the
   * list and returns. Callers just proceed as normal, and we simply accept
   * that the DOM list will be empty (until successfully set to a new value).
   */
  void InternalBaseValListWillChangeTo(const SVGLengthList& aNewValue);
  void InternalAnimValListWillChangeTo(const SVGLengthList& aNewValue);

  /**
   * Returns true if our attribute is animating (in which case our animVal is
   * not simply a mirror of our baseVal).
   */
  bool IsAnimating() const;

private:

  /**
   * Only our static GetDOMWrapper() factory method may create objects of our
   * type.
   */
  DOMSVGAnimatedLengthList(nsSVGElement *aElement, PRUint8 aAttrEnum, PRUint8 aAxis)
    : mBaseVal(nsnull)
    , mAnimVal(nsnull)
    , mElement(aElement)
    , mAttrEnum(aAttrEnum)
    , mAxis(aAxis)
  {}

  ~DOMSVGAnimatedLengthList();

  /// Get a reference to this DOM wrapper object's internal counterpart.
  SVGAnimatedLengthList& InternalAList();
  const SVGAnimatedLengthList& InternalAList() const;

  // Weak refs to our DOMSVGLengthList baseVal/animVal objects. These objects
  // are friends and take care of clearing these pointers when they die, making
  // these true weak references.
  DOMSVGLengthList *mBaseVal;
  DOMSVGLengthList *mAnimVal;

  // Strong ref to our element to keep it alive. We hold this not only for
  // ourself, but also for our base/animVal and all of their items.
  nsRefPtr<nsSVGElement> mElement;

  PRUint8 mAttrEnum;
  PRUint8 mAxis;
};

} // namespace mozilla

#endif // MOZILLA_DOMSVGANIMATEDLENGTHLIST_H__
