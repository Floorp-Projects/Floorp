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
//  This is a string sequence handling routine to take complex
//  structures and merge them into a chunk of memory.
//
//  Written by: Rich Pizzarro (rhp@netscape.com)
//  November 1997
//
#include <string.h>
#include <stdlib.h>
#include <windows.h>
#include <windowsx.h>

#include "nsstrseq.h"

#ifndef NULL
#define NULL '\0'
#endif

#define MARKER '\377'

//
// Delete an existing string sequence
//
void NSStrSeqDelete(NSstringSeq seq)
{
  if (seq != NULL)
    free(seq);
  
  seq = NULL;
}

//
// Allocate a new sequence, copying the given strings into it. 
//
NSstringSeq NSStrSeqNew(LPSTR strings[])
{
  int size;
  if (!strings)
  {
    return NULL;
  }
  
  {
    int i;
    for (i=0,size=0; strings[i]; i++)
    {
      size+=strlen(strings[i])+1;
      switch (strings[i][0])
      {
        // Need to pad "" or anything starting with 255 
        // to allow for multiple blank strings in a row
      case 0:
      case MARKER:
        size++;
        break;
        
      default:
        break;
      }
    }
  }
  
  {
    NSstringSeq s=(NSstringSeq)malloc(size+1);
    if (!s)
    {	return NULL;}
    
    {
      int i,offset;
      for (i=0,offset=0; strings[i]; i++)
      {
        switch (strings[i][0])
        {
          // Need to pad "" or anything starting with 255
        case 0:
        case MARKER:
          s[offset++]=MARKER;
          break;
          
        default:
          break;
        }
        strcpy(s+offset,strings[i]);
        offset+=strlen(strings[i])+1;
      }
      s[offset]=0;
    }
    
    return s;
  }
}

//
// Get the # of bytes required for the sequence 
//
LONG NSStrSeqSize(NSstringSeq seq)
{
  const char* s;
  if (!seq)
    {
      return -1;
    }

  for (s=seq+1; ((*s) || (*(s-1))); s++)
    ;

  // At this point, s points to the second 0 
  // of the double 0 at the end 
  return (s-seq)+1;
}

//
// Get the # of strings in the sequence 
//
LONG NSStrSeqNumStrs(NSstringSeq seq)
{
  const char* s;
  int N;
  if (!seq)
  {
    return -1;
  }
  
  for (s=seq+1,N=0; ((*s) || (*(s-1))); s++)
  {
    if (!(*s))
      N++;
  }
  
  return N;
}

static LPSTR correct(LPSTR s)
{
  if (s[0]==MARKER)
    return s+1;
  else  // Anup , 4/96
    return s;
}

//
// Extract the index'th string in the sequence
//
LPSTR NSStrSeqGet(NSstringSeq seq, LONG index)
{
  char* s;
  int N;

  if (!seq)
  {
    return NULL;
  }
  
  if (index<0)
  {
    return NULL;
  }
  
  if (!index)
    return correct(seq);
  
  for (s=seq+1,N=0; ((*s) || (*(s-1))) && (N<index); s++)
  {
    if (!(*s))
      N++;
  }
  
  if (N==index)
    return correct(s);
  
  return NULL;
}

LPSTR * NSStrSeqGetAll(NSstringSeq seq)
{
  LONG N=NSStrSeqNumStrs(seq);
  if (N<0)
    return NULL;
  
  {
    char** res=(char**)malloc( (size_t) ((N+1)*sizeof(char*)) );
    int i;
    
    if (!res)
    {
      return NULL;
    }
    for (i=0; i<N; i++)
      res[i]=NSStrSeqGet(seq,i);
    res[N]=NULL;
    
    return res;
  }
}
