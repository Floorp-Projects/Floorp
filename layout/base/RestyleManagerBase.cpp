/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/RestyleManagerBase.h"
#include "mozilla/StyleSetHandle.h"
#include "nsIFrame.h"

namespace mozilla {

RestyleManagerBase::RestyleManagerBase(nsPresContext* aPresContext)
  : mPresContext(aPresContext)
  , mRestyleGeneration(1)
  , mHoverGeneration(0)
  , mObservingRefreshDriver(false)
{
  MOZ_ASSERT(mPresContext);
}

/**
 * Calculates the change hint and the restyle hint for a given content state
 * change.
 *
 * This is called from both Restyle managers.
 */
void
RestyleManagerBase::ContentStateChangedInternal(Element* aElement,
                                                EventStates aStateMask,
                                                nsChangeHint* aOutChangeHint,
                                                nsRestyleHint* aOutRestyleHint)
{
  MOZ_ASSERT(aOutChangeHint);
  MOZ_ASSERT(aOutRestyleHint);

  StyleSetHandle styleSet = PresContext()->StyleSet();
  NS_ASSERTION(styleSet, "couldn't get style set");

  *aOutChangeHint = nsChangeHint(0);
  // Any change to a content state that affects which frames we construct
  // must lead to a frame reconstruct here if we already have a frame.
  // Note that we never decide through non-CSS means to not create frames
  // based on content states, so if we already don't have a frame we don't
  // need to force a reframe -- if it's needed, the HasStateDependentStyle
  // call will handle things.
  nsIFrame* primaryFrame = aElement->GetPrimaryFrame();
  CSSPseudoElementType pseudoType = CSSPseudoElementType::NotPseudo;
  if (primaryFrame) {
    // If it's generated content, ignore LOADING/etc state changes on it.
    if (!primaryFrame->IsGeneratedContentFrame() &&
        aStateMask.HasAtLeastOneOfStates(NS_EVENT_STATE_BROKEN |
                                         NS_EVENT_STATE_USERDISABLED |
                                         NS_EVENT_STATE_SUPPRESSED |
                                         NS_EVENT_STATE_LOADING)) {
      *aOutChangeHint = nsChangeHint_ReconstructFrame;
    } else {
      uint8_t app = primaryFrame->StyleDisplay()->mAppearance;
      if (app) {
        nsITheme *theme = PresContext()->GetTheme();
        if (theme && theme->ThemeSupportsWidget(PresContext(),
                                                primaryFrame, app)) {
          bool repaint = false;
          theme->WidgetStateChanged(primaryFrame, app, nullptr, &repaint, nullptr);
          if (repaint) {
            *aOutChangeHint |= nsChangeHint_RepaintFrame;
          }
        }
      }
    }

    pseudoType = primaryFrame->StyleContext()->GetPseudoType();

    primaryFrame->ContentStatesChanged(aStateMask);
  }

  if (pseudoType >= CSSPseudoElementType::Count) {
    *aOutRestyleHint = styleSet->HasStateDependentStyle(aElement, aStateMask);
  } else if (nsCSSPseudoElements::PseudoElementSupportsUserActionState(
                                                                  pseudoType)) {
    // If aElement is a pseudo-element, we want to check to see whether there
    // are any state-dependent rules applying to that pseudo.
    Element* ancestor = ElementForStyleContext(nullptr, primaryFrame,
                                               pseudoType);
    *aOutRestyleHint = styleSet->HasStateDependentStyle(ancestor, pseudoType, aElement,
                                                       aStateMask);
  } else {
    *aOutRestyleHint = nsRestyleHint(0);
  }

  if (aStateMask.HasState(NS_EVENT_STATE_HOVER) && *aOutRestyleHint != 0) {
    IncrementHoverGeneration();
  }

  if (aStateMask.HasState(NS_EVENT_STATE_VISITED)) {
    // Exposing information to the page about whether the link is
    // visited or not isn't really something we can worry about here.
    // FIXME: We could probably do this a bit better.
    *aOutChangeHint |= nsChangeHint_RepaintFrame;
  }
}

} // namespace mozilla
