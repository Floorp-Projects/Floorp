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
 * $Id: jvmp_exec.c,v 1.2 2001/07/12 19:58:24 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include "shmtran.h"
#include "string.h"
int g_msg_id;

static int test(int i, char* s, void* ptr);

int JVMP_ExecuteShmRequest(JVMP_ShmRequest* req)
{
  char* string_val;
  jpointer jpointer_val;
  int      int_val;
  char sign[20];
  char* buf;

  if (!req) return 0;
  switch(req->func_no)
    {
    case 1:
      strcpy(sign, "ISA[8]a[0]");
      JVMP_DecodeRequest(req, 1, sign, 
			 &int_val, &string_val, &jpointer_val, &buf);
      buf = malloc(100000);
      strcpy(buf, "Privet");
      sprintf(sign, "ISA[8]a[%d]", 100000);
      req->retval = test(int_val, string_val, jpointer_val);      
      JVMP_EncodeRequest(req, sign, int_val, string_val, jpointer_val, buf);
      break;
    default:
      printf("no such function\n");
      return 0;
      break;
    }
  return 1;
}


static int test(int i, char* s, void* ptr)
{

  printf("called with i=%d s=\"%s\" ptr[5]=%c \n", i, s, ((char*)ptr)[5]
	 );
  ((char*)ptr)[5] = 'B';
  return 19;
}


int main(int argc, char** argv)
{
  int id = 0;
  /* one and only argument should be message queue id used to communicate
     with host application */ 
  if (argc != 2) return 1;
  id = atoi(argv[1]);
  g_msg_id = id;
  JVMP_ShmInit();
  JVMP_ShmMessageLoop(g_msg_id);
  return 0;
}
