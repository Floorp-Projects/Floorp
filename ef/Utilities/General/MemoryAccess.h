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

#ifndef MEMORYACCESS_H
#define MEMORYACCESS_H

// Aligned big-endian memory access
inline Uint16 readBigUHalfword(const void *p);
inline Int16 readBigSHalfword(const void *p);
inline Uint32 readBigUWord(const void *p);
inline Int32 readBigSWord(const void *p);

inline void writeBigHalfword(void *p, Int16 v);
inline void writeBigWord(void *p, Int32 v);

// Unaligned big-endian memory access
inline Uint16 readBigUHalfwordUnaligned(const void *p);
inline Int16 readBigSHalfwordUnaligned(const void *p);
inline Uint32 readBigUWordUnaligned(const void *p);
inline Int32 readBigSWordUnaligned(const void *p);

inline void writeBigHalfwordUnaligned(void *p, Int16 v);
inline void writeBigWordUnaligned(void *p, Int32 v);

// Aligned little-endian memory access
inline Uint16 readLittleUHalfword(const void *p);
inline Int16 readLittleSHalfword(const void *p);
inline Uint32 readLittleUWord(const void *p);
inline Int32 readLittleSWord(const void *p);

inline void writeLittleHalfword(void *p, Int16 v);
inline void writeLittleWord(void *p, Int32 v);

// Unaligned little-endian memory access
inline Uint16 readLittleUHalfwordUnaligned(const void *p);
inline Int16 readLittleSHalfwordUnaligned(const void *p);
inline Uint32 readLittleUWordUnaligned(const void *p);
inline Int32 readLittleSWordUnaligned(const void *p);

inline void writeLittleHalfwordUnaligned(void *p, Int16 v);
inline void writeLittleWordUnaligned(void *p, Int32 v);


// --- INLINES ----------------------------------------------------------------


