/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioNode.h"
#include "AudioContext.h"
#include "nsContentUtils.h"
#include "mozilla/ErrorResult.h"

namespace mozilla {
namespace dom {

inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            mozilla::dom::AudioNode::Output& aField,
                            const char* aName,
                            unsigned aFlags)
{
  CycleCollectionNoteChild(aCallback, aField.mDestination.get(), aName, aFlags);
}

inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            mozilla::dom::AudioNode::Input& aField,
                            const char* aName,
                            unsigned aFlags)
{
  CycleCollectionNoteChild(aCallback, aField.mSource.get(), aName, aFlags);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_3(AudioNode,
                                        mContext, mInputs, mOutputs)

NS_IMPL_CYCLE_COLLECTING_ADDREF(AudioNode)
NS_IMPL_CYCLE_COLLECTING_RELEASE(AudioNode)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AudioNode)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

AudioNode::AudioNode(AudioContext* aContext)
  : mContext(aContext)
{
  MOZ_ASSERT(aContext);
  SetIsDOMBinding();
}

void
AudioNode::Connect(AudioNode& aDestination, uint32_t aOutput,
                   uint32_t aInput, ErrorResult& aRv)
{
  if (aOutput >= MaxNumberOfOutputs() ||
      aInput >= aDestination.MaxNumberOfInputs()) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  if (Context() != aDestination.Context()) {
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return;
  }

  // XXX handle cycle detection per spec

  Output output(&aDestination, aInput);
  mOutputs.EnsureLengthAtLeast(aOutput + 1);
  mOutputs.ReplaceElementAt(aOutput, output);
  Input input(this, aOutput);
  aDestination.mInputs.EnsureLengthAtLeast(aInput + 1);
  aDestination.mInputs.ReplaceElementAt(aInput, input);
}

void
AudioNode::Disconnect(uint32_t aOutput, ErrorResult& aRv)
{
  if (aOutput >= NumberOfOutputs()) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  // We do a copy of the objects to AddRef source and destination
  // objects so that they don't go away before we're done here.
  const Output output = mOutputs[aOutput];
  if (!output) {
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return;
  }
  const Input input = output.mDestination->mInputs[output.mInput];

  MOZ_ASSERT(Context() == output.mDestination->Context());
  MOZ_ASSERT(aOutput == input.mOutput);

  if (!input || input.mSource != this) {
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return;
  }

  output.mDestination->mInputs.ReplaceElementAt(output.mInput, Input());
  mOutputs.ReplaceElementAt(input.mOutput, Output());
}

}
}

