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
/* unicvrt.c   
 * ---------   
 * 			   
 *
 * This file implements conversions from one Unicode format to another
 * Unicode format.
 *
 * There are no conversions to/from other encodings.
 *
 * There are streams conversion between UTF8 and UCS2, and UTF8 and UTF7.
 * It generates a DLL on Win 32, and at present, normal libraries on mac, X, and
 * Win16. 
 */

#define _UNICVT_DLL_  1

#include "intlpriv.h"
#include "unicpriv.h"
#include "xp.h"
#include <string.h>

#ifdef XP_WIN32
#define XP_ALLOC_PRIV	malloc
#else
#define XP_ALLOC_PRIV	XP_ALLOC
#endif

typedef struct utf7_encoding_method_data {
	int16			*fromb64;
	unsigned char	*tob64;
	unsigned char	*shift;
	unsigned char	startshift;
	unsigned char	endshift;
} utf7_encoding_method_data;


int32
ucs2_to_utf8_buffer(const uint16 *ucs2p, int32 num_chars, 
		unsigned char *utf8p, int32 num_utf8_bytes, int32 *utf8_bytes_written);


/* Private Helper function prototypes */

PRIVATE int16 one_utf8_to_ucs2_char(const unsigned char *utf8p, const unsigned char *utf8endp, 
							   uint16 *onecharp);
 
PRIVATE int16 one_ucs2_to_utf8_char(unsigned char *tobufp, 
		unsigned char *tobufendp, uint16 onechar);

PRIVATE unsigned char *intl_utf72utf8(	CCCDataObject		obj,
				const unsigned char	*utf7buf,
				int32				utf7bufsz,
				utf7_encoding_method_data*	opt
				);
PRIVATE unsigned char *intl_utf82utf7(	CCCDataObject		obj,
				const unsigned char	*utf8buf,	
				int32				utf8bufsz,
				utf7_encoding_method_data*	opt
				);


PRIVATE uint16  pad_and_write(uint32 buffer, unsigned char *tobufp, 
							int16 bufferBitCount, utf7_encoding_method_data*	opt);

PRIVATE void swap_ucs2_bytes(unsigned char *ucsbuf, int32 ucsbufsz); 


/* Private constants */

#define MAX_UCS2			0xFFFF	 
#define DEFAULT_CHAR		0x003F	/* Default char is "?" */
#define BYTE_MASK			0xBF
#define BYTE_MARK			0x80

#define MAX_ASCII			0x7F
#define NOT_BASE64			-1





/* Take care of different API for different platforms */


#ifdef XP_WIN32

/* UNICVTAPI def now accomplished in libi18n.h */
/*#define  UNICVTAPI __declspec(dllexport)*/


/* THIS #define IS VERY BAD AND SHOULD BE CHANGED WHEN WE REVISIT
 * THE ERROR HANDLING STUFF AND MOVE IT ALL OUT OF XPSTR.H
 * THE CALL SHOULD BE: extern int MK_OUT_OF_MEMORY; BUT WE HAVE
 * CHICKEN AND EGG LINKING PROBLEMS ON WIN32 BECAUSE THE DLL
 * MUST BE COMPILED BEFORE THE int IS DECLARED.
 */

#define MK_OUT_OF_MEMORY	-207

#else /* !XP_WIN32 */

/* UNICVTAPI def now accomplished in libi18n.h */
/*#define UNICVTAPI*/

extern int MK_OUT_OF_MEMORY;

#endif /*!XP_WIN32 */



/* UCS-2 to UTF-8 conversion routines */

/*
 * mz_ucs2utf8
 * -----------
 * 
 * Takes a CCCDataObject, a buffer of UCS-2 data, and the size of that buffer.
 * Allocates and returns the translation of the UCS-2 data in UTF-8. The caller
 * is responsible for freeing the allocated memory. If the UCS-2 data is not
 * complete, and ends on a character boundary, the extra byte of data is stored
 * in uncvtbuf, and will be used the next time this function is called.
 *
 * Note about swapping: UCS-2 data can come in big-endian or little-endian
 * order, so we need to be aware of the need to potentially swap the data.
 * On the very first block of the stream we will discover (because UCS-2
 * always begins with a byte order mark) whether the data is of the same or
 * opposite endian-ness from us. 
 * The information is store in FromCSID
 * The use of uncvtbuf:
 *   uncvtbuf[0] is 0 or 1
 *	 uncvtbuf[0] == 0 - there are no left over last time
 *	 uncvtbuf[0] == 1 - there one byte left over last time stored in uncvtbuf[1]
 *
 */
MODULE_PRIVATE UNICVTAPI unsigned char *
mz_ucs2utf8(	CCCDataObject		obj,
				const unsigned char	*ucsbuf,	/* UCS-2 buf for conv */
				int32				ucsbufsz)	/* UCS-2 buf size in bytes */
{
	int32	tobufsz;
	unsigned char *tobuf = NULL;
	unsigned char *tobufp, *tobufendp,*ucsp, *ucsendp;
	int16	numUTF8bytes;
	uint16 	onechar;
	XP_Bool needToSwap = FALSE;
	int 	scanstate = 0;
	unsigned p1=0, p2;
	unsigned char *uncvtbuf =INTL_GetCCCUncvtbuf(obj);


	if(INTL_GetCCCFromCSID(obj) ==  CS_UCS2_SWAP)
		needToSwap = TRUE;

	/* Allocate Memory */
	/* In the worst case, one UCS2 could expand to three byte */
    /* so, the ration is 2:3 	*/
	tobufsz = (3*(ucsbufsz + 1)) / 2 + 2;
	if ((tobuf = (unsigned char *)XP_ALLOC_PRIV(tobufsz)) == (unsigned char *)NULL) 
	{
		INTL_SetCCCRetval(obj, MK_OUT_OF_MEMORY);
		return(NULL);
	}

	/* do the set up */
	tobufendp = tobuf + tobufsz;	/* point to the end of buffer */
	tobufp = tobuf;					/* point to the begining of buffer */
	ucsp = (unsigned char *)ucsbuf;
	ucsendp = (unsigned char *)ucsbuf + ucsbufsz;

	/* Get the unconvert byte */
	if(uncvtbuf[0] > 0)
	{
		p1 = uncvtbuf[1];
		scanstate++; 
	}
	/* Do the conversion */
	while( ucsp < ucsendp ) 
 	{
		if(scanstate++ == 0)
		{
			p1 = *ucsp;
		}
		else 
		{
			p2 = *ucsp;
			scanstate = 0;
			onechar = (p1 << 8) | (p2);
			/* Look for (and strip) BYTE_ORDER_MARK */
			if(onechar == NEEDS_SWAP_MARK) 
			{
				INTL_SetCCCFromCSID(obj, CS_UCS2_SWAP);
				needToSwap = TRUE;
			} 
			else if(onechar == BYTE_ORDER_MARK)  
			{
				INTL_SetCCCFromCSID(obj, CS_UCS2);
				needToSwap = FALSE;
			} 
			else
			{
				if(needToSwap)
					numUTF8bytes = one_ucs2_to_utf8_char(tobufp, tobufendp, 
									(uint16)((p2 << 8) | (p1)));
				else
					numUTF8bytes = one_ucs2_to_utf8_char(tobufp, tobufendp, onechar);

				if(numUTF8bytes == -1) 
					break; /* out of space in tobuf */

				tobufp += numUTF8bytes;
			}
		}
		ucsp ++;
	}
	*tobufp = '\0';								/* NULL terminate dest. data */
	INTL_SetCCCLen(obj, tobufp - tobuf);		/* length of processed data, in bytes */

	/* If there are left over, set it to uncvtbuf[1] */
	if((uncvtbuf[0] = scanstate) != 0)
		uncvtbuf[1] = p1;
	return(tobuf);
}

