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

#ifndef _nsMsgZapIt_H_
#define _nsMsgZapIt_H_

// The whole purpose of this class is to redefine operator new for any subclass
// so that it will automatically zero out the whole class for me; thanks.

class nsMsgZapIt {
public:
#if defined(XP_OS2) && defined(__DEBUG_ALLOC__)
  static void* operator new(size_t size, const char *file, size_t line);
#else
  static void* operator new(size_t size);
#endif
};



#endif /* _nsMsgZapIt_H_ */
