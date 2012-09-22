/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "AudioNode.h"

namespace mozilla {
namespace dom {

class AudioSourceNode : public AudioNode
{
public:
  explicit AudioSourceNode(AudioContext* aContext);

  NS_DECL_ISUPPORTS_INHERITED

};

}
}

