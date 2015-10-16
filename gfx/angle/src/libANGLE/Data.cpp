//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Data.cpp: Container class for all GL relevant state, caps and objects

#include "libANGLE/Data.h"
#include "libANGLE/ResourceManager.h"

namespace gl
{

Data::Data(uintptr_t contextIn,
           GLint clientVersionIn,
           const State &stateIn,
           const Caps &capsIn,
           const TextureCapsMap &textureCapsIn,
           const Extensions &extensionsIn,
           const ResourceManager *resourceManagerIn)
    : context(contextIn),
      clientVersion(clientVersionIn),
      state(&stateIn),
      caps(&capsIn),
      textureCaps(&textureCapsIn),
      extensions(&extensionsIn),
      resourceManager(resourceManagerIn)
{}

Data::~Data()
{
}

}
