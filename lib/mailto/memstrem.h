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
/*lame but i need it, mjudge*/

#ifndef _MEMSTREM_H
#define _MEMSTREM_H

#define DEFAULT_BUFFER_AMT 90

class memstream
{
public:
    memstream();
    memstream(int32 size, int32 bufferdelta = DEFAULT_BUFFER_AMT);
    ~memstream();
    int write(const char *t_input,int bytesize);
    char *str(){frozen = TRUE;return mem;}
    void release(){frozen = FALSE;}
    void clear();
private:
    int32 sizealloc;
    int32 memloc;
    char *mem;
    XP_Bool frozen;
    int32 buffer; //grow by how much?
};

#endif //_MEMSTREM_H
