/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "unicpriv.h"
#define CHK_GR94(b) ( (PRUint8) 0xa0 < (PRUint8) (b) && (PRUint8) (b) < (PRUint8) 0xff )
#define CHK_GR94_2Byte(b1,b2) (CHK_GR94(b1) && CHK_GR94(b2))
/*=================================================================================

=================================================================================*/
typedef  PRBool (*uSubScannerFunc) (unsigned char* in, PRUint16* out);
/*=================================================================================

=================================================================================*/

typedef PRBool (*uScannerFunc) (
		uShiftTable 			*shift,
		PRInt32*				state,
		unsigned char		*in,
		PRUint16				*out,
		PRUint32 				inbuflen,
		PRUint32*				inscanlen
);

MODULE_PRIVATE PRBool uScan(		
		uShiftTable 			*shift,
		PRInt32*				state,
		unsigned char		*in,
		PRUint16				*out,
		PRUint32 				inbuflen,
		PRUint32*				inscanlen
);

#define uSubScanner(sub,in,out)	(* m_subscanner[sub])((in),(out))

PRIVATE PRBool uCheckAndScanAlways1Byte(
		uShiftTable 			*shift,
		PRInt32*				state,
		unsigned char		*in,
		PRUint16				*out,
		PRUint32 				inbuflen,
		PRUint32*				inscanlen
);
PRIVATE PRBool uCheckAndScanAlways2Byte(
		uShiftTable 			*shift,
		PRInt32*				state,
		unsigned char		*in,
		PRUint16				*out,
		PRUint32 				inbuflen,
		PRUint32*				inscanlen
);
PRIVATE PRBool uCheckAndScanAlways2ByteShiftGR(
		uShiftTable 			*shift,
		PRInt32*				state,
		unsigned char		*in,
		PRUint16				*out,
		PRUint32 				inbuflen,
		PRUint32*				inscanlen
);
PRIVATE PRBool uCheckAndScanByTable(
		uShiftTable 			*shift,
		PRInt32*				state,
		unsigned char		*in,
		PRUint16				*out,
		PRUint32 				inbuflen,
		PRUint32*				inscanlen
);
PRIVATE PRBool uCheckAndScan2ByteGRPrefix8F(
		uShiftTable 			*shift,
		PRInt32*				state,
		unsigned char		*in,
		PRUint16				*out,
		PRUint32 				inbuflen,
		PRUint32*				inscanlen
);
PRIVATE PRBool uCheckAndScan2ByteGRPrefix8EA2(
		uShiftTable 			*shift,
		PRInt32*				state,
		unsigned char		*in,
		PRUint16				*out,
		PRUint32 				inbuflen,
		PRUint32*				inscanlen
);

