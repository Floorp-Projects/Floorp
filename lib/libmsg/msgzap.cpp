/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "msg.h"
#include "xp_mcom.h"
#include "msgzap.h"

#if defined(XP_OS2) && defined(__DEBUG_ALLOC__)
void* MSG_ZapIt::operator new(size_t size, const char *file, size_t line) {
  void* rv = ::operator new(size, file, line);
  if (rv) {
    XP_MEMSET(rv, 0, size);
  }
  return rv;
}
#else
void* MSG_ZapIt::operator new(size_t size) {
  void* rv = ::operator new(size);
  if (rv) {
    XP_MEMSET(rv, 0, size);
  }
  return rv;
}

#endif
