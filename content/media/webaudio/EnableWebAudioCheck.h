/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EnableWebAudioCheck_h_
#define EnableWebAudioCheck_h_

namespace mozilla {
namespace dom {

// This is a helper class which enables Web Audio to be enabled or disabled
// as whole.  Individual Web Audio object classes should inherit from this.
class EnableWebAudioCheck {
public:
  static bool PrefEnabled();
};

}
}

#endif

