/*-
 * Copyright (c) 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Margo Seltzer.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(unix)
#define MY_LSEEK lseek
#else
#define MY_LSEEK new_lseek
extern long new_lseek(int fd, long pos, int start);
#endif

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)hash_page.c	8.7 (Berkeley) 8/16/94";
#endif /* LIBC_SCCS and not lint */

#include "watcomfx.h"

/*
 * PACKAGE:  hashing
 *
 * DESCRIPTION:
 *	Page manipulation for hashing package.
 *
 * ROUTINES:
 *
 * External
 *	__get_page
 *	__add_ovflpage
 * Internal
 *	overflow_page
 *	open_temp
 */
#ifndef macintosh
#include <sys/types.h>
#endif

#if defined(macintosh)
#include <unistd.h>
#endif

#include <errno.h>
#include <fcntl.h>
#if defined(_WIN32) || defined(_WINDOWS) 
#include <io.h>
#endif
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32) && !defined(_WINDOWS) && !defined(macintosh) && !defined(XP_OS2_VACPP)
#include <unistd.h>
#endif

#include <assert.h>

#include "mcom_db.h"
#include "hash.h"
#include "page.h"
/* #include "extern.h" */

extern int mkstempflags(char *path, int extraFlags);

static uint32	*fetch_bitmap __P((HTAB *, uint32));
static uint32	 first_free __P((uint32));
static int	 open_temp __P((HTAB *));
static uint16	 overflow_page __P((HTAB *));
static void	 squeeze_key __P((uint16 *, const DBT *, const DBT *));
static int	 ugly_split
		    __P((HTAB *, uint32, BUFHEAD *, BUFHEAD *, int, int));

#define	PAGE_INIT(P) { \
	((uint16 *)(P))[0] = 0; \
	((uint16 *)(P))[1] = hashp->BSIZE - 3 * sizeof(uint16); \
	((uint16 *)(P))[2] = hashp->BSIZE; \
}

/* implement a new lseek using lseek that
 * writes zero's when extending a file
 * beyond the end.
 */
long new_lseek(int fd, long offset, int origin)
{
 	long cur_pos=0;
	long end_pos=0;
	long seek_pos=0;

	if(origin == SEEK_CUR)
      {	
      	if(offset < 1)							  
	    	return(lseek(fd, offset, SEEK_CUR));

		cur_pos = lseek(fd, 0, SEEK_CUR);

		if(cur_pos < 0)
			return(cur_pos);
	  }
										 
	end_pos = lseek(fd, 0, SEEK_END);
	if(end_pos < 0)
		return(end_pos);

	if(origin == SEEK_SET)
		seek_pos = offset;
	else if(origin == SEEK_CUR)
		seek_pos = cur_pos + offset;
	else if(origin == SEEK_END)
		seek_pos = end_pos + offset;
 	else
	  {
	  	assert(0);
		return(-1);
	  }

 	/* the seek position desired is before the
	 * end of the file.  We don't need
	 * to do anything special except the seek.
	 */
 	if(seek_pos <= end_pos)
 		return(lseek(fd, seek_pos, SEEK_SET));
 		
 	  /* the seek position is beyond the end of the
 	   * file.  Write zero's to the end.
 	   *
	   * we are already at the end of the file so
	   * we just need to "write()" zeros for the
	   * difference between seek_pos-end_pos and
	   * then seek to the position to finish
	   * the call
 	   */
 	  { 
 	 	char buffer[1024];
	   	long len = seek_pos-end_pos;
	   	memset(&buffer, 0, 1024);
	   	while(len > 0)
	      {
	        write(fd, (char*)&buffer, (size_t)(1024 > len ? len : 1024));
		    len -= 1024;
		  }
		return(lseek(fd, seek_pos, SEEK_SET));
	  }		

}

/*
 * This is called AFTER we have verified that there is room on the page for
 * the pair (PAIRFITS has returned true) so we go right ahead and start moving
 * stuff on.
 */
static void
putpair(char *p, const DBT *key, DBT * val)
{
	register uint16 *bp, n, off;

	bp = (uint16 *)p;

	/* Enter the key first. */
	n = bp[0];

	off = OFFSET(bp) - key->size;
	memmove(p + off, key->data, key->size);
	bp[++n] = off;

	/* Now the data. */
	off -= val->size;
	memmove(p + off, val->data, val->size);
	bp[++n] = off;

	/* Adjust page info. */
	bp[0] = n;
	bp[n + 1] = off - ((n + 3) * sizeof(uint16));
	bp[n + 2] = off;
}

/*
 * Returns:
 *	 0 OK
 *	-1 error
 */