#ifdef IS_BIG_ENDIAN
 inline Uint16 readBigUHalfword(const void *p) {return *static_cast<const Uint16 *>(p);}
 inline Int16 readBigSHalfword(const void *p) {return *static_cast<const Int16 *>(p);}
 inline Uint32 readBigUWord(const void *p) {return *static_cast<const Uint32 *>(p);}
 inline Int32 readBigSWord(const void *p) {return *static_cast<const Int32 *>(p);}

 inline void writeBigHalfword(void *p, Int16 v) {*static_cast<Int16 *>(p) = v;}
 inline void writeBigWord(void *p, Int32 v) {*static_cast<Int32 *>(p) = v;}

 #ifdef MISALIGNED_MEMORY_ACCESS_OK
  inline Uint16 readBigUHalfwordUnaligned(const void *p) {return *static_cast<const Uint16 *>(p);}
  inline Int16 readBigSHalfwordUnaligned(const void *p) {return *static_cast<const Int16 *>(p);}
  inline Uint32 readBigUWordUnaligned(const void *p) {return *static_cast<const Uint32 *>(p);}
  inline Int32 readBigSWordUnaligned(const void *p) {return *static_cast<const Int32 *>(p);}

  inline void writeBigHalfwordUnaligned(void *p, Int16 v) {*static_cast<Int16 *>(p) = v;}
  inline void writeBigWordUnaligned(void *p, Int32 v) {*static_cast<Int32 *>(p) = v;}

 #else
  inline Uint16 readBigUHalfwordUnaligned(const void *p)
	{const Uint8 *q = static_cast<const Uint8 *>(p); return q[0]<<8 | q[1];}
  inline Int16 readBigSHalfwordUnaligned(const void *p)
	{const Uint8 *q = static_cast<const Uint8 *>(p); return q[0]<<8 | q[1];}
  inline Uint32 readBigUWordUnaligned(const void *p)
	{const Uint8 *q = static_cast<const Uint8 *>(p); return q[0]<<24 | q[1]<<16 | q[2]<<8 | q[3];}
  inline Int32 readBigSWordUnaligned(const void *p)
	{const Uint8 *q = static_cast<const Uint8 *>(p); return q[0]<<24 | q[1]<<16 | q[2]<<8 | q[3];}

  inline void writeBigHalfwordUnaligned(void *p, Int16 v)
	{Uint8 *q = static_cast<Uint8 *>(p); q[0] = (Uint8)(v>>8); q[1] = (Uint8)v;}
  inline void writeBigWordUnaligned(void *p, Int32 v)
	{Uint8 *q = static_cast<Uint8 *>(p);
	 q[0] = (Uint8)(v>>24); q[1] = (Uint8)(v>>16); q[2] = (Uint8)(v>>8); q[3] = (Uint8)v;}
 #endif

 inline Uint16 readLittleUHalfword(const void *p)
	{const Uint8 *q = static_cast<const Uint8 *>(p); return q[0] | q[1]<<8;}
 inline Int16 readLittleSHalfword(const void *p)
	{const Uint8 *q = static_cast<const Uint8 *>(p); return q[0] | q[1]<<8;}
 inline Uint32 readLittleUWord(const void *p)
	{const Uint8 *q = static_cast<const Uint8 *>(p); return q[0] | q[1]<<8 | q[2]<<16 | q[3]<<24;}
 inline Int32 readLittleSWord(const void *p)
	{const Uint8 *q = static_cast<const Uint8 *>(p); return q[0] | q[1]<<8 | q[2]<<16 | q[3]<<24;}

 inline void writeLittleHalfword(void *p, Int16 v)
	{Uint8 *q = static_cast<Uint8 *>(p); q[0] = (Uint8)v; q[1] = (Uint8)(v>>8);}
 inline void writeLittleWord(void *p, Int32 v)
	{Uint8 *q = static_cast<Uint8 *>(p);
	 q[0] = (Uint8)v; q[1] = (Uint8)(v>>8); q[2] = (Uint8)(v>>16); q[3] = (Uint8)(v>>24);}

 inline Uint16 readLittleUHalfwordUnaligned(const void *p)
	{const Uint8 *q = static_cast<const Uint8 *>(p); return q[0] | q[1]<<8;}
 inline Int16 readLittleSHalfwordUnaligned(const void *p)
	{const Uint8 *q = static_cast<const Uint8 *>(p); return q[0] | q[1]<<8;}
 inline Uint32 readLittleUWordUnaligned(const void *p)
	{const Uint8 *q = static_cast<const Uint8 *>(p); return q[0] | q[1]<<8 | q[2]<<16 | q[3]<<24;}
 inline Int32 readLittleSWordUnaligned(const void *p)
	{const Uint8 *q = static_cast<const Uint8 *>(p); return q[0] | q[1]<<8 | q[2]<<16 | q[3]<<24;}

 inline void writeLittleHalfwordUnaligned(void *p, Int16 v)
	{Uint8 *q = static_cast<Uint8 *>(p); q[0] = (Uint8)v; q[1] = (Uint8)(v>>8);}
 inline void writeLittleWordUnaligned(void *p, Int32 v)
	{Uint8 *q = static_cast<Uint8 *>(p);
	 q[0] = (Uint8)v; q[1] = (Uint8)(v>>8); q[2] = (Uint8)(v>>16); q[3] = (Uint8)(v>>24);}