/* UTF-8 to UCS-2 */

 /* 
  * mz_utf82ucs
  * -----------
  *
  * This function takes a streams object, a buffer of utf8 data, and the
  * size of that buffer. It allocates, fills, and returns a buffer of the 
  * equivalent UCS-2 data. The caller is responsible for freeing that
  * data. If the UTF-8 data cannot be completely converted, the unconverted
  * final bytes will be stored in uncvtbuf and used on the next call.
  * 
  * Note: UCS-2 data must always begin with a byte order mark, so we
  * must write that at the beginning of our stream. This function 
  * employs obj->cvtflag to determine if it is indeed at the beginning
  * of the stream. obj->cvtflag starts at 0, and we switch it to 1
  * as we write the byte order mark. 
  *
  * A note on endian-ness: This function will return UCS-2 data of the
  * same endian-ness as the machine we are running on. To generate data
  * of the opposite endian-ness, use mz_utf82ucsswap.
  */


MODULE_PRIVATE UNICVTAPI unsigned char *
mz_utf82ucs(	CCCDataObject		obj,
				const unsigned char	*utf8buf,	/* UTF-8 buf for conv */
				int32				utf8bufsz)	/* UTF-8 buf size in bytes */


{

	unsigned char	*tobuf = NULL;
	int32			tobufsz;
	unsigned char	*tobufp, *utf8p;		/* current byte in bufs	*/
 	unsigned char	*tobufendp, *utf8endp;	/* end of buffers		*/
 	int32					uncvtlen;
	unsigned char *uncvtbuf = INTL_GetCCCUncvtbuf(obj);

 

	uint16 onechar;
	int16 numoctets;


#define ucsbufsz	tobufsz
#define ucsbuf		tobuf
#define ucsp		tobufp
#define ucsendp	tobufendp
											  	/* Allocate a dest buffer:		*/


	/* At worst, all the octets are ASCII, and each 1 byte of UTF 8
	 * will take 2 bytes of UCS-2, plus 2 for NULL termination (and
	 * possibly 2 for byte order mark)
	 */

	uncvtlen = strlen((char *)uncvtbuf);
	tobufsz = 2*(utf8bufsz + uncvtlen) + 4;  
	if (!tobufsz) {
		return NULL;
	}

	if ((tobuf = (unsigned char *)XP_ALLOC_PRIV(tobufsz)) == (unsigned char *)NULL) {
		INTL_SetCCCRetval(obj, MK_OUT_OF_MEMORY);
		return(NULL);
	}

	
	/* Initialize pointers, etc.	*/
 	utf8p = (unsigned char *)utf8buf;
 	utf8endp = utf8p + utf8bufsz - 1; /* leave room for NULL termination (as sentinel?)*/

#define uncvtp	tobufp		/* use tobufp as temp index for uncvtbuf */ 		 		
							/* If prev. unconverted chars, append unconverted
							 * chars w/new chars and try to process.
							 */

 	if (uncvtbuf[0] != '\0') {
 		uncvtp = uncvtbuf + uncvtlen;
 		while (uncvtp < (uncvtbuf + UNCVTBUF_SIZE) &&
														utf8p <= utf8endp)
			 *uncvtp++ = *utf8p++;

 		*uncvtp = '\0';						/* nul terminate as sentinel */
 		utf8p = uncvtbuf;				/* process unconverted first */
 		utf8endp = uncvtp - 1;

 	} 

#undef uncvtp
 	
 	tobufp = tobuf;
 	tobufendp = tobufp + tobufsz - 3;		/* save space for terminating null */

	/* write byte order mark */

	  if(!(INTL_GetCCCCvtflag(obj))) {
		*((uint16 *) tobufp) = (uint16) BYTE_ORDER_MARK;
		tobufp += 2;
		INTL_SetCCCCvtflag(obj, TRUE);
	  }

 WHILELOOP:

	while( (tobufp <= tobufendp) && (utf8p <= utf8endp) ) {


		numoctets = one_utf8_to_ucs2_char(utf8p, utf8endp, &onechar);
		if(numoctets == -1) break; /* not enought utf8 data */
		utf8p += numoctets;



		/* Check to make sure there's space to write onechar */
		if((tobufp+2) >= tobufendp) break;

		*((uint16 *) tobufp) = (onechar <= MAX_UCS2 ? onechar :  DEFAULT_CHAR);
			
		tobufp +=2;
	
	}
	if(uncvtbuf[0] != '\0') {			/* Just processed unconverted chars.
											 * ucsp points to 1st unprocessed char 
											 * in ucsbuf. Some may have been 
											 * processed while processing unconverted
											 * chars, so setup ptrs. not to process
											 * them twice.
											 */

											/* If nothing was converted, there wasn't
											 * enough UCS-2 data. Stop and get more
											 * data.
											 */

		if(utf8p == uncvtbuf) {		/* nothing was converted */
			*tobufp = '\0';
			return(NULL);
		}
		utf8endp = (unsigned char *) utf8buf + utf8bufsz - 1; 
		utf8p = 	(unsigned char *) utf8buf + (utf8p - uncvtbuf - uncvtlen);
		uncvtbuf[0] = '\0';		   /* No more unconverted chars.*/
		goto WHILELOOP;					   /* Process new data */
	}

	 *tobufp = '\0';				/* NULL terminate dest. data */

	INTL_SetCCCLen(obj, tobufp - tobuf);		/* length of processed data, in bytes */

	if(utf8p <= utf8endp) {			/*  unconverted utf8 left? */
		tobufp = uncvtbuf;		/* just using tobufp as a temp index. */
		while (utf8p <= utf8endp)
				*tobufp++ = *utf8p++;
		*tobufp = '\0';				/* NULL terminate, as a sentinel */
	}


#undef ucsbufsz	 
#undef ucsbuf		 
#undef ucsp		 
#undef ucsendp	 
							

	return(tobuf);
}



/*
 * mz_utf82ucsswap
 * ---------------
 *
 * mz_utf82ucs will convert the UTF-8 data to UCS-2 data of the same
 * endian-ness of the platform the client is running on. Occasionally,
 * this is not what is desired. mz_utf82ucsswap converts the UTF-8
 * data to UCS-2 of the opposite endian-ness.
 */

  
MODULE_PRIVATE UNICVTAPI unsigned char *
mz_utf82ucsswap(	CCCDataObject		obj,
				const unsigned char	*utf8buf,	/* UTF-8 buf for conv */
				int32				utf8bufsz)	/* UTF-8 buf size in bytes */
{

	unsigned char *result;

	result = mz_utf82ucs(obj, utf8buf, utf8bufsz);
	swap_ucs2_bytes(result, INTL_GetCCCLen(obj));
	return(result);

}


/* UTF-7 to UTF-8 conversion routines */




/* mz_utf72utf8
 * ------------
 * 
 * Takes a streams object, a buffer of UTF-7 data, and the size of
 * that buffer.  Allocates, fills, and returns a buffer of UTF-8 
 * data. (Its size is returned in the CCCDataObject.) The caller
 * is responsible for freeing the returned buffer. 
 * 
 * Note: UTF-7 has the property that multiple characters of UTF-7 
 * may make up a single character of UTF-8. Also, a single UTF-7 char 
 * may contribute bits to more than one utf8 character. If such a
 * UTF-7 character is involved at the end of the current chunk, it won't
 * be save-able in uncvtbuf. For this reason, we also need to
 * save the bit buffer. It turns out that we also need to save the
 * fact that we are within a shifted sequence, because there is no
 * other way for that information to persist between chunks of a 
 * stream. If we save a buffer, then we are certainly in the middle
 * of a shifted sequence, but even if there is no buffer to save, we
 * may still be in a shifted sequence.
 * 
 * The streams module gives me one int32 - obj->cvtflag - in which
 * to save my state.  This means that to save all my data, I'll need
 * to do a few bit-wise operations.
 *
 * Arbitrarily, the top two bytes will hold the buffer, the next byte
 * holds the count of relevant bits in the buffer, and the low order
 * byte will hold 0 if we are not in a shiftSequence, 1 if we are.
 *
 * Since we will only save a buffer and bufferBitCount if we are
 * in a shift sequence when this chunk terminates, obj->cvtflag == 0
 * when we do not terminate in a shift sequence.
 */