extern int
__delpair(HTAB *hashp, BUFHEAD *bufp, int ndx)
{
	register uint16 *bp, newoff;
	register int n;
	uint16 pairlen;

	bp = (uint16 *)bufp->page;
	n = bp[0];

	if (bp[ndx + 1] < REAL_KEY)
		return (__big_delete(hashp, bufp));
	if (ndx != 1)
		newoff = bp[ndx - 1];
	else
		newoff = hashp->BSIZE;
	pairlen = newoff - bp[ndx + 1];

	if (ndx != (n - 1)) {
		/* Hard Case -- need to shuffle keys */
		register int i;
		register char *src = bufp->page + (int)OFFSET(bp);
		uint32 dst_offset = (uint32)OFFSET(bp) + (uint32)pairlen;
		register char *dst = bufp->page + dst_offset;
		uint32 length = bp[ndx + 1] - OFFSET(bp);

		/*
		 * +-----------+XXX+---------+XXX+---------+---------> +infinity
		 * |           |             |             |
		 * 0           src_offset    dst_offset    BSIZE
		 *
		 * Dst_offset is > src_offset, so if src_offset were bad, dst_offset
		 * would be too, therefore we check only dst_offset.
		 *
		 * If dst_offset is >= BSIZE, either OFFSET(bp), or pairlen, or both
		 * is corrupted.
		 *
		 * Once we know dst_offset is < BSIZE, we can subtract it from BSIZE
		 * to get an upper bound on length.
		 */
		if(dst_offset > (uint32)hashp->BSIZE)
			return(DATABASE_CORRUPTED_ERROR);

		if(length > (uint32)(hashp->BSIZE - dst_offset))
			return(DATABASE_CORRUPTED_ERROR);

		memmove(dst, src, length);

		/* Now adjust the pointers */
		for (i = ndx + 2; i <= n; i += 2) {
			if (bp[i + 1] == OVFLPAGE) {
				bp[i - 2] = bp[i];
				bp[i - 1] = bp[i + 1];
			} else {
				bp[i - 2] = bp[i] + pairlen;
				bp[i - 1] = bp[i + 1] + pairlen;
			}
		}
	}
	/* Finally adjust the page data */
	bp[n] = OFFSET(bp) + pairlen;
	bp[n - 1] = bp[n + 1] + pairlen + 2 * sizeof(uint16);
	bp[0] = n - 2;
	hashp->NKEYS--;

	bufp->flags |= BUF_MOD;
	return (0);
}
/*
 * Returns:
 *	 0 ==> OK
 *	-1 ==> Error
 */
extern int
__split_page(HTAB *hashp, uint32 obucket, uint32 nbucket)
{
	register BUFHEAD *new_bufp, *old_bufp;
	register uint16 *ino;
	register uint16 *tmp_uint16_array;
	register char *np;
	DBT key, val;
    uint16 n, ndx;
	int retval;
	uint16 copyto, diff, moved;
	size_t off;
	char *op;

	copyto = (uint16)hashp->BSIZE;
	off = (uint16)hashp->BSIZE;
	old_bufp = __get_buf(hashp, obucket, NULL, 0);
	if (old_bufp == NULL)
		return (-1);
	new_bufp = __get_buf(hashp, nbucket, NULL, 0);
	if (new_bufp == NULL)
		return (-1);

	old_bufp->flags |= (BUF_MOD | BUF_PIN);
	new_bufp->flags |= (BUF_MOD | BUF_PIN);

	ino = (uint16 *)(op = old_bufp->page);
	np = new_bufp->page;

	moved = 0;

	for (n = 1, ndx = 1; n < ino[0]; n += 2) {
		if (ino[n + 1] < REAL_KEY) {
			retval = ugly_split(hashp, obucket, old_bufp, new_bufp,
			    (int)copyto, (int)moved);
			old_bufp->flags &= ~BUF_PIN;
			new_bufp->flags &= ~BUF_PIN;
			return (retval);

		}
		key.data = (uint8 *)op + ino[n];

		/* check here for ino[n] being greater than
		 * off.  If it is then the database has
		 * been corrupted.
		 */
		if(ino[n] > off)
			return(DATABASE_CORRUPTED_ERROR);

		key.size = off - ino[n];

#ifdef DEBUG
		/* make sure the size is positive */
		assert(((int)key.size) > -1);
#endif

		if (__call_hash(hashp, (char *)key.data, key.size) == obucket) {
			/* Don't switch page */
			diff = copyto - off;
			if (diff) {
				copyto = ino[n + 1] + diff;
				memmove(op + copyto, op + ino[n + 1],
				    off - ino[n + 1]);
				ino[ndx] = copyto + ino[n] - ino[n + 1];
				ino[ndx + 1] = copyto;
			} else
				copyto = ino[n + 1];
			ndx += 2;
		} else {
			/* Switch page */
			val.data = (uint8 *)op + ino[n + 1];
			val.size = ino[n] - ino[n + 1];

			/* if the pair doesn't fit something is horribly
			 * wrong.  LJM
			 */
			tmp_uint16_array = (uint16*)np;
			if(!PAIRFITS(tmp_uint16_array, &key, &val))
				return(DATABASE_CORRUPTED_ERROR);

			putpair(np, &key, &val);
			moved += 2;
		}

		off = ino[n + 1];
	}

	/* Now clean up the page */
	ino[0] -= moved;
	FREESPACE(ino) = copyto - sizeof(uint16) * (ino[0] + 3);
	OFFSET(ino) = copyto;

#ifdef DEBUG3
	(void)fprintf(stderr, "split %d/%d\n",
	    ((uint16 *)np)[0] / 2,
	    ((uint16 *)op)[0] / 2);
#endif
	/* unpin both pages */
	old_bufp->flags &= ~BUF_PIN;
	new_bufp->flags &= ~BUF_PIN;
	return (0);
}

