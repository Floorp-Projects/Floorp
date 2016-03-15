//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Query11.h: Defines the rx::Query11 class which implements rx::QueryImpl.

#ifndef LIBANGLE_RENDERER_D3D_D3D11_QUERY11_H_
#define LIBANGLE_RENDERER_D3D_D3D11_QUERY11_H_

#include "libANGLE/renderer/QueryImpl.h"

namespace rx
{
class Renderer11;

class Query11 : public QueryImpl
{
  public:
    Query11(Renderer11 *renderer, GLenum type);
    virtual ~Query11();

    virtual gl::Error begin();
    virtual gl::Error end();
    virtual gl::Error queryCounter();
    virtual gl::Error getResult(GLint *params);
    virtual gl::Error getResult(GLuint *params);
    virtual gl::Error getResult(GLint64 *params);
    virtual gl::Error getResult(GLuint64 *params);
    virtual gl::Error isResultAvailable(bool *available);

  private:
    gl::Error testQuery();

    template <typename T>
    gl::Error getResultBase(T *params);

    GLuint64 mResult;

    bool mQueryFinished;

    Renderer11 *mRenderer;
    ID3D11Query *mQuery;
    ID3D11Query *mTimestampBeginQuery;
    ID3D11Query *mTimestampEndQuery;
};

}

#endif // LIBANGLE_RENDERER_D3D_D3D11_QUERY11_H_
