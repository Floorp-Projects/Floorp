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
/*	cvchcode.c	*/

#include "intlpriv.h"
#include "xp.h"
#include "libi18n.h"


extern cscvt_t cscvt_tbl[];

struct RealCCCDataObject {
	struct INTL_CCCFuncs *funcs_pointer;
	CCCRADFunc           report_autodetect;
	void                 *autodetect_closure;
    CCCFunc              cvtfunc;
    int32                jismode;
    int32                cvtflag;                /* cvt func dependent flag   */
    unsigned char        uncvtbuf[UNCVTBUF_SIZE];
    uint16               default_doc_csid;
    int16                from_csid;
    int16                to_csid;
    int                  retval;                 /* error value for return    */
    int32                len;                    /* byte len of converted buf */
};


/* 
 * report_autodetect
 */
PRIVATE void
CallCCCReportAutoDetect(CCCDataObject obj, uint16 detected_doc_csid)
{
	struct RealCCCDataObject *c = (struct RealCCCDataObject *)obj;
	if (c->report_autodetect)
		(c->report_autodetect)(c->autodetect_closure, obj, detected_doc_csid);
}

PRIVATE void
SetCCCReportAutoDetect(CCCDataObject obj,
						    CCCRADFunc report_autodetect,
							void *autodetect_closure)
{
	struct RealCCCDataObject *c = (struct RealCCCDataObject *)obj;

	c->report_autodetect = report_autodetect;
	c->autodetect_closure = autodetect_closure;
}

/* 
 * cvtfunc
 */
PRIVATE CCCFunc
GetCCCCvtfunc(CCCDataObject obj)
{
	struct RealCCCDataObject *c = (struct RealCCCDataObject *)obj;

	return c->cvtfunc;
}

PRIVATE void
SetCCCCvtfunc(CCCDataObject obj, CCCFunc cvtfunc)
{
	struct RealCCCDataObject *c = (struct RealCCCDataObject *)obj;

	c->cvtfunc = cvtfunc;
}


/* 
 * jismode
 */
PRIVATE int32
GetCCCJismode(CCCDataObject obj)
{
	struct RealCCCDataObject *c = (struct RealCCCDataObject *)obj;

	return c->jismode;
}

PRIVATE void
SetCCCJismode(CCCDataObject obj, int32 jismode)
{
	struct RealCCCDataObject *c = (struct RealCCCDataObject *)obj;

	c->jismode = jismode;
}

/* 
 * cvtflag
 */

PRIVATE int32
GetCCCCvtflag(CCCDataObject obj)
{
	struct RealCCCDataObject *c = (struct RealCCCDataObject *)obj;

	return c->cvtflag;
}

PRIVATE void
SetCCCCvtflag(CCCDataObject obj, int32 cvtflag)
{
	struct RealCCCDataObject *c = (struct RealCCCDataObject *)obj;

	c->cvtflag = cvtflag;
}

/* 
 * uncvtbuf
 */
PRIVATE unsigned char*
GetCCCUncvtbuf(CCCDataObject obj)
{
	struct RealCCCDataObject *c = (struct RealCCCDataObject *)obj;

	return c->uncvtbuf;
}

/* 
 * len
 */
PRIVATE int32
GetCCCLen(CCCDataObject obj)
{
	struct RealCCCDataObject *c = (struct RealCCCDataObject *)obj;

	return c->len;
}

PRIVATE void
SetCCCLen(CCCDataObject obj, int32 len)
{
	struct RealCCCDataObject *c = (struct RealCCCDataObject *)obj;

	c->len = len;
}

/* 
 * retval
 */
PRIVATE int
GetCCCRetval(CCCDataObject obj)
{
	struct RealCCCDataObject *c = (struct RealCCCDataObject *)obj;

	return c->retval;
}

PRIVATE void
SetCCCRetval(CCCDataObject obj, int retval)
{
	struct RealCCCDataObject *c = (struct RealCCCDataObject *)obj;

	c->retval = retval;
}

/* 
 * default_doc_csid
 */
PRIVATE void
SetCCCDefaultCSID(CCCDataObject obj, uint16 default_doc_csid)
{
	struct RealCCCDataObject *c = (struct RealCCCDataObject *)obj;

	c->default_doc_csid = default_doc_csid;
}

PRIVATE uint16
GetCCCDefaultCSID(CCCDataObject obj)
{
	struct RealCCCDataObject *c = (struct RealCCCDataObject *)obj;

	return c->default_doc_csid;
}

/* 
 * from_csid
 */
PRIVATE uint16
GetCCCFromCSID(CCCDataObject obj)
{
	struct RealCCCDataObject *c = (struct RealCCCDataObject *)obj;

	return c->from_csid;
}

PRIVATE void
SetCCCFromCSID(CCCDataObject obj, uint16 from_csid)
{
	struct RealCCCDataObject *c = (struct RealCCCDataObject *)obj;

	c->from_csid = from_csid;
}

/* 
 * to_csid
 */
PRIVATE uint16
GetCCCToCSID(CCCDataObject obj)
{
	struct RealCCCDataObject *c = (struct RealCCCDataObject *)obj;

	return c->to_csid;
}

PRIVATE void
SetCCCToCSID(CCCDataObject obj, uint16 to_csid)
{
	struct RealCCCDataObject *c = (struct RealCCCDataObject *)obj;

	c->to_csid = to_csid;
}