/*
 * Called when we encounter an overflow or big key/data page during split
 * handling.  This is special cased since we have to begin checking whether
 * the key/data pairs fit on their respective pages and because we may need
 * overflow pages for both the old and new pages.
 *
 * The first page might be a page with regular key/data pairs in which case
 * we have a regular overflow condition and just need to go on to the next
 * page or it might be a big key/data pair in which case we need to fix the
 * big key/data pair.
 *
 * Returns:
 *	 0 ==> success
 *	-1 ==> failure
 */

/* the maximum number of loops we will allow UGLY split to chew
 * on before we assume the database is corrupted and throw it
 * away.
 */
#define MAX_UGLY_SPLIT_LOOPS 10000

static int
ugly_split(HTAB *hashp, uint32 obucket, BUFHEAD *old_bufp,
 BUFHEAD *new_bufp,/* Same as __split_page. */ int copyto, int moved)
	/* int copyto;	 First byte on page which contains key/data values. */
	/* int moved;	 Number of pairs moved to new page. */
{
	register BUFHEAD *bufp;	/* Buffer header for ino */
	register uint16 *ino;	/* Page keys come off of */
	register uint16 *np;	/* New page */
	register uint16 *op;	/* Page keys go on to if they aren't moving */
    uint32 loop_detection=0;

	BUFHEAD *last_bfp;	/* Last buf header OVFL needing to be freed */
	DBT key, val;
	SPLIT_RETURN ret;
	uint16 n, off, ov_addr, scopyto;
	char *cino;		/* Character value of ino */
	int status;

	bufp = old_bufp;
	ino = (uint16 *)old_bufp->page;
	np = (uint16 *)new_bufp->page;
	op = (uint16 *)old_bufp->page;
	last_bfp = NULL;
	scopyto = (uint16)copyto;	/* ANSI */

	n = ino[0] - 1;
	while (n < ino[0]) {


        /* this function goes nuts sometimes and never returns. 
         * I havent found the problem yet but I need a solution
         * so if we loop too often we assume a database curruption error
         * :LJM
         */
        loop_detection++;

        if(loop_detection > MAX_UGLY_SPLIT_LOOPS)
            return DATABASE_CORRUPTED_ERROR;

		if (ino[2] < REAL_KEY && ino[2] != OVFLPAGE) {
			if ((status = __big_split(hashp, old_bufp,
			    new_bufp, bufp, bufp->addr, obucket, &ret)))
				return (status);
			old_bufp = ret.oldp;
			if (!old_bufp)
				return (-1);
			op = (uint16 *)old_bufp->page;
			new_bufp = ret.newp;
			if (!new_bufp)
				return (-1);
			np = (uint16 *)new_bufp->page;
			bufp = ret.nextp;
			if (!bufp)
				return (0);
			cino = (char *)bufp->page;
			ino = (uint16 *)cino;
			last_bfp = ret.nextp;
		} else if (ino[n + 1] == OVFLPAGE) {
			ov_addr = ino[n];
			/*
			 * Fix up the old page -- the extra 2 are the fields
			 * which contained the overflow information.
			 */
			ino[0] -= (moved + 2);
			FREESPACE(ino) =
			    scopyto - sizeof(uint16) * (ino[0] + 3);
			OFFSET(ino) = scopyto;

			bufp = __get_buf(hashp, ov_addr, bufp, 0);
			if (!bufp)
				return (-1);

			ino = (uint16 *)bufp->page;
			n = 1;
			scopyto = hashp->BSIZE;
			moved = 0;

			if (last_bfp)
				__free_ovflpage(hashp, last_bfp);
			last_bfp = bufp;
		}
		/* Move regular sized pairs of there are any */
		off = hashp->BSIZE;
		for (n = 1; (n < ino[0]) && (ino[n + 1] >= REAL_KEY); n += 2) {
			cino = (char *)ino;
			key.data = (uint8 *)cino + ino[n];
			key.size = off - ino[n];
			val.data = (uint8 *)cino + ino[n + 1];
			val.size = ino[n] - ino[n + 1];
			off = ino[n + 1];

			if (__call_hash(hashp, (char*)key.data, key.size) == obucket) {
				/* Keep on old page */
				if (PAIRFITS(op, (&key), (&val)))
					putpair((char *)op, &key, &val);
				else {
					old_bufp =
					    __add_ovflpage(hashp, old_bufp);
					if (!old_bufp)
						return (-1);
					op = (uint16 *)old_bufp->page;
					putpair((char *)op, &key, &val);
				}
				old_bufp->flags |= BUF_MOD;
			} else {
				/* Move to new page */
				if (PAIRFITS(np, (&key), (&val)))
					putpair((char *)np, &key, &val);
				else {
					new_bufp =
					    __add_ovflpage(hashp, new_bufp);
					if (!new_bufp)
						return (-1);
					np = (uint16 *)new_bufp->page;
					putpair((char *)np, &key, &val);
				}
				new_bufp->flags |= BUF_MOD;
			}
		}
	}
	if (last_bfp)
		__free_ovflpage(hashp, last_bfp);
	return (0);
}

