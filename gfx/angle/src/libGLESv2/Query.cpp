//
// Copyright (c) 2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Query.cpp: Implements the gl::Query class

#include "libGLESv2/Query.h"
#include "libGLESv2/renderer/QueryImpl.h"

namespace gl
{
Query::Query(rx::QueryImpl *impl, GLuint id)
    : RefCountObject(id),
      mQuery(impl)
{
}

Query::~Query()
{
    delete mQuery;
}

void Query::begin()
{
    // TODO: Rather than keeping track of whether the query was successfully
    // created via a boolean in the GL-level Query object, we should probably
    // use the error system to track these failed creations at the context level,
    // and reset the active query ID for the target to 0 upon failure.
    mStarted = mQuery->begin();
}

void Query::end()
{
    mQuery->end();
}

GLuint Query::getResult()
{
    return mQuery->getResult();
}

GLboolean Query::isResultAvailable()
{
    return mQuery->isResultAvailable();
}

GLenum Query::getType() const
{
    return mQuery->getType();
}

bool Query::isStarted() const
{
    return mStarted;
}

}
