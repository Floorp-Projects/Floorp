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
#ifndef __SEQUENCES_OF_STRINGS_H_
#define __SEQUENCES_OF_STRINGS_H	


typedef LPSTR NSstringSeq;

#ifdef __cplusplus
extern "C"
{
#endif

void NSStrSeqDelete(NSstringSeq seq);
NSstringSeq NSStrSeqNew(LPSTR strings[]);

// Get the # of bytes required for the sequence 
LONG NSStrSeqSize(NSstringSeq seq);

// Get the # of strings in the sequence
LONG NSStrSeqNumStrs(NSstringSeq seq);

// Extract the index'th string in the sequence
LPSTR NSStrSeqGet(NSstringSeq seq, LONG index);

// Build an array of all the strings in the sequence
LPSTR *NSStrSeqGetAll(NSstringSeq seq);

#ifdef __cplusplus
}
#endif


#endif // __sequences_of_strings_h_