/*
 * Add the given pair to the page
 *
 * Returns:
 *	0 ==> OK
 *	1 ==> failure
 */
extern int
__addel(HTAB *hashp, BUFHEAD *bufp, const DBT *key, const DBT * val)
{
	register uint16 *bp, *sop;
	int do_expand;

	bp = (uint16 *)bufp->page;
	do_expand = 0;
	while (bp[0] && (bp[2] < REAL_KEY || bp[bp[0]] < REAL_KEY))
		/* Exception case */
		if (bp[2] == FULL_KEY_DATA && bp[0] == 2)
			/* This is the last page of a big key/data pair
			   and we need to add another page */
			break;
		else if (bp[2] < REAL_KEY && bp[bp[0]] != OVFLPAGE) {
			bufp = __get_buf(hashp, bp[bp[0] - 1], bufp, 0);
			if (!bufp)
			  {
#ifdef DEBUG
				assert(0);
#endif
				return (-1);
			  }
			bp = (uint16 *)bufp->page;
		} else
			/* Try to squeeze key on this page */
			if (FREESPACE(bp) > PAIRSIZE(key, val)) {
			  {
				squeeze_key(bp, key, val);

				/* LJM: I added this because I think it was
				 * left out on accident.
				 * if this isn't incremented nkeys will not
				 * be the actual number of keys in the db.
				 */
				hashp->NKEYS++;
				return (0);
			  }
			} else {
				bufp = __get_buf(hashp, bp[bp[0] - 1], bufp, 0);
				if (!bufp)
			      {
#ifdef DEBUG
				    assert(0);
#endif
					return (-1);
				  }
				bp = (uint16 *)bufp->page;
			}

	if (PAIRFITS(bp, key, val))
		putpair(bufp->page, key, (DBT *)val);
	else {
		do_expand = 1;
		bufp = __add_ovflpage(hashp, bufp);
		if (!bufp)
	      {
#ifdef DEBUG
		    assert(0);
#endif
			return (-1);
		  }
		sop = (uint16 *)bufp->page;

		if (PAIRFITS(sop, key, val))
			putpair((char *)sop, key, (DBT *)val);
		else
			if (__big_insert(hashp, bufp, key, val))
	          {
#ifdef DEBUG
		        assert(0);
#endif
			    return (-1);
		      }
	}
	bufp->flags |= BUF_MOD;
	/*
	 * If the average number of keys per bucket exceeds the fill factor,
	 * expand the table.
	 */
	hashp->NKEYS++;
	if (do_expand ||
	    (hashp->NKEYS / (hashp->MAX_BUCKET + 1) > hashp->FFACTOR))
		return (__expand_table(hashp));
	return (0);
}

/*
 *
 * Returns:
 *	pointer on success
 *	NULL on error
 */
extern BUFHEAD *
__add_ovflpage(HTAB *hashp, BUFHEAD *bufp)
{
	register uint16 *sp;
	uint16 ndx, ovfl_num;
#ifdef DEBUG1
	int tmp1, tmp2;
#endif
	sp = (uint16 *)bufp->page;

	/* Check if we are dynamically determining the fill factor */
	if (hashp->FFACTOR == DEF_FFACTOR) {
		hashp->FFACTOR = sp[0] >> 1;
		if (hashp->FFACTOR < MIN_FFACTOR)
			hashp->FFACTOR = MIN_FFACTOR;
	}
	bufp->flags |= BUF_MOD;
	ovfl_num = overflow_page(hashp);
#ifdef DEBUG1
	tmp1 = bufp->addr;
	tmp2 = bufp->ovfl ? bufp->ovfl->addr : 0;
#endif
	if (!ovfl_num || !(bufp->ovfl = __get_buf(hashp, ovfl_num, bufp, 1)))
		return (NULL);
	bufp->ovfl->flags |= BUF_MOD;
#ifdef DEBUG1
	(void)fprintf(stderr, "ADDOVFLPAGE: %d->ovfl was %d is now %d\n",
	    tmp1, tmp2, bufp->ovfl->addr);
#endif
	ndx = sp[0];
	/*
	 * Since a pair is allocated on a page only if there's room to add
	 * an overflow page, we know that the OVFL information will fit on
	 * the page.
	 */
	sp[ndx + 4] = OFFSET(sp);
	sp[ndx + 3] = FREESPACE(sp) - OVFLSIZE;
	sp[ndx + 1] = ovfl_num;
	sp[ndx + 2] = OVFLPAGE;
	sp[0] = ndx + 2;
#ifdef HASH_STATISTICS
	hash_overflows++;
#endif
	return (bufp->ovfl);
}