PRIVATE PRBool uCheckAndScanAlways2ByteSwap(
		uShiftTable 			*shift,
		PRInt32*				state,
		unsigned char		*in,
		PRUint16				*out,
		PRUint32 				inbuflen,
		PRUint32*				inscanlen
);
PRIVATE PRBool uCheckAndScanAlways4Byte(
		uShiftTable 			*shift,
		PRInt32*				state,
		unsigned char		*in,
		PRUint16				*out,
		PRUint32 				inbuflen,
		PRUint32*				inscanlen
);
PRIVATE PRBool uCheckAndScanAlways4ByteSwap(
		uShiftTable 			*shift,
		PRInt32*				state,
		unsigned char		*in,
		PRUint16				*out,
		PRUint32 				inbuflen,
		PRUint32*				inscanlen
);
PRIVATE PRBool uCheckAndScan2ByteGRPrefix8EA3(
		uShiftTable 			*shift,
		PRInt32*				state,
		unsigned char		*in,
		PRUint16				*out,
		PRUint32 				inbuflen,
		PRUint32*				inscanlen
);
PRIVATE PRBool uCheckAndScan2ByteGRPrefix8EA4(
		uShiftTable 			*shift,
		PRInt32*				state,
		unsigned char		*in,
		PRUint16				*out,
		PRUint32 				inbuflen,
		PRUint32*				inscanlen
);
PRIVATE PRBool uCheckAndScan2ByteGRPrefix8EA5(
		uShiftTable 			*shift,
		PRInt32*				state,
		unsigned char		*in,
		PRUint16				*out,
		PRUint32 				inbuflen,
		PRUint32*				inscanlen
);
PRIVATE PRBool uCheckAndScan2ByteGRPrefix8EA6(
		uShiftTable 			*shift,
		PRInt32*				state,
		unsigned char		*in,
		PRUint16				*out,
		PRUint32 				inbuflen,
		PRUint32*				inscanlen
);
PRIVATE PRBool uCheckAndScan2ByteGRPrefix8EA7(
		uShiftTable 			*shift,
		PRInt32*				state,
		unsigned char		*in,
		PRUint16				*out,
		PRUint32 				inbuflen,
		PRUint32*				inscanlen
);
PRIVATE PRBool uCheckAndScanAlways1ByteShiftGL(
		uShiftTable 			*shift,
		PRInt32*				state,
		unsigned char		*in,
		PRUint16				*out,
		PRUint32 				inbuflen,
		PRUint32*				inscanlen
);
PRIVATE PRBool uCnSAlways8BytesComposedHangul(
		uShiftTable 			*shift,
		PRInt32*				state,
		unsigned char		*in,
		PRUint16				*out,
		PRUint32 				inbuflen,
		PRUint32*				inscanlen
);

PRIVATE PRBool uCnSAlways8BytesGLComposedHangul(
		uShiftTable 			*shift,
		PRInt32*				state,
		unsigned char		*in,
		PRUint16				*out,
		PRUint32 				inbuflen,
		PRUint32*				inscanlen
);

PRIVATE PRBool uScanComposedHangulCommon(
		uShiftTable 			*shift,
		PRInt32*				state,
		unsigned char		*in,
		PRUint16				*out,
		PRUint32 				inbuflen,
		PRUint32*				inscanlen,
                PRUint8 mask
);

PRIVATE PRBool uScanAlways2Byte(
		unsigned char*		in,
		PRUint16*				out
);
PRIVATE PRBool uScanAlways2ByteShiftGR(
		unsigned char*		in,
		PRUint16*				out
);
PRIVATE PRBool uScanAlways1Byte(
		unsigned char*		in,
		PRUint16*				out
);
PRIVATE PRBool uScanAlways1BytePrefix8E(
		unsigned char*		in,
		PRUint16*				out
);
PRIVATE PRBool uScanAlways2ByteUTF8(
		unsigned char*		in,
		PRUint16*				out
);
PRIVATE PRBool uScanAlways3ByteUTF8(
		unsigned char*		in,
		PRUint16*				out
);
/*=================================================================================

=================================================================================*/
PRIVATE uScannerFunc m_scanner[uNumOfCharsetType] =
{
	uCheckAndScanAlways1Byte,
	uCheckAndScanAlways2Byte,
	uCheckAndScanByTable,
	uCheckAndScanAlways2ByteShiftGR,
	uCheckAndScan2ByteGRPrefix8F,
	uCheckAndScan2ByteGRPrefix8EA2,
	uCheckAndScanAlways2ByteSwap,
	uCheckAndScanAlways4Byte,
	uCheckAndScanAlways4ByteSwap,
	uCheckAndScan2ByteGRPrefix8EA3,
	uCheckAndScan2ByteGRPrefix8EA4,
	uCheckAndScan2ByteGRPrefix8EA5,
	uCheckAndScan2ByteGRPrefix8EA6,
	uCheckAndScan2ByteGRPrefix8EA7,
	uCheckAndScanAlways1ByteShiftGL,
        uCnSAlways8BytesComposedHangul
};

/*=================================================================================

=================================================================================*/

