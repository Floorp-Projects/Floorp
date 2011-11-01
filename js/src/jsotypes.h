/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * This section typedefs the old 'native' types to the new PR<type>s.
 * These definitions are scheduled to be eliminated at the earliest
 * possible time. The NSPR API is implemented and documented using
 * the new definitions.
 */

/*
 * Note that we test for PROTYPES_H, not JSOTYPES_H.  This is to avoid
 * double-definitions of scalar types such as uint32, if NSPR's
 * protypes.h is also included.
 */
#ifndef PROTYPES_H
#define PROTYPES_H

/* SVR4 typedef of uint is commonly found on UNIX machines. */
#ifdef XP_UNIX
#include <sys/types.h>
#else
typedef JSUintn uint;
#endif

typedef JSUintn uintn;
typedef JSUint64 uint64;
typedef JSUint32 uint32;
typedef JSUint16 uint16;
typedef JSUint8 uint8;

#ifndef _XP_Core_
typedef JSIntn intn;
#endif

/*
 * On AIX 4.3, sys/inttypes.h (which is included by sys/types.h, a very
 * common header file) defines the types int8, int16, int32, and int64.
 * So we don't define these four types here to avoid conflicts in case
 * the code also includes sys/types.h.
 */
#if defined(AIX) && defined(HAVE_SYS_INTTYPES_H)
#include <sys/inttypes.h>
#elif defined(__SUNPRO_C) || defined(__SUNPRO_CC)
typedef JSInt64 int64;

/* Explicit signed keyword for bitfield types is required. */
/* Some compilers may treat them as unsigned without it. */
typedef signed int int32;
typedef signed short int16;
typedef signed char int8;
#else
typedef JSInt64 int64;

/* /usr/include/model.h on HP-UX defines int8, int16, and int32 */
typedef JSInt32 int32;
typedef JSInt16 int16;
typedef JSInt8 int8;
#endif /* AIX && HAVE_SYS_INTTYPES_H */

#endif /* !defined(PROTYPES_H) */
