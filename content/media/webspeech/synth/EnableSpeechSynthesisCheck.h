/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_EnableSpeechSynthesisCheck_h
#define mozilla_dom_EnableSpeechSynthesisCheck_h

namespace mozilla {
namespace dom {

// This is a helper class which enables Web Speech to be enabled or disabled
// as whole.  Individual Web Speech object classes should inherit from this.
class EnableSpeechSynthesisCheck
{
public:
  static bool PrefEnabled();
};

}
}

#endif