PRIVATE uSubScannerFunc m_subscanner[uNumOfCharType] =
{
	uScanAlways1Byte,
	uScanAlways2Byte,
	uScanAlways2ByteShiftGR,
	uScanAlways1BytePrefix8E,
	uScanAlways2ByteUTF8,
	uScanAlways3ByteUTF8
};
/*=================================================================================

=================================================================================*/
MODULE_PRIVATE PRBool uScan(		
		uShiftTable 			*shift,
		PRInt32*				state,
		unsigned char		*in,
		PRUint16				*out,
		PRUint32 				inbuflen,
		PRUint32*				inscanlen
)
{
	return (* m_scanner[shift->classID]) (shift,state,in,out,inbuflen,inscanlen);
}
/*=================================================================================

=================================================================================*/
PRIVATE PRBool uScanAlways1Byte(
		unsigned char*		in,
		PRUint16*				out
)
{
    *out = (PRUint16) in[0];
	return PR_TRUE;
}

/*=================================================================================

=================================================================================*/
PRIVATE PRBool uScanAlways2Byte(
		unsigned char*		in,
		PRUint16*				out
)
{
    *out = (PRUint16) (( in[0] << 8) | (in[1]));
	return PR_TRUE;
}
/*=================================================================================

=================================================================================*/
PRIVATE PRBool uScanAlways2ByteShiftGR(
		unsigned char*		in,
		PRUint16*				out
)
{
    *out = (PRUint16) ((( in[0] << 8) | (in[1])) &  0x7F7F);
	return PR_TRUE;
}

/*=================================================================================

=================================================================================*/
PRIVATE PRBool uScanAlways1BytePrefix8E(
		unsigned char*		in,
		PRUint16*				out
)
{
	*out = (PRUint16) in[1];
	return PR_TRUE;
}
/*=================================================================================

=================================================================================*/
PRIVATE PRBool uScanAlways2ByteUTF8(
		unsigned char*		in,
		PRUint16*				out
)
{
    *out = (PRUint16) (((in[0] & 0x001F) << 6 )| (in[1] & 0x003F));
	return PR_TRUE;
}

/*=================================================================================

=================================================================================*/
PRIVATE PRBool uScanAlways3ByteUTF8(
		unsigned char*		in,
		PRUint16*				out
)
{
    *out = (PRUint16) (((in[0] & 0x000F) << 12 ) | ((in[1] & 0x003F) << 6)
						| (in[2] & 0x003F));
	return PR_TRUE;
}
/*=================================================================================

=================================================================================*/
PRIVATE PRBool uCheckAndScanAlways1Byte(
		uShiftTable 			*shift,
		PRInt32*				state,
		unsigned char		*in,
		PRUint16				*out,
		PRUint32 				inbuflen,
		PRUint32*				inscanlen
)
{
	/*	Don't check inlen. The caller should ensure it is larger than 0 */
	*inscanlen = 1;
	*out = (PRUint16) in[0];

	return PR_TRUE;
}

