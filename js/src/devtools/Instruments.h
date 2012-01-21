/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Instruments_h__
#define Instruments_h__

#ifdef __APPLE__

namespace Instruments {

bool Start();
void Pause();
bool Resume();
void Stop(const char* profileName);

}

#endif /* __APPLE__ */

#endif /* Instruments_h__ */