/*
 * Returns:
 *	 0 indicates SUCCESS
 *	-1 indicates FAILURE
 */
extern int
__get_page(HTAB *hashp,
	char * p,
	uint32 bucket, 
	int is_bucket, 
	int is_disk, 
	int is_bitmap)
{
	register int fd, page;
	size_t size;
	int rsize;
	uint16 *bp;

	fd = hashp->fp;
	size = hashp->BSIZE;

	if ((fd == -1) || !is_disk) {
		PAGE_INIT(p);
		return (0);
	}
	if (is_bucket)
		page = BUCKET_TO_PAGE(bucket);
	else
		page = OADDR_TO_PAGE(bucket);
	if ((MY_LSEEK(fd, (off_t)page << hashp->BSHIFT, SEEK_SET) == -1) ||
	    ((rsize = read(fd, p, size)) == -1))
		return (-1);

	bp = (uint16 *)p;
	if (!rsize)
		bp[0] = 0;	/* We hit the EOF, so initialize a new page */
	else
		if ((unsigned)rsize != size) {
			errno = EFTYPE;
			return (-1);
		}

	if (!is_bitmap && !bp[0]) {
		PAGE_INIT(p);
	} else {

#ifdef DEBUG
		if(BYTE_ORDER == LITTLE_ENDIAN)
		  {
			int is_little_endian;
			is_little_endian = BYTE_ORDER;
		  }
		else if(BYTE_ORDER == BIG_ENDIAN)
		  {
			int is_big_endian;
			is_big_endian = BYTE_ORDER;
		  }
		else
		  {
			assert(0);
		  }
#endif

		if (hashp->LORDER != BYTE_ORDER) {
			register int i, max;

			if (is_bitmap) {
				max = hashp->BSIZE >> 2; /* divide by 4 */
				for (i = 0; i < max; i++)
					M_32_SWAP(((int *)p)[i]);
			} else {
				M_16_SWAP(bp[0]);
				max = bp[0] + 2;

	    		/* bound the size of max by
	     		 * the maximum number of entries
	     		 * in the array
	     		 */
				if((unsigned)max > (size / sizeof(uint16)))
					return(DATABASE_CORRUPTED_ERROR);

				/* do the byte order swap
				 */
				for (i = 1; i <= max; i++)
					M_16_SWAP(bp[i]);
			}
		}

		/* check the validity of the page here
		 * (after doing byte order swaping if necessary)
		 */
		if(!is_bitmap && bp[0] != 0)
		  {
			uint16 num_keys = bp[0];
			uint16 offset;
			uint16 i;

			/* bp[0] is supposed to be the number of
			 * entries currently in the page.  If
			 * bp[0] is too large (larger than the whole
			 * page) then the page is corrupted
			 */
			if(bp[0] > (size / sizeof(uint16)))
				return(DATABASE_CORRUPTED_ERROR);
			
			/* bound free space */
			if(FREESPACE(bp) > size)
				return(DATABASE_CORRUPTED_ERROR);
		
			/* check each key and data offset to make
 			 * sure they are all within bounds they
 			 * should all be less than the previous
 			 * offset as well.
 			 */
			offset = size;
			for(i=1 ; i <= num_keys; i+=2)
  			  {
				/* ignore overflow pages etc. */
				if(bp[i+1] >= REAL_KEY)
	  			  {
						
					if(bp[i] > offset || bp[i+1] > bp[i])			
						return(DATABASE_CORRUPTED_ERROR);
			
					offset = bp[i+1];
	  			  }
				else
	  			  {
					/* there are no other valid keys after
		 			 * seeing a non REAL_KEY
		 			 */
					break;
	  			  }
  			  }
		}
	}
	return (0);
}

/*
 * Write page p to disk
 *
 * Returns:
 *	 0 ==> OK
 *	-1 ==>failure
 */
