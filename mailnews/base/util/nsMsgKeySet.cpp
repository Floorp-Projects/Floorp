/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "msgCore.h"    // precompiled header...
#include "prlog.h"

#include "MailNewsTypes.h"
#include "nsMsgKeySet.h"
#include "prprf.h"
#include "prmem.h"

#if defined(DEBUG_seth_) || defined(DEBUG_sspitzer_)
#define DEBUG_MSGKEYSET 1
#endif

/* A compressed encoding for sets of article.  This is usually for lines from
   the newsrc, which have article lists like

   1-29627,29635,29658,32861-32863

   so the data has these properties:

   - strictly increasing
   - large subsequences of monotonically increasing ranges
   - gaps in the set are usually small, but not always
   - consecutive ranges tend to be large

   The biggest win is to run-length encode the data, storing ranges as two
   numbers (start+length or start,end). We could also store each number as a
   delta from the previous number for further compression, but that gets kind
   of tricky, since there are no guarentees about the sizes of the gaps, and
   we'd have to store variable-length words.

   Current data format:

   DATA := SIZE [ CHUNK ]*
   CHUNK := [ RANGE | VALUE ]
   RANGE := -LENGTH START
   START := VALUE
   LENGTH := PRInt32
   VALUE := a literal positive integer, for now
   it could also be an offset from the previous value.
   LENGTH could also perhaps be a less-than-32-bit quantity,
   at least most of the time.

   Lengths of CHUNKs are stored negative to distinguish the beginning of
   a chunk from a literal: negative means two-word sequence, positive
   means one-word sequence.

   0 represents a literal 0, but should not occur, and should never occur
   except in the first position.

   A length of -1 won't occur either, except temporarily - a sequence of
   two elements is represented as two literals, since they take up the same
   space.

   Another optimization we make is to notice that we typically ask the
   question ``is N a member of the set'' for increasing values of N. So the
   set holds a cache of the last value asked for, and can simply resume the
   search from there.  */

MOZ_DECL_CTOR_COUNTER(nsMsgKeySet)


nsMsgKeySet::nsMsgKeySet(/* MSG_NewsHost* host*/)
{
	MOZ_COUNT_CTOR(nsMsgKeySet);
	m_cached_value = -1;
	m_cached_value_index = 0;
	m_length = 0;
	m_data_size = 10;
	m_data = (PRInt32 *) PR_Malloc (sizeof (PRInt32) * m_data_size);
#ifdef NEWSRC_DOES_HOST_STUFF
	m_host = host;
#endif
}


nsMsgKeySet::~nsMsgKeySet()
{
	MOZ_COUNT_DTOR(nsMsgKeySet);
	PR_FREEIF(m_data);
}


PRBool nsMsgKeySet::Grow()
{
	PRInt32 new_size;
	PRInt32 *new_data;
	new_size = m_data_size * 2;
	new_data = (PRInt32 *) PR_REALLOC (m_data, sizeof (PRInt32) * new_size);
	if (! new_data)
		return PR_FALSE;
	m_data_size = new_size;
	m_data = new_data;
	return PR_TRUE;
}


nsMsgKeySet::nsMsgKeySet(const char* numbers /* , MSG_NewsHost* host */)
{
	PRInt32 *head, *tail, *end;
    MOZ_COUNT_CTOR(nsMsgKeySet);

#ifdef NEWSRC_DOES_HOST_STUFF
	m_host = host;
#endif
	m_cached_value = -1;
	m_cached_value_index = 0;
	m_length = 0;
	m_data_size = 10;
	m_data = (PRInt32 *) PR_Malloc (sizeof (PRInt32) * m_data_size);
	if (!m_data) return;

	head = m_data;
	tail = head;
	end = head + m_data_size;

	if(!numbers) {
		return;
	}

	while (nsCRT::IsAsciiSpace (*numbers)) numbers++;
	while (*numbers) {
		PRInt32 from = 0;
		PRInt32 to;

		if (tail >= end - 4) {
			/* out of room! */
			PRInt32 tailo = tail - head;
			if (!Grow()) {
				PR_FREEIF(m_data);
				return;
			}
			/* data may have been relocated */
			head = m_data;
			tail = head + tailo;
			end = head + m_data_size;
		}

		while (nsCRT::IsAsciiSpace(*numbers)) numbers++;
		if (*numbers && !nsCRT::IsAsciiDigit(*numbers)) {
			break;			/* illegal character */
		}
		while (nsCRT::IsAsciiDigit (*numbers)) {
			from = (from * 10) + (*numbers++ - '0');
		}
		while (nsCRT::IsAsciiSpace (*numbers)) numbers++;
		if (*numbers != '-') {
			to = from;
		} else {
			to = 0;
			numbers++;
			while (*numbers >= '0' && *numbers <= '9')
				to = (to * 10) + (*numbers++ - '0');
			while (nsCRT::IsAsciiSpace (*numbers)) numbers++;
		}

		if (to < from) to = from; /* illegal */

		/* This is a hack - if the newsrc file specifies a range 1-x as
		   being read, we internally pretend that article 0 is read as well.
		   (But if only 2-x are read, then 0 is not read.)  This is needed
		   because some servers think that article 0 is an article (I think)
		   but some news readers (including Netscape 1.1) choke if the .newsrc
		   file has lines beginning with 0...   ### */
		if (from == 1) from = 0;

		if (to == from) {
			/* Write it as a literal */
			*tail = from;
			tail++;
		} else /* Write it as a range. */ {
			*tail = -(to - from);
			tail++;
			*tail = from;
			tail++;
		}

		while (*numbers == ',' || nsCRT::IsAsciiSpace (*numbers)) {
			numbers++;
		}
	}

	m_length = tail - head; /* size of data */
}



