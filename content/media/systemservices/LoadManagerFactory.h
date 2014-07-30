/* -*- Mode: C++; tab-width: 50; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _LOADMANAGERFACTORY_H_
#define _LOADMANAGERFACTORY_H_

namespace mozilla {

class LoadManager;

mozilla::LoadManager* LoadManagerBuild();
void LoadManagerDestroy(mozilla::LoadManager* aLoadManager);

} //namespace

#endif /* _LOADMANAGERFACTORY_H_ */
