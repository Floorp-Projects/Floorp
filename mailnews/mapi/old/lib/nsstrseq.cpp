/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
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
