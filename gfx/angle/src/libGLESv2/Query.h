//
// Copyright (c) 2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Query.h: Defines the gl::Query class

#ifndef LIBGLESV2_QUERY_H_
#define LIBGLESV2_QUERY_H_

#include "common/angleutils.h"
#include "common/RefCountObject.h"

#include "angle_gl.h"

namespace rx
{
class QueryImpl;
}

namespace gl
{

class Query : public RefCountObject
{
  public:
    Query(rx::QueryImpl *impl, GLuint id);
    virtual ~Query();

    void begin();
    void end();

    GLuint getResult();
    GLboolean isResultAvailable();

    GLenum getType() const;
    bool isStarted() const;

  private:
    DISALLOW_COPY_AND_ASSIGN(Query);

    bool mStarted;

    rx::QueryImpl *mQuery;
};

}

#endif   // LIBGLESV2_QUERY_H_