extern int
__put_page(HTAB *hashp, char *p, uint32 bucket, int is_bucket, int is_bitmap)
{
	register int fd, page;
	size_t size;
	int wsize;
	off_t offset;

	size = hashp->BSIZE;
	if ((hashp->fp == -1) && open_temp(hashp))
		return (-1);
	fd = hashp->fp;

	if (hashp->LORDER != BYTE_ORDER) {
		register int i;
		register int max;

		if (is_bitmap) {
			max = hashp->BSIZE >> 2;	/* divide by 4 */
			for (i = 0; i < max; i++)
				M_32_SWAP(((int *)p)[i]);
		} else {
			max = ((uint16 *)p)[0] + 2;

            /* bound the size of max by
             * the maximum number of entries
             * in the array
             */
            if((unsigned)max > (size / sizeof(uint16)))
                return(DATABASE_CORRUPTED_ERROR);

			for (i = 0; i <= max; i++)
				M_16_SWAP(((uint16 *)p)[i]);

		}
	}

	if (is_bucket)
		page = BUCKET_TO_PAGE(bucket);
	else
		page = OADDR_TO_PAGE(bucket);
	offset = (off_t)page << hashp->BSHIFT;
	if ((MY_LSEEK(fd, offset, SEEK_SET) == -1) ||
	    ((wsize = write(fd, p, size)) == -1))
		/* Errno is set */
		return (-1);
	if ((unsigned)wsize != size) {
		errno = EFTYPE;
		return (-1);
	}
#if defined(_WIN32) || defined(_WINDOWS) 
	if (offset + size > hashp->file_size) {
		hashp->updateEOF = 1;
	}
#endif
	/* put the page back the way it was so that it isn't byteswapped
	 * if it remains in memory - LJM
	 */
	if (hashp->LORDER != BYTE_ORDER) {
		register int i;
		register int max;

		if (is_bitmap) {
			max = hashp->BSIZE >> 2;	/* divide by 4 */
			for (i = 0; i < max; i++)
				M_32_SWAP(((int *)p)[i]);
		} else {
    		uint16 *bp = (uint16 *)p;

			M_16_SWAP(bp[0]);
			max = bp[0] + 2;

			/* no need to bound the size if max again
			 * since it was done already above
			 */

			/* do the byte order re-swap
			 */
			for (i = 1; i <= max; i++)
				M_16_SWAP(bp[i]);
		}
	}

	return (0);
}

#define BYTE_MASK	((1 << INT_BYTE_SHIFT) -1)
/*
 * Initialize a new bitmap page.  Bitmap pages are left in memory
 * once they are read in.
 */
extern int
__ibitmap(HTAB *hashp, int pnum, int nbits, int ndx)
{
	uint32 *ip;
	size_t clearbytes, clearints;

	if ((ip = (uint32 *)malloc((size_t)hashp->BSIZE)) == NULL)
		return (1);
	hashp->nmaps++;
	clearints = ((nbits - 1) >> INT_BYTE_SHIFT) + 1;
	clearbytes = clearints << INT_TO_BYTE;
	(void)memset((char *)ip, 0, clearbytes);
	(void)memset(((char *)ip) + clearbytes, 0xFF,
	    hashp->BSIZE - clearbytes);
	ip[clearints - 1] = ALL_SET << (nbits & BYTE_MASK);
	SETBIT(ip, 0);
	hashp->BITMAPS[ndx] = (uint16)pnum;
	hashp->mapp[ndx] = ip;
	return (0);
}

static uint32
first_free(uint32 map)
{
	register uint32 i, mask;

	mask = 0x1;
	for (i = 0; i < BITS_PER_MAP; i++) {
		if (!(mask & map))
			return (i);
		mask = mask << 1;
	}
	return (i);
}

