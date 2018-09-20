//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// RefCountObject.h: Defines the gl::RefCountObject base class that provides
// lifecycle support for GL objects using the traditional BindObject scheme, but
// that need to be reference counted for correct cross-context deletion.
// (Concretely, textures, buffers and renderbuffers.)

#ifndef LIBANGLE_REFCOUNTOBJECT_H_
#define LIBANGLE_REFCOUNTOBJECT_H_

#include "angle_gl.h"
#include "common/debug.h"
#include "libANGLE/Error.h"

#include <cstddef>

namespace angle
{

template <typename ContextT, typename ErrorT>
class RefCountObject : angle::NonCopyable
{
  public:
    using ContextType = ContextT;
    using ErrorType   = ErrorT;

    RefCountObject() : mRefCount(0) {}

    virtual ErrorType onDestroy(const ContextType *context) { return ErrorType::NoError(); }

    void addRef() const { ++mRefCount; }

    void release(const ContextType *context)
    {
        ASSERT(mRefCount > 0);
        if (--mRefCount == 0)
        {
            ANGLE_SWALLOW_ERR(onDestroy(context));
            delete this;
        }
    }

    size_t getRefCount() const { return mRefCount; }

  protected:
    virtual ~RefCountObject() { ASSERT(mRefCount == 0); }

    mutable size_t mRefCount;
};

template <class ObjectType, typename ContextT, typename ErrorT>
class BindingPointer
{
  public:
    using ContextType = ContextT;
    using ErrorType   = ErrorT;

    BindingPointer()
        : mObject(nullptr)
    {
    }

    BindingPointer(ObjectType *object) : mObject(object)
    {
        if (mObject)
        {
            mObject->addRef();
        }
    }

    BindingPointer(const BindingPointer &other) : mObject(other.mObject)
    {
        if (mObject)
        {
            mObject->addRef();
        }
    }

    BindingPointer &operator=(BindingPointer &&other)
    {
        std::swap(mObject, other.mObject);
        return *this;
    }

    virtual ~BindingPointer()
    {
        // Objects have to be released before the resource manager is destroyed, so they must be explicitly cleaned up.
        ASSERT(mObject == nullptr);
    }

    virtual void set(const ContextType *context, ObjectType *newObject)
    {
        // addRef first in case newObject == mObject and this is the last reference to it.
        if (newObject != nullptr)
        {
            reinterpret_cast<RefCountObject<ContextType, ErrorType> *>(newObject)->addRef();
        }

        // Store the old pointer in a temporary so we can set the pointer before calling release.
        // Otherwise the object could still be referenced when its destructor is called.
        ObjectType *oldObject = mObject;
        mObject = newObject;
        if (oldObject != nullptr)
        {
            reinterpret_cast<RefCountObject<ContextType, ErrorType> *>(oldObject)->release(context);
        }
    }

    ObjectType *get() const { return mObject; }
    ObjectType *operator->() const { return mObject; }

    bool operator==(const BindingPointer &other) const { return mObject == other.mObject; }

    bool operator!=(const BindingPointer &other) const { return !(*this == other); }

  private:
    ObjectType *mObject;
};
}  // namespace angle

namespace gl
{
class Context;

template <class ObjectType>
class BindingPointer;

using RefCountObjectNoID = angle::RefCountObject<Context, Error>;

class RefCountObject : public gl::RefCountObjectNoID
{
  public:
    explicit RefCountObject(GLuint id) : mId(id) {}

    GLuint id() const { return mId; }

  protected:
    ~RefCountObject() override {}

  private:
    GLuint mId;
};

template <class ObjectType>
class BindingPointer : public angle::BindingPointer<ObjectType, Context, Error>
{
  public:
    using ContextType = typename angle::BindingPointer<ObjectType, Context, Error>::ContextType;
    using ErrorType   = typename angle::BindingPointer<ObjectType, Context, Error>::ErrorType;

    BindingPointer() {}

    BindingPointer(ObjectType *object) : angle::BindingPointer<ObjectType, Context, Error>(object)
    {
    }

    GLuint id() const
    {
        ObjectType *obj = this->get();
        return obj ? obj->id() : 0;
    }
};

template <class ObjectType>
class OffsetBindingPointer : public gl::BindingPointer<ObjectType>
{
  public:
    using ContextType = typename gl::BindingPointer<ObjectType>::ContextType;
    using ErrorType   = typename gl::BindingPointer<ObjectType>::ErrorType;

    OffsetBindingPointer() : mOffset(0), mSize(0) { }

    void set(const ContextType *context, ObjectType *newObject) override
    {
        BindingPointer<ObjectType>::set(context, newObject);
        mOffset = 0;
        mSize = 0;
    }

    void set(const ContextType *context, ObjectType *newObject, GLintptr offset, GLsizeiptr size)
    {
        BindingPointer<ObjectType>::set(context, newObject);
        mOffset = offset;
        mSize = size;
    }

    GLintptr getOffset() const { return mOffset; }
    GLsizeiptr getSize() const { return mSize; }

    bool operator==(const OffsetBindingPointer<ObjectType> &other) const
    {
        return this->get() == other.get() && mOffset == other.mOffset && mSize == other.mSize;
    }

    bool operator!=(const OffsetBindingPointer<ObjectType> &other) const
    {
        return !(*this == other);
    }

  private:
    GLintptr mOffset;
    GLsizeiptr mSize;
};
}  // namespace gl

namespace egl
{
class Display;

using RefCountObject = angle::RefCountObject<Display, Error>;

template <class ObjectType>
using BindingPointer = angle::BindingPointer<ObjectType, Display, Error>;

}  // namespace egl

#endif   // LIBANGLE_REFCOUNTOBJECT_H_
