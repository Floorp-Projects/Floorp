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
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   rocallahan@mozilla.com
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

#ifndef NSSVGEFFECTS_H_
#define NSSVGEFFECTS_H_

#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsReferencedElement.h"
#include "nsStubMutationObserver.h"
#include "nsSVGUtils.h"
#include "nsInterfaceHashtable.h"
#include "nsURIHashKey.h"

class nsSVGClipPathFrame;
class nsSVGFilterFrame;
class nsSVGMaskFrame;

/*
 * This interface allows us to be notified when a piece of SVG content is
 * re-rendered.
 *
 * Concrete implementations of this interface need to implement
 * "GetTarget()" to specify the piece of SVG content that they'd like to
 * monitor, and they need to implement "DoUpdate" to specify how we'll react
 * when that content gets re-rendered. They also need to implement a
 * constructor and destructor, which should call StartListening and
 * StopListening, respectively.
 */
class nsSVGRenderingObserver : public nsStubMutationObserver {
public:
  typedef mozilla::dom::Element Element;
  nsSVGRenderingObserver()
    : mInObserverList(PR_FALSE)
    {}
  virtual ~nsSVGRenderingObserver()
    {}

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIMutationObserver
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  void InvalidateViaReferencedElement();
  nsIFrame* GetReferencedFrame();

  /**
   * @param aOK this is only for the convenience of callers. We set *aOK to false
   * if this function returns null.
   */
  nsIFrame* GetReferencedFrame(nsIAtom* aFrameType, PRBool* aOK);

  Element* GetReferencedElement();

protected:
  // Non-virtual protected methods
  void StartListening();
  void StopListening();

  // Virtual protected methods
  virtual void DoUpdate() = 0; // called when the referenced resource changes.

  // This is an internally-used version of GetReferencedElement that doesn't
  // forcibly add us as an observer. (whereas GetReferencedElement does)
  virtual Element* GetTarget() = 0;

  // Whether we're in our referenced element's observer list at this time.
  PRPackedBool mInObserverList;
};


/*
 * SVG elements reference supporting resources by element ID. We need to
 * track when those resources change and when the DOM changes in ways
 * that affect which element is referenced by a given ID (e.g., when
 * element IDs change). The code here is responsible for that.
 * 
 * When a frame references a supporting resource, we create a property
 * object derived from nsSVGIDRenderingObserver to manage the relationship. The
 * property object is attached to the referencing frame.
 */
class nsSVGIDRenderingObserver : public nsSVGRenderingObserver {
public:
  typedef mozilla::dom::Element Element;
  nsSVGIDRenderingObserver(nsIURI* aURI, nsIFrame *aFrame,
                         PRBool aReferenceImage);
  virtual ~nsSVGIDRenderingObserver();

protected:
  Element* GetTarget() { return mElement.get(); }

  // This is called when the referenced resource changes.
  virtual void DoUpdate();

  class SourceReference : public nsReferencedElement {
  public:
    SourceReference(nsSVGIDRenderingObserver* aContainer) : mContainer(aContainer) {}
  protected:
    virtual void ElementChanged(Element* aFrom, Element* aTo) {
      mContainer->StopListening();
      nsReferencedElement::ElementChanged(aFrom, aTo);
      mContainer->StartListening();
      mContainer->DoUpdate();
    }
    /**
     * Override IsPersistent because we want to keep tracking the element
     * for the ID even when it changes.
     */
    virtual PRBool IsPersistent() { return PR_TRUE; }
  private:
    nsSVGIDRenderingObserver* mContainer;
  };
  
  SourceReference mElement;
  // The frame that this property is attached to
   nsIFrame *mFrame;
  // When a presshell is torn down, we don't delete the properties for
  // each frame until after the frames are destroyed. So here we remember
  // the presshell for the frames we care about and, before we use the frame,
  // we test the presshell to see if it's destroying itself. If it is,
  // then the frame pointer is not valid and we know the frame has gone away.
  nsIPresShell *mFramePresShell;
};

class nsSVGFilterProperty :
  public nsSVGIDRenderingObserver, public nsISVGFilterProperty {
public:
  nsSVGFilterProperty(nsIURI *aURI, nsIFrame *aFilteredFrame,
                      PRBool aReferenceImage)
    : nsSVGIDRenderingObserver(aURI, aFilteredFrame, aReferenceImage) {}

  /**
   * @return the filter frame, or null if there is no filter frame
   */
  nsSVGFilterFrame *GetFilterFrame();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsISVGFilterProperty
  virtual void Invalidate() { DoUpdate(); }

private:
  // nsSVGRenderingObserver
  virtual void DoUpdate();
};