nsMsgKeySet*
nsMsgKeySet::Create(/*MSG_NewsHost* host*/)
{
  nsMsgKeySet* set = new nsMsgKeySet(/* host */);
	if (set && set->m_data == NULL) {
		delete set;
		set = NULL;
	}
	return set;
}


nsMsgKeySet*
nsMsgKeySet::Create(const char* value /* , MSG_NewsHost* host */)
{
#ifdef DEBUG_MSGKEYSET
    printf("create from %s\n",value);
#endif

	nsMsgKeySet* set = new nsMsgKeySet(value /* , host */);
	if (set && set->m_data == NULL) {
		delete set;
		set = NULL;
	}
	return set;
}



/* Returns the lowest non-member of the set greater than 0.
 */
PRInt32
nsMsgKeySet::FirstNonMember ()
{
	if (m_length <= 0) {
		return 1;
	} else if(m_data[0] < 0 && m_data[1] != 1 && m_data[1] != 0) {
		/* first range not equal to 0 or 1, always return 1 */
		return 1;
	} else if (m_data[0] < 0) {
		/* it's a range */
		/* If there is a range [N-M] we can presume that M+1 is not in the
		   set. */
		return (m_data[1] - m_data[0] + 1);
	} else {
		/* it's a literal */
		if (m_data[0] == 1) {
			/* handle "1,..." */
			if (m_length > 1 && m_data[1] == 2) {
				/* This is "1,2,M-N,..." or "1,2,M,..."  where M >= 4.  Note
				   that M will never be 3, because in that case we would have
				   started with a range: "1-3,..." */
				return 3;
			} else {
				return 2;              /* handle "1,M-N,.." or "1,M,..."
										  where M >= 3; */
			}
		}
		else if (m_data[0] == 0) {
			/* handle "0,..." */
			if (m_length > 1 && m_data[1] == 1) {
				/* this is 0,1, (see above) */
				return 2;
			}
			else {
				return 1;
			}

		} else {
			/* handle "M,..." where M >= 2. */
			return 1;
		}
	}
}


nsresult 
nsMsgKeySet::Output(char **outputStr)
{
  NS_ENSURE_ARG(outputStr);
	PRInt32 size;
	PRInt32 *head;
	PRInt32 *tail;
	PRInt32 *end;
	PRInt32 s_size;
	char *s_head;
	char *s, *s_end;
	PRInt32 last_art = -1;

  *outputStr = nsnull;

	size = m_length;
	head = m_data;
	tail = head;
	end = head + size;

	s_size = (size * 12) + 10;	// dmb - try to make this allocation get used at least once.
  s_head = (char *) nsMemory::Alloc(s_size);
	s_head[0] = '\0';			// otherwise, s_head will contain garbage.
	s = s_head;
	s_end = s + s_size;

	if (! s) return NS_ERROR_OUT_OF_MEMORY;

	while (tail < end) {
		PRInt32 from;
		PRInt32 to;

		if (s > (s_end - (12 * 2 + 10))) { /* 12 bytes for each number (enough
											  for "2147483647" aka 2^31-1),
											  plus 10 bytes of slop. */
			PRInt32 so = s - s_head;
			s_size += 200;
			char* tmp = new char[s_size];
			if (tmp) PL_strcpy(tmp, s_head);
      nsMemory::Free(s_head);
			s_head = tmp;
			if (!s_head) return NS_ERROR_OUT_OF_MEMORY;
			s = s_head + so;
			s_end = s_head + s_size;
		}

		if (*tail < 0) {
			/* it's a range */
			from = tail[1];
			to = from + (-(tail[0]));
			tail += 2;
		}
		else /* it's a literal */
			{
				from = *tail;
				to = from;
				tail++;
			}
		if (from == 0) {
			from = 1;				/* See 'hack' comment above  ### */
		}
		if (from <= last_art) from = last_art + 1;
		if (from <= to) {
			if (from < to) {
				PR_snprintf(s, s_end - s, "%lu-%lu,", from, to);
			} else {
				PR_snprintf(s, s_end - s, "%lu,", from);
			}
			s += PL_strlen(s);
			last_art = to;
		}
	}
	if (last_art >= 0) {
		s--;							/* Strip off the last ',' */
	}

	*s = 0;

	*outputStr = s_head;
  return NS_OK;
}

