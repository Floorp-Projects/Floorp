// -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// The contents of this file are subject to the Netscape Public
// License Version 1.1 (the "License"); you may not use this file
// except in compliance with the License. You may obtain a copy of
// the License at http://www.mozilla.org/NPL/
//
// Software distributed under the License is distributed on an "AS
// IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
// implied. See the License for the specific language governing
// rights and limitations under the License.
//
// The Original Code is the JavaScript 2 Prototype.
//
// The Initial Developer of the Original Code is Netscape
// Communications Corporation.  Portions created by Netscape are
// Copyright (C) 1998 Netscape Communications Corporation. All
// Rights Reserved.

#ifndef systemtypes_h
#define systemtypes_h

#include <cstddef>

// Define int8, int16, int32, int64, uint8, uint16, uint32, uint64, and uint.
#ifdef XP_BEOS
 #include <SupportDefs.h>
#else
 #ifdef XP_UNIX
  #include <sys/types.h>
 #else
  typedef unsigned int uint;
  typedef unsigned char uchar;
 #endif
 typedef unsigned long long uint64;
 #if !defined(XP_MAC) && !defined(_WIN32) && !defined(XP_OS2)
  typedef unsigned int uint32;
 #else
  typedef unsigned long uint32;
 #endif
 typedef unsigned short uint16;
 typedef unsigned char uint8;
 #ifdef AIX4_3
  #include <sys/inttypes.h>
 #else
  typedef long long int64;
  #ifdef HPUX
   #include <model.h>
  #else
   #if !defined(WIN32) || !defined(_WINSOCK2API_)  /* defines its own "int32" */
    #if !defined(XP_MAC) && !defined(_WIN32) && !defined(XP_OS2)
     typedef int int32;
    #else
     typedef long int32;
    #endif
   #endif
   typedef short int16;
   typedef signed char int8;
  #endif /* HPUX */
 #endif /* AIX4_3 */
#endif	/* XP_BEOS */

typedef double float64;
typedef float float32;

#endif