/*=================================================================================

=================================================================================*/
PRIVATE PRBool uCheckAndScanAlways2Byte(
		uShiftTable 			*shift,
		PRInt32*				state,
		unsigned char		*in,
		PRUint16				*out,
		PRUint32 				inbuflen,
		PRUint32*				inscanlen
)
{
	if(inbuflen < 2)
		return PR_FALSE;
	else
	{
		*inscanlen = 2;
		*out = ((in[0] << 8) | ( in[1])) ;
		return PR_TRUE;
	}
}
/*=================================================================================

=================================================================================*/
PRIVATE PRBool uCheckAndScanAlways2ByteShiftGR(
		uShiftTable 			*shift,
		PRInt32*				state,
		unsigned char		*in,
		PRUint16				*out,
		PRUint32 				inbuflen,
		PRUint32*				inscanlen
)
{
  /*
   *Both bytes should be in the range of [0xa1,0xfe] for 94x94 character sets
   * invoked on GR. No encoding implemented in Mozilla uses 96x96 char. sets.
   */
	if(inbuflen < 2 || ! CHK_GR94_2Byte(in[1],in[0]))
		return PR_FALSE;
	else
	{
		*inscanlen = 2;
		*out = (((in[0] << 8) | ( in[1]))  & 0x7F7F);
		return PR_TRUE;
	}
}
/*=================================================================================

=================================================================================*/
PRIVATE PRBool uCheckAndScanByTable(
		uShiftTable 			*shift,
		PRInt32*				state,
		unsigned char		*in,
		PRUint16				*out,
		PRUint32 				inbuflen,
		PRUint32*				inscanlen
)
{
	PRInt16 i;
	uShiftCell* cell = &(shift->shiftcell[0]);
	PRInt16 itemnum = shift->numOfItem;
	for(i=0;i<itemnum;i++)
	{
		if( ( in[0] >=  cell[i].shiftin.Min) &&
			( in[0] <=  cell[i].shiftin.Max))
		{
			if(inbuflen < cell[i].reserveLen)
				return PR_FALSE;
			else
			{
				*inscanlen = cell[i].reserveLen;
				return (uSubScanner(cell[i].classID,in,out));
			}
		}
	}
	return PR_FALSE;
}
/*=================================================================================

=================================================================================*/
PRIVATE PRBool uCheckAndScan2ByteGRPrefix8F(
		uShiftTable 			*shift,
		PRInt32*				state,
		unsigned char		*in,
		PRUint16				*out,
		PRUint32 				inbuflen,
		PRUint32*				inscanlen
)
{
	if((inbuflen < 3) ||(in[0] != 0x8F) || ! CHK_GR94_2Byte(in[1],in[2]))
		return PR_FALSE;
	else
	{
		*inscanlen = 3;
		*out = (((in[1] << 8) | ( in[2]))  & 0x7F7F);
		return PR_TRUE;
	}
}
/*=================================================================================

=================================================================================*/
PRIVATE PRBool uCheckAndScan2ByteGRPrefix8EA2(
		uShiftTable 			*shift,
		PRInt32*				state,
		unsigned char		*in,
		PRUint16				*out,
		PRUint32 				inbuflen,
		PRUint32*				inscanlen
)
{
	if((inbuflen < 4) || (in[0] != 0x8E) || (in[1] != 0xA2) || ! CHK_GR94_2Byte(in[2],in[3]))
		return PR_FALSE;
	else
	{
		*inscanlen = 4;
		*out = (((in[2] << 8) | ( in[3]))  & 0x7F7F);
		return PR_TRUE;
	}
}

/*=================================================================================

=================================================================================*/
PRIVATE PRBool uCheckAndScanAlways2ByteSwap(
		uShiftTable 			*shift,
		PRInt32*				state,
		unsigned char		*in,
		PRUint16				*out,
		PRUint32 				inbuflen,
		PRUint32*				inscanlen
)
{
	if(inbuflen < 2)
		return PR_FALSE;
	else
	{
		*inscanlen = 2;
		*out = ((in[1] << 8) | ( in[0])) ;
		return PR_TRUE;
	}
}
/*=================================================================================

=================================================================================*/
PRIVATE PRBool uCheckAndScanAlways4Byte(
		uShiftTable 			*shift,
		PRInt32*				state,
		unsigned char		*in,
		PRUint16				*out,
		PRUint32 				inbuflen,
		PRUint32*				inscanlen
)
{
	if(inbuflen < 4)
		return PR_FALSE;
	else
	{
		*inscanlen = 4;
                if((0 ==in[0]) && ( 0==in[1]))
		    *out = ((in[2] << 8) | ( in[3])) ;
                else
		    *out = 0xFFFD ;
		return PR_TRUE;
	}
}

