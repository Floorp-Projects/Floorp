/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "intlpriv.h"
#include "ugen.h"
#include "xp.h"
/*=================================================================================

=================================================================================*/
typedef  XP_Bool (*uSubGeneratorFunc) (uint16 in, unsigned char* out);
/*=================================================================================

=================================================================================*/

typedef XP_Bool (*uGeneratorFunc) (
		uShiftTable 			*shift,
		int32*				state,
		uint16				in,
		unsigned char*		out,
		uint16 				outbuflen,
		uint16*				outlen
);

MODULE_PRIVATE XP_Bool uGenerate(		
		uShiftTable 			*shift,
		int32*				state,
		uint16				in,
		unsigned char*		out,
		uint16 				outbuflen,
		uint16*				outlen
);

#define uSubGennerator(sub,in,out)	(* m_subgenerator[sub])((in),(out))

PRIVATE XP_Bool uCheckAndGenAlways1Byte(
		uShiftTable 			*shift,
		int32*				state,
		uint16				in,
		unsigned char*		out,
		uint16 				outbuflen,
		uint16*				outlen
);
PRIVATE XP_Bool uCheckAndGenAlways2Byte(
		uShiftTable 			*shift,
		int32*				state,
		uint16				in,
		unsigned char*		out,
		uint16 				outbuflen,
		uint16*				outlen
);
PRIVATE XP_Bool uCheckAndGenAlways2ByteShiftGR(
		uShiftTable 			*shift,
		int32*				state,
		uint16				in,
		unsigned char*		out,
		uint16 				outbuflen,
		uint16*				outlen
);
PRIVATE XP_Bool uCheckAndGenByTable(
		uShiftTable 			*shift,
		int32*				state,
		uint16				in,
		unsigned char*		out,
		uint16 				outbuflen,
		uint16*				outlen
);
PRIVATE XP_Bool uCheckAndGen2ByteGRPrefix8F(
		uShiftTable 			*shift,
		int32*				state,
		uint16				in,
		unsigned char*		out,
		uint16 				outbuflen,
		uint16*				outlen
);
PRIVATE XP_Bool uCheckAndGen2ByteGRPrefix8EA2(
		uShiftTable 			*shift,
		int32*				state,
		uint16				in,
		unsigned char*		out,
		uint16 				outbuflen,
		uint16*				outlen
);


PRIVATE XP_Bool uGenAlways2Byte(
		uint16 				in,
		unsigned char*		out
);
PRIVATE XP_Bool uGenAlways2ByteShiftGR(
		uint16 				in,
		unsigned char*		out
);
PRIVATE XP_Bool uGenAlways1Byte(
		uint16 				in,
		unsigned char*		out
);
PRIVATE XP_Bool uGenAlways1BytePrefix8E(
		uint16 				in,
		unsigned char*		out
);
PRIVATE XP_Bool uGenAlways2ByteUTF8(
		uint16 				in,
		unsigned char*		out
);
PRIVATE XP_Bool uGenAlways3ByteUTF8(
		uint16 				in,
		unsigned char*		out
);
/*=================================================================================

=================================================================================*/
PRIVATE uGeneratorFunc m_generator[uNumOfCharsetType] =
{
	uCheckAndGenAlways1Byte,
	uCheckAndGenAlways2Byte,
	uCheckAndGenByTable,
	uCheckAndGenAlways2ByteShiftGR,
	uCheckAndGen2ByteGRPrefix8F,
	uCheckAndGen2ByteGRPrefix8EA2,
};

/*=================================================================================

=================================================================================*/

PRIVATE uSubGeneratorFunc m_subgenerator[uNumOfCharType] =
{
	uGenAlways1Byte,
	uGenAlways2Byte,
	uGenAlways2ByteShiftGR,
	uGenAlways1BytePrefix8E,
	uGenAlways2ByteUTF8,
	uGenAlways3ByteUTF8

};
/*=================================================================================

=================================================================================*/
MODULE_PRIVATE XP_Bool uGenerate(		
		uShiftTable 			*shift,
		int32*				state,
		uint16				in,
		unsigned char*		out,
		uint16 				outbuflen,
		uint16*				outlen
)
{
	return (* m_generator[shift->classID]) (shift,state,in,out,outbuflen,outlen);
}
/*=================================================================================

=================================================================================*/
PRIVATE XP_Bool uGenAlways1Byte(
		uint16 				in,
		unsigned char*		out
)
{
	out[0] = (unsigned char)in;
	return TRUE;
}

