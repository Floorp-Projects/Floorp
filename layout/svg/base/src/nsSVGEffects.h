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
#include "nsTHashtable.h"

class nsSVGClipPathFrame;
class nsSVGFilterFrame;
class nsSVGMaskFrame;

/*
 * SVG elements reference supporting resources by element ID. We need to
 * track when those resources change and when the DOM changes in ways
 * that affect which element is referenced by a given ID (e.g., when
 * element IDs change). The code here is responsible for that.
 * 
 * When a frame references a supporting resource, we create a property
 * object derived from nsSVGRenderingObserver to manage the relationship. The
 * property object is attached to the referencing frame.
 */
class nsSVGRenderingObserver : public nsStubMutationObserver {
public:
  nsSVGRenderingObserver(nsIURI* aURI, nsIFrame *aFrame);
  virtual ~nsSVGRenderingObserver();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIMutationObserver
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  void InvalidateViaReferencedFrame();

  nsIFrame* GetReferencedFrame();
  /**
   * @param aOK this is only for the convenience of callers. We set *aOK to false
   * if this function returns null.
   */
  nsIFrame* GetReferencedFrame(nsIAtom* aFrameType, PRBool* aOK);

protected:
  // This is called when the referenced resource changes.
  virtual void DoUpdate();

  class SourceReference : public nsReferencedElement {
  public:
    SourceReference(nsSVGRenderingObserver* aContainer) : mContainer(aContainer) {}
  protected:
    virtual void ContentChanged(nsIContent* aFrom, nsIContent* aTo) {
      if (aFrom) {
        aFrom->RemoveMutationObserver(mContainer);
      }
      nsReferencedElement::ContentChanged(aFrom, aTo);
      if (aTo) {
        aTo->AddMutationObserver(mContainer);
      }
      mContainer->DoUpdate();
    }
    /**
     * Override IsPersistent because we want to keep tracking the element
     * for the ID even when it changes.
     */
    virtual PRBool IsPersistent() { return PR_TRUE; }
  private:
    nsSVGRenderingObserver* mContainer;
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
  nsIFrame *mReferencedFrame;
  nsIPresShell *mReferencedFramePresShell;
};

class nsSVGFilterProperty :
  public nsSVGRenderingObserver, public nsISVGFilterProperty {
public:
  nsSVGFilterProperty(nsIURI *aURI, nsIFrame *aFilteredFrame)
    : nsSVGRenderingObserver(aURI, aFilteredFrame) {}

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

class nsSVGMarkerProperty : public nsSVGRenderingObserver {
public:
  nsSVGMarkerProperty(nsIURI *aURI, nsIFrame *aFrame)
    : nsSVGRenderingObserver(aURI, aFrame) {}

protected:
  virtual void DoUpdate();
};

class nsSVGTextPathProperty : public nsSVGRenderingObserver {
public:
  nsSVGTextPathProperty(nsIURI *aURI, nsIFrame *aFrame)
    : nsSVGRenderingObserver(aURI, aFrame) {}

protected:
  virtual void DoUpdate();
};
 
class nsSVGPaintingProperty : public nsSVGRenderingObserver {
public:
  nsSVGPaintingProperty(nsIURI *aURI, nsIFrame *aFrame)
    : nsSVGRenderingObserver(aURI, aFrame) {}

protected:
  virtual void DoUpdate();
};

/**
 * A manager for one-shot nsSVGRenderingObserver tracking.
 * nsSVGRenderingObservers can be added or removed. They are not strongly
 * referenced so an observer must be removed before it before it dies.
 * When InvalidateAll is called, all outstanding references get
 * InvalidateViaReferencedFrame()
 * called on them and the list is cleared. The intent is that
 * the observer will force repainting of whatever part of the document
 * is needed, and then at paint time the observer will be re-added.
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
  void InvalidateAll();

private:
  nsTHashtable<nsVoidPtrHashKey> mObservers;
};

class nsSVGEffects {
public:
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
  static void AddRenderingObserver(nsIFrame *aFrame, nsSVGRenderingObserver *aObserver);
  /**
   * @param aFrame must be a first-continuation.
   */
  static void RemoveRenderingObserver(nsIFrame *aFrame, nsSVGRenderingObserver *aObserver);
  /**
   * This can be called on any first-continuation frame. We walk up to
   * the nearest observable frame and invalidate its observers.
   */
  static void InvalidateRenderingObservers(nsIFrame *aFrame);
  /**
   * This can be called on any frame. Direct observers
   * of this frame are all invalidated.
   */
  static void InvalidateDirectRenderingObservers(nsIFrame *aFrame);

  /**
   * Get an nsSVGMarkerProperty for the frame, creating a fresh one if necessary
   */
  static nsSVGMarkerProperty *
  GetMarkerProperty(nsIURI *aURI, nsIFrame *aFrame, nsIAtom *aProp);
  /**
   * Get an nsSVGTextPathProperty for the frame, creating a fresh one if necessary
   */
  static nsSVGTextPathProperty *
  GetTextPathProperty(nsIURI *aURI, nsIFrame *aFrame, nsIAtom *aProp);
  /**
   * Get an nsSVGPaintingProperty for the frame, creating a fresh one if necessary
   */
  static nsSVGPaintingProperty *
  GetPaintingProperty(nsIURI *aURI, nsIFrame *aFrame, nsIAtom *aProp);
};

#endif /*NSSVGEFFECTS_H_*/