PRInt32 
nsMsgKeySet::GetLastMember()
{
	if (m_length > 1)
	{
		PRInt32 nextToLast = m_data[m_length - 2];
		if (nextToLast < 0)	// is range at end?
		{
			PRInt32 last = m_data[m_length - 1];
			return (-nextToLast + last - 1);
		}
		else	// no, so last number must be last member
		{
			return m_data[m_length - 1];
		}
	}
	else if (m_length == 1)
		return m_data[0];	// must be only 1 read.
	else
		return 0;
}

void nsMsgKeySet::SetLastMember(PRInt32 newHighWaterMark)
{
	if (newHighWaterMark < GetLastMember())
	{
		while (PR_TRUE)
		{
			if (m_length > 1)
			{
				PRInt32 nextToLast = m_data[m_length - 2];
				PRInt32 curHighWater;
				if (nextToLast < 0)	// is range at end?
				{
					PRInt32 rangeStart = m_data[m_length - 1];
					PRInt32 rangeLength = -nextToLast;
					curHighWater = (rangeLength + rangeStart - 1);
					if (curHighWater > newHighWaterMark)
					{
						if (rangeStart > newHighWaterMark)	
						{
							m_length -= 2;	// throw away whole range
						}
						else if (rangeStart == newHighWaterMark)
						{
							// turn range into single element.
							m_data[m_length - 2] = newHighWaterMark;
							m_length--;
							break;
						}
						else	// just shorten range
						{
							m_data[m_length - 2] = -(newHighWaterMark - rangeStart);
							break;
						}
					}
					else {
						// prevent the infinite loop
						// see bug #13062
						break;
					}	
				}
				else if (m_data[m_length - 1] > newHighWaterMark)	// no, so last number must be last member
				{
					m_length--;
				}
				else
					break;
			}
			else 
				break;
		}
		// well, the whole range is probably invalid, because the server probably re-ordered ids, 
		// but what can you do?
#ifdef NEWSRC_DOES_HOST_STUFF
		if (m_host) 
			m_host->MarkDirty();
#endif
	}
}

PRInt32 
nsMsgKeySet::GetFirstMember()
{
	if (m_length > 1)
	{
		PRInt32 first = m_data[0];
		if (first < 0)	// is range at start?
		{
			PRInt32 second = m_data[1];
			return (second);
		}
		else	// no, so first number must be first member
		{
			return m_data[0];
		}
	}
	else if (m_length == 1)
		return m_data[0];	// must be only 1 read.
	else
		return 0;
}

/* Re-compresses a `nsMsgKeySet' object.

   The assumption is made that the `nsMsgKeySet' is syntactically correct
   (all ranges have a length of at least 1, and all values are non-
   decreasing) but will optimize the compression, for example, merging
   consecutive literals or ranges into one range.

   Returns PR_TRUE if successful, PR_FALSE if there wasn't enough memory to
   allocate scratch space.

   #### This should be changed to modify the buffer in place.

   Also note that we never call Optimize() unless we actually changed
   something, so it's a great place to tell the MSG_NewsHost* that something
   changed.
   */
PRBool
nsMsgKeySet::Optimize()
{
	PRInt32 input_size;
	PRInt32 output_size;
	PRInt32 *input_tail;
	PRInt32 *output_data;
	PRInt32 *output_tail;
	PRInt32 *input_end;
	PRInt32 *output_end;

	input_size = m_length;
	output_size = input_size + 1;
	input_tail = m_data;
	output_data = (PRInt32 *) PR_Malloc (sizeof (PRInt32) * output_size);
	output_tail = output_data;
	input_end = input_tail + input_size;
	output_end = output_data + output_size;

	if (!output_data) return PR_FALSE;

	/* We're going to modify the set, so invalidate the cache. */
	m_cached_value = -1;

	while (input_tail < input_end) {
		PRInt32 from, to;
		PRBool range_p = (*input_tail < 0);

		if (range_p) {
			/* it's a range */
			from = input_tail[1];
			to = from + (-(input_tail[0]));

			/* Copy it over */
			*output_tail++ = *input_tail++;
			*output_tail++ = *input_tail++;				
		} else {
			/* it's a literal */
			from = *input_tail;
			to = from;

			/* Copy it over */
			*output_tail++ = *input_tail++;
		}
		NS_ASSERTION(output_tail < output_end, "invalid end of output string");
		if (output_tail >= output_end) {
			PR_Free(output_data);
			return PR_FALSE;
		}

		/* As long as this chunk is followed by consecutive chunks,
		   keep extending it. */
		while (input_tail < input_end &&
			   ((*input_tail > 0 && /* literal... */
				 *input_tail == to + 1) || /* ...and consecutive, or */
				(*input_tail <= 0 && /* range... */
				 input_tail[1] == to + 1)) /* ...and consecutive. */
			   ) {
			if (! range_p) {
				/* convert the literal to a range. */
				output_tail++;
				output_tail [-2] = 0;
				output_tail [-1] = from;
				range_p = PR_TRUE;
			}

			if (*input_tail > 0) { /* literal */
				output_tail[-2]--; /* increase length by 1 */
				to++;
				input_tail++;
			} else {
				PRInt32 L2 = (- *input_tail) + 1;
				output_tail[-2] -= L2; /* increase length by N */
				to += L2;
				input_tail += 2;
			}
		}
	}

	PR_Free (m_data);
	m_data = output_data;
	m_data_size = output_size;
	m_length = output_tail - output_data;

	/* One last pass to turn [N - N+1] into [N, N+1]. */
	output_tail = output_data;
	output_end = output_tail + m_length;
	while (output_tail < output_end) {
		if (*output_tail < 0) {
			/* it's a range */
			if (output_tail[0] == -1) {
				output_tail[0] = output_tail[1];
				output_tail[1]++;
			}
			output_tail += 2;
		} else {
			/* it's a literal */
			output_tail++;
		}
	}

#ifdef NEWSRC_DOES_HOST_STUFF
	if (m_host) m_host->MarkDirty();
#endif
	return PR_TRUE;
}



