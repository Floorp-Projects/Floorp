/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef _NO_NSPR_H_
#define _NO_NSPR_H_

#include <string.h>
#include "plstr.h"

/* Header file for NSPR utilities if NSPR is not being used */
extern "C" {
inline uint32 PL_strlen(const char *str) { return strlen(str); }

inline char *PL_strdup(const char *str) { return strdup(str); }

inline char *PL_strchr(const char *s, char c) { return strchr(s, c); }
inline char *PL_strrchr(const char *s, char c) { return strrchr(s, c); }

inline char *PL_strcpy(char *dest, const char *src) {
  return strcpy(dest, src);
}

inline char *PL_strncpy(char *dest, const char *src, uint32 max) {
  return strncpy(dest, src, max);
}


inline char *PL_strcat(char *dst, const char *src) { 
  return strcat(dst, src);
}

inline int32 PL_strcasecmp(const char *a, const char *b) {
  return strcasecmp(a, b);
}

inline int32 PL_strncasecmp(const char *a, const char *b, uint32 n) {
  return strncasecmp(a, b, n);
}

inline char *PL_strstr(const char *big, const char *little) {
  return strstr(big, little);
}


inline int32 PL_strcmp(const char *a, const char *b) {
  return strcmp(a, b);
}

inline int32 PL_strncmp(const char *a, const char *b, uint32 n) {
  return strncmp(a, b, n);
}

} /* extern "C" */
#endif /* _NO_NSPR_H_ */