#else /* !IS_BIG_ENDIAN */

 inline Uint16 readBigUHalfword(const void *p)
	{const Uint8 *q = static_cast<const Uint8 *>(p); return q[0]<<8 | q[1];}
 inline Int16 readBigSHalfword(const void *p)
	{const Uint8 *q = static_cast<const Uint8 *>(p); return q[0]<<8 | q[1];}
 inline Uint32 readBigUWord(const void *p)
	{const Uint8 *q = static_cast<const Uint8 *>(p); return q[0]<<24 | q[1]<<16 | q[2]<<8 | q[3];}
 inline Int32 readBigSWord(const void *p)
	{const Uint8 *q = static_cast<const Uint8 *>(p); return q[0]<<24 | q[1]<<16 | q[2]<<8 | q[3];}

 inline void writeBigHalfword(void *p, Int16 v)
	{Uint8 *q = static_cast<Uint8 *>(p); q[0] = (Uint8)(v>>8); q[1] = (Uint8)v;}
 inline void writeBigWord(void *p, Int32 v)
	{Uint8 *q = static_cast<Uint8 *>(p);
	 q[0] = (Uint8)(v>>24); q[1] = (Uint8)(v>>16); q[2] = (Uint8)(v>>8); q[3] = (Uint8)v;}

 inline Uint16 readBigUHalfwordUnaligned(const void *p)
	{const Uint8 *q = static_cast<const Uint8 *>(p); return q[0]<<8 | q[1];}
 inline Int16 readBigSHalfwordUnaligned(const void *p)
	{const Uint8 *q = static_cast<const Uint8 *>(p); return q[0]<<8 | q[1];}
 inline Uint32 readBigUWordUnaligned(const void *p)
	{const Uint8 *q = static_cast<const Uint8 *>(p); return q[0]<<24 | q[1]<<16 | q[2]<<8 | q[3];}
 inline Int32 readBigSWordUnaligned(const void *p)
	{const Uint8 *q = static_cast<const Uint8 *>(p); return q[0]<<24 | q[1]<<16 | q[2]<<8 | q[3];}

 inline void writeBigHalfwordUnaligned(void *p, Int16 v)
	{Uint8 *q = static_cast<Uint8 *>(p); q[0] = (Uint8)(v>>8); q[1] = (Uint8)v;}
 inline void writeBigWordUnaligned(void *p, Int32 v)
	{Uint8 *q = static_cast<Uint8 *>(p);
	 q[0] = (Uint8)(v>>24); q[1] = (Uint8)(v>>16); q[2] = (Uint8)(v>>8); q[3] = (Uint8)v;}

 inline Uint16 readLittleUHalfword(const void *p) {return *static_cast<const Uint16 *>(p);}
 inline Int16 readLittleSHalfword(const void *p) {return *static_cast<const Int16 *>(p);}
 inline Uint32 readLittleUWord(const void *p) {return *static_cast<const Uint32 *>(p);}
 inline Int32 readLittleSWord(const void *p) {return *static_cast<const Int32 *>(p);}

 inline void writeLittleHalfword(void *p, Int16 v) {*static_cast<Int16 *>(p) = v;}
 inline void writeLittleWord(void *p, Int32 v) {*static_cast<Int32 *>(p) = v;}

 #ifdef MISALIGNED_MEMORY_ACCESS_OK
  inline Uint16 readLittleUHalfwordUnaligned(const void *p) {return *static_cast<const Uint16 *>(p);}
  inline Int16 readLittleSHalfwordUnaligned(const void *p) {return *static_cast<const Int16 *>(p);}
  inline Uint32 readLittleUWordUnaligned(const void *p) {return *static_cast<const Uint32 *>(p);}
  inline Int32 readLittleSWordUnaligned(const void *p) {return *static_cast<const Int32 *>(p);}

  inline void writeLittleHalfwordUnaligned(void *p, Int16 v) {*static_cast<Int16 *>(p) = v;}
  inline void writeLittleWordUnaligned(void *p, Int32 v) {*static_cast<Int32 *>(p) = v;}

 #else
  inline Uint16 readLittleUHalfwordUnaligned(const void *p)
	{const Uint8 *q = static_cast<const Uint8 *>(p); return q[0] | q[1]<<8;}
  inline Int16 readLittleSHalfwordUnaligned(const void *p)
	{const Uint8 *q = static_cast<const Uint8 *>(p); return q[0] | q[1]<<8;}
  inline Uint32 readLittleUWordUnaligned(const void *p)
	{const Uint8 *q = static_cast<const Uint8 *>(p); return q[0] | q[1]<<8 | q[2]<<16 | q[3]<<24;}
  inline Int32 readLittleSWordUnaligned(const void *p)
	{const Uint8 *q = static_cast<const Uint8 *>(p); return q[0] | q[1]<<8 | q[2]<<16 | q[3]<<24;}

  inline void writeLittleHalfwordUnaligned(void *p, Int16 v)
	{Uint8 *q = static_cast<Uint8 *>(p); q[0] = (Uint8)v; q[1] = (Uint8)(v>>8);}
  inline void writeLittleWordUnaligned(void *p, Int32 v)
	{Uint8 *q = static_cast<Uint8 *>(p);
	 q[0] = (Uint8)v; q[1] = (Uint8)(v>>8); q[2] = (Uint8)(v>>16); q[3] = (Uint8)(v>>24);}
 #endif
#endif /* IS_BIG_ENDIAN */
#endif