PRBool
nsMsgKeySet::IsMember(PRInt32 number)
{
	PRBool value = PR_FALSE;
	PRInt32 size;
	PRInt32 *head;
	PRInt32 *tail;
	PRInt32 *end;

	size = m_length;
	head = m_data;
	tail = head;
	end = head + size;

	/* If there is a value cached, and that value is smaller than the
	   value we're looking for, skip forward that far. */
	if (m_cached_value > 0 &&
		m_cached_value < number) {
		tail += m_cached_value_index;
	}

	while (tail < end) {
		if (*tail < 0) {
			/* it's a range */
			PRInt32 from = tail[1];
			PRInt32 to = from + (-(tail[0]));
			if (from > number) {
				/* This range begins after the number - we've passed it. */
				value = PR_FALSE;
				goto DONE;
			} else if (to >= number) {
				/* In range. */
				value = PR_TRUE;
				goto DONE;
			} else {
				tail += 2;
			}
		}
		else {
			/* it's a literal */
			if (*tail == number) {
				/* bang */
				value = PR_TRUE;
				goto DONE;
			} else if (*tail > number) {
				/* This literal is after the number - we've passed it. */
				value = PR_FALSE;
				goto DONE;
			} else {
				tail++;
			}
		}
	}

DONE:
	/* Store the position of this chunk for next time. */
	m_cached_value = number;
	m_cached_value_index = tail - head;

	return value;
}


int
nsMsgKeySet::Add(PRInt32 number)
{
	PRInt32 size;
	PRInt32 *head;
	PRInt32 *tail;
	PRInt32 *end;

#ifdef DEBUG_MSGKEYSET
    printf("add %d\n",number);
#endif
    
	size = m_length;
	head = m_data;
	tail = head;
	end = head + size;

	NS_ASSERTION (number >= 0, "can't have negative items");
	if (number < 0)
		return 0;

	/* We're going to modify the set, so invalidate the cache. */
	m_cached_value = -1;

	while (tail < end) {
		if (*tail < 0) {
			/* it's a range */
			PRInt32 from = tail[1];
			PRInt32 to = from + (-(tail[0]));

			if (from <= number && to >= number) {
				/* This number is already present - we don't need to do
				   anything. */
				return 0;
			}

			if (to > number) {
				/* We have found the point before which the new number
				   should be inserted. */
				break;
			}

			tail += 2;
		} else {
			/* it's a literal */
			if (*tail == number) {
				/* This number is already present - we don't need to do
				   anything. */
				return 0;
			}

			if (*tail > number) {
				/* We have found the point before which the new number
				   should be inserted. */
				break;
			}

			tail++;
		}
	}

	/* At this point, `tail' points to a position in the set which represents
	   a value greater than `new'; or it is at `end'. In the interest of
	   avoiding massive duplication of code, simply insert a literal here and
	   then run the optimizer.
	   */
	int mid = (tail - head); 

	if (m_data_size <= m_length + 1) {
		int endo = end - head;
		if (!Grow()) {
			return NS_ERROR_OUT_OF_MEMORY;
		}
		head = m_data;
		end = head + endo;
	}

	if (tail == end) {
		/* at the end */
		/* Add a literal to the end. */
		m_data[m_length++] = number;
	} else {
		/* need to insert (or edit) in the middle */
		PRInt32 i;
		for (i = size; i > mid; i--) {
			m_data[i] = m_data[i-1];
		}
		m_data[i] = number;
		m_length++;
	}

	Optimize();
	return 1;
}



