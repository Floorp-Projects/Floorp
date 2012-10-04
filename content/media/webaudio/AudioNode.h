/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "nsWrapperCache.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/Attributes.h"
#include "EnableWebAudioCheck.h"
#include "nsAutoPtr.h"
#include "nsTArray.h"
#include "AudioContext.h"

struct JSContext;

namespace mozilla {

class ErrorResult;

namespace dom {

class AudioNode : public nsISupports,
                  public nsWrapperCache,
                  public EnableWebAudioCheck
{
public:
  explicit AudioNode(AudioContext* aContext);
  virtual ~AudioNode() {}

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(AudioNode)

  AudioContext* GetParentObject() const
  {
    return mContext;
  }

  AudioContext* Context() const
  {
    return mContext;
  }

  void Connect(AudioNode& aDestination, uint32_t aOutput,
               uint32_t aInput, ErrorResult& aRv);

  void Disconnect(uint32_t aOutput, ErrorResult& aRv);

  uint32_t NumberOfInputs() const
  {
    return mInputs.Length();
  }
  uint32_t NumberOfOutputs() const
  {
    return mOutputs.Length();
  }

  // The following two virtual methods must be implemented by each node type
  // to provide the maximum number of input and outputs they accept.
  virtual uint32_t MaxNumberOfInputs() const = 0;
  virtual uint32_t MaxNumberOfOutputs() const = 0;

  struct Output {
    enum { InvalidIndex = 0xffffffff };
    Output()
      : mInput(InvalidIndex)
    {
    }
    Output(AudioNode* aDestination, uint32_t aInput)
      : mDestination(aDestination),
        mInput(aInput)
    {
    }

    // Check whether the slot is valid
    typedef void**** ConvertibleToBool;
    operator ConvertibleToBool() const {
      return ConvertibleToBool(mDestination && mInput != InvalidIndex);
    }

    // Needed for the CC traversal
    AudioNode* get() const {
      return mDestination;
    }

    nsRefPtr<AudioNode> mDestination;
    // This is an index into mDestination->mInputs which specifies the Input
    // object corresponding to this Output node.
    const uint32_t mInput;
  };
  struct Input {
    enum { InvalidIndex = 0xffffffff };
    Input()
      : mOutput(InvalidIndex)
    {
    }
    Input(AudioNode* aSource, uint32_t aOutput)
      : mSource(aSource),
        mOutput(aOutput)
    {
    }

    // Check whether the slot is valid
    typedef void**** ConvertibleToBool;
    operator ConvertibleToBool() const {
      return ConvertibleToBool(mSource && mOutput != InvalidIndex);
    }

    // Needed for the CC traversal
    AudioNode* get() const {
      return mSource;
    }

    nsRefPtr<AudioNode> mSource;
    // This is an index into mSource->mOutputs which specifies the Output
    // object corresponding to this Input node.
    const uint32_t mOutput;
  };

private:
  nsRefPtr<AudioContext> mContext;
  nsTArray<Input> mInputs;
  nsTArray<Output> mOutputs;
};

}
}

