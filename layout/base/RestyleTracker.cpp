/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A class which manages pending restyles.  This handles keeping track
 * of what nodes restyles need to happen on and so forth.
 */

#include "RestyleTracker.h"

#include "GeckoProfiler.h"
#include "nsDocShell.h"
#include "nsFrameManager.h"
#include "nsIDocument.h"
#include "nsStyleChangeList.h"
#include "mozilla/GeckoRestyleManager.h"
#include "RestyleTrackerInlines.h"
#include "nsTransitionManager.h"
#include "mozilla/RestyleTimelineMarker.h"

namespace mozilla {

#ifdef RESTYLE_LOGGING
static nsCString
GetDocumentURI(nsIDocument* aDocument)
{
  nsCString result;
  nsAutoString url;
  nsresult rv = aDocument->GetDocumentURI(url);
  if (NS_SUCCEEDED(rv)) {
    result.Append(NS_ConvertUTF16toUTF8(url).get());
  }

  return result;
}

static nsCString
FrameTagToString(dom::Element* aElement)
{
  nsCString result;
  nsIFrame* frame = aElement->GetPrimaryFrame();
  if (frame) {
    nsFrame::ListTag(result, frame);
  } else {
    nsAutoString buf;
    aElement->NodeInfo()->NameAtom()->ToString(buf);
    result.AppendPrintf("(%s@%p)", NS_ConvertUTF16toUTF8(buf).get(), aElement);
  }
  return result;
}
#endif

inline nsIDocument*
RestyleTracker::Document() const {
  return mRestyleManager->PresContext()->Document();
}

#define RESTYLE_ARRAY_STACKSIZE 128

struct RestyleEnumerateData : RestyleTracker::Hints {
  RefPtr<dom::Element> mElement;
  UniqueProfilerBacktrace mBacktrace;
};

inline void
RestyleTracker::ProcessOneRestyle(Element* aElement,
                                  nsRestyleHint aRestyleHint,
                                  nsChangeHint aChangeHint,
                                  const RestyleHintData& aRestyleHintData)
{
  NS_PRECONDITION((aRestyleHint & eRestyle_LaterSiblings) == 0,
                  "Someone should have handled this before calling us");
  NS_PRECONDITION(Document(), "Must have a document");
  NS_PRECONDITION(aElement->GetComposedDoc() == Document(),
                  "Element has unexpected document");

  LOG_RESTYLE("aRestyleHint = %s, aChangeHint = %s",
              GeckoRestyleManager::RestyleHintToString(aRestyleHint).get(),
              GeckoRestyleManager::ChangeHintToString(aChangeHint).get());

  nsIFrame* primaryFrame = aElement->GetPrimaryFrame();

  if (aRestyleHint & ~eRestyle_LaterSiblings) {
#ifdef RESTYLE_LOGGING
    if (ShouldLogRestyle() && primaryFrame &&
        GeckoRestyleManager::StructsToLog() != 0) {
      LOG_RESTYLE("style context tree before restyle:");
      LOG_RESTYLE_INDENT();
      primaryFrame->StyleContext()->AsGecko()->LogStyleContextTree(
          LoggingDepth(), GeckoRestyleManager::StructsToLog());
    }
#endif
    mRestyleManager->RestyleElement(aElement, primaryFrame, aChangeHint,
                                    *this, aRestyleHint, aRestyleHintData);
  } else if (aChangeHint &&
             (primaryFrame ||
              (aChangeHint & nsChangeHint_ReconstructFrame))) {
    // Don't need to recompute style; just apply the hint
    nsStyleChangeList changeList(StyleBackendType::Gecko);
    changeList.AppendChange(primaryFrame, aElement, aChangeHint);
    mRestyleManager->ProcessRestyledFrames(changeList);
  }
}

void
RestyleTracker::DoProcessRestyles()
{
  nsAutoCString docURL("N/A");
  if (profiler_is_active()) {
    nsIURI *uri = Document()->GetDocumentURI();
    if (uri) {
      docURL = uri->GetSpecOrDefault();
    }
  }
  PROFILER_LABEL_DYNAMIC("RestyleTracker", "ProcessRestyles",
                         js::ProfileEntry::Category::CSS, docURL.get());

  nsDocShell* docShell = static_cast<nsDocShell*>(mRestyleManager->PresContext()->GetDocShell());
  RefPtr<TimelineConsumers> timelines = TimelineConsumers::Get();
  bool isTimelineRecording = timelines && timelines->HasConsumer(docShell);

  // Create a AnimationsWithDestroyedFrame during restyling process to
  // stop animations and transitions on elements that have no frame at the end
  // of the restyling process.
  RestyleManager::AnimationsWithDestroyedFrame
    animationsWithDestroyedFrame(mRestyleManager);

  // Create a ReframingStyleContexts struct on the stack and put it in our
  // mReframingStyleContexts for almost all of the remaining scope of
  // this function.
  //
  // It needs to be *in* scope during BeginProcessingRestyles, which
  // might (if mDoRebuildAllStyleData is true) do substantial amounts of
  // restyle processing.
  //
  // However, it needs to be *out* of scope during
  // EndProcessingRestyles, since we should release the style contexts
  // it holds prior to any EndReconstruct call that
  // EndProcessingRestyles makes.  This is because in EndReconstruct we
  // try to destroy the old rule tree using the GC mechanism, which
  // means it only gets destroyed if it's unreferenced (and if it's
  // referenced, we assert).  So we want the ReframingStyleContexts
  // (which holds old style contexts) to be destroyed before the
  // EndReconstruct so those style contexts go away before
  // EndReconstruct.
  {
    GeckoRestyleManager::ReframingStyleContexts
      reframingStyleContexts(mRestyleManager);

    mRestyleManager->BeginProcessingRestyles(*this);

    LOG_RESTYLE("Processing %d pending %srestyles with %d restyle roots for %s",
                mPendingRestyles.Count(),
                mRestyleManager->PresContext()->TransitionManager()->
                  InAnimationOnlyStyleUpdate()
                  ? (const char*) "animation " : (const char*) "",
                static_cast<int>(mRestyleRoots.Length()),
                GetDocumentURI(Document()).get());
    LOG_RESTYLE_INDENT();

    // loop so that we process any restyle events generated by processing
    while (mPendingRestyles.Count()) {
      if (mHaveLaterSiblingRestyles) {
        // Convert them to individual restyles on all the later siblings
        AutoTArray<RefPtr<Element>, RESTYLE_ARRAY_STACKSIZE> laterSiblingArr;
        for (auto iter = mPendingRestyles.Iter(); !iter.Done(); iter.Next()) {
          auto element = static_cast<dom::Element*>(iter.Key());
          MOZ_ASSERT(!element->IsStyledByServo(),
                     "Should not have Servo-styled elements here");
          // Only collect the entries that actually need restyling by us (and
          // haven't, for example, already been restyled).
          // It's important to not mess with the flags on entries not in our
          // document.
          if (element->GetComposedDoc() == Document() &&
              element->HasFlag(RestyleBit()) &&
              (iter.Data()->mRestyleHint & eRestyle_LaterSiblings)) {
            laterSiblingArr.AppendElement(element);
          }
        }
        for (uint32_t i = 0; i < laterSiblingArr.Length(); ++i) {
          Element* element = laterSiblingArr[i];
          MOZ_ASSERT(!element->IsStyledByServo());
          for (nsIContent* sibling = element->GetNextSibling();
               sibling;
               sibling = sibling->GetNextSibling()) {
            if (sibling->IsElement()) {
              LOG_RESTYLE("adding pending restyle for %s due to "
                          "eRestyle_LaterSiblings hint on %s",
                          FrameTagToString(sibling->AsElement()).get(),
                          FrameTagToString(element->AsElement()).get());
              if (AddPendingRestyle(sibling->AsElement(), eRestyle_Subtree,
                                    nsChangeHint(0))) {
                  // Nothing else to do here; we'll handle the following
                  // siblings when we get to |sibling| in laterSiblingArr.
                break;
              }
            }
          }
        }

        // Now remove all those eRestyle_LaterSiblings bits
        for (uint32_t i = 0; i < laterSiblingArr.Length(); ++i) {
          Element* element = laterSiblingArr[i];
          NS_ASSERTION(element->HasFlag(RestyleBit()), "How did that happen?");
          RestyleData* data;
#ifdef DEBUG
          bool found =
#endif
            mPendingRestyles.Get(element, &data);
          NS_ASSERTION(found, "Where did our entry go?");
          data->mRestyleHint =
            nsRestyleHint(data->mRestyleHint & ~eRestyle_LaterSiblings);
        }

        LOG_RESTYLE("%d pending restyles after expanding out "
                    "eRestyle_LaterSiblings", mPendingRestyles.Count());

        mHaveLaterSiblingRestyles = false;
      }

      uint32_t rootCount;
      while ((rootCount = mRestyleRoots.Length())) {
        // Make sure to pop the element off our restyle root array, so
        // that we can freely append to the array as we process this
        // element.
        RefPtr<Element> element;
        element.swap(mRestyleRoots[rootCount - 1]);
        mRestyleRoots.RemoveElementAt(rootCount - 1);

        LOG_RESTYLE("processing style root %s at index %d",
                    FrameTagToString(element).get(), rootCount - 1);
        LOG_RESTYLE_INDENT();

        // Do the document check before calling GetRestyleData, since we
        // don't want to do the sibling-processing GetRestyleData does if
        // the node is no longer relevant.
        if (element->GetComposedDoc() != Document()) {
          // Content node has been removed from our document; nothing else
          // to do here
          LOG_RESTYLE("skipping, no longer in the document");
          continue;
        }

        nsAutoPtr<RestyleData> data;
        if (!GetRestyleData(element, data)) {
          LOG_RESTYLE("skipping, already restyled");
          continue;
        }

        if (isTimelineRecording) {
          timelines->AddMarkerForDocShell(docShell, Move(
            MakeUnique<RestyleTimelineMarker>(
              data->mRestyleHint, MarkerTracingType::START)));
        }

        Maybe<AutoProfilerTracing> tracing;
        if (profiler_feature_active(ProfilerFeature::Restyle)) {
          tracing.emplace("Paint", "Styles", Move(data->mBacktrace));
        }
        ProcessOneRestyle(element, data->mRestyleHint, data->mChangeHint,
                          data->mRestyleHintData);
        AddRestyleRootsIfAwaitingRestyle(data->mDescendants);

        if (isTimelineRecording) {
          timelines->AddMarkerForDocShell(docShell, Move(
            MakeUnique<RestyleTimelineMarker>(
              data->mRestyleHint, MarkerTracingType::END)));
        }
      }

      if (mHaveLaterSiblingRestyles) {
        // Keep processing restyles for now
        continue;
      }

      // Now we only have entries with change hints left.  To be safe in
      // case of reentry from the handing of the change hint, use a
      // scratch array instead of calling out to ProcessOneRestyle while
      // enumerating the hashtable.  Use the stack if we can, otherwise
      // fall back on heap-allocation.
      AutoTArray<RestyleEnumerateData, RESTYLE_ARRAY_STACKSIZE> restyleArr;
      RestyleEnumerateData* restylesToProcess =
        restyleArr.AppendElements(mPendingRestyles.Count());
      if (restylesToProcess) {
        RestyleEnumerateData* restyle = restylesToProcess;
#ifdef RESTYLE_LOGGING
        uint32_t count = 0;
#endif
        for (auto iter = mPendingRestyles.Iter(); !iter.Done(); iter.Next()) {
          auto element = static_cast<dom::Element*>(iter.Key());
          RestyleTracker::RestyleData* data = iter.Data();

          // Only collect the entries that actually need restyling by us (and
          // haven't, for example, already been restyled).
          // It's important to not mess with the flags on entries not in our
          // document.
          if (element->GetComposedDoc() != Document() ||
              !element->HasFlag(RestyleBit())) {
            LOG_RESTYLE("skipping pending restyle %s, already restyled or no "
                        "longer in the document",
                        FrameTagToString(element).get());
            continue;
          }

          NS_ASSERTION(
            !element->HasFlag(RootBit()) ||
            // Maybe we're just not reachable via the frame tree?
            (element->GetFlattenedTreeParent() &&
             (!element->GetFlattenedTreeParent()->GetPrimaryFrame() ||
              element->GetFlattenedTreeParent()->GetPrimaryFrame()->IsLeaf() ||
              element->GetComposedDoc()->GetShell()->FrameManager()
                ->GetDisplayContentsStyleFor(element))) ||
            // Or not reachable due to an async reinsert we have
            // pending?  If so, we'll have a reframe hint around.
            // That incidentally makes it safe that we still have
            // the bit, since any descendants that didn't get added
            // to the roots list because we had the bits will be
            // completely restyled in a moment.
            (data->mChangeHint & nsChangeHint_ReconstructFrame),
            "Why did this not get handled while processing mRestyleRoots?");

          // Unset the restyle bits now, so if they get readded later as we
          // process we won't clobber that adding of the bit.
          element->UnsetFlags(RestyleBit() |
                              RootBit() |
                              ConditionalDescendantsBit());

          restyle->mElement = element;
          restyle->mRestyleHint = data->mRestyleHint;
          restyle->mChangeHint = data->mChangeHint;
          // We can move data since we'll be clearing mPendingRestyles after
          // we finish enumerating it.
          restyle->mRestyleHintData = Move(data->mRestyleHintData);
          restyle->mBacktrace = Move(data->mBacktrace);

#ifdef RESTYLE_LOGGING
          count++;
#endif

          // Increment to the next slot in the array
          restyle++;
        }

        RestyleEnumerateData* lastRestyle = restyle;

        // Clear the hashtable now that we don't need it anymore
        mPendingRestyles.Clear();

#ifdef RESTYLE_LOGGING
        uint32_t index = 0;
#endif
        for (RestyleEnumerateData* currentRestyle = restylesToProcess;
             currentRestyle != lastRestyle;
             ++currentRestyle) {
          LOG_RESTYLE("processing pending restyle %s at index %d/%d",
                      FrameTagToString(currentRestyle->mElement).get(),
                      index++, count);
          LOG_RESTYLE_INDENT();

          Maybe<AutoProfilerTracing> tracing;
          if (profiler_feature_active(ProfilerFeature::Restyle)) {
            tracing.emplace("Paint", "Styles", Move(currentRestyle->mBacktrace));
          }
          if (isTimelineRecording) {
            timelines->AddMarkerForDocShell(docShell, Move(
              MakeUnique<RestyleTimelineMarker>(
                currentRestyle->mRestyleHint, MarkerTracingType::START)));
          }

          ProcessOneRestyle(currentRestyle->mElement,
                            currentRestyle->mRestyleHint,
                            currentRestyle->mChangeHint,
                            currentRestyle->mRestyleHintData);

          if (isTimelineRecording) {
            timelines->AddMarkerForDocShell(docShell, Move(
              MakeUnique<RestyleTimelineMarker>(
                currentRestyle->mRestyleHint, MarkerTracingType::END)));
          }
        }
      }
    }
  }

  // mPendingRestyles is now empty.
  mHaveSelectors = false;

  mRestyleManager->EndProcessingRestyles();
}

bool
RestyleTracker::GetRestyleData(Element* aElement, nsAutoPtr<RestyleData>& aData)
{
  NS_PRECONDITION(aElement->GetComposedDoc() == Document(),
                  "Unexpected document; this will lead to incorrect behavior!");

  if (!aElement->HasFlag(RestyleBit())) {
    NS_ASSERTION(!aElement->HasFlag(RootBit()), "Bogus root bit?");
    return false;
  }

  mPendingRestyles.RemoveAndForget(aElement, aData);
  NS_ASSERTION(aData.get(), "Must have data if restyle bit is set");

  if (aData->mRestyleHint & eRestyle_LaterSiblings) {
    // Someone readded the eRestyle_LaterSiblings hint for this
    // element.  Leave it around for now, but remove the other restyle
    // hints and the change hint for it.  Also unset its root bit,
    // since it's no longer a root with the new restyle data.

    // During a normal restyle, we should have already processed the
    // mDescendants array the last time we processed the restyle
    // for this element.  But in RebuildAllStyleData, we don't initially
    // expand out eRestyle_LaterSiblings, so we can get in here the
    // first time we need to process a restyle for this element.  In that
    // case, it's fine for us to have a non-empty mDescendants, since
    // we know that RebuildAllStyleData adds eRestyle_ForceDescendants
    // and we're guaranteed we'll restyle the entire tree.
    NS_ASSERTION(mRestyleManager->InRebuildAllStyleData() ||
                 aData->mDescendants.IsEmpty(),
                 "expected descendants to be handled by now");

    RestyleData* newData = new RestyleData;
    newData->mChangeHint = nsChangeHint(0);
    newData->mRestyleHint = eRestyle_LaterSiblings;
    mPendingRestyles.Put(aElement, newData);
    aElement->UnsetFlags(RootBit());
    aData->mRestyleHint =
      nsRestyleHint(aData->mRestyleHint & ~eRestyle_LaterSiblings);
  } else {
    aElement->UnsetFlags(mRestyleBits);
  }

  return true;
}

void
RestyleTracker::AddRestyleRootsIfAwaitingRestyle(
                                   const nsTArray<RefPtr<Element>>& aElements)
{
  // The RestyleData for a given element has stored in mDescendants
  // the list of descendants we need to end up restyling.  Since we
  // won't necessarily end up restyling them, due to the restyle
  // process finishing early (see how RestyleResult::eStop is handled
  // in ElementRestyler::Restyle), we add them to the list of restyle
  // roots to handle the next time around the
  // RestyleTracker::DoProcessRestyles loop.
  //
  // Note that aElements must maintain the same invariant
  // that mRestyleRoots does, i.e. that ancestors appear after descendants.
  // Since we call AddRestyleRootsIfAwaitingRestyle only after we have
  // removed the restyle root we are currently processing from the end of
  // mRestyleRoots, and the only elements we get here in aElements are
  // descendants of that restyle root, we are safe to simply append to the
  // end of mRestyleRoots to maintain its invariant.
  for (size_t i = 0; i < aElements.Length(); i++) {
    Element* element = aElements[i];
    if (element->HasFlag(RestyleBit())) {
      mRestyleRoots.AppendElement(element);
    }
  }
}

void
RestyleTracker::ClearSelectors()
{
  if (!mHaveSelectors) {
    return;
  }
  for (auto it = mPendingRestyles.Iter(); !it.Done(); it.Next()) {
    RestyleData* data = it.Data();
    if (data->mRestyleHint & eRestyle_SomeDescendants) {
      data->mRestyleHint =
        (data->mRestyleHint & ~eRestyle_SomeDescendants) | eRestyle_Subtree;
      data->mRestyleHintData.mSelectorsForDescendants.Clear();
    } else {
      MOZ_ASSERT(data->mRestyleHintData.mSelectorsForDescendants.IsEmpty());
    }
  }
  mHaveSelectors = false;
}

} // namespace mozilla