int
nsMsgKeySet::Remove(PRInt32 number)
{
	PRInt32 size;
	PRInt32 *head;
	PRInt32 *tail;
	PRInt32 *end;
#ifdef DEBUG_MSGKEYSET
    printf("remove %d\n",number);
#endif

	size = m_length;
	head = m_data;
	tail = head;
	end = head + size;

	// **** I am not sure this is a right thing to comment the following
	// statements out. The reason for this is due to the implementation of
	// offline save draft and template. We use faked UIDs (negative ids) for
	// offline draft and template in order to distinguish them from real
	// UID. David I need your help here. **** jt

	// PR_ASSERT(number >= 0);
	// if (number < 0) {
	//	return -1;
	/// }

	/* We're going to modify the set, so invalidate the cache. */
	m_cached_value = -1;

	while (tail < end) {
		PRInt32 mid = (tail - m_data);

		if (*tail < 0) {
			/* it's a range */
			PRInt32 from = tail[1];
			PRInt32 to = from + (-(tail[0]));

			if (number < from || number > to) {
				/* Not this range */
				tail += 2;
				continue;
			}

			if (to == from + 1) {
				/* If this is a range [N - N+1] and we are removing M
				   (which must be either N or N+1) replace it with a
				   literal. This reduces the length by 1. */
				m_data[mid] = (number == from ? to : from);
				while (++mid < m_length) {
					m_data[mid] = m_data[mid+1];
				}
				m_length--;
				Optimize();
				return 1;
			} else if (to == from + 2) {
				/* If this is a range [N - N+2] and we are removing M,
				   replace it with the literals L,M (that is, either
				   (N, N+1), (N, N+2), or (N+1, N+2). The overall
				   length remains the same. */
				m_data[mid] = from;
				m_data[mid+1] = to;
				if (from == number) {
					m_data[mid] = from+1;
				} else if (to == number) {
					m_data[mid+1] = to-1;
				}
				Optimize();
				return 1;
			} else if (from == number) {
				/* This number is at the beginning of a long range (meaning a
				   range which will still be long enough to remain a range.)
				   Increase start and reduce length of the range. */
				m_data[mid]++;
				m_data[mid+1]++;
				Optimize();
				return 1;
			} else if (to == number) {
				/* This number is at the end of a long range (meaning a range
				   which will still be long enough to remain a range.)
				   Just decrease the length of the range. */
				m_data[mid]++;
				Optimize();
				return 1;
			} else {
				/* The number being deleted is in the middle of a range which
				   must be split. This increases overall length by 2.
				   */
				PRInt32 i;
				int endo = end - head;
				if (m_data_size - m_length <= 2) {
					if (!Grow()) return NS_ERROR_OUT_OF_MEMORY;
				}
				head = m_data;
				end = head + endo;

				for (i = m_length + 2; i > mid + 2; i--) {
					m_data[i] = m_data[i-2];
				}

				m_data[mid] = (- (number - from - 1));
				m_data[mid+1] = from;
				m_data[mid+2] = (- (to - number - 1));
				m_data[mid+3] = number + 1;
				m_length += 2;

				/* Oops, if we've ended up with a range with a 0 length,
				   which is illegal, convert it to a literal, which reduces
				   the overall length by 1. */
				if (m_data[mid] == 0) {
					/* first range */
					m_data[mid] = m_data[mid+1];
					for (i = mid + 1; i < m_length; i++) {
						m_data[i] = m_data[i+1];
					}
					m_length--;
				}
				if (m_data[mid+2] == 0) {
					/* second range */
					m_data[mid+2] = m_data[mid+3];
					for (i = mid + 3; i < m_length; i++) {
						m_data[i] = m_data[i+1];
					}
					m_length--;
				}
				Optimize();
				return 1;
			}
		} else {
			/* it's a literal */
			if (*tail != number) {
				/* Not this literal */
				tail++;
				continue;
			}

			/* Excise this literal. */
			m_length--;
			while (mid < m_length) {
				m_data[mid] = m_data[mid+1];
				mid++;
			}
			Optimize();
			return 1;
		}
	}

	/* It wasn't here at all. */
	return 0;
}


static PRInt32*
msg_emit_range(PRInt32* tmp, PRInt32 a, PRInt32 b)
{
	if (a == b) {
		*tmp++ = a;
	} else {
		NS_ASSERTION(a < b && a >= 0, "range is out of order");
		*tmp++ = -(b - a);
		*tmp++ = a;
	}
	return tmp;
}


int
nsMsgKeySet::AddRange(PRInt32 start, PRInt32 end)
{
	PRInt32 tmplength;
	PRInt32* tmp;
	PRInt32* in;
	PRInt32* out;
	PRInt32* tail;
	PRInt32 a;
	PRInt32 b;
	PRBool didit = PR_FALSE;

	/* We're going to modify the set, so invalidate the cache. */
	m_cached_value = -1;

	NS_ASSERTION(start <= end, "invalid range");
	if (start > end) return -1;

	if (start == end) {
		return Add(start);
	}

	tmplength = m_length + 2;
	tmp = (PRInt32*) PR_Malloc(sizeof(PRInt32) * tmplength);

	if (!tmp) return NS_ERROR_OUT_OF_MEMORY;

	in = m_data;
	out = tmp;
	tail = in + m_length;

#define EMIT(x, y) out = msg_emit_range(out, x, y)

	while (in < tail) {
		// Set [a,b] to be this range.
		if (*in < 0) {
			b = - *in++;
			a = *in++;
			b += a;
		} else {
			a = b = *in++;
		}

		if (a <= start && b >= end) {
			// We already have the entire range marked.
			PR_Free(tmp);
			return 0;
		}
		if (start > b + 1) {
			// No overlap yet.
			EMIT(a, b);
		} else if (end < a - 1) {
			// No overlap, and we passed it.
			EMIT(start, end);
			EMIT(a, b);
			didit = PR_TRUE;
			break;
		} else {
			// The ranges overlap.  Suck this range into our new range, and
			// keep looking for other ranges that might overlap.
			start = start < a ? start : a;
			end = end > b ? end : b;
		}
	}
	if (!didit) EMIT(start, end);
	while (in < tail) {
		*out++ = *in++;
	}

#undef EMIT

	PR_Free(m_data);
	m_data = tmp;
	m_length = out - tmp;
	m_data_size = tmplength;
#ifdef NEWSRC_DOES_HOST_STUFF
	if (m_host) m_host->MarkDirty();
#endif
	return 1;
}