/*=================================================================================

=================================================================================*/
PRIVATE PRBool uCheckAndScanAlways4ByteSwap(
		uShiftTable 			*shift,
		PRInt32*				state,
		unsigned char		*in,
		PRUint16				*out,
		PRUint32 				inbuflen,
		PRUint32*				inscanlen
)
{
	if(inbuflen < 4)
		return PR_FALSE;
	else
	{
		*inscanlen = 4;
                if((0 ==in[2]) && ( 0==in[3]))
		    *out = ((in[1] << 8) | ( in[0])) ;
                else
		    *out = 0xFFFD ;
		return PR_TRUE;
	}
}
/*=================================================================================

=================================================================================*/
PRIVATE PRBool uCheckAndScan2ByteGRPrefix8EA3(
		uShiftTable 			*shift,
		PRInt32*				state,
		unsigned char		*in,
		PRUint16				*out,
		PRUint32 				inbuflen,
		PRUint32*				inscanlen
)
{
	if((inbuflen < 4) || (in[0] != 0x8E) || (in[1] != 0xA3) || ! CHK_GR94_2Byte(in[2],in[3]))
		return PR_FALSE;
	else
	{
		*inscanlen = 4;
		*out = (((in[2] << 8) | ( in[3]))  & 0x7F7F);
		return PR_TRUE;
	}
}
/*=================================================================================

=================================================================================*/
PRIVATE PRBool uCheckAndScan2ByteGRPrefix8EA4(
		uShiftTable 			*shift,
		PRInt32*				state,
		unsigned char		*in,
		PRUint16				*out,
		PRUint32 				inbuflen,
		PRUint32*				inscanlen
)
{
	if((inbuflen < 4) || (in[0] != 0x8E) || (in[1] != 0xA4) || ! CHK_GR94_2Byte(in[2],in[3]))
		return PR_FALSE;
	else
	{
		*inscanlen = 4;
		*out = (((in[2] << 8) | ( in[3]))  & 0x7F7F);
		return PR_TRUE;
	}
}
/*=================================================================================

=================================================================================*/
PRIVATE PRBool uCheckAndScan2ByteGRPrefix8EA5(
		uShiftTable 			*shift,
		PRInt32*				state,
		unsigned char		*in,
		PRUint16				*out,
		PRUint32 				inbuflen,
		PRUint32*				inscanlen
)
{
	if((inbuflen < 4) || (in[0] != 0x8E) || (in[1] != 0xA5) || ! CHK_GR94_2Byte(in[2],in[3]))
		return PR_FALSE;
	else
	{
		*inscanlen = 4;
		*out = (((in[2] << 8) | ( in[3]))  & 0x7F7F);
		return PR_TRUE;
	}
}
/*=================================================================================

=================================================================================*/
PRIVATE PRBool uCheckAndScan2ByteGRPrefix8EA6(
		uShiftTable 			*shift,
		PRInt32*				state,
		unsigned char		*in,
		PRUint16				*out,
		PRUint32 				inbuflen,
		PRUint32*				inscanlen
)
{
	if((inbuflen < 4) || (in[0] != 0x8E) || (in[1] != 0xA6) || ! CHK_GR94_2Byte(in[2],in[3]))
		return PR_FALSE;
	else
	{
		*inscanlen = 4;
		*out = (((in[2] << 8) | ( in[3]))  & 0x7F7F);
		return PR_TRUE;
	}
}
/*=================================================================================

=================================================================================*/
PRIVATE PRBool uCheckAndScan2ByteGRPrefix8EA7(
		uShiftTable 			*shift,
		PRInt32*				state,
		unsigned char		*in,
		PRUint16				*out,
		PRUint32 				inbuflen,
		PRUint32*				inscanlen
)
{
	if((inbuflen < 4) || (in[0] != 0x8E) || (in[1] != 0xA7) || ! CHK_GR94_2Byte(in[2],in[3]))
		return PR_FALSE;
	else
	{
		*inscanlen = 4;
		*out = (((in[2] << 8) | ( in[3]))  & 0x7F7F);
		return PR_TRUE;
	}
}
/*=================================================================================

=================================================================================*/
PRIVATE PRBool uCheckAndScanAlways1ByteShiftGL(
		uShiftTable 			*shift,
		PRInt32*				state,
		unsigned char		*in,
		PRUint16				*out,
		PRUint32 				inbuflen,
		PRUint32*				inscanlen
)
{
	/*	Don't check inlen. The caller should ensure it is larger than 0 */
	*inscanlen = 1;
	*out = (PRUint16) in[0] | 0x80;

	return PR_TRUE;
}
/*=================================================================================

=================================================================================*/
#define SBase 0xAC00
#define SCount 11172
#define LCount 19
#define VCount 21
#define TCount 28
#define NCount (VCount * TCount)
PRIVATE PRBool uScanComposedHangulCommon(
		uShiftTable 			*shift,
		PRInt32*				state,
		unsigned char		*in,
		PRUint16				*out,
		PRUint32 				inbuflen,
		PRUint32*				inscanlen,
                PRUint8    mask
)
{
        
        PRUint16 LIndex, VIndex, TIndex;
        /* no 8 bytes, not in a4 range, or the first 2 byte are not a4d4 */
        if((inbuflen < 8) || ((mask & 0xa4) != in[0]) || ((mask& 0xd4) != in[1]) ||
           ((mask&0xa4) != in[2] ) || ((mask&0xa4) != in[4]) || ((mask&0xa4) != in[6]))
		return PR_FALSE;

        /* Compute LIndex  */
        if((in[3] < (mask&0xa1)) && (in[3] > (mask&0xbe))) { /* illegal leading consonant */
 	  return PR_FALSE;
        } else {
          static PRUint8 lMap[] = {
          /*        A1   A2   A3   A4   A5   A6   A7  */
                     0,   1,0xff,   2,0xff,0xff,   3,
          /*   A8   A9   AA   AB   AC   AD   AE   AF  */
                4,   5,0xff,0xff,0xff,0xff,0xff,0xff,
          /*   B0   B1   B2   B3   B4   B5   B6   B7  */
             0xff,   6,   7,   8,0xff,   9,  10,  11,
          /*   B8   B9   BA   BB   BC   BD   BE       */
               12,  13,  14,  15,  16,  17,  18     
          };

          LIndex = lMap[in[3] - (0xa1 & mask)];
          if(0xff == (0xff & LIndex))
 	    return PR_FALSE;
        }
        
        /* Compute VIndex  */
        if((in[5] < (mask&0xbf)) && (in[5] > (mask&0xd3))) { /* illegal medial vowel */
 	  return PR_FALSE;
        } else {
          VIndex = in[5] - (mask&0xbf);
        }

        /* Compute TIndex  */
        if((mask&0xd4) == in[7])  
        {
          TIndex = 0;
        } else if((in[7] < (mask&0xa1)) && (in[7] > (mask&0xbe))) {/* illegal trailling consonant */
 	  return PR_FALSE;
        } else {
          static PRUint8 tMap[] = {
          /*        A1   A2   A3   A4   A5   A6   A7  */
                     1,   2,   3,   4,   5,   6,   7,
          /*   A8   A9   AA   AB   AC   AD   AE   AF  */
             0xff,   8,   9,  10,  11,  12,  13,  14,
          /*   B0   B1   B2   B3   B4   B5   B6   B7  */
               15,  16,  17,0xff,  18,  19,  20,  21,
          /*   B8   B9   BA   BB   BC   BD   BE       */
               22,0xff,  23,  24,  25,  26,  27     
          };
          TIndex = tMap[in[3] - (mask&0xa1)];
          if(0xff == (0xff & TIndex))
 	    return PR_FALSE;
        }

	*inscanlen = 8;
        /* the following line is from Unicode 2.0 page 3-13 item 5 */
        *out = ( LIndex * VCount + VIndex) * TCount + TIndex + SBase;

	return PR_TRUE;
}
/*=================================================================================

=================================================================================*/
PRIVATE PRBool uCnSAlways8BytesComposedHangul(
		uShiftTable 			*shift,
		PRInt32*				state,
		unsigned char		*in,
		PRUint16				*out,
		PRUint32 				inbuflen,
		PRUint32*				inscanlen
)
{
   return uScanComposedHangulCommon(shift,state,in,out,inbuflen,inscanlen,0xff);
}
/*=================================================================================

=================================================================================*/
PRIVATE PRBool uCnSAlways8BytesGLComposedHangul(
		uShiftTable 			*shift,
		PRInt32*				state,
		unsigned char		*in,
		PRUint16				*out,
		PRUint32 				inbuflen,
		PRUint32*				inscanlen
)
{
   return uScanComposedHangulCommon(shift,state,in,out,inbuflen,inscanlen,0x7f);
}
