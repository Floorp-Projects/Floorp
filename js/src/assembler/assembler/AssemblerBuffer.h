/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef assembler_assembler_AssemblerBuffer_h
#define assembler_assembler_AssemblerBuffer_h

#include "assembler/wtf/Platform.h"

#if ENABLE_ASSEMBLER

#include <string.h>
#include <limits.h>
#include "assembler/jit/ExecutableAllocator.h"
#include "assembler/wtf/Assertions.h"

#include <stdarg.h>
#include "jsfriendapi.h"
#include "jsopcode.h"
#include "jsutil.h"

#include "jit/IonSpewer.h"
#include "js/RootingAPI.h"

#define PRETTY_PRINT_OFFSET(os) (((os)<0)?"-":""), (((os)<0)?-(os):(os))

#define FIXME_INSN_PRINTING                                 \
    do {                                                    \
        spew("FIXME insn printing %s:%d",                   \
             __FILE__, __LINE__);                           \
    } while (0)

namespace JSC {

    class AssemblerBuffer {
        static const size_t inlineCapacity = 256;
    public:
        AssemblerBuffer()
            : m_buffer(m_inlineBuffer)
            , m_capacity(inlineCapacity)
            , m_size(0)
            , m_allocSize(0)
            , m_oom(false)
        {
        }

        ~AssemblerBuffer()
        {
            if (m_buffer != m_inlineBuffer)
                js_free(m_buffer);
        }

        void ensureSpace(size_t space)
        {
            if (m_size > m_capacity - space)
                grow();
        }

        bool isAligned(size_t alignment) const
        {
            return !(m_size & (alignment - 1));
        }

        void putByteUnchecked(int value)
        {
            ASSERT(!(m_size > m_capacity - 4));
            m_buffer[m_size] = char(value);
            m_size++;
        }

        void putByte(int value)
        {
            if (m_size > m_capacity - 4)
                grow();
            putByteUnchecked(value);
        }

        void putShortUnchecked(int value)
        {
            ASSERT(!(m_size > m_capacity - 4));
            *reinterpret_cast<short*>(&m_buffer[m_size]) = short(value);
            m_size += 2;
        }

        void putShort(int value)
        {
            if (m_size > m_capacity - 4)
                grow();
            putShortUnchecked(value);
        }

        void putIntUnchecked(int value)
        {
            ASSERT(!(m_size > m_capacity - 4));
            *reinterpret_cast<int*>(&m_buffer[m_size]) = value;
            m_size += 4;
        }

        void putInt64Unchecked(int64_t value)
        {
            ASSERT(!(m_size > m_capacity - 8));
            *reinterpret_cast<int64_t*>(&m_buffer[m_size]) = value;
            m_size += 8;
        }

        void putInt(int value)
        {
            if (m_size > m_capacity - 4)
                grow();
            putIntUnchecked(value);
        }

        void* data() const
        {
            return m_buffer;
        }

        size_t size() const
        {
            return m_size;
        }

        size_t allocSize() const
        {
            return m_allocSize;
        }

        bool oom() const
        {
            return m_oom;
        }

        /*
         * The user must check for a NULL return value, which means
         * no code was generated, or there was an OOM.
         */
        void* executableAllocAndCopy(ExecutableAllocator* allocator, ExecutablePool** poolp, CodeKind kind)
        {
            if (m_oom || m_size == 0) {
                *poolp = NULL;
                return 0;
            }

            m_allocSize = js::AlignBytes(m_size, sizeof(void *));

            void* result = allocator->alloc(m_allocSize, poolp, kind);
            if (!result) {
                *poolp = NULL;
                return 0;
            }
            JS_ASSERT(*poolp);

            ExecutableAllocator::makeWritable(result, m_size);

            return memcpy(result, m_buffer, m_size);
        }

        unsigned char *buffer() const {
            ASSERT(!m_oom);
            return reinterpret_cast<unsigned char *>(m_buffer);
        }

