/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef prprintf_h___
#define prprintf_h___
/*
 * Portable, flexible, and buffer-safe sprintf variants.
 */
#include <stdarg.h>
#include <stddef.h>

PR_BEGIN_EXTERN_C

/* XXX not used by JSRef */
typedef int
(*PR_vsxprintf_callback)(void *arg, const char *s, size_t slen);

typedef int
(*PR_sxprintf_callback) (void *arg, const char *s, size_t len);

/*
 * sprintf into a fixed size buffer. Guarantees that a NUL is at the end
 * of the buffer. Returns the length of the written output, NOT including
 * the NUL, or (size_t)-1 if an error occurs.
 */
extern PR_PUBLIC_API(size_t)
PR_snprintf(char *out, size_t outlen, const char *fmt, ...);

/*
 * sprintf into a malloc'd buffer. Return a pointer to the malloc'd
 * buffer on success, NULL on failure.
 */
extern PR_PUBLIC_API(char *)
PR_smprintf(const char *fmt, ...);

/*
 * sprintf into a function. The function "stuff" is called with a string
 * to place into the output. "arg" is an opaque pointer used by the stuff
 * function to hold any state needed to do the storage of the output
 * data. The return value is a count of the number of characters fed to
 * the stuff function, or (size_t)-1 if an error occurs.
 */
extern PR_PUBLIC_API(size_t)
PR_sxprintf(PR_sxprintf_callback stuff, void *arg, const char *fmt, ...);

/*
 * "append" sprintf into a malloc'd buffer. "last" is the last value of
 * the malloc'd buffer. sprintf will append data to the end of last,
 * growing it as necessary using realloc. If last is NULL, PR_ssprintf
 * will allocate the initial string. The return value is the new value of
 * last for subsequent calls, or NULL if there is a malloc failure.
 */
extern PR_PUBLIC_API(char *)
PR_sprintf_append(char *last, const char *fmt, ...);

/*
 * Variable-argument-list forms of the above.
 */
extern PR_PUBLIC_API(size_t)
PR_vsnprintf(char *out, size_t outlen, const char *fmt, va_list ap);

extern PR_PUBLIC_API(char *)
PR_vsmprintf(const char *fmt, va_list ap);

extern PR_PUBLIC_API(size_t)
PR_vsxprintf(PR_vsxprintf_callback stuff, void *arg, const char *fmt,
	     va_list ap);

extern PR_PUBLIC_API(char *)
PR_vsprintf_append(char *last, const char *fmt, va_list ap);

PR_END_EXTERN_C

#endif /* prprintf_h___ */
