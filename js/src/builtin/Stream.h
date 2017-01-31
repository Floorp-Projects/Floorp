/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_Stream_h
#define builtin_Stream_h

#include "vm/NativeObject.h"

namespace js {

class AutoSetNewObjectMetadata;

class ReadableStream : public NativeObject
{
  public:
    static ReadableStream* createDefaultStream(JSContext* cx, HandleValue underlyingSource,
                                               HandleValue size, HandleValue highWaterMark);
    static ReadableStream* createByteStream(JSContext* cx, HandleValue underlyingSource,
                                            HandleValue highWaterMark);

    inline bool readable() const;
    inline bool closed() const;
    inline bool errored() const;
    inline bool disturbed() const;

    enum State {
         Readable  = 1 << 0,
         Closed    = 1 << 1,
         Errored   = 1 << 2,
         Disturbed = 1 << 3
    };

  private:
    static ReadableStream* createStream(JSContext* cx);

  public:
    static bool constructor(JSContext* cx, unsigned argc, Value* vp);
    static const ClassSpec classSpec_;
    static const Class class_;
    static const ClassSpec protoClassSpec_;
    static const Class protoClass_;
};

class ReadableStreamDefaultReader : public NativeObject
{
  public:
    static bool constructor(JSContext* cx, unsigned argc, Value* vp);
    static const ClassSpec classSpec_;
    static const Class class_;
    static const ClassSpec protoClassSpec_;
    static const Class protoClass_;
};

class ReadableStreamBYOBReader : public NativeObject
{
  public:
    static bool constructor(JSContext* cx, unsigned argc, Value* vp);
    static const ClassSpec classSpec_;
    static const Class class_;
    static const ClassSpec protoClassSpec_;
    static const Class protoClass_;
};

class ReadableStreamDefaultController : public NativeObject
{
  public:
    static bool constructor(JSContext* cx, unsigned argc, Value* vp);
    static const ClassSpec classSpec_;
    static const Class class_;
    static const ClassSpec protoClassSpec_;
    static const Class protoClass_;
};

class ReadableByteStreamController : public NativeObject
{
  public:
    static bool constructor(JSContext* cx, unsigned argc, Value* vp);
    static const ClassSpec classSpec_;
    static const Class class_;
    static const ClassSpec protoClassSpec_;
    static const Class protoClass_;
};

class ReadableStreamBYOBRequest : public NativeObject
{
  public:
    static bool constructor(JSContext* cx, unsigned argc, Value* vp);
    static const ClassSpec classSpec_;
    static const Class class_;
    static const ClassSpec protoClassSpec_;
    static const Class protoClass_;
};

class ByteLengthQueuingStrategy : public NativeObject
{
  public:
    static bool constructor(JSContext* cx, unsigned argc, Value* vp);
    static const ClassSpec classSpec_;
    static const Class class_;
    static const ClassSpec protoClassSpec_;
    static const Class protoClass_;
};

class CountQueuingStrategy : public NativeObject
{
  public:
    static bool constructor(JSContext* cx, unsigned argc, Value* vp);
    static const ClassSpec classSpec_;
    static const Class class_;
    static const ClassSpec protoClassSpec_;
    static const Class protoClass_;
};

} // namespace js

#endif /* builtin_Stream_h */
