/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsBlockDebugFlags_h__
#define nsBlockDebugFlags_h__

#undef NOISY_FIRST_LETTER  // enables debug output for first-letter specific
                           // layout
#undef NOISY_FLOAT  // enables debug output for float reflow (the in/out metrics
                    // for the floated block)
#undef NOISY_FLOAT_CLEARING
#undef NOISY_FINAL_SIZE  // enables debug output for desired width/height
                         // computation, once all children have been reflowed
#undef NOISY_REMOVE_FRAME
#undef NOISY_COMBINED_AREA  // enables debug output for combined area
                            // computation
#undef NOISY_BLOCK_DIR_MARGINS
#undef NOISY_REFLOW_REASON     // gives a little info about why each reflow was
                               // requested
#undef REFLOW_STATUS_COVERAGE  // I think this is most useful for printing, to
                               // see which frames return "incomplete"
#undef NOISY_BLOCK_INVALIDATE  // enables debug output for all calls to
                               // invalidate
#undef REALLY_NOISY_REFLOW     // some extra debug info

#endif  // nsBlockDebugFlags_h__