class nsSVGMarkerProperty : public nsSVGIDRenderingObserver {
public:
  nsSVGMarkerProperty(nsIURI *aURI, nsIFrame *aFrame, PRBool aReferenceImage)
    : nsSVGIDRenderingObserver(aURI, aFrame, aReferenceImage) {}

protected:
  virtual void DoUpdate();
};

class nsSVGTextPathProperty : public nsSVGIDRenderingObserver {
public:
  nsSVGTextPathProperty(nsIURI *aURI, nsIFrame *aFrame, PRBool aReferenceImage)
    : nsSVGIDRenderingObserver(aURI, aFrame, aReferenceImage) {}

protected:
  virtual void DoUpdate();
};
 
class nsSVGPaintingProperty : public nsSVGIDRenderingObserver {
public:
  nsSVGPaintingProperty(nsIURI *aURI, nsIFrame *aFrame, PRBool aReferenceImage)
    : nsSVGIDRenderingObserver(aURI, aFrame, aReferenceImage) {}

protected:
  virtual void DoUpdate();
};

/**
 * A manager for one-shot nsSVGRenderingObserver tracking.
 * nsSVGRenderingObservers can be added or removed. They are not strongly
 * referenced so an observer must be removed before it dies.
 * When InvalidateAll is called, all outstanding references get
 * InvalidateViaReferencedElement()
 * called on them and the list is cleared. The intent is that
 * the observer will force repainting of whatever part of the document
 * is needed, and then at paint time the observer will do a clean lookup
 * of the referenced element and [re-]add itself to the element's observer list.
 * 
 * InvalidateAll must be called before this object is destroyed, i.e.
 * before the referenced frame is destroyed. This should normally happen
 * via nsSVGContainerFrame::RemoveFrame, since only frames in the frame
 * tree should be referenced.
 */
class nsSVGRenderingObserverList {
public:
  nsSVGRenderingObserverList() {
    MOZ_COUNT_CTOR(nsSVGRenderingObserverList);
    mObservers.Init(5);
  }

  ~nsSVGRenderingObserverList() {
    InvalidateAll();
    MOZ_COUNT_DTOR(nsSVGRenderingObserverList);
  }

  void Add(nsSVGRenderingObserver* aObserver)
  { mObservers.PutEntry(aObserver); }
  void Remove(nsSVGRenderingObserver* aObserver)
  { mObservers.RemoveEntry(aObserver); }
#ifdef DEBUG
  PRBool Contains(nsSVGRenderingObserver* aObserver)
  { return (mObservers.GetEntry(aObserver) != nsnull); }
#endif
  PRBool IsEmpty()
  { return mObservers.Count() == 0; }

  /**
   * Drop all our observers, and notify them that we have changed and dropped
   * our reference to them.
   */
  void InvalidateAll();

private:
  nsTHashtable<nsVoidPtrHashKey> mObservers;
};

class nsSVGEffects {
public:
  typedef mozilla::dom::Element Element;
  typedef mozilla::FramePropertyDescriptor FramePropertyDescriptor;
  typedef nsInterfaceHashtable<nsURIHashKey, nsIMutationObserver>
    URIObserverHashtable;

  static void DestroySupports(void* aPropertyValue)
  {
    (static_cast<nsISupports*>(aPropertyValue))->Release();
  }

  static void DestroyHashtable(void* aPropertyValue)
  {
    delete static_cast<URIObserverHashtable*> (aPropertyValue);
  }

  NS_DECLARE_FRAME_PROPERTY(FilterProperty, DestroySupports)
  NS_DECLARE_FRAME_PROPERTY(MaskProperty, DestroySupports)
  NS_DECLARE_FRAME_PROPERTY(ClipPathProperty, DestroySupports)
  NS_DECLARE_FRAME_PROPERTY(MarkerBeginProperty, DestroySupports)
  NS_DECLARE_FRAME_PROPERTY(MarkerMiddleProperty, DestroySupports)
  NS_DECLARE_FRAME_PROPERTY(MarkerEndProperty, DestroySupports)
  NS_DECLARE_FRAME_PROPERTY(FillProperty, DestroySupports)
  NS_DECLARE_FRAME_PROPERTY(StrokeProperty, DestroySupports)
  NS_DECLARE_FRAME_PROPERTY(HrefProperty, DestroySupports)
  NS_DECLARE_FRAME_PROPERTY(BackgroundImageProperty, DestroyHashtable)

  struct EffectProperties {
    nsSVGFilterProperty*   mFilter;
    nsSVGPaintingProperty* mMask;
    nsSVGPaintingProperty* mClipPath;