PRInt32
nsMsgKeySet::CountMissingInRange(PRInt32 range_start, PRInt32 range_end)
{
	PRInt32 count;
	PRInt32 *head;
	PRInt32 *tail;
	PRInt32 *end;

	NS_ASSERTION (range_start >= 0 && range_end >= 0 && range_end >= range_start, "invalid range");
	if (range_start < 0 || range_end < 0 || range_end < range_start) return -1;

	head = m_data;
	tail = head;
	end = head + m_length;

	count = range_end - range_start + 1;

	while (tail < end) {
		if (*tail < 0) {
			/* it's a range */
			PRInt32 from = tail[1];
			PRInt32 to = from + (-(tail[0]));
			if (from < range_start) from = range_start;
			if (to > range_end) to = range_end;

			if (to >= from)
				count -= (to - from + 1);

			tail += 2;
		} else {
			/* it's a literal */
			if (*tail >= range_start && *tail <= range_end) count--;
			tail++;
		}
		NS_ASSERTION (count >= 0, "invalid count");
	}
	return count;
}


int 
nsMsgKeySet::FirstMissingRange(PRInt32 min, PRInt32 max,
								  PRInt32* first, PRInt32* last)
{
	PRInt32 size;
	PRInt32 *head;
	PRInt32 *tail;
	PRInt32 *end;
	PRInt32 from = 0;
	PRInt32 to = 0;
	PRInt32 a;
	PRInt32 b;

	NS_ASSERTION(first && last, "invalid parameter");
	if (!first || !last) return -1;

	*first = *last = 0;

	NS_ASSERTION(min <= max && min > 0, "invalid min or max param");
	if (min > max || min <= 0) return -1;

	size = m_length;
	head = m_data;
	tail = head;
	end = head + size;

	while (tail < end) {
		a = to + 1;
		if (*tail < 0) {			/* We got a range. */
			from = tail[1];
			to = from + (-(tail[0]));
			tail += 2;
		} else {
			from = to = tail[0];
			tail++;
		}
		b = from - 1;
		/* At this point, [a,b] is the range of unread articles just before
		   the current range of read articles [from,to].  See if this range
		   intersects the [min,max] range we were given. */
		if (a > max) return 0;	/* It's hopeless; there are none. */
		if (a <= b && b >= min) {
			/* Ah-hah!  We found an intersection. */
			*first = a > min ? a : min;
			*last = b < max ? b : max;
			return 0;
		}
	} 
	/* We found no holes in the newsrc that overlaps the range, nor did we hit
	   something read beyond the end of the range.  So, the great infinite
	   range of unread articles at the end of any newsrc line intersects the
	   range we want, and we just need to return that. */
	a = to + 1;
	*first = a > min ? a : min;
	*last = max;
	return 0;
}

// I'm guessing we didn't include this because we didn't think we're going
// to need it. I'm not so sure. I'm putting it in for now.
int 
nsMsgKeySet::LastMissingRange(PRInt32 min, PRInt32 max,
								  PRInt32* first, PRInt32* last)
{
  PRInt32 size;
  PRInt32 *head;
  PRInt32 *tail;
  PRInt32 *end;
  PRInt32 from = 0;
  PRInt32 to = 0;
  PRInt32 a;
  PRInt32 b;

  NS_ASSERTION(first && last, "invalid null param");
  if (!first || !last) return -1;

  *first = *last = 0;


  NS_ASSERTION(min <= max && min > 0, "invalid min or max");
  if (min > max || min <= 0) return -1;

  size = m_length;
  head = m_data;
  tail = head;
  end = head + size;

  while (tail < end) {
	a = to + 1;
	if (*tail < 0) {			/* We got a range. */
	  from = tail[1];
	  to = from + (-(tail[0]));
	  tail += 2;
	} else {
	  from = to = tail[0];
	  tail++;
	}
	b = from - 1;
	/* At this point, [a,b] is the range of unread articles just before
	   the current range of read articles [from,to].  See if this range
	   intersects the [min,max] range we were given. */
	if (a > max) return 0;	/* We're done.  If we found something, it's already
							   sitting in [*first,*last]. */
	if (a <= b && b >= min) {
	  /* Ah-hah!  We found an intersection. */
	  *first = a > min ? a : min;
	  *last = b < max ? b : max;
	  /* Continue on, looking for a later range. */
	}
  }
  if (to < max) {
	/* The great infinite range of unread articles at the end of any newsrc
	   line intersects the range we want, and we just need to return that. */
	a = to + 1;
	*first = a > min ? a : min;
	*last = max;
  }
  return 0;
}




