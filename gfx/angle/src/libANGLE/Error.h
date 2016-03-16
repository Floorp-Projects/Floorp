//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Error.h: Defines the egl::Error and gl::Error classes which encapsulate API errors
// and optional error messages.

#ifndef LIBANGLE_ERROR_H_
#define LIBANGLE_ERROR_H_

#include "angle_gl.h"
#include <EGL/egl.h>

#include <string>
#include <memory>

namespace gl
{

class Error final
{
  public:
    explicit inline Error(GLenum errorCode);
    Error(GLenum errorCode, const char *msg, ...);
    Error(GLenum errorCode, GLuint id, const char *msg, ...);
    inline Error(const Error &other);
    inline Error(Error &&other);

    inline Error &operator=(const Error &other);
    inline Error &operator=(Error &&other);

    inline GLenum getCode() const;
    inline GLuint getID() const;
    inline bool isError() const;

    const std::string &getMessage() const;

    // Useful for mocking and testing
    bool operator==(const Error &other) const;
    bool operator!=(const Error &other) const;

  private:
    void createMessageString() const;

    GLenum mCode;
    GLuint mID;
    mutable std::unique_ptr<std::string> mMessage;
};

template <typename T>
class ErrorOrResult
{
  public:
    ErrorOrResult(const gl::Error &error) : mError(error) {}
    ErrorOrResult(T &&result) : mError(GL_NO_ERROR), mResult(std::move(result)) {}

    bool isError() const { return mError.isError(); }
    const gl::Error &getError() const { return mError; }
    T &&getResult() { return std::move(mResult); }

  private:
    Error mError;
    T mResult;
};

}  // namespace gl

namespace egl
{

class Error final
{
  public:
    explicit inline Error(EGLint errorCode);
    Error(EGLint errorCode, const char *msg, ...);
    Error(EGLint errorCode, EGLint id, const char *msg, ...);
    inline Error(const Error &other);
    inline Error(Error &&other);

    inline Error &operator=(const Error &other);
    inline Error &operator=(Error &&other);

    inline EGLint getCode() const;
    inline EGLint getID() const;
    inline bool isError() const;

    const std::string &getMessage() const;

  private:
    void createMessageString() const;

    EGLint mCode;
    EGLint mID;
    mutable std::unique_ptr<std::string> mMessage;
};

}  // namespace egl

#include "Error.inl"

#endif // LIBANGLE_ERROR_H_
