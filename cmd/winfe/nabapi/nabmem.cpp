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
//
//  mem.cpp
//  Written by: Rich Pizzarro (rhp@netscape.com)
//  November 1997
//  This implements various memory management functions for use with
//  Address Book API features of Communicator
//
#include <windows.h>     
#include <memory.h> 
#include <malloc.h>
#include <abapi.h>   // lives in communicator winfe

#include "nabmem.h"
#include "nsstrseq.h"
#include "trace.h"
#include "nabutl.h"
#include "xpapi.h"

LPSTR
CheckNullString(LPSTR inStr)
{
  static UCHAR  str[1];

  str[0] = '\0';
  if (inStr == NULL)
    return((LPSTR)str);
  else
    return(inStr);
}

void
FreeAddrStruct(NABAddrBookDescType *ptr)
{
  if (!ptr)
    return;
  if (ptr->description != NULL)
    free(ptr->description);
  if (ptr->fileName != NULL)
    free(ptr->fileName);
}

BOOL
ValidNonNullString(LPSTR ptr)
{
  if (!ptr)
    return(FALSE);

  if (ptr[0] == '\0')
    return(FALSE);

  return(TRUE);
}