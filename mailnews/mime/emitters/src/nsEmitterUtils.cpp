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
#include "prmem.h"
#include "plstr.h"

extern "C" char *
nsEscapeHTML(const char * string)
{
  if (!string)
    return NULL;

  char *rv = (char *) PR_MALLOC(PL_strlen(string)*4 + 1); 
              /* The +1 is for the trailing null! */
  char *ptr = rv;

  if(rv)
  {
		  for(; *string != '\0'; string++)
		    {
        if(*string == '<')
        {
          *ptr++ = '&';
          *ptr++ = 'l';
          *ptr++ = 't';
          *ptr++ = ';';
        }
        else if(*string == '>')
        {
          *ptr++ = '&';
          *ptr++ = 'g';
          *ptr++ = 't';
          *ptr++ = ';';
        }
        else if(*string == '&')
        {
          *ptr++ = '&';
          *ptr++ = 'a';
          *ptr++ = 'm';
          *ptr++ = 'p';
          *ptr++ = ';';
        }
        else
        {
          *ptr++ = *string;
        }
		    }
      *ptr = '\0';
  }

  return(rv);
}
