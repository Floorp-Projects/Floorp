/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Writable stream writer abstract operations. */

#include "builtin/streams/WritableStreamWriterOperations.h"

#include "builtin/streams/WritableStream.h"  // js::WritableStream
#include "builtin/streams/WritableStreamDefaultController.h"  // js::WritableStream::controller
#include "builtin/streams/WritableStreamDefaultControllerOperations.h"  // js::WritableStreamDefaultControllerGetDesiredSize
#include "builtin/streams/WritableStreamDefaultWriter.h"  // js::WritableStreamDefaultWriter
#include "js/Value.h"  // JS::Value, JS::{Int32,Null}Value

#include "builtin/streams/WritableStreamDefaultWriter-inl.h"  // js::WritableStreamDefaultWriter::stream
#include "vm/Compartment-inl.h"  // js::UnwrapAndTypeCheckThis

using JS::Int32Value;
using JS::NullValue;
using JS::NumberValue;
using JS::Value;

/*** 4.6. Writable stream writer abstract operations ************************/

/**
 * Streams spec, 4.6.7.
 * WritableStreamDefaultWriterGetDesiredSize ( writer )
 */
Value js::WritableStreamDefaultWriterGetDesiredSize(
    const WritableStreamDefaultWriter* unwrappedWriter) {
  // Step 1: Let stream be writer.[[ownerWritableStream]].
  const WritableStream* unwrappedStream = unwrappedWriter->stream();

  // Step 2: Let state be stream.[[state]].
  // Step 3: If state is "errored" or "erroring", return null.
  if (unwrappedStream->errored() || unwrappedStream->erroring()) {
    return NullValue();
  }

  // Step 4: If state is "closed", return 0.
  if (unwrappedStream->closed()) {
    return Int32Value(0);
  }

  // Step 5: Return
  //         ! WritableStreamDefaultControllerGetDesiredSize(
  //             stream.[[writableStreamController]]).
  return NumberValue(WritableStreamDefaultControllerGetDesiredSize(
      unwrappedStream->controller()));
}
