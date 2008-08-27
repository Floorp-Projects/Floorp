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

#include "nsSVGEffects.h"
#include "nsISupportsImpl.h"
#include "nsSVGOuterSVGFrame.h"

NS_IMPL_ISUPPORTS1(nsSVGPropertyBase, nsIMutationObserver)

nsSVGPropertyBase::nsSVGPropertyBase(nsIURI *aURI,
                                     nsIFrame *aFrame)
  : mElement(this), mFrame(aFrame)
{
  // Start watching the target element
  mElement.Reset(aFrame->GetContent(), aURI);
  if (mElement.get()) {
    mElement.get()->AddMutationObserver(this);
  }
}

nsSVGPropertyBase::~nsSVGPropertyBase()
{
  if (mElement.get()) {
    mElement.get()->RemoveMutationObserver(this);
  }
}

nsIFrame*
nsSVGPropertyBase::GetReferencedFrame(nsIAtom* aFrameType, PRBool* aOK)
{
  if (mElement.get()) {
    nsIFrame *frame =
      static_cast<nsGenericElement*>(mElement.get())->GetPrimaryFrame();
    if (frame && frame->GetType() == aFrameType)
      return frame;
  }
  if (aOK) {
    *aOK = PR_FALSE;
  }
  return nsnull;
}

void
nsSVGPropertyBase::AttributeChanged(nsIDocument *aDocument,
                                    nsIContent *aContent,
                                    PRInt32 aNameSpaceID,
                                    nsIAtom *aAttribute,
                                    PRInt32 aModType,
                                    PRUint32 aStateMask)
{
  DoUpdate();
}

void
nsSVGPropertyBase::ContentAppended(nsIDocument *aDocument,
                                   nsIContent *aContainer,
                                   PRInt32 aNewIndexInContainer)
{
  DoUpdate();
}

void
nsSVGPropertyBase::ContentInserted(nsIDocument *aDocument,
                                   nsIContent *aContainer,
                                   nsIContent *aChild,
                                   PRInt32 aIndexInContainer)
{
  DoUpdate();
}

void
nsSVGPropertyBase::ContentRemoved(nsIDocument *aDocument,
                                  nsIContent *aContainer,
                                  nsIContent *aChild,
                                  PRInt32 aIndexInContainer)
{
  DoUpdate();
}

NS_IMPL_ISUPPORTS_INHERITED1(nsSVGFilterProperty,
                             nsSVGPropertyBase,
                             nsISVGFilterProperty)

nsSVGFilterProperty::nsSVGFilterProperty(nsIURI *aURI,
                                         nsIFrame *aFilteredFrame)
  : nsSVGPropertyBase(aURI, aFilteredFrame)
{
  UpdateRect();
}

void
nsSVGFilterProperty::UpdateRect()
{
  nsSVGFilterFrame *filter = GetFilterFrame(nsnull);
  if (filter) {
    mFilterRect = filter->GetInvalidationRegion(mFrame, mFrame->GetRect());
  } else {
    mFilterRect = nsRect();
  }
}

void
nsSVGFilterProperty::DoUpdate()
{
  nsSVGOuterSVGFrame *outerSVGFrame = nsSVGUtils::GetOuterSVGFrame(mFrame);
  if (outerSVGFrame) {
    outerSVGFrame->Invalidate(mFilterRect);
    UpdateRect();
    outerSVGFrame->Invalidate(mFilterRect);
  }
}

void
nsSVGFilterProperty::ParentChainChanged(nsIContent *aContent)
{
  if (aContent->IsInDoc())
    return;

  nsSVGOuterSVGFrame *outerSVGFrame = nsSVGUtils::GetOuterSVGFrame(mFrame);
  if (outerSVGFrame) {
    outerSVGFrame->InvalidateCoveredRegion(mFrame);
  }

  mFrame->DeleteProperty(nsGkAtoms::filter);
}

void
nsSVGClipPathProperty::DoUpdate()
{
  nsSVGOuterSVGFrame *outerSVGFrame = nsSVGUtils::GetOuterSVGFrame(mFrame);
  if (outerSVGFrame) {
    outerSVGFrame->InvalidateCoveredRegion(mFrame);
  }
}