/*
	tables for RFC1642- UTF7
*/

PRIVATE	int16 rfc1642_fromb64[128] = 
{
	/*   0 */  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
	/*  10 */  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
	/*  20 */  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
	/*  30 */  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
	/*  40 */  -1,  -1,  -1,  62,  -1,  -1,  -1,  63,  52,  53,
	/*  50 */  54,  55,  56,  57,  58,  59,  60,  61,  -1,  -1,
	/*  60 */  -1,  -1,  -1,  -1,  -1,   0,   1,   2,   3,   4,
	/*  70 */   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,
	/*  80 */  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,
	/*  90 */  25,  -1,  -1,  -1,  -1,  -1,  -1,  26,  27,  28,
	/* 100 */  29,  30,  31,  32,  33,  34,  35,  36,  37,  38,
	/* 110 */  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,
	/* 120 */  49,  50,  51,  -1,  -1,  -1,  -1,  -1
};
PRIVATE	unsigned char rfc1642_tob64[64] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
PRIVATE	unsigned char rfc1642_shift[128] = {
/*			0		1		2		3		4		5		6		7	*/
/*			8		9		A		B		C		D		E		F	*/
/* 0x00 */	TRUE,	TRUE,	TRUE,	TRUE,	TRUE,	TRUE,	TRUE,	TRUE,
/* 0x08 */	TRUE,	FALSE,	FALSE,	TRUE,	TRUE,	FALSE,	TRUE,	TRUE,
/* 0x10 */	TRUE,	TRUE,	TRUE,	TRUE,	TRUE,	TRUE,	TRUE,	TRUE,
/* 0x18 */	TRUE,	TRUE,	TRUE,	TRUE,	TRUE,	TRUE,	TRUE,	TRUE,
/* 0x20 */	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,
/* 0x28 */	FALSE,	FALSE,	FALSE,	TRUE,	FALSE,	FALSE,	FALSE,	FALSE,
/* 0x30 */	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,
/* 0x38 */	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,
/* 0x40 */	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,
/* 0x48 */	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,
/* 0x50 */	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,
/* 0x58 */	FALSE,	FALSE,	FALSE,	FALSE,	TRUE,	FALSE,	FALSE,	FALSE,
/* 0x60 */	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,
/* 0x68 */	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,
/* 0x70 */	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,
/* 0x78 */	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	TRUE,	TRUE
};

PRIVATE utf7_encoding_method_data rfc1642_utf7 = {
	rfc1642_fromb64,
	rfc1642_tob64,
	rfc1642_shift,
	(unsigned char)'+',
	(unsigned char)'-'
};


/*
	tables for RFC2060- IMAP4rev1 Mail Box Name
*/
PRIVATE	int16 rfc2060_fromb64[128] = 
{
	/*   0 */  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
	/*  10 */  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
	/*  20 */  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
	/*  30 */  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
	/*  40 */  -1,  -1,  -1,  62,  63,  -1,  -1,  -1,  52,  53,
	/*  50 */  54,  55,  56,  57,  58,  59,  60,  61,  -1,  -1,
	/*  60 */  -1,  -1,  -1,  -1,  -1,   0,   1,   2,   3,   4,
	/*  70 */   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,
	/*  80 */  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,
	/*  90 */  25,  -1,  -1,  -1,  -1,  -1,  -1,  26,  27,  28,
	/* 100 */  29,  30,  31,  32,  33,  34,  35,  36,  37,  38,
	/* 110 */  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,
	/* 120 */  49,  50,  51,  -1,  -1,  -1,  -1,  -1
};
PRIVATE	unsigned char rfc2060_tob64[64] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+,";
PRIVATE	unsigned char rfc2060_shift[128] = {
/*			0		1		2		3		4		5		6		7	*/
/*			8		9		A		B		C		D		E		F	*/
/* 0x00 */	TRUE,	TRUE,	TRUE,	TRUE,	TRUE,	TRUE,	TRUE,	TRUE,
/* 0x08 */	TRUE,	FALSE,	FALSE,	TRUE,	TRUE,	FALSE,	TRUE,	TRUE,
/* 0x10 */	TRUE,	TRUE,	TRUE,	TRUE,	TRUE,	TRUE,	TRUE,	TRUE,
/* 0x18 */	TRUE,	TRUE,	TRUE,	TRUE,	TRUE,	TRUE,	TRUE,	TRUE,
/* 0x20 */	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	TRUE,	FALSE,
/* 0x28 */	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,
/* 0x30 */	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,
/* 0x38 */	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,
/* 0x40 */	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,
/* 0x48 */	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,
/* 0x50 */	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,
/* 0x58 */	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,
/* 0x60 */	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,
/* 0x68 */	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,
/* 0x70 */	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,
/* 0x78 */	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	FALSE,	TRUE
};

PRIVATE utf7_encoding_method_data rfc2060_utf7 = {
	rfc2060_fromb64,
	rfc2060_tob64,
	rfc2060_shift,
	(unsigned char)'&',
	(unsigned char)'-'
};

MODULE_PRIVATE UNICVTAPI unsigned char *
mz_utf72utf8(	CCCDataObject		obj,
				const unsigned char	*utf7buf,	/* UTF-7 buf for conv */
				int32				utf7bufsz)	/* UTF-7 buf size in bytes */
{
	return intl_utf72utf8(obj,utf7buf, utf7bufsz, &rfc1642_utf7);
}
MODULE_PRIVATE UNICVTAPI unsigned char *
mz_imap4utf72utf8(	CCCDataObject		obj,
				const unsigned char	*utf7buf,	/* UTF-7 buf for conv */
				int32				utf7bufsz)	/* UTF-7 buf size in bytes */
{
	return intl_utf72utf8(obj,utf7buf, utf7bufsz, &rfc2060_utf7);
}

PRIVATE unsigned char *
intl_utf72utf8(	CCCDataObject		obj,
				const unsigned char	*utf7buf,	/* UTF-7 buf for conv */
				int32				utf7bufsz,	/* UTF-7 buf size in bytes */
				utf7_encoding_method_data*	opt)