PUBLIC unsigned char *
INTL_CallCharCodeConverter(CCCDataObject obj, const unsigned char *buf,
	int32 bufsz)
{
	return (INTL_GetCCCCvtfunc(obj))(obj, buf, bufsz);
}


	/* INTL_GetCharCodeConverter:
	 * RETURN: 1 if converter found, else 0
	 * Also, sets:
	 *			obj->cvtfunc:	function handle for chararcter
	 *					code set streams converter
	 *			obj->cvtflag:	(Optional) flag to converter
	 *					function
	 *			obj->from_csid:	Code set converting from
	 *			obj->to_csid:	Code set converting to
	 * If the arg to_csid==0, then use the the conversion  for the
	 * first conversion entry that matches the from_csid.
	 */
PUBLIC int
INTL_GetCharCodeConverter(	register int16	from_csid,
				register int16	to_csid,
				CCCDataObject	obj)
{
	register cscvt_t		*cscvtp;

	if (from_csid == CS_DEFAULT)
		INTL_SetCCCFromCSID(obj, INTL_GetCCCDefaultCSID(obj)); 
	else
		INTL_SetCCCFromCSID(obj, from_csid);

	if(to_csid == 0)	/* unknown TO codeset */
		to_csid = INTL_DocToWinCharSetID(from_csid);
	INTL_SetCCCToCSID(obj, to_csid);

		/* Look-up conversion method given FROM and TO char. code sets	*/
	cscvtp = cscvt_tbl;

	while (cscvtp->from_csid) {
		if ((cscvtp->from_csid == from_csid)  && (cscvtp->to_csid == to_csid))
			break;
		cscvtp++;
	}
	INTL_SetCCCCvtflag(obj, cscvtp->cvtflag);
	INTL_SetCCCCvtfunc(obj, cscvtp->cvtmethod);

	return (INTL_GetCCCCvtfunc(obj)) ? 1 : 0;
}

/* WARNING: THIS TABLE AND THE STRUCT MUST BE IN SYNC WITH EACH OTHER */
PRIVATE struct INTL_CCCFuncs ccc_funcs = {
    /* set_report_autodetect */  SetCCCReportAutoDetect,
    /* call_report_autodetect */ CallCCCReportAutoDetect,
    /* set_cvtfunc */            SetCCCCvtfunc,
    /* get_cvtfunc */            GetCCCCvtfunc,
    /* set_jismode */            SetCCCJismode,
    /* get_jismode */            GetCCCJismode,
    /* set_cvtflag */            SetCCCCvtflag,
    /* get_cvtflag */            GetCCCCvtflag,
    /* get_uncvtbuf */           GetCCCUncvtbuf,
    /* set_default_doc_csid */   SetCCCDefaultCSID,
    /* get_default_doc_csid */   GetCCCDefaultCSID,
    /* set_from_csid */          SetCCCFromCSID,
    /* get_from_csid */          GetCCCFromCSID,
    /* set_to_csid */            SetCCCToCSID,
    /* get_to_csid */            GetCCCToCSID,
    /* set_retval */             SetCCCRetval,
    /* get_retval */             GetCCCRetval,
    /* set_len */                SetCCCLen,
    /* get_len */                GetCCCLen
};


PUBLIC CCCDataObject
INTL_CreateCharCodeConverter()
{
	struct RealCCCDataObject *obj;
	obj = XP_NEW_ZAP(struct RealCCCDataObject);
	obj->funcs_pointer = &ccc_funcs;
	obj->default_doc_csid = INTL_DefaultDocCharSetID(0);
	return (CCCDataObject) obj;
}

PUBLIC void
INTL_DestroyCharCodeConverter(CCCDataObject obj)
{
	XP_FREE(obj);
}

PRIVATE unsigned char *intl_conv (int16 fromcsid, int16 tocsid, unsigned char *pSrc, uint32 block_size);
PRIVATE
unsigned char *intl_conv(int16 fromcsid, int16 tocsid, unsigned char *pSrc, uint32 block_size)
{
    CCCDataObject	obj;
	unsigned char *pDest = NULL;
	if (NULL != (obj = INTL_CreateCharCodeConverter()))
	{
		if(0 != INTL_GetCharCodeConverter(fromcsid, tocsid, obj)) 
		{
			CCCFunc cvtfunc;
			if (NULL != (cvtfunc = INTL_GetCCCCvtfunc(obj))) 
			{
				if(pSrc == (pDest = (unsigned char *)cvtfunc(obj, pSrc, block_size)) )
				{
					/* if it use the same buffer to do conversion, we return NULL */
					pDest = NULL;
				}
			}
		}
		INTL_DestroyCharCodeConverter(obj);
	}
	return pDest ;
}

PUBLIC unsigned char *
INTL_ConvertLineWithoutAutoDetect (int16 fromcsid, int16 tocsid, unsigned char *pSrc, uint32 block_size)
{
#ifdef XP_OS2
    if (fromcsid != tocsid)
    {
        return intl_conv(fromcsid, tocsid, pSrc, block_size);
    else
    {
        return XP_STRDUP(pSrc);
    }
#else
	return intl_conv(fromcsid, tocsid, pSrc, block_size);
#endif /* XP_OS2 */
}