    protected:
        void append(const char* data, size_t size)
        {
            if (m_size > m_capacity - size)
                grow(size);

            // If we OOM and size > inlineCapacity, this would crash.
            if (m_oom)
                return;
            memcpy(m_buffer + m_size, data, size);
            m_size += size;
        }

        /*
         * OOM handling: This class can OOM in the grow() method trying to
         * allocate a new buffer. In response to an OOM, we need to avoid
         * crashing and report the error. We also want to make it so that
         * users of this class need to check for OOM only at certain points
         * and not after every operation.
         *
         * Our strategy for handling an OOM is to set m_oom, and then set
         * m_size to 0, preserving the current buffer. This way, the user
         * can continue assembling into the buffer, deferring OOM checking
         * until the user wants to read code out of the buffer.
         *
         * See also the |executableAllocAndCopy| and |buffer| methods.
         */

        void grow(size_t extraCapacity = 0)
        {
            char* newBuffer;

            /*
             * If |extraCapacity| is zero (as it almost always is) this is an
             * allocator-friendly doubling growth strategy.
             */
            size_t doubleCapacity = m_capacity + m_capacity;

            // Check for overflow.
            if (doubleCapacity < m_capacity) {
                m_size = 0;
                m_oom = true;
                return;
            }

            size_t newCapacity = doubleCapacity + extraCapacity;

            // Check for overflow.
            if (newCapacity < doubleCapacity) {
                m_size = 0;
                m_oom = true;
                return;
            }

            if (m_buffer == m_inlineBuffer) {
                newBuffer = static_cast<char*>(js_malloc(newCapacity));
                if (!newBuffer) {
                    m_size = 0;
                    m_oom = true;
                    return;
                }
                memcpy(newBuffer, m_buffer, m_size);
            } else {
                newBuffer = static_cast<char*>(js_realloc(m_buffer, newCapacity));
                if (!newBuffer) {
                    m_size = 0;
                    m_oom = true;
                    return;
                }
            }

            m_buffer = newBuffer;
            m_capacity = newCapacity;
        }

        char m_inlineBuffer[inlineCapacity];
        char* m_buffer;
        size_t m_capacity;
        size_t m_size;
        size_t m_allocSize;
        bool m_oom;
    };

    class GenericAssembler
    {
        js::Sprinter *printer;

      public:

        bool isOOLPath;

        GenericAssembler()
          : printer(NULL)
          , isOOLPath(false)
        {}

        void setPrinter(js::Sprinter *sp) {
            printer = sp;
        }

        void spew(const char *fmt, ...)
#ifdef __GNUC__
            __attribute__ ((format (printf, 2, 3)))
#endif
        {
            if (printer || js::jit::IonSpewEnabled(js::jit::IonSpew_Codegen)) {
                // Buffer to hold the formatted string. Note that this may contain
                // '%' characters, so do not pass it directly to printf functions.
                char buf[200];

                va_list va;
                va_start(va, fmt);
                int i = vsnprintf(buf, sizeof(buf), fmt, va);
                va_end(va);

                if (i > -1) {
                    if (printer)
                        printer->printf("%s\n", buf);
                    js::jit::IonSpew(js::jit::IonSpew_Codegen, "%s", buf);
                }
            }
        }

        static void staticSpew(const char *fmt, ...)
#ifdef __GNUC__
            __attribute__ ((format (printf, 1, 2)))
#endif
        {
            if (js::jit::IonSpewEnabled(js::jit::IonSpew_Codegen)) {
                char buf[200];

                va_list va;
                va_start(va, fmt);
                int i = vsnprintf(buf, sizeof(buf), fmt, va);
                va_end(va);

                if (i > -1)
                    js::jit::IonSpew(js::jit::IonSpew_Codegen, "%s", buf);
            }
        }
    };

} // namespace JSC

#endif // ENABLE(ASSEMBLER)

#endif /* assembler_assembler_AssemblerBuffer_h */
