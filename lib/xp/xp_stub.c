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

#include "prlog.h"
#undef MOZILLA_CLIENT
#define RESOURCE_STR
#include "allxpstr.h"

#ifdef DEBUG
char * 
NOT_NULL (const char *x) 
{
    PR_ASSERT(x);
    return (char *)x;
}
#endif


#ifdef DEBUG
void XP_AssertAtLine( char *pFileName, int iLine )
{
    PR_Assert("XP Assert", pFileName, iLine);
}
#endif

/* XP_GetString
 *
 * This one takes XP string ID (which is used mainly by libnet, libsec) 
 * and loads String from resource file (netscape.rc3).
 */
#ifndef MOZILLA_CLIENT
/*
 * This routine picks up a builtin (compiled in) numbered string.
 * (I.e. error messages, etc.)
 *
 * If you are getting an unresolved symbol XP_GetString and you don't
 * want to get strings from resources (which is how the Win, Mac and X
 * versions of the Navigator do it), then you can define your own
 * wrapper function like this:
 */
char *
XP_GetString(int16 i)
{
	extern char * XP_GetBuiltinString(int16 i);

	return XP_GetBuiltinString(i);
}
#endif /* ! MOZILLA_CLIENT */

char *
XP_GetBuiltinString(int16 i)
{
	static char	buf[128];
	char		*ret;

	i += RES_OFFSET;

	if
	(
		((ret = mcom_include_merrors_i_strings (i)) != 0) ||
		((ret = mcom_include_secerr_i_strings  (i)) != 0) ||
		((ret = mcom_include_sec_dialog_strings(i)) != 0) ||
		((ret = mcom_include_sslerr_i_strings  (i)) != 0) ||
		((ret = mcom_include_xp_error_i_strings(i)) != 0) ||
		((ret = mcom_include_xp_msg_i_strings  (i)) != 0)
	)
	{
		return ret;
	}

	(void) sprintf(buf, "XP_GetBuiltinString: %d not found", i);

	return buf;
}



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

