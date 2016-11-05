/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * Copyright 2016 Mozilla Foundation
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

#ifndef wasm_generated_source_map_h
#define wasm_generated_source_map_h

#include "mozilla/Vector.h"

#include "vm/StringBuffer.h"

namespace js {

namespace wasm {

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

typedef UniquePtr<GeneratedSourceMap> UniqueGeneratedSourceMap;

// Helper class, StringBuffer wrapper, to track the position (line and column)
// within the generated source.
class WasmPrintBuffer
{
    StringBuffer& stringBuffer_;
    uint32_t lineno_;
    uint32_t column_;

  public:
    explicit WasmPrintBuffer(StringBuffer& stringBuffer)
      : stringBuffer_(stringBuffer),
        lineno_(1),
        column_(1)
    {}
    inline char processChar(char ch) {
        if (ch == '\n') {
            lineno_++; column_ = 1;
        } else
            column_++;
        return ch;
    }
    inline char16_t processChar(char16_t ch) {
        if (ch == '\n') {
            lineno_++; column_ = 1;
        } else
            column_++;
        return ch;
    }
    bool append(const char ch) {
        return stringBuffer_.append(processChar(ch));
    }
    bool append(const char16_t ch) {
        return stringBuffer_.append(processChar(ch));
    }
    bool append(const char* str, size_t length) {
        for (size_t i = 0; i < length; i++)
            processChar(str[i]);
        return stringBuffer_.append(str, length);
    }
    bool append(const char16_t* begin, const char16_t* end) {
        for (const char16_t* p = begin; p != end; p++)
            processChar(*p);
        return stringBuffer_.append(begin, end);
    }
    bool append(const char16_t* str, size_t length) {
        return append(str, str + length);
    }
    template <size_t ArrayLength>
    bool append(const char (&array)[ArrayLength]) {
        static_assert(ArrayLength > 0, "null-terminated");
        MOZ_ASSERT(array[ArrayLength - 1] == '\0');
        return append(array, ArrayLength - 1);
    }
    char16_t getChar(size_t index) {
        return stringBuffer_.getChar(index);
    }
    size_t length() {
        return stringBuffer_.length();
    }
    StringBuffer& stringBuffer() { return stringBuffer_; }
    uint32_t lineno() { return lineno_; }
    uint32_t column() { return column_; }
};

}  // namespace wasm

}  // namespace js

#endif // namespace wasm_generated_source_map_h
