/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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
