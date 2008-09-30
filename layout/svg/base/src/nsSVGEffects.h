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
#include "nsSVGFilterFrame.h"
#include "nsSVGClipPathFrame.h"
#include "nsSVGMaskFrame.h"

class nsSVGPropertyBase : public nsStubMutationObserver {
public:
  nsSVGPropertyBase(nsIURI* aURI, nsIFrame *aFrame);
  virtual ~nsSVGPropertyBase();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIMutationObserver
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

protected:
  // This is called when the referenced filter/mask/clipPath changes
  virtual void DoUpdate() = 0;

  class SourceReference : public nsReferencedElement {
  public:
    SourceReference(nsSVGPropertyBase* aContainer) : mContainer(aContainer) {}
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
    nsSVGPropertyBase* mContainer;
  };
  
  /**
   * @param aOK this is only for the convenience of callers. We set *aOK to false
   * if this function returns null.
   */
  nsIFrame* GetReferencedFrame(nsIAtom* aFrameType, PRBool* aOK);

  SourceReference mElement;
  nsIFrame *mFrame;
};

class nsSVGFilterProperty :
  public nsSVGPropertyBase, public nsISVGFilterProperty {
public:
  nsSVGFilterProperty(nsIURI *aURI, nsIFrame *aFilteredFrame);

  nsSVGFilterFrame *GetFilterFrame(PRBool *aOK) {
    return static_cast<nsSVGFilterFrame *>
      (GetReferencedFrame(nsGkAtoms::svgFilterFrame, aOK));
  }
  void UpdateRect();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIMutationObserver
  NS_DECL_NSIMUTATIONOBSERVER_PARENTCHAINCHANGED

  // nsISVGFilterProperty
  virtual void Invalidate() { DoUpdate(); }

private:
  // nsSVGPropertyBase
  virtual void DoUpdate();
  
  // Tracks a bounding box for the filtered mFrame. We need this so that
  // if the filter changes we know how much to redraw. We only need this
  // for SVG frames since regular frames have an overflow area
  // that includes the filtered area.
  nsRect mFilterRect;
};

class nsSVGClipPathProperty : public nsSVGPropertyBase {
public:
  nsSVGClipPathProperty(nsIURI *aURI, nsIFrame *aClippedFrame)
    : nsSVGPropertyBase(aURI, aClippedFrame) {}

  nsSVGClipPathFrame *GetClipPathFrame(PRBool *aOK) {
    return static_cast<nsSVGClipPathFrame *>
      (GetReferencedFrame(nsGkAtoms::svgClipPathFrame, aOK));
  }

  // nsIMutationObserver
  NS_DECL_NSIMUTATIONOBSERVER_PARENTCHAINCHANGED

private:
  virtual void DoUpdate();
};

class nsSVGMaskProperty : public nsSVGPropertyBase {
public:
  nsSVGMaskProperty(nsIURI *aURI, nsIFrame *aMaskedFrame)
    : nsSVGPropertyBase(aURI, aMaskedFrame) {}

  nsSVGMaskFrame *GetMaskFrame(PRBool *aOK) {
    return static_cast<nsSVGMaskFrame *>
      (GetReferencedFrame(nsGkAtoms::svgMaskFrame, aOK));
  }

  // nsIMutationObserver
  NS_DECL_NSIMUTATIONOBSERVER_PARENTCHAINCHANGED

private:
  virtual void DoUpdate();
};

class nsSVGEffects {
public:
  struct EffectProperties {
    nsSVGFilterProperty*   mFilter;
    nsSVGMaskProperty*     mMask;
    nsSVGClipPathProperty* mClipPath;
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
    PRBool ok;
    return prop ? prop->GetFilterFrame(&ok) : nsnull;
  }
};

#endif /*NSSVGEFFECTS_H_*/
