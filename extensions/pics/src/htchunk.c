
/*  W3 Copyright statement 
Copyright 1995 by: Massachusetts Institute of Technology (MIT), INRIA</H2>

This W3C software is being provided by the copyright holders under the
following license. By obtaining, using and/or copying this software,
you agree that you have read, understood, and will comply with the
following terms and conditions: 

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee or royalty is hereby
granted, provided that the full text of this NOTICE appears on
<EM>ALL</EM> copies of the software and documentation or portions
thereof, including modifications, that you make. 

<B>THIS SOFTWARE IS PROVIDED "AS IS," AND COPYRIGHT HOLDERS MAKE NO
REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED.  BY WAY OF EXAMPLE,
BUT NOT LIMITATION, COPYRIGHT HOLDERS MAKE NO REPRESENTATIONS OR
WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE OR
THAT THE USE OF THE SOFTWARE OR DOCUMENTATION WILL NOT INFRINGE ANY
THIRD PARTY PATENTS, COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS.
COPYRIGHT HOLDERS WILL BEAR NO LIABILITY FOR ANY USE OF THIS SOFTWARE
OR DOCUMENTATION.

The name and trademarks of copyright holders may NOT be used
in advertising or publicity pertaining to the software without
specific, written prior permission.  Title to copyright in this
software and any associated documentation will at all times remain
with copyright holders. 
*/
/*								      HTChunk.c
**	CHUNK HANDLING:	FLEXIBLE ARRAYS
**
**	(c) COPYRIGHT MIT 1995.
**	Please first read the full copyright statement in the file COPYRIGH.
**	@(#) $Id: htchunk.c,v 1.1 1999/03/18 22:32:48 neeti%netscape.com Exp $
**
** history:	AL, HF	28 Apr 94, Now chunk->data is filled by '\0' so
**			that the string is terminated at any time. That makes
**			HTChunk_terminate not needed any more, but never mind.
**		EGP	15 Mar 96, Added CString conversions.
**
*/

/* Library include files */
#include "xp_core.h"
#include "plstr.h"
/* #include "sysdep.h"  7/9/97 -- jhines */
#include "htutils.h"
#include "htchunk.h"				         /* Implemented here */

/*	Create a chunk with a certain allocation unit
**	--------------
*/
PUBLIC HTChunk * HTChunk_new (int grow)
{
    HTChunk * ch;
    if ((ch = (HTChunk  *) HT_CALLOC(1, sizeof(HTChunk))) == NULL)
        HT_OUTOFMEM("HTChunk_new");
    ch->growby = grow;
    return ch;
}


/*	Clear a chunk of all data
**	--------------------------
**	Zero the space but do NOT HT_FREE it. We zero because we promise to have
**	a NUL terminated string at all times.
*/
PUBLIC void HTChunk_clear (HTChunk * ch)
{
    if (ch) {
	ch->size = 0;
	memset((void *) ch->data, '\0', ch->allocated);
    }
}


/*	Free a chunk
**	------------
*/
PUBLIC void HTChunk_delete (HTChunk * ch)
{
    if (ch) {
	HT_FREE(ch->data);
    	HT_FREE(ch);
    }
}

/*	Create a chunk from an allocated string
**	---------------------------------------
*/
PUBLIC HTChunk * HTChunk_fromCString (char * str, int grow)
{
    HTChunk * ch;
    ch = HTChunk_new(grow);
    if (str) {
	ch->data = str;			/* can't handle non-allocated str */
	ch->size = PL_strlen(str);
    }
    return ch;
}

/*	Free a chunk but keep the data
**	------------------------------
*/
PUBLIC char * HTChunk_toCString (HTChunk * ch)
{
    char * ret = 0;
    if (ch) {
	ret = ch->data;
    	HT_FREE(ch);
    }
    return ret;
}

/*	Append a character
**	------------------
*/
PUBLIC void HTChunk_putc (HTChunk * ch, char c)
{
    if (ch) {
	if (ch->size >= ch->allocated-1) {
	    if (ch->data) {
		if ((ch->data = (char  *) HT_REALLOC(ch->data,ch->allocated+ch->growby)) == NULL)
		    HT_OUTOFMEM("HTChunk_putc");
		memset((void *) (ch->data + ch->allocated), '\0', ch->growby);
	    } else {
		if ((ch->data = (char  *) HT_CALLOC(1, ch->allocated+ch->growby)) == NULL)
		    HT_OUTOFMEM("HTChunk_putc");
	    }
	    ch->allocated += ch->growby;
	}
	*(ch->data+ch->size++) = c;
    }
}

/*	Append a string
**	---------------
*/
PUBLIC void HTChunk_puts (HTChunk * ch, const char * s)
{
     HTChunk_putb(ch, s, (int) PL_strlen(s));
}

/*	Append a block
**	---------------
**	The string is always zero terminated
*/
PUBLIC void HTChunk_putb (HTChunk * ch, const char * block, int len)
{
    if (ch && block && len) {
	int needed = ch->size+len;
	if (needed >= ch->allocated) {
	    ch->allocated = needed - needed%ch->growby + ch->growby;
	    if (ch->data) {
		if ((ch->data = (char *) HT_REALLOC(ch->data, ch->allocated)) == NULL)
		    HT_OUTOFMEM("HTChunk_putb");
	        memset((void *) (ch->data + needed), '\0', ch->allocated-needed);
	    } else {
		if ((ch->data = (char *) HT_CALLOC(1, ch->allocated)) == NULL)
		    HT_OUTOFMEM("HTChunk_putb");
	    }
	}
	memcpy((void *) (ch->data+ch->size), block, len);
	ch->size = needed;
    }
}


/*	Ensure a certain size
**	---------------------
*/
PUBLIC void HTChunk_ensure (HTChunk * ch, int len)
{
    if (ch && len) {
	int needed = ch->size+len;
	if (needed >= ch->allocated) {
	    ch->allocated = needed - needed%ch->growby + ch->growby;
	    if (ch->data) {
		if ((ch->data = (char  *) HT_REALLOC(ch->data, ch->allocated)) == NULL)
		    HT_OUTOFMEM("HTChunk_putb");
	        memset((void *) (ch->data + ch->size), '\0', ch->allocated-ch->size);
	    } else {
		if ((ch->data = (char  *) HT_CALLOC(1, ch->allocated)) == NULL)
		    HT_OUTOFMEM("ch->data ");
	    }
	}
    }
#if 0
    if (needed <= ch->allocated) return;
    ch->allocated = needed-1 - ((needed-1) % ch->growby)
    			     + ch->growby; /* Round up */
    ch->data = ch->data ? (char *)PR_REALLOC(ch->data, ch->allocated)
			: (char *)HT_MALLOC(ch->allocated);
    if (ch->data == NULL) HT_OUTOFMEM(__FILE__, "HTChunk_ensure");
#endif
}
