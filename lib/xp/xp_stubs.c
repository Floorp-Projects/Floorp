/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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


/* Unix to start, I'm guessing Win32 and Mac should also stub
   this out here until these get re-implemented.  Put the stubs
   here in xp-land so we can stop cut-and-pasting stubs in the
   rest of the tree. */

#ifdef XP_UNIX

#include "xp_mcom.h"

int XP_ReBuffer (const char *net_buffer, int32 net_buffer_size,
                        uint32 desired_buffer_size,
                        char **bufferP, uint32 *buffer_sizeP,
                        uint32 *buffer_fpP,
                        int32 (*per_buffer_fn) (char *buffer,
                                                uint32 buffer_size,
                                                void *closure),
                        void *closure) 
{ 

  printf("XP_ReBuffer not implemented, stubbed in lib/xp/xp_stubs.c\n"); 
  return(0); 
}


void XP_Trace( const char *message, ... ) 
{ 
  printf("XP_Trace not implemented, stubbed in lib/xp/xp_stubs.c\n"); 
}

#endif