static uint16
overflow_page(HTAB *hashp)
{
	register uint32 *freep=NULL;
	register int max_free, offset, splitnum;
	uint16 addr;
	uint32 i;
	int bit, first_page, free_bit, free_page, in_use_bits, j;
#ifdef DEBUG2
	int tmp1, tmp2;
#endif
	splitnum = hashp->OVFL_POINT;
	max_free = hashp->SPARES[splitnum];

	free_page = (max_free - 1) >> (hashp->BSHIFT + BYTE_SHIFT);
	free_bit = (max_free - 1) & ((hashp->BSIZE << BYTE_SHIFT) - 1);

	/* Look through all the free maps to find the first free block */
	first_page = hashp->LAST_FREED >>(hashp->BSHIFT + BYTE_SHIFT);
	for ( i = first_page; i <= (unsigned)free_page; i++ ) {
		if (!(freep = (uint32 *)hashp->mapp[i]) &&
		    !(freep = fetch_bitmap(hashp, i)))
			return (0);
		if (i == (unsigned)free_page)
			in_use_bits = free_bit;
		else
			in_use_bits = (hashp->BSIZE << BYTE_SHIFT) - 1;
		
		if (i == (unsigned)first_page) {
			bit = hashp->LAST_FREED &
			    ((hashp->BSIZE << BYTE_SHIFT) - 1);
			j = bit / BITS_PER_MAP;
			bit = bit & ~(BITS_PER_MAP - 1);
		} else {
			bit = 0;
			j = 0;
		}
		for (; bit <= in_use_bits; j++, bit += BITS_PER_MAP)
			if (freep[j] != ALL_SET)
				goto found;
	}

	/* No Free Page Found */
	hashp->LAST_FREED = hashp->SPARES[splitnum];
	hashp->SPARES[splitnum]++;
	offset = hashp->SPARES[splitnum] -
	    (splitnum ? hashp->SPARES[splitnum - 1] : 0);

#define	OVMSG	"HASH: Out of overflow pages.  Increase page size\n"
	if (offset > SPLITMASK) {
		if (++splitnum >= NCACHED) {
#ifndef macintosh
			(void)write(STDERR_FILENO, OVMSG, sizeof(OVMSG) - 1);
#endif
			return (0);
		}
		hashp->OVFL_POINT = splitnum;
		hashp->SPARES[splitnum] = hashp->SPARES[splitnum-1];
		hashp->SPARES[splitnum-1]--;
		offset = 1;
	}

	/* Check if we need to allocate a new bitmap page */
	if (free_bit == (hashp->BSIZE << BYTE_SHIFT) - 1) {
		free_page++;
		if (free_page >= NCACHED) {
#ifndef macintosh
			(void)write(STDERR_FILENO, OVMSG, sizeof(OVMSG) - 1);
#endif
			return (0);
		}
		/*
		 * This is tricky.  The 1 indicates that you want the new page
		 * allocated with 1 clear bit.  Actually, you are going to
		 * allocate 2 pages from this map.  The first is going to be
		 * the map page, the second is the overflow page we were
		 * looking for.  The init_bitmap routine automatically, sets
		 * the first bit of itself to indicate that the bitmap itself
		 * is in use.  We would explicitly set the second bit, but
		 * don't have to if we tell init_bitmap not to leave it clear
		 * in the first place.
		 */
		if (__ibitmap(hashp,
		    (int)OADDR_OF(splitnum, offset), 1, free_page))
			return (0);
		hashp->SPARES[splitnum]++;
#ifdef DEBUG2
		free_bit = 2;
#endif
		offset++;
		if (offset > SPLITMASK) {
			if (++splitnum >= NCACHED) {
#ifndef macintosh
				(void)write(STDERR_FILENO, OVMSG,
				    sizeof(OVMSG) - 1);
#endif
				return (0);
			}
			hashp->OVFL_POINT = splitnum;
			hashp->SPARES[splitnum] = hashp->SPARES[splitnum-1];
			hashp->SPARES[splitnum-1]--;
			offset = 0;
		}
	} else {
		/*
		 * Free_bit addresses the last used bit.  Bump it to address
		 * the first available bit.
		 */
		free_bit++;
		SETBIT(freep, free_bit);
	}

	/* Calculate address of the new overflow page */
	addr = OADDR_OF(splitnum, offset);
#ifdef DEBUG2
	(void)fprintf(stderr, "OVERFLOW_PAGE: ADDR: %d BIT: %d PAGE %d\n",
	    addr, free_bit, free_page);
#endif
	return (addr);

found:
	bit = bit + first_free(freep[j]);
	SETBIT(freep, bit);
#ifdef DEBUG2
	tmp1 = bit;
	tmp2 = i;
#endif
	/*
	 * Bits are addressed starting with 0, but overflow pages are addressed
	 * beginning at 1. Bit is a bit addressnumber, so we need to increment
	 * it to convert it to a page number.
	 */
	bit = 1 + bit + (i * (hashp->BSIZE << BYTE_SHIFT));
	if (bit >= hashp->LAST_FREED)
		hashp->LAST_FREED = bit - 1;

	/* Calculate the split number for this page */
	for (i = 0; (i < (unsigned)splitnum) && (bit > hashp->SPARES[i]); i++) {}
	offset = (i ? bit - hashp->SPARES[i - 1] : bit);
	if (offset >= SPLITMASK)
		return (0);	/* Out of overflow pages */
	addr = OADDR_OF(i, offset);
#ifdef DEBUG2
	(void)fprintf(stderr, "OVERFLOW_PAGE: ADDR: %d BIT: %d PAGE %d\n",
	    addr, tmp1, tmp2);
#endif

	/* Allocate and return the overflow page */
	return (addr);
}

/*
 * Mark this overflow page as free.
 */
extern void
__free_ovflpage(HTAB *hashp, BUFHEAD *obufp)
{
	uint16 addr;
	uint32 *freep;
	uint32 bit_address, free_page, free_bit;
	uint16 ndx;

	if(!obufp || !obufp->addr)
	    return;

	addr = obufp->addr;
#ifdef DEBUG1
	(void)fprintf(stderr, "Freeing %d\n", addr);
#endif
	ndx = (((uint16)addr) >> SPLITSHIFT);
	bit_address =
	    (ndx ? hashp->SPARES[ndx - 1] : 0) + (addr & SPLITMASK) - 1;
	if (bit_address < (uint32)hashp->LAST_FREED)
		hashp->LAST_FREED = bit_address;
	free_page = (bit_address >> (hashp->BSHIFT + BYTE_SHIFT));
	free_bit = bit_address & ((hashp->BSIZE << BYTE_SHIFT) - 1);

	if (!(freep = hashp->mapp[free_page])) 
		freep = fetch_bitmap(hashp, free_page);

#ifdef DEBUG
	/*
	 * This had better never happen.  It means we tried to read a bitmap
	 * that has already had overflow pages allocated off it, and we
	 * failed to read it from the file.
	 */
	if (!freep)
	  {
		assert(0);
		return;
	  }
#endif
	CLRBIT(freep, free_bit);
#ifdef DEBUG2
	(void)fprintf(stderr, "FREE_OVFLPAGE: ADDR: %d BIT: %d PAGE %d\n",
	    obufp->addr, free_bit, free_page);
#endif
	__reclaim_buf(hashp, obufp);
}

