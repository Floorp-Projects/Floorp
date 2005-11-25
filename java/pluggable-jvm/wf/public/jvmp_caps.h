/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *  
 * The Original Code is The Waterfall Java Plugin Module
 *  
 * The Initial Developer of the Original Code is Sun Microsystems Inc
 * Portions created by Sun Microsystems Inc are Copyright (C) 2001
 * All Rights Reserved.
 * 
 * $Id: jvmp_caps.h,v 1.3 2005/11/25 21:56:45 timeless%mozdev.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */

#ifndef _JVMP_CAPS_H
#define _JVMP_CAPS_H
#ifdef __cplusplus
extern "C" {
#endif
#define JVMP_MAX_CAPS_BYTES 128
#define JVMP_MAX_CAPS JVMP_MAX_CAPS_BYTES * 8
/* latest system capability used by Waterfall - use JVMP_*_USER_CAP macros
   to maintaian user caps */
#define JVMP_MAX_SYS_CAPS_BYTES 2
#define JVMP_MAX_SYS_CAPS JVMP_MAX_SYS_CAPS_BYTES * 8
struct JVMP_SecurityCap
{
  unsigned char bits[JVMP_MAX_CAPS_BYTES];
};
/* really action and capability - the same, capability is bitmask and
   action is bitfield */ 
typedef struct JVMP_SecurityCap JVMP_SecurityCap;
typedef struct JVMP_SecurityCap JVMP_SecurityAction;
#define JVMP_ALLOW_ALL_CAPS(cap) \
        { int _i; for(_i = 0; _i < JVMP_MAX_CAPS_BYTES; _i++) \
                       ((cap).bits)[_i] = 0xff; }
/* maybe memset()  */
#define JVMP_FORBID_ALL_CAPS(cap) \
        { int _i; for(_i = 0; _i < JVMP_MAX_CAPS_BYTES; _i++) \
                       ((cap).bits)[_i] = 0x0; }
#define JVMP_ALLOW_CAP(cap, action_no) \
        (cap).bits[action_no >> 3] |= 1 << (action_no & 0x7);
                   
#define JVMP_FORBID_CAP(cap, action_no) \
        (cap).bits[action_no >> 3] &= ~(1 << (action_no & 0x7));

#define JVMP_IS_CAP_ALLOWED(cap, action_no) \
        ( ((cap).bits[action_no >> 3]) & (1 << (action_no & 0x7))) ? 1 : 0
#define JVMP_ALLOW_USER_CAP(cap, cap_no) JVMP_ALLOW_CAP(cap, cap_no + JVMP_MAX_SYS_CAPS)
#define JVMP_FORBID_USER_CAP(cap, cap_no) JVMP_FORBID_CAP(cap, cap_no + JVMP_MAX_SYS_CAPS)
/* not completely safe */ 
#define JVMP_FORBID_ALL_USER_CAPS(cap) \
        { int _i; for(_i = JVMP_MAX_SYS_CAPS_BYTES; _i < JVMP_MAX_CAPS_BYTES; _i++) \
                       ((cap).bits)[_i] = 0x0; }
#define JVMP_ALLOW_ALL_USER_CAPS(cap) \
        { int _i; for(_i = JVMP_MAX_SYS_CAPS_BYTES; _i < JVMP_MAX_CAPS_BYTES; _i++) \
                       ((cap).bits)[_i] = 0xff; }
#define JVMP_SET_CAPS(target, caps) \
        { int _i; for(_i = 0; _i < JVMP_MAX_CAPS_BYTES; _i++) \
                       ((target).bits)[_i] |= ((caps).bits)[_i]; }
#define JVMP_RESET_CAPS(target, caps) \
        { int _i; for(_i = 0; _i < JVMP_MAX_CAPS_BYTES; _i++) \
                       ((target).bits)[_i] &= ~ ((caps).bits)[_i]; }

#include "jvmp_cap_vals.h"
#ifdef __cplusplus
}
#endif
#endif