{

 	unsigned char	*tobuf = NULL;
 	int32			tobufsz;
 	unsigned char	*tobufp, *utf7p;		/* current byte in bufs	*/
 	unsigned char	*tobufendp, *utf7endp;	/* end of buffers		*/
 	int32					uncvtlen;
	
	uint16	oneUCS2char;
	unsigned char	onechar;
	int16			numoctets;
	int16 mustnotshift = 0;
	int16 inShiftSequence;

	uint32 buffer;
	uint32 buffertemp = 0;
	int16 bufferBitCount;
	unsigned char *uncvtbuf = INTL_GetCCCUncvtbuf(obj);

	/* set up table to convert ASCII values of base64 chars to
	 * their base 64 value. If there is no conversion, use -1 as sentinel.
	 */


	/* initialize data saved from previous stream */

	int32 flag = INTL_GetCCCCvtflag(obj);
	inShiftSequence = flag & 1;
	buffer = 0xFFFF0000 & flag;
	bufferBitCount = (uint16) ((0x0000FF00 & flag) >> 8);

#define utf8bufsz	tobufsz
#define utf8buf		tobuf
#define utf8p		tobufp
#define utf8endp	tobufendp
											  	/* Allocate a dest buffer:		*/


	/* UTF-7 characters that are directly encoded will be one octet UTF-8
	 * chars. Shifted chars will take 2.7 octets (plus shift in or out chars)
	 * to make 2 or 3 octet UTF-8 chars. So in the worst input, all the UTF-7
	 * data would convert to 3 octet UTF-8 data, and we would need 1/9th as
	 * many UTF-7 characters, plus 1 to round up, plus 1 for NULL termination.
	 */

	uncvtlen = strlen((char *)uncvtbuf);
	tobufsz = (int32) (1.2*(utf7bufsz + uncvtlen) + 2); 

	if ((tobuf = (unsigned char *)XP_ALLOC_PRIV(tobufsz)) == (unsigned char *)NULL) 
	{
		INTL_SetCCCRetval(obj, MK_OUT_OF_MEMORY);
		return(NULL);
	}
										/* Initialize pointers, etc.	*/
 	utf7p = (unsigned char *)utf7buf;
 	utf7endp = utf7p + utf7bufsz - 1;

#define uncvtp	tobufp		/* use tobufp as temp index for uncvtbuf */ 		 		
							/* If prev. unconverted chars, append unconverted
							 * chars w/new chars and try to process.
							 */

 	if (uncvtbuf[0] != '\0') 
	{
 		uncvtp = uncvtbuf + uncvtlen;
 		while (uncvtp < (uncvtbuf + UNCVTBUF_SIZE) &&
														utf7p <= utf7endp)
 			*uncvtp++ = *utf7p++; 


 		*uncvtp = '\0';						/* nul terminate as sentinel */
 		utf7p = uncvtbuf;				/* process unconverted first */
 		utf7endp = uncvtp - 1;
 	}
#undef uncvtp
 	
 	tobufp = tobuf;
 	tobufendp = tobufp + tobufsz - 2;		

WHILELOOP:

	while( (tobufp <= tobufendp) && (utf7p <= utf7endp) ) 
	{

	 
		onechar = *utf7p++;
		
		
		/* If I'm not in the shift sequence, and I have the start symbol,
		 * absorb it and loop again. Otherwise, if I have a legal character
		 * for a non-shifted sequence, (ASCII) write it directly. This is
		 * ok, because ASCII is just ASCII in UTF-8, so don't need to worry
		 * about UCS-2 conversion.
		 */

		if(!inShiftSequence) 
		{

			if(onechar == opt->startshift) 
			{
				if(*utf7p == opt->endshift) 
				{
					*tobufp++ = opt->startshift;
					utf7p++;
				} else inShiftSequence = TRUE;
				continue;
			}
				
			if(onechar <= MAX_ASCII) *tobufp++ = onechar;
			else continue;
			
		} 
		else 
		{	/* inShiftSequence is TRUE */

			/* onechar is not a base64 allowable char if it is non-ASCII or
			 * if it is a non-base64 char from the ASCII set. 
			 */
			mustnotshift = (onechar > MAX_ASCII || 
					(opt->fromb64[onechar] == NOT_BASE64));

			/* If I'm in the shift sequence, and get the opt->endshift character,
			 * I want to absorb it and turn off shifting. If I get another
			 * non-shiftable character, I want to write it and turn off shifting.
			 * If I get an illegal character, I discard it and keep looping. 
			 */

			if(mustnotshift) 
			{

				if(!(onechar == opt->endshift)) 
				{

					if(onechar > MAX_ASCII) 
						continue;
					
					*tobufp++ = onechar;
				}

				inShiftSequence = FALSE;
				buffer = 0;			/* flush buffer at end of shift sequence */
				bufferBitCount = 0;
				
			
			} 
			else 
			{ 

				buffertemp = opt->fromb64[onechar] & 0x0000003F;	/* grab 6-bit base64 char */
				buffer |= buffertemp << (26 - bufferBitCount); /* 26 is 32 - 6 bits */
				bufferBitCount += 6;

				/* Flush the buffer of a UCS-2 character (won't be more than one)  */
				
				if(bufferBitCount > 15) 
				{

					oneUCS2char = (int16) ((buffer & 0xFFFF0000) >> 16);
					numoctets = one_ucs2_to_utf8_char(tobufp, tobufendp, oneUCS2char);
					if(numoctets == -1) break; /* out of space in tobuf */
					tobufp += numoctets;
					bufferBitCount -= 16;
					buffer <<= 16;
				}

			}

		} /* end of inShiftSequence == TRUE */

	}	   /* end of conversion while loop */



	if(uncvtbuf[0] != '\0') 
	{										/* Just processed unconverted chars.
											 * ucsp points to 1st unprocessed char 
											 * in ucsbuf. Some may have been 
											 * processed while processing unconverted
											 * chars, so setup ptrs. not to process
											 * them twice.
											 */

											/* If nothing was converted, there wasn't
											 * enough UCS-2 data. Stop and get more
											 * data.
											 */

		if(utf7p == uncvtbuf) 
		{	/* nothing was converted */
			*tobufp = '\0';
			INTL_SetCCCLen(obj, 0);
			return(NULL);
		}

		/* set up to read ucsbuf */
		utf7endp = (unsigned char *) utf7buf + utf7bufsz - 1; 
		utf7p = 	(unsigned char *) utf7buf + (utf7p - uncvtbuf - uncvtlen);
		uncvtbuf[0] = '\0';		   /* No more unconverted chars.*/
		goto WHILELOOP;					   /* Process new data */
	}


	*tobufp = '\0';					/* NULL terminate dest. data */
	INTL_SetCCCLen(obj, tobufp - tobuf);		/* length of processed data, in bytes */

	/* If we're in a shift sequence, we need to save away our buffer
	 * and the buffer bit count (although if all that's left in the buffer
	 * is padding 0's, we don't need to worry about it and should reset
	 * the bitCount to 0.)
	 */
	
	INTL_SetCCCCvtflag(obj,((inShiftSequence ? 1 : 0 ) |
							(buffer & 0xFFFF0000) |
							((bufferBitCount << 8) & 0x0000FF00)));
	
	/* Now check for unconverted data from utf7p */
	if(utf7p <= utf7endp) 
	{		
		int l = utf7endp - utf7p + 1;
		memcpy(uncvtbuf, utf7p, l);
		uncvtbuf[l] = '\0';
	}

#undef utf8bufsz
#undef utf8buf		
#undef utf8p		
#undef utf8endp	
 
	return(tobuf);

}


/* UTF-8 to UTF-7 */


 /*
  * mz_utf82utf7
  * ------------
  *
  * This function takes a CCCDataObject, a buffer of UTF-8 data, and the
  * size of that buffer. It allocates and returns a buffer of the
  * corresponding UTF-7 data (returning the size as a field in the
  * CCCDataObject). The caller is responsible for freeing the returned
  * data. If there are extra data at the end of the UTF-8 buffer which 
  * cannot be translated into UTF-7 (ie, an incomplete character), it
  * will be saved in the uncvtbuf of the CCCDataObject and used on the
  * next call. 
  *
  * UTF-7 is a variant of base-64, and like base-64, it accumulates
  * bits in a bit buffer, transforming them to UTF-7 chars when it
  * has multiples of 6 bits. If the UTF-8 data being translated does
  * not happen to terminate with a multiple of 6 bits, the final 
  * char will be padded with 0's, and the shift sequence terminated.
  * For this reason, we will *never* be inside a shift sequence in
  * between chunks of data. This may mean that the final stream of
  * data has sequences that look like +[some UTF-7 data]-+[more data]-,
  * with a plus immediately following a -. Although unconventional,
  * this is in fact legal UTF-7. 
  *
  * Finally, there are two formats of UTF-7, one extremely conservative
  * fashion which shifts every character which could possibly be
  * considered unsafe, and another which is somewhat more lax. Which
  * of these is used is determined by obj->cvtflag. By default (cvtflag == 0)
  * we employ the safer form of conversion. The differing characters
  * are: !\"#$%&*;<=>@[]^_`{|}
  */ 
/* Tables */   