    /**
     * @return the clip-path frame, or null if there is no clip-path frame
     * @param aOK if a clip-path was specified but the designated element
     * does not exist or is an element of the wrong type, *aOK is set
     * to false. Otherwise *aOK is untouched.
     */
    nsSVGClipPathFrame *GetClipPathFrame(PRBool *aOK);
    /**
     * @return the mask frame, or null if there is no mask frame
     * @param aOK if a mask was specified but the designated element
     * does not exist or is an element of the wrong type, *aOK is set
     * to false. Otherwise *aOK is untouched.
     */
    nsSVGMaskFrame *GetMaskFrame(PRBool *aOK);
    /**
     * @return the filter frame, or null if there is no filter frame
     * @param aOK if a filter was specified but the designated element
     * does not exist or is an element of the wrong type, *aOK is set
     * to false. Otherwise *aOK is untouched.
     */
    nsSVGFilterFrame *GetFilterFrame(PRBool *aOK) {
      if (!mFilter)
        return nsnull;
      nsSVGFilterFrame *filter = mFilter->GetFilterFrame();
      if (!filter) {
        *aOK = PR_FALSE;
      }
      return filter;
    }
  };

  /**
   * @param aFrame should be the first continuation
   */
  static EffectProperties GetEffectProperties(nsIFrame *aFrame);
  /**
   * Called by nsCSSFrameConstructor when style changes require the
   * effect properties on aFrame to be updated
   */
  static void UpdateEffects(nsIFrame *aFrame);
  /**
   * @param aFrame should be the first continuation
   */
  static nsSVGFilterProperty *GetFilterProperty(nsIFrame *aFrame);
  static nsSVGFilterFrame *GetFilterFrame(nsIFrame *aFrame) {
    nsSVGFilterProperty *prop = GetFilterProperty(aFrame);
    return prop ? prop->GetFilterFrame() : nsnull;
  }

  /**
   * @param aFrame must be a first-continuation.
   */
  static void AddRenderingObserver(Element *aElement, nsSVGRenderingObserver *aObserver);
  /**
   * @param aFrame must be a first-continuation.
   */
  static void RemoveRenderingObserver(Element *aElement, nsSVGRenderingObserver *aObserver);
  /**
   * This can be called on any frame. We invalidate the observers of aFrame's
   * element, if any, or else walk up to the nearest observable SVG parent
   * frame with observers and invalidate them instead.
   *
   * Note that this method is very different to e.g.
   * nsNodeUtils::AttributeChanged which walks up the content node tree all the
   * way to the root node (not stopping if it encounters a non-container SVG
   * node) invalidating all mutation observers (not just
   * nsSVGRenderingObservers) on all nodes along the way (not just the first
   * node it finds with observers). In other words, by doing all the
   * things in parentheses in the preceding sentence, this method uses
   * knowledge about our implementation and what can be affected by SVG effects
   * to make invalidation relatively lightweight when an SVG effect changes.
   */
  static void InvalidateRenderingObservers(nsIFrame *aFrame);
  /**
   * This can be called on any element or frame. Only direct observers of this
   * (frame's) element, if any, are invalidated.
   */
  static void InvalidateDirectRenderingObservers(Element *aElement);
  static void InvalidateDirectRenderingObservers(nsIFrame *aFrame);

  /**
   * Get an nsSVGMarkerProperty for the frame, creating a fresh one if necessary
   */
  static nsSVGMarkerProperty *
  GetMarkerProperty(nsIURI *aURI, nsIFrame *aFrame,
                    const FramePropertyDescriptor *aProperty);
  /**
   * Get an nsSVGTextPathProperty for the frame, creating a fresh one if necessary
   */
  static nsSVGTextPathProperty *
  GetTextPathProperty(nsIURI *aURI, nsIFrame *aFrame,
                      const FramePropertyDescriptor *aProperty);
  /**
   * Get an nsSVGPaintingProperty for the frame, creating a fresh one if necessary
   */
  static nsSVGPaintingProperty *
  GetPaintingProperty(nsIURI *aURI, nsIFrame *aFrame,
                      const FramePropertyDescriptor *aProperty);
  /**
   * Get an nsSVGPaintingProperty for the frame for that URI, creating a fresh
   * one if necessary
   */
  static nsSVGPaintingProperty *
  GetPaintingPropertyForURI(nsIURI *aURI, nsIFrame *aFrame,
                            const FramePropertyDescriptor *aProp);
};

#endif /*NSSVGEFFECTS_H_*/
