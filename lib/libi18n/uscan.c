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
typedef  XP_Bool (*uSubScannerFunc) (unsigned char* in, uint16* out);
/*=================================================================================

=================================================================================*/

typedef XP_Bool (*uScannerFunc) (
		uShiftTable 			*shift,
		int32*				state,
		unsigned char		*in,
		uint16				*out,
		uint16 				inbuflen,
		uint16*				inscanlen
);

MODULE_PRIVATE XP_Bool uScan(		
		uShiftTable 			*shift,
		int32*				state,
		unsigned char		*in,
		uint16				*out,
		uint16 				inbuflen,
		uint16*				inscanlen
);

#define uSubScanner(sub,in,out)	(* m_subscanner[sub])((in),(out))

PRIVATE XP_Bool uCheckAndScanAlways1Byte(
		uShiftTable 			*shift,
		int32*				state,
		unsigned char		*in,
		uint16				*out,
		uint16 				inbuflen,
		uint16*				inscanlen
);
PRIVATE XP_Bool uCheckAndScanAlways2Byte(
		uShiftTable 			*shift,
		int32*				state,
		unsigned char		*in,
		uint16				*out,
		uint16 				inbuflen,
		uint16*				inscanlen
);
PRIVATE XP_Bool uCheckAndScanAlways2ByteShiftGR(
		uShiftTable 			*shift,
		int32*				state,
		unsigned char		*in,
		uint16				*out,
		uint16 				inbuflen,
		uint16*				inscanlen
);
PRIVATE XP_Bool uCheckAndScanByTable(
		uShiftTable 			*shift,
		int32*				state,
		unsigned char		*in,
		uint16				*out,
		uint16 				inbuflen,
		uint16*				inscanlen
);
PRIVATE XP_Bool uCheckAndScan2ByteGRPrefix8F(
		uShiftTable 			*shift,
		int32*				state,
		unsigned char		*in,
		uint16				*out,
		uint16 				inbuflen,
		uint16*				inscanlen
);
PRIVATE XP_Bool uCheckAndScan2ByteGRPrefix8EA2(
		uShiftTable 			*shift,
		int32*				state,
		unsigned char		*in,
		uint16				*out,
		uint16 				inbuflen,
		uint16*				inscanlen
);