MODULE_PRIVATE UNICVTAPI unsigned char *
mz_utf82utf7(	CCCDataObject		obj,
				const unsigned char	*utf8buf,	/* UTF-8 buf for conv */
				int32				utf8bufsz)	/* UTF-8 buf size in bytes */
{
	return intl_utf82utf7(obj,utf8buf, utf8bufsz, &rfc1642_utf7);
}
MODULE_PRIVATE UNICVTAPI unsigned char *
mz_utf82imap4utf7(	CCCDataObject		obj,
				const unsigned char	*utf8buf,	/* UTF-8 buf for conv */
				int32				utf8bufsz)	/* UTF-8 buf size in bytes */
{
	return intl_utf82utf7(obj,utf8buf, utf8bufsz, &rfc2060_utf7);
}
PRIVATE unsigned char *
intl_utf82utf7(	CCCDataObject		obj,
				const unsigned char	*utf8buf,	/* UTF-8 buf for conv */
				int32				utf8bufsz,	/* UTF-8 buf size in bytes */
				utf7_encoding_method_data*	opt)
{																					


 	unsigned char	*tobuf = NULL;
	int32			tobufsz;
	unsigned char	*tobufp, *utf8p;		/* current byte in bufs	*/
 	unsigned char	*tobufendp, *utf8endp;	/* end of buffers		*/
 	int32					uncvtlen;
	unsigned char *uncvtbuf = INTL_GetCCCUncvtbuf(obj);


	uint16 onechar;
	int16 numoctets;
	int16 inShiftSequence = FALSE;
	int16 needToShift = FALSE;
	uint32 buffer = 0;
	uint32 buffertemp = 0;
	int16 bufferBitCount = 0;
	unsigned char oneBase64char;



#define utf7bufsz	tobufsz
#define utf7buf		tobuf
#define utf7p		tobufp
#define utf7endp	tobufendp

	
	/* Allocate a dest buffer:		*/

	uncvtlen = strlen((char *)uncvtbuf);
	tobufsz = 3*(utf8bufsz + uncvtlen) +1;  
	if (!tobufsz) {
		return NULL;
	}

	if ((tobuf = (unsigned char *)XP_ALLOC_PRIV(tobufsz)) == (unsigned char *)NULL) {
		INTL_SetCCCRetval(obj, MK_OUT_OF_MEMORY);
		return(NULL);
	}
										/* Initialize pointers, etc.	*/
 	utf8p = (unsigned char *)utf8buf;
 	utf8endp = utf8p + utf8bufsz - 1; /* leave room for NULL termination (as sentinel?)*/

#define uncvtp	tobufp		/* use tobufp as temp index for uncvtbuf */ 		 		
							/* If prev. unconverted chars, append unconverted
							 * chars w/new chars and try to process.
							 */

 	if (uncvtbuf[0] != '\0') {
 		uncvtp = uncvtbuf + uncvtlen;
		/* This is not leaving space for a NULL !!!!!!!!!!!! */
 		while (uncvtp < (uncvtbuf + UNCVTBUF_SIZE) &&
														utf8p <= utf8endp)
			 *uncvtp++ = *utf8p++;

 		*uncvtp = '\0';						/* nul terminate as sentinel */
 		utf8p = uncvtbuf;				/* process unconverted first */
 		utf8endp = uncvtp - 1;
 	}
#undef uncvtp
 

 	tobufp = tobuf;
 	tobufendp = tobufp + tobufsz - 2;		/* save space for terminating null*/

		


 WHILELOOP:

	while( (tobufp <= tobufendp) && (utf8p <= utf8endp) ) {

		/* convert one char's worth of utf8 to ucs2 */
		numoctets = one_utf8_to_ucs2_char(utf8p, utf8endp, &onechar);
		if(numoctets == -1) break; /* out of input*/
		utf8p += numoctets;

		/* we need to be shifted if the character is non-ASCII or 
		 * is an ASCII character that should be shifted.
		 */
		needToShift = (onechar > MAX_ASCII) || (opt->shift[onechar]);


		if(!needToShift && inShiftSequence)	  {

			if(bufferBitCount > 0) {
				if((tobufp+2) > tobufendp) break;
				bufferBitCount = pad_and_write(buffer, tobufp, bufferBitCount, opt);
				if (!bufferBitCount) {	/* buffer successfully flushed */
					tobufp+=2;
					buffer = 0;
				}
			
			} else {
				if((tobufp+1) > tobufendp) break;
				*tobufp++ = opt->endshift;
			}
			inShiftSequence = FALSE; /* now just fallthrough to next case*/
		}

		if(!needToShift && 	!inShiftSequence) {
			if((tobufp+1) > tobufendp) break;
			*tobufp++ = (char) onechar;
		} 

		if(needToShift && !inShiftSequence)	 {
			*tobufp++ = opt->startshift;
			if(onechar == opt->startshift) { /* special-case behavior if onechar is a + */
				if((tobufp+1) > tobufendp) break;
				*tobufp++ = opt->endshift;
			}
			else inShiftSequence = TRUE; 
		}

		if(needToShift && inShiftSequence) {
			
			buffertemp = onechar & 0x0000FFFF;
			buffer |= buffertemp << (16 - bufferBitCount);	
											/* ^--16 is the size of the int32 minus
											 * the size of onechar */
			bufferBitCount += 16;


			/* Flush the buffer of as many base64 characters as we can form */
			while(bufferBitCount>5) {
	 			  if(tobufp > tobufendp) break;
				  oneBase64char = (char)  ((buffer & 0xFC000000) >> 26);
				  *tobufp++ =  opt->tob64[oneBase64char];
				  buffer <<= 6;
				  bufferBitCount -= 6;
			}
		}


	} /* end of while loop */



	if(uncvtbuf[0] != '\0') {			/* Just processed unconverted chars.
											 * ucsp points to 1st unprocessed char 
											 * in ucsbuf. Some may have been 
											 * processed while processing unconverted
											 * chars, so setup ptrs. not to process
											 * them twice.
											 */

											/* If nothing was converted, there wasn't
											 * enough UTF-8 data. Stop and get more
											 * data.
											 */

		if(utf8p == uncvtbuf) {		/* nothing was converted */
			*tobufp = '\0';
			return(NULL);
		}
		utf8endp = (unsigned char *) utf8buf + utf8bufsz - 1; 
		utf8p = 	(unsigned char *) utf8buf + (utf8p - uncvtbuf - uncvtlen);
		uncvtbuf[0] = '\0';		   /* No more unconverted chars.*/
		goto WHILELOOP;					   /* Process new data */
	}


	/* Anything left in the buffer at this point should be padded with 0's
	 * and appended to tobuf. */

	if(inShiftSequence) {

		if(bufferBitCount > 0) {

			if((tobufp+2) <= tobufendp) {
				bufferBitCount = pad_and_write(buffer, tobufp, bufferBitCount,  opt);
				if (!bufferBitCount) { /* buffer successfully flushed */
					tobufp+=2;
					buffer = 0;
				}
			}

		}  else {
			 if((tobufp+1) <= tobufendp) *tobufp++ = opt->endshift;
		}

		inShiftSequence = FALSE;
	}


	 *tobufp = '\0';				/* NULL terminate dest. data */


	INTL_SetCCCLen(obj, tobufp - tobuf);		/* length of processed data, in bytes */

	if(utf8p <= utf8endp) {			/*  unconverted utf8 left? */
		tobufp = uncvtbuf;		/* just using tobufp as a temp index. */
		while (utf8p <= utf8endp)
				*tobufp++ = *utf8p++;
		*tobufp = '\0';				/* NULL terminate, as a sentinel if nothing else.*/
	}


#undef utf7bufsz	 
#undef utf7buf		 
#undef utf7p		 
#undef utf7endp	 
							

	return(tobuf);
}


