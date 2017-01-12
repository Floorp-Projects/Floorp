/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsprf_h
#define jsprf_h

/*
** API for PR printf like routines. Supports the following formats
**      %d - decimal
**      %u - unsigned decimal
**      %x - unsigned hex
**      %X - unsigned uppercase hex
**      %o - unsigned octal
**      %hd, %hu, %hx, %hX, %ho - "short" versions of above
**      %ld, %lu, %lx, %lX, %lo - "long" versions of above
**      %lld, %llu, %llx, %llX, %llo - "long long" versions of above
**      %zd, %zo, %zu, %zx, %zX - size_t versions of above
**      %Id, %Io, %Iu, %Ix, %IX - size_t versions of above (for Windows compat)
**           You should use PRI*SIZE macros instead
**      %s - string
**      %c - character
**      %p - pointer (deals with machine dependent pointer size)
**      %f - float
**      %g - float
*/

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/SizePrintfMacros.h"
#include "mozilla/Types.h"

#include <stdarg.h>

#include "jstypes.h"

namespace mozilla {

/*
** sprintf into a malloc'd buffer. Return a pointer to the malloc'd
** buffer on success, nullptr on failure. Call "SmprintfFree" to release
** the memory returned.
*/
extern MFBT_API char* Smprintf(const char* fmt, ...)
    MOZ_FORMAT_PRINTF(1, 2);

/*
** Free the memory allocated, for the caller, by Smprintf
*/
extern MFBT_API void SmprintfFree(char* mem);

/*
** "append" sprintf into a malloc'd buffer. "last" is the last value of
** the malloc'd buffer. sprintf will append data to the end of last,
** growing it as necessary using realloc. If last is nullptr, SmprintfAppend
** will allocate the initial string. The return value is the new value of
** last for subsequent calls, or nullptr if there is a malloc failure.
*/
extern MFBT_API char* SmprintfAppend(char* last, const char* fmt, ...)
    MOZ_FORMAT_PRINTF(2, 3);

/*
** va_list forms of the above.
*/
extern MFBT_API char* Vsmprintf(const char* fmt, va_list ap);
extern MFBT_API char* VsmprintfAppend(char* last, const char* fmt, va_list ap);

/*
 * This class may be subclassed to provide a way to get the output of
 * a printf-like call, as the output is generated.
 */
class PrintfTarget
{
public:
    /* The Printf-like interface.  */
    bool MFBT_API print(const char* format, ...) MOZ_FORMAT_PRINTF(2, 3);

    /* The Vprintf-like interface.  */
    bool MFBT_API vprint(const char* format, va_list);

protected:
    MFBT_API PrintfTarget() : mEmitted(0) { }
    virtual ~PrintfTarget() { }

    /* Subclasses override this.  It is called when more output is
       available.  It may be called with len==0.  This should return
       true on success, or false on failure.  */
    virtual bool append(const char* sp, size_t len) = 0;

private:

    /* Number of bytes emitted so far.  */
    size_t mEmitted;

    /* The implementation calls this to emit bytes and update
       mEmitted.  */
    bool emit(const char* sp, size_t len) {
        mEmitted += len;
        return append(sp, len);
    }

    bool fill2(const char* src, int srclen, int width, int flags);
    bool fill_n(const char* src, int srclen, int width, int prec, int type, int flags);
    bool cvt_l(long num, int width, int prec, int radix, int type, int flags, const char* hxp);
    bool cvt_ll(int64_t num, int width, int prec, int radix, int type, int flags, const char* hexp);
    bool cvt_f(double d, const char* fmt0, const char* fmt1);
    bool cvt_s(const char* s, int width, int prec, int flags);
};

} // namespace mozilla

/* Wrappers for mozilla::Smprintf and friends that are used throughout
   JS.  */

extern JS_PUBLIC_API(char*) JS_smprintf(const char* fmt, ...)
    MOZ_FORMAT_PRINTF(1, 2);

extern JS_PUBLIC_API(void) JS_smprintf_free(char* mem);

extern JS_PUBLIC_API(char*) JS_sprintf_append(char* last, const char* fmt, ...)
     MOZ_FORMAT_PRINTF(2, 3);

extern JS_PUBLIC_API(char*) JS_vsmprintf(const char* fmt, va_list ap);
extern JS_PUBLIC_API(char*) JS_vsprintf_append(char* last, const char* fmt, va_list ap);

#endif /* jsprf_h */