PRIVATE XP_Bool uScanAlways2Byte(
		unsigned char*		in,
		uint16*				out
);
PRIVATE XP_Bool uScanAlways2ByteShiftGR(
		unsigned char*		in,
		uint16*				out
);
PRIVATE XP_Bool uScanAlways1Byte(
		unsigned char*		in,
		uint16*				out
);
PRIVATE XP_Bool uScanAlways1BytePrefix8E(
		unsigned char*		in,
		uint16*				out
);
PRIVATE XP_Bool uScanAlways2ByteUTF8(
		unsigned char*		in,
		uint16*				out
);
PRIVATE XP_Bool uScanAlways3ByteUTF8(
		unsigned char*		in,
		uint16*				out
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
MODULE_PRIVATE XP_Bool uScan(		
		uShiftTable 			*shift,
		int32*				state,
		unsigned char		*in,
		uint16				*out,
		uint16 				inbuflen,
		uint16*				inscanlen
)
{
	return (* m_scanner[shift->classID]) (shift,state,in,out,inbuflen,inscanlen);
}
/*=================================================================================

=================================================================================*/
PRIVATE XP_Bool uScanAlways1Byte(
		unsigned char*		in,
		uint16*				out
)
{
    *out = (uint16) in[0];
	return TRUE;
}

/*=================================================================================

=================================================================================*/
PRIVATE XP_Bool uScanAlways2Byte(
		unsigned char*		in,
		uint16*				out
)
{
    *out = (uint16) (( in[0] << 8) | (in[1]));
	return TRUE;
}
/*=================================================================================

=================================================================================*/
PRIVATE XP_Bool uScanAlways2ByteShiftGR(
		unsigned char*		in,
		uint16*				out
)
{
    *out = (uint16) ((( in[0] << 8) | (in[1])) &  0x7F7F);
	return TRUE;
}

/*=================================================================================

=================================================================================*/
PRIVATE XP_Bool uScanAlways1BytePrefix8E(
		unsigned char*		in,
		uint16*				out
)
{
	*out = (uint16) in[1];
	return TRUE;
}
/*=================================================================================

=================================================================================*/
PRIVATE XP_Bool uScanAlways2ByteUTF8(
		unsigned char*		in,
		uint16*				out
)
{
    *out = (uint16) (((in[0] & 0x001F) << 6 )| (in[1] & 0x003F));
	return TRUE;
}

/*=================================================================================

=================================================================================*/
PRIVATE XP_Bool uScanAlways3ByteUTF8(
		unsigned char*		in,
		uint16*				out
)
{
    *out = (uint16) (((in[0] & 0x000F) << 12 ) | ((in[1] & 0x003F) << 6)
						| (in[2] & 0x003F));
	return TRUE;
}
/*=================================================================================

=================================================================================*/
PRIVATE XP_Bool uCheckAndScanAlways1Byte(
		uShiftTable 			*shift,
		int32*				state,
		unsigned char		*in,
		uint16				*out,
		uint16 				inbuflen,
		uint16*				inscanlen
)
{
	/*	Don't check inlen. The caller should ensure it is larger than 0 */
	*inscanlen = 1;
	*out = (uint16) in[0];

	return TRUE;
}

/*=================================================================================

=================================================================================*/
PRIVATE XP_Bool uCheckAndScanAlways2Byte(
		uShiftTable 			*shift,
		int32*				state,
		unsigned char		*in,
		uint16				*out,
		uint16 				inbuflen,
		uint16*				inscanlen
)
{
	if(inbuflen < 2)
		return FALSE;
	else
	{
		*inscanlen = 2;
		*out = ((in[0] << 8) | ( in[1])) ;
		return TRUE;
	}
}
/*=================================================================================

=================================================================================*/
PRIVATE XP_Bool uCheckAndScanAlways2ByteShiftGR(
		uShiftTable 			*shift,
		int32*				state,
		unsigned char		*in,
		uint16				*out,
		uint16 				inbuflen,
		uint16*				inscanlen
)
{
	if(inbuflen < 2)
		return FALSE;
	else
	{
		*inscanlen = 2;
		*out = (((in[0] << 8) | ( in[1]))  & 0x7F7F);
		return TRUE;
	}
}
/*=================================================================================

=================================================================================*/
PRIVATE XP_Bool uCheckAndScanByTable(
		uShiftTable 			*shift,
		int32*				state,
		unsigned char		*in,
		uint16				*out,
		uint16 				inbuflen,
		uint16*				inscanlen
)
{
	int16 i;
	uShiftCell* cell = &(shift->shiftcell[0]);
	int16 itemnum = shift->numOfItem;
	for(i=0;i<itemnum;i++)
	{
		if( ( in[0] >=  cell[i].shiftin.Min) &&
			( in[0] <=  cell[i].shiftin.Max))
		{
			if(inbuflen < cell[i].reserveLen)
				return FALSE;
			else
			{
				*inscanlen = cell[i].reserveLen;
				return (uSubScanner(cell[i].classID,in,out));
			}
		}
	}
	return FALSE;
}
/*=================================================================================

=================================================================================*/
PRIVATE XP_Bool uCheckAndScan2ByteGRPrefix8F(
		uShiftTable 			*shift,
		int32*				state,
		unsigned char		*in,
		uint16				*out,
		uint16 				inbuflen,
		uint16*				inscanlen
)
{
	if((inbuflen < 3) ||(in[0] != 0x8F))
		return FALSE;
	else
	{
		*inscanlen = 3;
		*out = (((in[1] << 8) | ( in[2]))  & 0x7F7F);
		return TRUE;
	}
}
/*=================================================================================

=================================================================================*/
PRIVATE XP_Bool uCheckAndScan2ByteGRPrefix8EA2(
		uShiftTable 			*shift,
		int32*				state,
		unsigned char		*in,
		uint16				*out,
		uint16 				inbuflen,
		uint16*				inscanlen
)
{
	if((inbuflen < 4) || (in[0] != 0x8E) || (in[1] != 0xA2))
		return FALSE;
	else
	{
		*inscanlen = 4;
		*out = (((in[2] << 8) | ( in[3]))  & 0x7F7F);
		return TRUE;
	}
}


