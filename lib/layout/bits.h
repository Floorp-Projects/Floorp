/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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


// TABS 3
//
// Bits array implementation
//
// Lloyd W. Tabb
//
#ifndef _bits_h_
#define _bits_h_

//
//

#ifdef min
#undef min
#endif
#define min(a, b) (((a) < (b)) ? (a) : (b))
#ifdef max
#undef max
#endif
#define max(a, b) (((a) > (b)) ? (a) : (b))

class BitReference {
public:
    uint8 *m_pBits;
    uint8 m_mask;
    BitReference(uint8* pBits, uint8 mask){
        m_pBits = pBits;
        m_mask = mask;
    }

    int operator= (int i){
        *m_pBits = (i ? *m_pBits | m_mask : *m_pBits & ~m_mask);
        return !!i;
    }

    operator int(void){
        return !!(*m_pBits & m_mask);
    }
};

#if 00 /* some compiler don't seem to handle this type of template */


template <int iSize>
class TBitArray {
private:
    uint8 m_Bits[iSize/8+1];
public:
    BitArray(){
        memset(&m_Bits,0, iSize/8+1);
    }

    BitReference operator [] (int i){
        return BitReference(&m_Bits[i >> 3], 1 << (i & 7));
    }

};



#endif


#define BIT_ARRAY_END   -1

class CBitArray {
private:
   uint8 *m_Bits;
   long size;
public:
   //
   //  This constructor is unused, and link clashes with 
   //  CBitArray(long n, int iFirst, ...) for some Cfront based
   //  compilers (HP, SGI 5.2, ...)...djw
   //
#if 0
   CBitArray(long n=0) : m_Bits(0), size(0)  {
      if( n ){
         SetSize(n);
      }
   }
#endif

   //
   //  Call this constructor with a maximum number, a series of bits and 
   //  BIT_ARRAY_END  for example: CBitArray a(100, 1 ,4 9 ,7, BIT_ARRAY_END);
   //
   //  CBitArray::CBitArray(long n, int iFirst, ...) is now edtutil.cpp.
   //  Had to move it into a C++ file so that it could be non-inline.
   //  varargs/inline is a non-portable combo...djw
   // 
   CBitArray(long n, int iFirst, ...);

   void SetSize(long n);

   long Size(){ return size; }

   BitReference operator [] (int i){
      XP_ASSERT(i >= 0 && i < size );
      return BitReference(&m_Bits[i >> 3], 1 << (i & 7));
   }

   void ClearAll();
};



#if 00
//
// This template will generate a Set that can contain all possible values.
//  for an enum.  Sets are created empty.
//
//  ASSUMES:
//    all enum elements are contiguous.
//
//  EXAMPLE:
//
//    enum CType {
//       ctAlpha,
//       ctVoul,
//       ctDigit,
//       ctPunctuation,
//
//       ctMax,
//    };
//
//    typedef TSet<CType,ctMax> Set_CType;
//
//    Set_CType x;
//    x.Add(ctAlpha);         // x has the attribute alpha
//    x.Add(ctVoul);          // x has the attribute Voul
//
//
template <class SetEnum, int setMax>
class TSet {
    char bits[setMax/8+1];
public:
   TSet(){
      memset(bits,0,sizeof(bits));
   }

   //
   // check to see if enum value is in the set
   //
   int In( SetEnum v ){
      return !!(bits[v>>3] & (1<<(v & 7)));
   }

   //
   // Remove Enum from the set
   //
   void Remove( SetEnum v ){
     // bits[v/8] &= ~(1<<v%8);
     bits[v>>3] &= ~(1<< (v & 7));
   }

   //
   // Add Enum to the set
   //
   void Add( SetEnum v ){
     //bits[v/8] |= 1<<v%8;
     bits[v>>3] |= 1<< (v & 7);
   }

};

#endif

#endif