#ifdef DEBUG /* A lot of test cases for the above */

#define countof(x) (sizeof(x) / sizeof(*(x)))

void
nsMsgKeySet::test_decoder (const char *string)
{
  nsMsgKeySet set(string /* , NULL */);
  char* tmp;
  set.Output(&tmp);
  printf ("\t\"%s\"\t--> \"%s\"\n", string, tmp);
  nsMemory::Free(tmp);
}


#define START(STRING) \
  string = STRING;	  \
  if (!(set = nsMsgKeySet::Create(string))) abort ()

#define FROB(N,PUSHP)									\
  i = N;												\
  if (!(NS_SUCCEEDED(set->Output(&s)))) abort ();					\
  printf ("%3lu: %-58s %c %3lu =\n", (unsigned long)set->m_length, s,	\
		  (PUSHP ? '+' : '-'), (unsigned long)i);						\
  nsMemory::Free(s);											\
  if (PUSHP												\
	  ? set->Add(i) < 0									\
	  : set->Remove(i) < 0)								\
	abort ();											\
  if (!(NS_SUCCEEDED(set->Output(&s)))) abort ();					\
  printf ("%3lu: %-58s optimized =\n", (unsigned long)set->m_length, s);	\
  nsMemory::Free(s);											\

#define END()							   \
  if (!(NS_SUCCEEDED(set->Output(&s)))) abort ();					\
  printf ("%3lu: %s\n\n", (unsigned long)set->m_length, s); \
  nsMemory::Free(s);											\
  delete set;							   \



void
nsMsgKeySet::test_adder (void)
{
  char *string;
  nsMsgKeySet *set;
  char *s;
  PRInt32 i;

  START("0-70,72-99,105,107,110-111,117-200");

  FROB(205, PR_TRUE);
  FROB(206, PR_TRUE);
  FROB(207, PR_TRUE);
  FROB(208, PR_TRUE);
  FROB(208, PR_TRUE);
  FROB(109, PR_TRUE);
  FROB(72, PR_TRUE);

  FROB(205, PR_FALSE);
  FROB(206, PR_FALSE);
  FROB(207, PR_FALSE);
  FROB(208, PR_FALSE);
  FROB(208, PR_FALSE);
  FROB(109, PR_FALSE);
  FROB(72, PR_FALSE);

  FROB(72, PR_TRUE);
  FROB(109, PR_TRUE);
  FROB(208, PR_TRUE);
  FROB(208, PR_TRUE);
  FROB(207, PR_TRUE);
  FROB(206, PR_TRUE);
  FROB(205, PR_TRUE);

  FROB(205, PR_FALSE);
  FROB(206, PR_FALSE);
  FROB(207, PR_FALSE);
  FROB(208, PR_FALSE);
  FROB(208, PR_FALSE);
  FROB(109, PR_FALSE);
  FROB(72, PR_FALSE);

  FROB(100, PR_TRUE);
  FROB(101, PR_TRUE);
  FROB(102, PR_TRUE);
  FROB(103, PR_TRUE);
  FROB(106, PR_TRUE);
  FROB(104, PR_TRUE);
  FROB(109, PR_TRUE);
  FROB(108, PR_TRUE);
  END();

  START("1-6"); FROB(7, PR_FALSE); END();
  START("1-6"); FROB(6, PR_FALSE); END();
  START("1-6"); FROB(5, PR_FALSE); END();
  START("1-6"); FROB(4, PR_FALSE); END();
  START("1-6"); FROB(3, PR_FALSE); END();
  START("1-6"); FROB(2, PR_FALSE); END();
  START("1-6"); FROB(1, PR_FALSE); END();
  START("1-6"); FROB(0, PR_FALSE); END();

  START("1-3"); FROB(1, PR_FALSE); END();
  START("1-3"); FROB(2, PR_FALSE); END();
  START("1-3"); FROB(3, PR_FALSE); END();

  START("1,3,5-7,9,10"); FROB(5, PR_FALSE); END();
  START("1,3,5-7,9,10"); FROB(6, PR_FALSE); END();
  START("1,3,5-7,9,10"); FROB(7, PR_FALSE); FROB(7, PR_TRUE); FROB(8, PR_TRUE);
  FROB (4, PR_TRUE); FROB (2, PR_FALSE); FROB (2, PR_TRUE);

  FROB (4, PR_FALSE); FROB (5, PR_FALSE); FROB (6, PR_FALSE); FROB (7, PR_FALSE);
  FROB (8, PR_FALSE); FROB (9, PR_FALSE); FROB (10, PR_FALSE); FROB (3, PR_FALSE);
  FROB (2, PR_FALSE); FROB (1, PR_FALSE); FROB (1, PR_FALSE); FROB (0, PR_FALSE);
  END();
}

#undef START
#undef FROB
#undef END



#define START(STRING) \
  string = STRING;	  \
  if (!(set = nsMsgKeySet::Create(string))) abort ()