/* Function: one_ucs2_to_utf8_char
 * 
 * Function takes one UCS-2 char and writes it to a UTF-8 buffer.
 * We need a UTF-8 buffer because we don't know before this 
 * function how many bytes of utf-8 data will be written. It also
 * takes a pointer to the end of the UTF-8 buffer so that we don't
 * overwrite data. This function returns the number of UTF-8 bytes
 * of data written, or -1 if the buffer would have been overrun.
 */

#define LINE_SEPARATOR		0x2028
#define PARAGRAPH_SEPARATOR	0x2029
PRIVATE int16 one_ucs2_to_utf8_char(unsigned char *tobufp, 
		unsigned char *tobufendp, uint16 onechar)

{

	 int16 numUTF8bytes = 0;

	if((onechar == LINE_SEPARATOR)||(onechar == PARAGRAPH_SEPARATOR))
	{
		strcpy((char*)tobufp, "\n");
		return strlen((char*)tobufp);;
	}

	 	if (onechar < 0x80) {				numUTF8bytes = 1;
		} else if (onechar < 0x800) {		numUTF8bytes = 2;
		} else if (onechar <= MAX_UCS2) {	numUTF8bytes = 3;
		} else { numUTF8bytes = 2;
				 onechar = DEFAULT_CHAR;
		} 
                
		tobufp += numUTF8bytes;

		/* return error if we don't have space for the whole character */
		if (tobufp > tobufendp) {
			return(-1); 
		}


		switch(numUTF8bytes) {

			case 3: *--tobufp = (onechar | BYTE_MARK) & BYTE_MASK; onechar >>=6;
					*--tobufp = (onechar | BYTE_MARK) & BYTE_MASK; onechar >>=6;
					*--tobufp = onechar |  THREE_OCTET_BASE;
					break;

			case 2: *--tobufp = (onechar | BYTE_MARK) & BYTE_MASK; onechar >>=6;
					*--tobufp = onechar | TWO_OCTET_BASE;
					break;
			case 1: *--tobufp = (unsigned char)onechar;  break;
		}

		return(numUTF8bytes);
}


/*
 * utf8_to_ucs2_char
 *
 * Convert a utf8 multibyte character to ucs2
 *
 * inputs: pointer to utf8 character(s)
 *         length of utf8 buffer ("read" length limit)
 *         pointer to return ucs2 character
 *
 * outputs: number of bytes in the utf8 character
 *          -1 if not a valid utf8 character sequence
 *          -2 if the buffer is too short
 */
MODULE_PRIVATE UNICVTAPI int16
utf8_to_ucs2_char(const unsigned char *utf8p, int16 buflen, uint16 *ucs2p)
{
	uint16 lead, cont1, cont2;

	/*
	 * Check for minimum buffer length
	 */
	if ((buflen < 1) || (utf8p == NULL)) {
		return -2;
	}
	lead = (uint16) (*utf8p);

	/*
	 * Check for a one octet sequence
	 */
	if (IS_UTF8_1ST_OF_1(lead)) {
		*ucs2p = lead & ONE_OCTET_MASK;
		return 1;
	}

	/*
	 * Check for a two octet sequence
	 */
	if (IS_UTF8_1ST_OF_2(*utf8p)) {
		if (buflen < 2)
			return -2;
		cont1 = (uint16) *(utf8p+1);
		if (!IS_UTF8_2ND_THRU_6TH(cont1))
			return -1;
		*ucs2p =  (lead & TWO_OCTET_MASK) << 6;
		*ucs2p |= cont1 & CONTINUING_OCTET_MASK;
		return 2;
	}

	/*
	 * Check for a three octet sequence
	 */
	else if (IS_UTF8_1ST_OF_3(lead)) {
		if (buflen < 3)
			return -2;
		cont1 = (uint16) *(utf8p+1);
		cont2 = (uint16) *(utf8p+2);
		if (   (!IS_UTF8_2ND_THRU_6TH(cont1))
			|| (!IS_UTF8_2ND_THRU_6TH(cont2)))
			return -1;
		*ucs2p =  (lead & THREE_OCTET_MASK) << 12;
		*ucs2p |= (cont1 & CONTINUING_OCTET_MASK) << 6;
		*ucs2p |= cont2 & CONTINUING_OCTET_MASK;
		return 3;
	}
	else { /* not a valid utf8/ucs2 character */
		return -1;
	}
}

UNICVTAPI int32
INTL_NumUTF8Chars(const unsigned char *utf8p)
{
	int num_chars = 0;

	while (*utf8p) {
		/*
		 * Check for a one octet sequence
		 */
		if (IS_UTF8_1ST_OF_1(*utf8p)) {
			num_chars += 1;
			utf8p += 1;
			continue;
		}

		/*
		 * Check for a two octet sequence
		 */
		else if (IS_UTF8_1ST_OF_2(*utf8p) 
			&& IS_UTF8_2ND_THRU_6TH(*(utf8p+1))) {
			num_chars += 2;
			utf8p += 2;
			continue;
		}

		/*
		 * Check for a three octet sequence
		 */
		else if (IS_UTF8_1ST_OF_3(*utf8p) 
			&& IS_UTF8_2ND_THRU_6TH(*(utf8p+1))
			&& IS_UTF8_2ND_THRU_6TH(*(utf8p+2))) {
			num_chars += 3;
			utf8p += 3;
			continue;
		}

		/*
		 * Not UTF8 : just muddle forward
		 */
		else {
			num_chars += 1;
			utf8p += 1;
		}

	}

	return num_chars;
}

PUBLIC UNICVTAPI uint16 *
INTL_UTF8ToUCS2(const unsigned char *utf8p, int32 *num_chars)
{
	uint16 *ucs2_chars;
	int32 num_utf8_chars, ucs2_len, num_ucs2_chars;
	int parse_cnt, inval_cnt;

	/*
	 * Figure the number of chars
	 */
	num_utf8_chars = INTL_NumUTF8Chars(utf8p);
	ucs2_len = num_utf8_chars*2;
	ucs2_chars = (uint16 *)XP_ALLOC_PRIV(ucs2_len + 2);
	if (!ucs2_chars) return NULL;
	/*

	 * Do the conversion
	 */
	num_ucs2_chars = utf8_to_ucs2_buffer(utf8p, strlen((char*)utf8p), 
								&parse_cnt, &inval_cnt, ucs2_chars,  ucs2_len);
	ucs2_chars[num_ucs2_chars] = 0; /* null terminator */

	/*
	 * return the result
	 */
	if (num_ucs2_chars > 0)
		*num_chars = num_ucs2_chars;
	else
		*num_chars = 0;
	return ucs2_chars;
}

PUBLIC UNICVTAPI unsigned char *
INTL_UCS2ToUTF8(const uint16 *ucs2p, int32 num_chars)
{
	unsigned char *utf8_chars;
	int32 num_utf8_bytes, num_bytes_written, dummy;
	int i;

	/*
	 * Figure the number of bytes for the utf8 string
	 */
	num_utf8_bytes =0;
	for (i=0; i<num_chars; i++) {
		if (ucs2p[i] <= 0x7F) /* 0-0x7f only need one byte */
			num_utf8_bytes += 1;
		else if (ucs2p[i] <= 0x3FF) /* 0x80-0x3ff only need two bytes */
			num_utf8_bytes += 2;
		else /* 0x400-0xffff need three bytes */
			num_utf8_bytes += 3;
	}
	utf8_chars = (unsigned char *)XP_ALLOC_PRIV(num_utf8_bytes + 1);
	if (!utf8_chars) return NULL;
	XP_MEMSET(utf8_chars, 0, num_utf8_bytes + 1);

	/*
	 * Do the conversion
	 */
	num_bytes_written = ucs2_to_utf8_buffer(ucs2p, num_chars, utf8_chars,  
											num_utf8_bytes, &dummy);
	/*
	 * return the result
	 */
	return utf8_chars;
}

