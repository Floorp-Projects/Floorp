//
// Copyright (c) 2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Query.cpp: Implements the gl::Query class

#include "libANGLE/Query.h"
#include "libANGLE/renderer/QueryImpl.h"

namespace gl
{
Query::Query(rx::QueryImpl *impl, GLuint id) : RefCountObject(id), mQuery(impl), mLabel()
{
}

Query::~Query()
{
    SafeDelete(mQuery);
}

void Query::setLabel(const std::string &label)
{
    mLabel = label;
}

const std::string &Query::getLabel() const
{
    return mLabel;
}

Error Query::begin(const Context *context)
{
    return mQuery->begin(context);
}

Error Query::end(const Context *context)
{
    return mQuery->end(context);
}

Error Query::queryCounter(const Context *context)
{
    return mQuery->queryCounter(context);
}

Error Query::getResult(const Context *context, GLint *params)
{
    return mQuery->getResult(context, params);
}

Error Query::getResult(const Context *context, GLuint *params)
{
    return mQuery->getResult(context, params);
}

Error Query::getResult(const Context *context, GLint64 *params)
{
    return mQuery->getResult(context, params);
}

Error Query::getResult(const Context *context, GLuint64 *params)
{
    return mQuery->getResult(context, params);
}

Error Query::isResultAvailable(const Context *context, bool *available)
{
    return mQuery->isResultAvailable(context, available);
}

QueryType Query::getType() const
{
    return mQuery->getType();
}

rx::QueryImpl *Query::getImplementation()
{
    return mQuery;
}

const rx::QueryImpl *Query::getImplementation() const
{
    return mQuery;
}
}
