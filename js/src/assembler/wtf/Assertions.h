/*
 * Copyright (C) 2003, 2006, 2007 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef assembler_wtf_Assertions_h
#define assembler_wtf_Assertions_h

#include "assembler/wtf/Platform.h"
#include "mozilla/Assertions.h"

#ifndef DEBUG
   /*
    * Prevent unused-variable warnings by defining the macro WTF uses to test
    * for assertions taking effect.
    */
#  define ASSERT_DISABLED 1
#endif

#ifndef ASSERT
#define ASSERT(assertion) MOZ_ASSERT(assertion)
#endif
#define ASSERT_UNUSED(variable, assertion) do { \
    (void)variable; \
    ASSERT(assertion); \
} while (0)
#define ASSERT_NOT_REACHED() MOZ_ASSUME_UNREACHABLE("wtf/Assertions.h")
#define CRASH() MOZ_CRASH()
#define COMPILE_ASSERT(exp, name) static_assert(exp, #name)

#endif /* assembler_wtf_Assertions_h */