/*
 * ucs2_to_utf8_buffer
 *
 * Convert a ucs2 buffer to a utf8 multibyte character string
 *
 * inputs: 
 *         pointer to return ucs2 buffer
 *         length of ucs2 buffer ("read" length limit)
 *         pointer to utf8 character(s)
 *         length of utf8 buffer ("write" length limit)
 *
 * outputs: returns number of charecters "read" from the ucs2 string
 *          sets *num_bytes_written to # of utf8 characters "written"
 */
int32
ucs2_to_utf8_buffer(const uint16 *ucs2p, int32 num_chars, 
		unsigned char *utf8p, int32 num_utf8_bytes, int32 *utf8_bytes_written)
{
	int i;

	/*
	 * Init values
	 */
	*utf8_bytes_written = 0;


	/*
	 * Convert the data
	 */
	for (i=0; i<num_chars; i++) {
		if (ucs2p[i] <= 0x7F) { /* 0-0x7f only need one byte */
			if (num_utf8_bytes < 1)
				break;
			utf8p[*utf8_bytes_written] = (unsigned char)ucs2p[i];
			num_utf8_bytes -= 1;
			*utf8_bytes_written += 1;
		}
		else if (ucs2p[i] <= 0x3FF) { /* 0x80-0x3ff only need two bytes */
			if (num_utf8_bytes < 2)
				break;
			utf8p[*utf8_bytes_written+0] = (unsigned char)
					(TWO_OCTET_BASE | ((ucs2p[i]>>6)&TWO_OCTET_MASK));
			utf8p[*utf8_bytes_written+1] = (unsigned char)
					(CONTINUING_OCTET_BASE | (ucs2p[i]&CONTINUING_OCTET_MASK));
			num_utf8_bytes -= 2;
			*utf8_bytes_written += 2;
		}
		else { /* 0x400-0xffff need three bytes */
			if (num_utf8_bytes < 3)
				break;
			utf8p[*utf8_bytes_written+0] = (unsigned char)
					(THREE_OCTET_BASE | ((ucs2p[i]>>12)&THREE_OCTET_MASK));
			utf8p[*utf8_bytes_written+1] = (unsigned char)
				(CONTINUING_OCTET_BASE | ((ucs2p[i]>>6)&CONTINUING_OCTET_MASK));
			utf8p[*utf8_bytes_written+2] = (unsigned char)
					(CONTINUING_OCTET_BASE | (ucs2p[i]&CONTINUING_OCTET_MASK));
			num_utf8_bytes -= 3;
			*utf8_bytes_written += 3;
		}
	}

	return i;
}

/*
 * utf8_to_ucs2_buffer
 *
 * Convert a utf8 multibyte character string and place in a ucs2 buffer
 *
 * inputs: pointer to utf8 character(s)
 *         length of utf8 buffer ("read" length limit)
 *         pointer to return ucs2 buffer
 *         length of ucs2 buffer ("write" length limit)
 *         pointer to return count of invalid bytes
 *
 * outputs: returns number of bytes "read" from the utf8 string
 *          sets *invalid_cnt to # of invalid utf8 characters "read"
 */
UNICVTAPI int32
utf8_to_ucs2_buffer(const unsigned char *utf8p, int16 utf8len, 
						int *parsed_cnt, int *invalid_cnt,
						uint16 *ucs2p, int32 ucs2len)
{
	int read_len, write_len;
	int char_len;

	/*
	 * Init the return values
	 */
	*parsed_cnt = 0;
	*invalid_cnt = 0;

	/*
	 * Check for minimum buffer lengths
	 */
	if ((utf8len < 1) || (utf8p == NULL)
		|| (ucs2len < 1) || (ucs2p == NULL)) {
		return 0;
	}

	/*
	 * Do the conversion
	 */
	for (read_len=0,write_len=0;
					(read_len<utf8len) && (write_len<ucs2len);
								read_len +=char_len)
		{
		char_len = utf8_to_ucs2_char(utf8p+read_len, utf8len-read_len, 
														(uint16*)ucs2p+write_len);
		if (char_len == -1) { /* invalid character */
			*invalid_cnt += 1;
			char_len = 1; /* try to resynchronize */
			*(ucs2p+write_len) = *(utf8p+read_len);
		}
		else if (char_len == -2) { /* buffer too short for last char */
			/* return with what we have so far */
			break;
		}
		/*
		 * Note we converted one
		 */
		*parsed_cnt += char_len;
		write_len += 1;
	}
	return write_len;
}

/* Function:  one_utf8_to_ucs2_char
 * 
 * Converts one UTF8 char to one UCS2 char. Needs to get UTF-8 from a
 * buffer of utf8 data, because we don't know how many octets it will 
 * be, not before this function is called. Take a pointer to the end of that
 * buffer to make sure we don't run past it. Put the resulting UCS-2
 * char into an int16 we're given a pointer to. Returns the number of
 * octets used in the utf-8 char we converted, and returns -1 if it
 * runs out of utf-8 data without a complete UCS-2 character.
 */
PRIVATE int16 one_utf8_to_ucs2_char(const unsigned char *utf8p, const unsigned char *utf8endp, 
							   uint16 *onecharp)
{

	int16 i, numoctets;
	uint32	ucs4 = 0;
	*onecharp = 0;

	if(*utf8p >= THREE_OCTET_BASE) numoctets = 3;
	else if (*utf8p >= TWO_OCTET_BASE) numoctets = 2;
	else numoctets = 1;
		
	/* See if all the data for the char is there */
	if ((utf8p + numoctets - 1) > utf8endp) {	
		return (-1);
	}


	for(i=numoctets; i>0; i--) {
		ucs4 += *utf8p++;
		if (i == 1) break;
		ucs4 <<= 6;
	}

	switch(numoctets) {

		case 3: ucs4 -= 0x000E2080UL; break;  /* truncating... */
		case 2: ucs4 -= 0x00003080UL; break;
	}
	*onecharp= (uint16)(ucs4 & 0x0000FFFFUL);
	return(numoctets);
}


/* 
 * Internal Function: pad_and_write
 * Checks to make sure there is less than one full base64 character in the
 * buffer, pad it with 0 to make up a full base64 character, write that
 * to tobuf, and write the shift termination character. (-)
 */

PRIVATE uint16  pad_and_write(uint32 buffer, unsigned char *tobufp, 
							int16 bufferBitCount, utf7_encoding_method_data*	opt)


{
	int16 oneBase64char;
	
	if(bufferBitCount >= 6) return(bufferBitCount);
	oneBase64char = ((unsigned char) (buffer >> 26));
	*tobufp++ = 	opt->tob64[oneBase64char];
	*tobufp = opt->endshift;
	return(0);
}


/* Function: swap_ucs2_bytes
 * 
 * Takes a buffer of ucs2 chars, and its size in *bytes*.
 *
 * This function is meant to cope with the problem that sometimes
 * UCS-2 data (because of the big-endian, little-endian problem?)
 * comes in in reversed order, and needs to be swapped to be
 * dealt with appropriately. 
 * 
 * This case can be detected at the very beginning of the stream,
 * because the first two bytes of any UCS-2 stream should be the
 * Byte Order Mark, or 0xFEFF. If instead you see 0xFFFE, you know
 * you need to swap. Neither of these are legal UCS-2 characters
 * otherwise, so you know that there is no danger of accidentally
 * triggering swapping with a legitimate UCS-2 stream.
 * Unfortunately, this marker is only present at the very beginning
 * of a stream; future chunks of the stream won't have the marker.
 * So if we ever detect that a stream needs to be swapped, we 
 * save that information by turning on the obj->cvtflag. If, on
 * future chunks, we see that that flag is turned on, we'll go
 * ahead and swap. 
 * Notice that if swapping is unnecessary, this function has 
 * no effect whatsoever.
 */	 