/*
 * Returns:
 *	 0 success
 *	-1 failure
 */
static int
open_temp(HTAB *hashp)
{
#ifdef XP_OS2
 	hashp->fp = mkstemp(NULL);
#else
#if !defined(_WIN32) && !defined(_WINDOWS) && !defined(macintosh)
	sigset_t set, oset;
#endif
#if !defined(macintosh)
	char * tmpdir;
	size_t len;
	char last;
#endif
	static const char namestr[] = "/_hashXXXXXX";
	char filename[1024];

#if !defined(_WIN32) && !defined(_WINDOWS) && !defined(macintosh)
	/* Block signals; make sure file goes away at process exit. */
	(void)sigfillset(&set);
	(void)sigprocmask(SIG_BLOCK, &set, &oset);
#endif

	filename[0] = 0;
#if defined(macintosh)
	strcat(filename, namestr + 1);
#else
	tmpdir = getenv("TMP");
	if (!tmpdir)
		tmpdir = getenv("TMPDIR");
	if (!tmpdir)
		tmpdir = getenv("TEMP");
	if (!tmpdir)
		tmpdir = ".";
	len = strlen(tmpdir);
	if (len && len < (sizeof filename - sizeof namestr)) {
		strcpy(filename, tmpdir);
	}
	len = strlen(filename);
	last = tmpdir[len - 1];
	strcat(filename, (last == '/' || last == '\\') ? namestr + 1 : namestr);
#endif

#if defined(_WIN32) || defined(_WINDOWS)
	if ((hashp->fp = mkstempflags(filename, _O_BINARY|_O_TEMPORARY)) != -1) {
		if (hashp->filename) {
			free(hashp->filename);
		}
		hashp->filename = strdup(filename);
		hashp->is_temp = 1;
	}
#else
	if ((hashp->fp = mkstemp(filename)) != -1) {
		(void)unlink(filename);
#if !defined(macintosh)
		(void)fcntl(hashp->fp, F_SETFD, 1);
#endif									  
	}
#endif

#if !defined(_WIN32) && !defined(_WINDOWS) && !defined(macintosh)
	(void)sigprocmask(SIG_SETMASK, &oset, (sigset_t *)NULL);
#endif
#endif  /* !OS2 */
	return (hashp->fp != -1 ? 0 : -1);
}

/*
 * We have to know that the key will fit, but the last entry on the page is
 * an overflow pair, so we need to shift things.
 */
static void
squeeze_key(uint16 *sp, const DBT * key, const DBT * val)
{
	register char *p;
	uint16 free_space, n, off, pageno;

	p = (char *)sp;
	n = sp[0];
	free_space = FREESPACE(sp);
	off = OFFSET(sp);

	pageno = sp[n - 1];
	off -= key->size;
	sp[n - 1] = off;
	memmove(p + off, key->data, key->size);
	off -= val->size;
	sp[n] = off;
	memmove(p + off, val->data, val->size);
	sp[0] = n + 2;
	sp[n + 1] = pageno;
	sp[n + 2] = OVFLPAGE;
	FREESPACE(sp) = free_space - PAIRSIZE(key, val);
	OFFSET(sp) = off;
}

static uint32 *
fetch_bitmap(HTAB *hashp, uint32 ndx)
{
	if (ndx >= (unsigned)hashp->nmaps)
		return (NULL);
	if ((hashp->mapp[ndx] = (uint32 *)malloc((size_t)hashp->BSIZE)) == NULL)
		return (NULL);
	if (__get_page(hashp,
	    (char *)hashp->mapp[ndx], hashp->BITMAPS[ndx], 0, 1, 1)) {
		free(hashp->mapp[ndx]);
		hashp->mapp[ndx] = NULL; /* NEW: 9-11-95 */
		return (NULL);
	}                 
	return (hashp->mapp[ndx]);
}

#ifdef DEBUG4
int
print_chain(int addr)
{
	BUFHEAD *bufp;
	short *bp, oaddr;

	(void)fprintf(stderr, "%d ", addr);
	bufp = __get_buf(hashp, addr, NULL, 0);
	bp = (short *)bufp->page;
	while (bp[0] && ((bp[bp[0]] == OVFLPAGE) ||
		((bp[0] > 2) && bp[2] < REAL_KEY))) {
		oaddr = bp[bp[0] - 1];
		(void)fprintf(stderr, "%d ", (int)oaddr);
		bufp = __get_buf(hashp, (int)oaddr, bufp, 0);
		bp = (short *)bufp->page;
	}
	(void)fprintf(stderr, "\n");
}
#endif
