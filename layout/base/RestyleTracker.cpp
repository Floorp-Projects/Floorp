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
#include "RestyleManager.h"
#include "RestyleTrackerInlines.h"
#include "nsTransitionManager.h"
#include "mozilla/RestyleTimelineMarker.h"

namespace mozilla {

#ifdef RESTYLE_LOGGING
static nsCString
GetDocumentURI(nsIDocument* aDocument)
{
  nsCString result;
  nsString url;
  aDocument->GetDocumentURI(url);
  result.Append(NS_ConvertUTF16toUTF8(url).get());
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

struct LaterSiblingCollector {
  RestyleTracker* tracker;
  nsTArray< nsRefPtr<dom::Element> >* elements;
};

static PLDHashOperator
CollectLaterSiblings(nsISupports* aElement,
                     nsAutoPtr<RestyleTracker::RestyleData>& aData,
                     void* aSiblingCollector)
{
  dom::Element* element =
    static_cast<dom::Element*>(aElement);
  LaterSiblingCollector* collector =
    static_cast<LaterSiblingCollector*>(aSiblingCollector);
  // Only collect the entries that actually need restyling by us (and
  // haven't, for example, already been restyled).
  // It's important to not mess with the flags on entries not in our
  // document.
  if (element->GetCrossShadowCurrentDoc() == collector->tracker->Document() &&
      element->HasFlag(collector->tracker->RestyleBit()) &&
      (aData->mRestyleHint & eRestyle_LaterSiblings)) {
    collector->elements->AppendElement(element);
  }

  return PL_DHASH_NEXT;
}

struct RestyleEnumerateData : RestyleTracker::Hints {
  nsRefPtr<dom::Element> mElement;
#if defined(MOZ_ENABLE_PROFILER_SPS) && !defined(MOZILLA_XPCOMRT_API)
  UniquePtr<ProfilerBacktrace> mBacktrace;
#endif
};

struct RestyleCollector {
  RestyleTracker* tracker;
  RestyleEnumerateData** restyleArrayPtr;
#ifdef RESTYLE_LOGGING
  uint32_t count;
#endif
};

static PLDHashOperator
CollectRestyles(nsISupports* aElement,
                nsAutoPtr<RestyleTracker::RestyleData>& aData,
                void* aRestyleCollector)
{
  dom::Element* element =
    static_cast<dom::Element*>(aElement);
  RestyleCollector* collector =
    static_cast<RestyleCollector*>(aRestyleCollector);
  // Only collect the entries that actually need restyling by us (and
  // haven't, for example, already been restyled).
  // It's important to not mess with the flags on entries not in our
  // document.
  if (element->GetCrossShadowCurrentDoc() != collector->tracker->Document() ||
      !element->HasFlag(collector->tracker->RestyleBit())) {
    LOG_RESTYLE_IF(collector->tracker, true,
                   "skipping pending restyle %s, already restyled or no longer "
                   "in the document", FrameTagToString(element).get());
    return PL_DHASH_NEXT;
  }

  NS_ASSERTION(!element->HasFlag(collector->tracker->RootBit()) ||
               // Maybe we're just not reachable via the frame tree?
               (element->GetFlattenedTreeParent() &&
                (!element->GetFlattenedTreeParent()->GetPrimaryFrame() ||
                 element->GetFlattenedTreeParent()->GetPrimaryFrame()->IsLeaf() ||
                 element->GetCurrentDoc()->GetShell()->FrameManager()
                   ->GetDisplayContentsStyleFor(element))) ||
               // Or not reachable due to an async reinsert we have
               // pending?  If so, we'll have a reframe hint around.
               // That incidentally makes it safe that we still have
               // the bit, since any descendants that didn't get added
               // to the roots list because we had the bits will be
               // completely restyled in a moment.
               (aData->mChangeHint & nsChangeHint_ReconstructFrame),
               "Why did this not get handled while processing mRestyleRoots?");

  // Unset the restyle bits now, so if they get readded later as we
  // process we won't clobber that adding of the bit.
  element->UnsetFlags(collector->tracker->RestyleBit() |
                      collector->tracker->RootBit());

  RestyleEnumerateData** restyleArrayPtr = collector->restyleArrayPtr;
  RestyleEnumerateData* currentRestyle = *restyleArrayPtr;
  currentRestyle->mElement = element;
  currentRestyle->mRestyleHint = aData->mRestyleHint;
  currentRestyle->mChangeHint = aData->mChangeHint;
  // We can move aData since we'll be clearing mPendingRestyles after
  // we finish enumerating it.
  currentRestyle->mRestyleHintData = Move(aData->mRestyleHintData);
#if defined(MOZ_ENABLE_PROFILER_SPS) && !defined(MOZILLA_XPCOMRT_API)
  currentRestyle->mBacktrace = Move(aData->mBacktrace);
#endif

#ifdef RESTYLE_LOGGING
  collector->count++;
#endif

  // Increment to the next slot in the array
  *restyleArrayPtr = currentRestyle + 1;

  return PL_DHASH_NEXT;
}

inline void
RestyleTracker::ProcessOneRestyle(Element* aElement,
                                  nsRestyleHint aRestyleHint,
                                  nsChangeHint aChangeHint,
                                  const RestyleHintData& aRestyleHintData)
{
  NS_PRECONDITION((aRestyleHint & eRestyle_LaterSiblings) == 0,
                  "Someone should have handled this before calling us");
  NS_PRECONDITION(Document(), "Must have a document");
  NS_PRECONDITION(aElement->GetCrossShadowCurrentDoc() == Document(),
                  "Element has unexpected document");

  LOG_RESTYLE("aRestyleHint = %s, aChangeHint = %s",
              RestyleManager::RestyleHintToString(aRestyleHint).get(),
              RestyleManager::ChangeHintToString(aChangeHint).get());

  nsIFrame* primaryFrame = aElement->GetPrimaryFrame();

  if (aRestyleHint & ~eRestyle_LaterSiblings) {
#ifdef RESTYLE_LOGGING
    if (ShouldLogRestyle() && primaryFrame &&
        RestyleManager::StructsToLog() != 0) {
      LOG_RESTYLE("style context tree before restyle:");
      LOG_RESTYLE_INDENT();
      primaryFrame->StyleContext()->LogStyleContextTree(
          LoggingDepth(), RestyleManager::StructsToLog());
    }
#endif
    mRestyleManager->RestyleElement(aElement, primaryFrame, aChangeHint,
                                    *this, aRestyleHint, aRestyleHintData);
  } else if (aChangeHint &&
             (primaryFrame ||
              (aChangeHint & nsChangeHint_ReconstructFrame))) {
    // Don't need to recompute style; just apply the hint
    nsStyleChangeList changeList;
    changeList.AppendChange(primaryFrame, aElement, aChangeHint);
    mRestyleManager->ProcessRestyledFrames(changeList);
  }
}

void
RestyleTracker::DoProcessRestyles()
{
  nsAutoCString docURL;
  if (profiler_is_active()) {
    nsIURI *uri = Document()->GetDocumentURI();
    if (uri) {
      uri->GetSpec(docURL);
    } else {
      docURL = "N/A";
    }
  }
  PROFILER_LABEL_PRINTF("RestyleTracker", "ProcessRestyles",
                        js::ProfileEntry::Category::CSS, "(%s)", docURL.get());

  bool isTimelineRecording = false;
  nsDocShell* docShell =
    static_cast<nsDocShell*>(mRestyleManager->PresContext()->GetDocShell());
  if (docShell) {
    docShell->GetRecordProfileTimelineMarkers(&isTimelineRecording);
  }

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
    RestyleManager::ReframingStyleContexts
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
        nsAutoTArray<nsRefPtr<Element>, RESTYLE_ARRAY_STACKSIZE> laterSiblingArr;
        LaterSiblingCollector siblingCollector = { this, &laterSiblingArr };
        mPendingRestyles.Enumerate(CollectLaterSiblings, &siblingCollector);
        for (uint32_t i = 0; i < laterSiblingArr.Length(); ++i) {
          Element* element = laterSiblingArr[i];
          for (nsIContent* sibling = element->GetNextSibling();
               sibling;
               sibling = sibling->GetNextSibling()) {
            if (sibling->IsElement()) {
              LOG_RESTYLE("adding pending restyle for %s due to "
                          "eRestyle_LaterSiblings hint on %s",
                          FrameTagToString(sibling->AsElement()).get(),
                          FrameTagToString(element->AsElement()).get());
              if (AddPendingRestyle(sibling->AsElement(), eRestyle_Subtree,
                                    NS_STYLE_HINT_NONE)) {
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
        nsRefPtr<Element> element;
        element.swap(mRestyleRoots[rootCount - 1]);
        mRestyleRoots.RemoveElementAt(rootCount - 1);

        LOG_RESTYLE("processing style root %s at index %d",
                    FrameTagToString(element).get(), rootCount - 1);
        LOG_RESTYLE_INDENT();

        // Do the document check before calling GetRestyleData, since we
        // don't want to do the sibling-processing GetRestyleData does if
        // the node is no longer relevant.
        if (element->GetCrossShadowCurrentDoc() != Document()) {
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
          UniquePtr<TimelineMarker> marker = MakeUnique<RestyleTimelineMarker>(
            data->mRestyleHint, TRACING_INTERVAL_START);
          TimelineConsumers::AddMarkerForDocShell(docShell, Move(marker));
        }

#if defined(MOZ_ENABLE_PROFILER_SPS) && !defined(MOZILLA_XPCOMRT_API)
        Maybe<GeckoProfilerTracingRAII> profilerRAII;
        if (profiler_feature_active("restyle")) {
          profilerRAII.emplace("Paint", "Styles", Move(data->mBacktrace));
        }
#endif
        ProcessOneRestyle(element, data->mRestyleHint, data->mChangeHint,
                          data->mRestyleHintData);
        AddRestyleRootsIfAwaitingRestyle(data->mDescendants);

        if (isTimelineRecording) {
          UniquePtr<TimelineMarker> marker = MakeUnique<RestyleTimelineMarker>(
            data->mRestyleHint, TRACING_INTERVAL_END);
          TimelineConsumers::AddMarkerForDocShell(docShell, Move(marker));
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
      nsAutoTArray<RestyleEnumerateData, RESTYLE_ARRAY_STACKSIZE> restyleArr;
      RestyleEnumerateData* restylesToProcess =
        restyleArr.AppendElements(mPendingRestyles.Count());
      if (restylesToProcess) {
        RestyleEnumerateData* lastRestyle = restylesToProcess;
        RestyleCollector collector = { this, &lastRestyle };
        mPendingRestyles.Enumerate(CollectRestyles, &collector);

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
                      index++, collector.count);
          LOG_RESTYLE_INDENT();

#if defined(MOZ_ENABLE_PROFILER_SPS) && !defined(MOZILLA_XPCOMRT_API)
          Maybe<GeckoProfilerTracingRAII> profilerRAII;
          if (profiler_feature_active("restyle")) {
            profilerRAII.emplace("Paint", "Styles", Move(currentRestyle->mBacktrace));
          }
#endif
          if (isTimelineRecording) {
            UniquePtr<TimelineMarker> marker = MakeUnique<RestyleTimelineMarker>(
              currentRestyle->mRestyleHint, TRACING_INTERVAL_START);
            TimelineConsumers::AddMarkerForDocShell(docShell, Move(marker));
          }

          ProcessOneRestyle(currentRestyle->mElement,
                            currentRestyle->mRestyleHint,
                            currentRestyle->mChangeHint,
                            currentRestyle->mRestyleHintData);

          if (isTimelineRecording) {
            UniquePtr<TimelineMarker> marker = MakeUnique<RestyleTimelineMarker>(
              currentRestyle->mRestyleHint, TRACING_INTERVAL_END);
            TimelineConsumers::AddMarkerForDocShell(docShell, Move(marker));
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
  NS_PRECONDITION(aElement->GetCrossShadowCurrentDoc() == Document(),
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
    NS_ASSERTION(aData->mDescendants.IsEmpty(),
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
                                   const nsTArray<nsRefPtr<Element>>& aElements)
{
  // The RestyleData for a given element has stored in mDescendants
  // the list of descendants we need to end up restyling.  Since we
  // won't necessarily end up restyling them, due to the restyle
  // process finishing early (see how eRestyleResult_Stop is handled
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