#define FROB(N,M)												\
  i = N;														\
  j = M;														\
  if (!(NS_SUCCEEDED(set->Output(&s)))) abort ();					\
  printf ("%3lu: %-58s + %3lu-%3lu =\n", (unsigned long)set->m_length, s, (unsigned long)i, (unsigned long)j);	\
  nsMemory::Free(s);											\
  switch (set->AddRange(i, j)) {								\
  case 0:														\
	printf("(no-op)\n");										\
	break;														\
  case 1:														\
	break;														\
  default:														\
	abort();													\
  }																\
  if (!(NS_SUCCEEDED(set->Output(&s)))) abort ();					\
  printf ("%3lu: %-58s\n", (unsigned long)set->m_length, s);						\
  nsMemory::Free(s);											\


#define END()							   \
  if (!(NS_SUCCEEDED(set->Output(&s)))) abort ();					\
  printf ("%3lu: %s\n\n", (unsigned long)set->m_length, s); \
  nsMemory::Free(s);											\
  delete set;


void
nsMsgKeySet::test_ranges(void)
{
  char *string;
  nsMsgKeySet *set;
  char *s;
  PRInt32 i;
  PRInt32 j;

  START("20-40,72-99,105,107,110-111,117-200");

  FROB(205, 208);
  FROB(50, 70);
  FROB(0, 10);
  FROB(112, 113);
  FROB(101, 101);
  FROB(5, 75);
  FROB(103, 109);
  FROB(2, 20);
  FROB(1, 9999);

  END();


#undef START
#undef FROB
#undef END
}




#define TEST(N)									  \
  if (! with_cache) set->m_cached_value = -1;	  \
  if (!(NS_SUCCEEDED(set->Output(&s)))) abort ();					\
  printf (" %3d = %s\n", N,						  \
		  (set->IsMember(N) ? "true" : "false")); \
  delete [] s

void
nsMsgKeySet::test_member(PRBool with_cache)
{
  nsMsgKeySet *set;
  char *s;

  s = "1-70,72-99,105,107,110-111,117-200";
  printf ("\n\nTesting %s (with%s cache)\n", s, with_cache ? "" : "out");
  if (!(set = Create(s))) {
	abort ();
  }

  TEST(-1);
  TEST(0);
  TEST(1);
  TEST(20);
  
  delete set;
  s = "0-70,72-99,105,107,110-111,117-200";
  printf ("\n\nTesting %s (with%s cache)\n", s, with_cache ? "" : "out");
  if (!(set = Create(s))) {
	abort ();
  }
  
  TEST(-1);
  TEST(0);
  TEST(1);
  TEST(20);
  TEST(69);
  TEST(70);
  TEST(71);
  TEST(72);
  TEST(73);
  TEST(74);
  TEST(104);
  TEST(105);
  TEST(106);
  TEST(107);
  TEST(108);
  TEST(109);
  TEST(110);
  TEST(111);
  TEST(112);
  TEST(116);
  TEST(117);
  TEST(118);
  TEST(119);
  TEST(200);
  TEST(201);
  TEST(65535);

  delete set;
}

#undef TEST


// static void
// test_newsrc (char *file)
// {
//   FILE *fp = fopen (file, "r");
//   char buf [1024];
//   if (! fp) abort ();
//   while (fgets (buf, sizeof (buf), fp))
// 	{
// 	  if (!strncmp (buf, "options ", 8))
// 		fwrite (buf, 1, strlen (buf), stdout);
// 	  else
// 		{
// 		  char *sep = buf;
// 		  while (*sep != 0 && *sep != ':' && *sep != '!')
// 			sep++;
// 		  if (*sep) sep++;
// 		  while (nsCRT::IsAsciiSpace (*sep)) sep++;
// 		  fwrite (buf, 1, sep - buf, stdout);
// 		  if (*sep)
// 			{
// 			  char *s;
// 			  msg_NewsRCSet *set = msg_parse_newsrc_set (sep, &allocinfo);
// 			  if (! set)
// 				abort ();
// 			  if (! msg_OptimizeNewsRCSet (set))
// 				abort ();
// 			  if (! ((s = msg_format_newsrc_set (set))))
// 				abort ();
// 			  msg_free_newsrc_set (set, &allocinfo);
// 			  fwrite (s, 1, strlen (s), stdout);
// 			  free (s);
// 			  fwrite ("\n", 1, 1, stdout);
// 			}
// 		}
// 	}
//   fclose (fp);
// }

void
nsMsgKeySet::RunTests ()
{

  test_decoder ("");
  test_decoder (" ");
  test_decoder ("0");
  test_decoder ("1");
  test_decoder ("123");
  test_decoder (" 123 ");
  test_decoder (" 123 4");
  test_decoder (" 1,2, 3, 4");
  test_decoder ("0-70,72-99,100,101");
  test_decoder (" 0-70 , 72 - 99 ,100,101 ");
  test_decoder ("0 - 268435455");
  /* This one overflows - we can't help it.
	 test_decoder ("0 - 4294967295"); */

  test_adder ();

  test_ranges();

  test_member (PR_FALSE);
  test_member (PR_TRUE);

  // test_newsrc ("/u/montulli/.newsrc");
  /* test_newsrc ("/u/jwz/.newsrc");*/
}

#endif /* DEBUG */
