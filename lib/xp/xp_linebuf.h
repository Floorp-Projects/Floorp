/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */


/*  xp_linebuf.h --- general line-buffering and de-buffering of text streams.
    Created: Jamie Zawinski <jwz@mozilla.org>,  8-Aug-98.
*/

#ifndef __XP_LINEBUF_H__
#define __XP_LINEBUF_H__

#include "xp_core.h"

XP_BEGIN_PROTOS

extern int XP_GrowBuffer (uint32 desired_size, uint32 element_size,
                          uint32 quantum, char **buffer, uint32 *size);

extern int XP_LineBuffer (const char *net_buffer, int32 net_buffer_size,
                          char **bufferP, uint32 *buffer_sizeP,
                          uint32 *buffer_fpP,
                          XP_Bool convert_newlines_p,
                          int32 (*per_line_fn) (char *line,
                                                uint32 line_length,
                                                void *closure),
                          void *closure);

extern int XP_ReBuffer (const char *net_buffer, int32 net_buffer_size,
                        uint32 desired_buffer_size,
                        char **bufferP, uint32 *buffer_sizeP,
                        uint32 *buffer_fpP,
                        int32 (*per_buffer_fn) (char *buffer,
                                                uint32 buffer_size,
                                                void *closure),
                        void *closure);

XP_END_PROTOS

#endif /* __XP_LINEBUF_H__ */