PRIVATE void	swap_ucs2_bytes(unsigned char *ucsbuf, int32 ucsbufsz)
{

	int32 i;
	unsigned char swapTemp = 0;

  		if(ucsbufsz%2) ucsbufsz--;

		for(i=0; i<ucsbufsz; i+=2) { 

			  swapTemp = ucsbuf[i];
			  ucsbuf[i] = ucsbuf[i+1];
			  ucsbuf[i+1] = swapTemp;
			 
		} 
	return; 
}







/* UCS-2 to UTF-7 jliu */


 /*
  * mz_ucs2utf7
  * ------------
  *
  * This function takes a CCCDataObject, a buffer of UCS-2 data, and the
  * size of that buffer. It allocates and returns a buffer of the
  * corresponding UTF-7 data (returning the size as a field in the
  * CCCDataObject). The caller is responsible for freeing the returned
  * data. If there are extra data at the end of the UTF-8 buffer which 
  * cannot be translated into UTF-7 (ie, an incomplete character), it
  * will be saved in the uncvtbuf of the CCCDataObject and used on the
  * next call. 
  *
  * UTF-7 is a variant of base-64, and like base-64, it accumulates
  * bits in a bit buffer, transforming them to UTF-7 chars when it
  * has multiples of 6 bits. If the UTF-8 data being translated does
  * not happen to terminate with a multiple of 6 bits, the final 
  * char will be padded with 0's, and the shift sequence terminated.
  * For this reason, we will *never* be inside a shift sequence in
  * between chunks of data. This may mean that the final stream of
  * data has sequences that look like +[some UTF-7 data]-+[more data]-,
  * with a plus immediately following a -. Although unconventional,
  * this is in fact legal UTF-7. 
  *
  * Finally, there are two formats of UTF-7, one extremely conservative
  * fashion which shifts every character which could possibly be
  * considered unsafe, and another which is somewhat more lax. Which
  * of these is used is determined by obj->cvtflag. By default (cvtflag == 0)
  * we employ the safer form of conversion. The differing characters
  * are: !\"#$%&*;<=>@[]^_`{|}
  */ 
/* Tables */   


MODULE_PRIVATE UNICVTAPI unsigned char *
mz_ucs2utf7(	CCCDataObject		obj,
				const unsigned char	*ucs2buf,	/* UTF-8 buf for conv */
				int32				ucs2bufsz)	/* UTF-8 buf size in bytes */
{
	utf7_encoding_method_data* opt = &rfc1642_utf7;
 	unsigned char	*tobuf = NULL;
	int32			tobufsz;
	unsigned char	*tobufp, *ucs2p;		/* current byte in bufs	*/
 	unsigned char	*tobufendp, *ucs2endp;	/* end of buffers		*/
 	int32					uncvtlen = 0;
	unsigned char *uncvtbuf = INTL_GetCCCUncvtbuf(obj);


	uint16 onechar;
	int16 inShiftSequence = FALSE;
	int16 needToShift = FALSE;
	uint32 buffer = 0;
	uint32 buffertemp = 0;
	int16 bufferBitCount = 0;
	unsigned char oneBase64char;
	XP_Bool needToSwap = FALSE;


	if( INTL_GetCCCFromCSID( obj ) == CS_UCS2_SWAP )
		needToSwap = TRUE;

	
	/* Allocate a dest buffer:
	** in the worst case, every Unicode character will cost 2+4 = 6 octetes
	*/

	uncvtlen = uncvtbuf[0];
	tobufsz = 6*( (ucs2bufsz + uncvtlen)/2 + 1 ) + 1;
	if (!tobufsz) {
		return NULL;
	}

	if ((tobuf = (unsigned char *)XP_ALLOC_PRIV(tobufsz)) == (unsigned char *)NULL) {
		INTL_SetCCCRetval(obj, MK_OUT_OF_MEMORY);
		return(NULL);
	}
										/* Initialize pointers, etc.	*/
 	ucs2p = (unsigned char *)ucs2buf;
 	ucs2endp = ucs2p + ucs2bufsz - 1; /* leave room for NULL termination (as sentinel?)*/

 	tobufp = tobuf;
 	tobufendp = tobufp + tobufsz - 2;		/* save space for terminating null*/

		
	while( (tobufp <= tobufendp) && (ucs2p < ucs2endp) ) {

		if( uncvtbuf[0] != 0 ){
			onechar = uncvtbuf[1];
			uncvtbuf[0] = 0;
		} else
			onechar = *ucs2p++;
		onechar <<= 8;
		onechar |= *ucs2p++;

		/* do the swap stuff */

		if( onechar == NEEDS_SWAP_MARK ){
			INTL_SetCCCFromCSID( obj, CS_UCS2_SWAP );
			needToSwap = TRUE;
			continue;
		} else if( onechar == BYTE_ORDER_MARK ){
			INTL_SetCCCFromCSID( obj, CS_UCS2 );
			needToSwap = FALSE;
			continue;
		}

		if( needToSwap ){
			onechar = ( onechar << 8 ) | ( onechar >> 8 );
		}

		/* we need to be shifted if the character is non-ASCII or 
		 * is an ASCII character that should be shifted.
		 */
		needToShift = (onechar > MAX_ASCII) || (opt->shift[onechar]);


		if(!needToShift && inShiftSequence)	  {

			if(bufferBitCount > 0) {
				if((tobufp+2) > tobufendp) break;
				bufferBitCount = pad_and_write(buffer, tobufp, bufferBitCount, opt);
				if (!bufferBitCount) {	/* buffer successfully flushed */
					tobufp+=2;
					buffer = 0;
				}
			
			} else {
				if((tobufp+1) > tobufendp) break;
				*tobufp++ = opt->endshift;
			}
			inShiftSequence = FALSE; /* now just fallthrough to next case*/
		}

		if(!needToShift && 	!inShiftSequence) {
			if((tobufp+1) > tobufendp) break;
			*tobufp++ = (char) onechar;
		} 

		if(needToShift && !inShiftSequence)	 {
			*tobufp++ = opt->startshift;
			if(onechar == opt->startshift) { /* special-case behavior if onechar is a + */
				if((tobufp+1) > tobufendp) break;
				*tobufp++ = opt->endshift;
			}
			else inShiftSequence = TRUE; 
		}

		if(needToShift && inShiftSequence) {
			
			buffertemp = onechar & 0x0000FFFF;
			buffer |= buffertemp << (16 - bufferBitCount);	
											/* ^--16 is the size of the int32 minus
											 * the size of onechar */
			bufferBitCount += 16;


			/* Flush the buffer of as many base64 characters as we can form */
			while(bufferBitCount>5) {
	 			  if(tobufp > tobufendp) break;
				  oneBase64char = (char)  ((buffer & 0xFC000000) >> 26);
				  *tobufp++ =  opt->tob64[oneBase64char];
				  buffer <<= 6;
				  bufferBitCount -= 6;
			}
		}


	} /* end of while loop */



	/* Anything left in the buffer at this point should be padded with 0's
	 * and appended to tobuf. */

	if(inShiftSequence) {

		if(bufferBitCount > 0) {

			if((tobufp+2) <= tobufendp) {
				bufferBitCount = pad_and_write(buffer, tobufp, bufferBitCount,  opt);
				if (!bufferBitCount) { /* buffer successfully flushed */
					tobufp+=2;
					buffer = 0;
				}
			}

		}  else {
			 if((tobufp+1) <= tobufendp) *tobufp++ = opt->endshift;
		}

		inShiftSequence = FALSE;
	}


	*tobufp = '\0';				/* NULL terminate dest. data */


	INTL_SetCCCLen(obj, tobufp - tobuf);		/* length of processed data, in bytes */

	if(ucs2p <= ucs2endp) {			/*  unconverted ucs2 left? */
		uncvtbuf[0] = 1;
		uncvtbuf[1] = *ucs2endp;
	} else
		uncvtbuf[0] = 0;


	return(tobuf);
}

