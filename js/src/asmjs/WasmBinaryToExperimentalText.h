/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * Copyright 2015 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef wasm_binary_to_experimental_text_h
#define wasm_binary_to_experimental_text_h

#include "mozilla/Vector.h"

#include "NamespaceImports.h"

#include "gc/Rooting.h"
#include "js/Class.h"

namespace js {

class StringBuffer;

namespace wasm {

struct ExperimentalTextFormatting
{
    bool allowAsciiOperators:1;
    bool reduceParens:1;
    bool groupBlocks:1;

    ExperimentalTextFormatting()
     : allowAsciiOperators(true),
       reduceParens(true),
       groupBlocks(true)
    {}
};

// The generated source location for the AST node/expression. The offset field refers
// an offset in an binary format file.
struct ExprLoc
{
    uint32_t lineno;
    uint32_t column;
    uint32_t offset;
    ExprLoc() : lineno(0), column(0), offset(0) {}
    ExprLoc(uint32_t lineno_, uint32_t column_, uint32_t offset_) : lineno(lineno_), column(column_), offset(offset_) {}
};

typedef mozilla::Vector<ExprLoc, 0, TempAllocPolicy> ExprLocVector;

// The generated source WebAssembly function lines and expressions ranges.
struct FunctionLoc
{
    size_t startExprsIndex;
    size_t endExprsIndex;
    uint32_t startLineno;
    uint32_t endLineno;
    FunctionLoc(size_t startExprsIndex_, size_t endExprsIndex_, uint32_t startLineno_, uint32_t endLineno_)
      : startExprsIndex(startExprsIndex_),
        endExprsIndex(endExprsIndex_),
        startLineno(startLineno_),
        endLineno(endLineno_)
    {}
};

typedef mozilla::Vector<FunctionLoc, 0, TempAllocPolicy> FunctionLocVector;

// The generated source map for WebAssembly binary file. This map is generated during
// building the text buffer (see BinaryToExperimentalText).
class GeneratedSourceMap
{
    ExprLocVector exprlocs_;
    FunctionLocVector functionlocs_;
    uint32_t totalLines_;

  public:
    explicit GeneratedSourceMap(JSContext* cx)
     : exprlocs_(cx),
       functionlocs_(cx),
       totalLines_(0)
    {}
    ExprLocVector& exprlocs() { return exprlocs_; }
    FunctionLocVector& functionlocs() { return functionlocs_; }

    uint32_t totalLines() { return totalLines_; }
    void setTotalLines(uint32_t val) { totalLines_ = val; }
};

// Translate the given binary representation of a wasm module into the module's textual
// representation.

MOZ_MUST_USE bool
BinaryToExperimentalText(JSContext* cx, const uint8_t* bytes, size_t length, StringBuffer& buffer,
                         const ExperimentalTextFormatting& formatting = ExperimentalTextFormatting(),
                         GeneratedSourceMap* sourceMap = nullptr);

}  // namespace wasm

}  // namespace js

#endif // namespace wasm_binary_to_experimental_text_h