/*=================================================================================

=================================================================================*/
PRIVATE XP_Bool uGenAlways2Byte(
		uint16 				in,
		unsigned char*		out
)
{
	out[0] = (unsigned char)((in >> 8) & 0xff);
	out[1] = (unsigned char)(in & 0xff);
	return TRUE;
}
/*=================================================================================

=================================================================================*/
PRIVATE XP_Bool uGenAlways2ByteShiftGR(
		uint16 				in,
		unsigned char*		out
)
{
	out[0] = (unsigned char)(((in >> 8) & 0xff) | 0x80);
	out[1] = (unsigned char)((in & 0xff) | 0x80);
	return TRUE;
}
/*=================================================================================

=================================================================================*/
PRIVATE XP_Bool uGenAlways1BytePrefix8E(
		uint16 				in,
		unsigned char*		out
)
{
	out[0] = 0x8E;
	out[1] = (unsigned char)(in  & 0xff);
	return TRUE;
}
/*=================================================================================

=================================================================================*/
PRIVATE XP_Bool uGenAlways2ByteUTF8(
		uint16 				in,
		unsigned char*		out
)
{
	out[0] = (unsigned char)(0xC0 | (( in >> 6 ) & 0x1F));
	out[1] = (unsigned char)(0x80 | (( in      ) & 0x3F));
	return TRUE;
}

/*=================================================================================

=================================================================================*/
PRIVATE XP_Bool uGenAlways3ByteUTF8(
		uint16 				in,
		unsigned char*		out
)
{
	out[0] = (unsigned char)(0xE0 | (( in >> 12 ) & 0x0F));
	out[1] = (unsigned char)(0x80 | (( in >> 6  ) & 0x3F));
	out[2] = (unsigned char)(0x80 | (( in       ) & 0x3F));
	return TRUE;
}
/*=================================================================================

=================================================================================*/
PRIVATE XP_Bool uCheckAndGenAlways1Byte(
		uShiftTable 			*shift,
		int32*				state,
		uint16				in,
		unsigned char*		out,
		uint16 				outbuflen,
		uint16*				outlen
)
{
	/*	Don't check inlen. The caller should ensure it is larger than 0 */
	*outlen = 1;
	out[0] = in & 0xff;
	return TRUE;
}

/*=================================================================================

=================================================================================*/
PRIVATE XP_Bool uCheckAndGenAlways2Byte(
		uShiftTable 			*shift,
		int32*				state,
		uint16				in,
		unsigned char*		out,
		uint16 				outbuflen,
		uint16*				outlen
)
{
	if(outbuflen < 2)
		return FALSE;
	else
	{
		*outlen = 2;
		out[0] = ((in >> 8 ) & 0xff);
		out[1] = in  & 0xff;
		return TRUE;
	}
}
/*=================================================================================

=================================================================================*/
PRIVATE XP_Bool uCheckAndGenAlways2ByteShiftGR(
		uShiftTable 			*shift,
		int32*				state,
		uint16				in,
		unsigned char*		out,
		uint16 				outbuflen,
		uint16*				outlen
)
{
	if(outbuflen < 2)
		return FALSE;
	else
	{
		*outlen = 2;
		out[0] = ((in >> 8 ) & 0xff) | 0x80;
		out[1] = (in  & 0xff)  | 0x80;
		return TRUE;
	}
}
/*=================================================================================

=================================================================================*/
PRIVATE XP_Bool uCheckAndGenByTable(
		uShiftTable 			*shift,
		int32*				state,
		uint16				in,
		unsigned char*		out,
		uint16 				outbuflen,
		uint16*				outlen
)
{
	int16 i;
	uShiftCell* cell = &(shift->shiftcell[0]);
	int16 itemnum = shift->numOfItem;
	unsigned char inH, inL;
	inH =	(in >> 8) & 0xff;
	inL = (in & 0xff );
	for(i=0;i<itemnum;i++)
	{
		if( ( inL >=  cell[i].shiftout.MinLB) &&
			( inL <=  cell[i].shiftout.MaxLB) &&
			( inH >=  cell[i].shiftout.MinHB) &&
			( inH <=  cell[i].shiftout.MaxHB)	)
		{
			if(outbuflen < cell[i].reserveLen)
				return FALSE;
			else
			{
				*outlen = cell[i].reserveLen;
				return (uSubGennerator(cell[i].classID,in,out));
			}
		}
	}
	return FALSE;
}
/*=================================================================================

=================================================================================*/
PRIVATE XP_Bool uCheckAndGen2ByteGRPrefix8F( uShiftTable 			*shift,
		int32*				state,
		uint16				in,
		unsigned char*		out,
		uint16 				outbuflen,
		uint16*				outlen
)
{
	if(outbuflen < 3)
		return FALSE;
	else
	{
		*outlen = 3;
		out[0] = 0x8F;
		out[1] = ((in >> 8 ) & 0xff) | 0x80;
		out[2] = (in  & 0xff)  | 0x80;
		return TRUE;
	}
}
/*=================================================================================

=================================================================================*/
PRIVATE XP_Bool uCheckAndGen2ByteGRPrefix8EA2( uShiftTable 			*shift,
		int32*				state,
		uint16				in,
		unsigned char*		out,
		uint16 				outbuflen,
		uint16*				outlen
)
{
	if(outbuflen < 4)
		return FALSE;
	else
	{
		*outlen = 4;
		out[0] = 0x8E;
		out[1] = 0xA2;
		out[2] = ((in >> 8 ) & 0xff) | 0x80;
		out[3] = (in  & 0xff)  | 0x80;
		return TRUE;
	}
}


