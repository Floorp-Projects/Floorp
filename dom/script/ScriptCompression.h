/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_loader_ScriptCompression_h
#define js_loader_ScriptCompression_h

#include "ErrorList.h"
#include "mozilla/Vector.h"

namespace JS::loader {

class ScriptLoadRequest;

/**
 * Compress the bytecode stored in a buffer. All data before the bytecode is
 * copied into the output buffer without modification.
 *
 * @param aBytecodeBuf buffer containing the uncompressed bytecode
 * @param aBytecodeOffset offset of the bytecode in the buffer
 * @param aCompressedBytecodeBufOut buffer to store the compressed bytecode in
 */
bool ScriptBytecodeCompress(
    mozilla::Vector<uint8_t>& aBytecodeBuf, size_t aBytecodeOffset,
    mozilla::Vector<uint8_t>& aCompressedBytecodeBufOut);

/**
 * Uncompress the bytecode stored in a buffer. All data before the bytecode is
 * copied into the output buffer without modification.
 *
 * @param aCompressedBytecodeBuf buffer containing the compressed bytecode
 * @param aBytecodeOffset offset of the bytecode in the buffer
 * @param aBytecodeBufOut buffer to store the uncompressed bytecode in
 */
bool ScriptBytecodeDecompress(mozilla::Vector<uint8_t>& aCompressedBytecodeBuf,
                              size_t aBytecodeOffset,
                              mozilla::Vector<uint8_t>& aBytecodeBufOut);

}  // namespace JS::loader

#endif  // js_loader_ScriptCompression_h