void
nsSVGClipPathProperty::ParentChainChanged(nsIContent *aContent)
{
  if (aContent->IsInDoc())
    return;

  nsSVGOuterSVGFrame *outerSVGFrame = nsSVGUtils::GetOuterSVGFrame(mFrame);
  if (outerSVGFrame) {
    outerSVGFrame->InvalidateCoveredRegion(mFrame);
  }

  mFrame->DeleteProperty(nsGkAtoms::clipPath);
}

void
nsSVGMaskProperty::DoUpdate()
{
  nsSVGOuterSVGFrame *outerSVGFrame = nsSVGUtils::GetOuterSVGFrame(mFrame);
  if (outerSVGFrame) {
    outerSVGFrame->InvalidateCoveredRegion(mFrame);
  }
}

void
nsSVGMaskProperty::ParentChainChanged(nsIContent *aContent)
{
  if (aContent->IsInDoc())
    return;

  nsSVGOuterSVGFrame *outerSVGFrame = nsSVGUtils::GetOuterSVGFrame(mFrame);
  if (outerSVGFrame) {
    outerSVGFrame->InvalidateCoveredRegion(mFrame);
  }

  mFrame->DeleteProperty(nsGkAtoms::mask);
}

static nsSVGPropertyBase *
CreateFilterProperty(nsIURI *aURI, nsIFrame *aFrame)
{ return new nsSVGFilterProperty(aURI, aFrame); }

static nsSVGPropertyBase *
CreateMaskProperty(nsIURI *aURI, nsIFrame *aFrame)
{ return new nsSVGMaskProperty(aURI, aFrame); }

static nsSVGPropertyBase *
CreateClipPathProperty(nsIURI *aURI, nsIFrame *aFrame)
{ return new nsSVGClipPathProperty(aURI, aFrame); }

static nsSVGPropertyBase *
GetEffectProperty(nsIURI *aURI, nsIFrame *aFrame, nsIAtom *aProp,
                  nsSVGPropertyBase * (* aCreate)(nsIURI *, nsIFrame *))
{
  if (!aURI)
    return nsnull;
  nsSVGPropertyBase *prop = static_cast<nsSVGPropertyBase*>(aFrame->GetProperty(aProp));
  if (prop)
    return prop;
  prop = aCreate(aURI, aFrame);
  if (!prop)
    return nsnull;
  NS_ADDREF(prop);
  aFrame->SetProperty(aProp,
                      static_cast<nsISupports*>(prop),
                      nsPropertyTable::SupportsDtorFunc);
  return prop;
}

nsSVGEffects::EffectProperties
nsSVGEffects::GetEffectProperties(nsIFrame *aFrame)
{
  NS_ASSERTION(!aFrame->GetPrevContinuation(), "aFrame should be first continuation");

  EffectProperties result;
  const nsStyleSVGReset *style = aFrame->GetStyleSVGReset();
  result.mFilter = static_cast<nsSVGFilterProperty*>
    (GetEffectProperty(style->mFilter, aFrame, nsGkAtoms::filter, CreateFilterProperty));
  result.mClipPath = static_cast<nsSVGClipPathProperty*>
    (GetEffectProperty(style->mClipPath, aFrame, nsGkAtoms::clipPath, CreateClipPathProperty));
  result.mMask = static_cast<nsSVGMaskProperty*>
    (GetEffectProperty(style->mMask, aFrame, nsGkAtoms::mask, CreateMaskProperty));
  return result;
}

void
nsSVGEffects::UpdateEffects(nsIFrame *aFrame)
{
  aFrame->DeleteProperty(nsGkAtoms::filter);
  aFrame->DeleteProperty(nsGkAtoms::mask);
  aFrame->DeleteProperty(nsGkAtoms::clipPath);
}

nsSVGFilterProperty *
nsSVGEffects::GetFilterProperty(nsIFrame *aFrame)
{
  NS_ASSERTION(!aFrame->GetPrevContinuation(), "aFrame should be first continuation");

  if (!aFrame->GetStyleSVGReset()->mFilter)
    return nsnull;

  return static_cast<nsSVGFilterProperty *>(aFrame->GetProperty(nsGkAtoms::filter));
}
