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

/*    addrutil.cpp --- parsing RFC822 addresses.
 */

#include "msg.h"
#include "msgprefs.h"

#undef FREEIF
#define FREEIF(obj) do { if (obj) { XP_FREE (obj); obj = 0; }} while (0)

extern "C"
{
	extern int MK_OUT_OF_MEMORY;
}

static int msg_quote_phrase_or_addr (char *address, int32 length,
									 XP_Bool addr_p);
static int msg_parse_rfc822_addresses (const char *line,
							char **names,
							char **addresses,
							XP_Bool quote_names_p,
							XP_Bool quote_addrs_p,
							XP_Bool first_only_p);


/* Given a string which contains a list of RFC822 addresses, parses it into
   their component names and mailboxes.

   The returned value is the number of addresses, or a negative error code;
   the names and addresses are returned into the provided pointers as
   consecutive null-terminated strings.  It is up to the caller to free them.
   Note that some of the strings may be zero-length.

   Either of the provided pointers may be NULL if the caller is not interested
   in those components.

   quote_names_p and quote_addrs_p control whether the returned strings should
   be quoted as RFC822 entities, or returned in a more human-presentable (but
   not necessarily parsable) form.

   If first_only_p is true, then only the first element of the list is
   returned; we don't bother parsing the rest.
 */
int
msg_parse_rfc822_addresses (const char *line,
							char **names,
							char **addresses,
							XP_Bool quote_names_p,
							XP_Bool quote_addrs_p,
							XP_Bool first_only_p)
{
  uint32 addr_count = 0;
  uint32 line_length;
  const char *line_end;
  const char *this_start;
  char *name_buf = 0, *name_out, *name_start;
  char *addr_buf = 0, *addr_out, *addr_start;
  XP_ASSERT (line);
  if (! line)
	return -1;
  if (names)
	*names = 0;
  if (addresses)
	*addresses = 0;
  line_length = XP_STRLEN (line);
  if (line_length == 0)
	return 0;

  name_buf = (char *) XP_ALLOC (line_length * 2 + 10);
  if (! name_buf)
	return MK_OUT_OF_MEMORY;

  addr_buf = (char *) XP_ALLOC (line_length * 2 + 10);
  if (! addr_buf)
	{
	  FREEIF (name_buf);
	  return MK_OUT_OF_MEMORY;
	}

  line_end = line;
  addr_out = addr_buf;
  name_out = name_buf;
  name_start = name_buf;
  addr_start = addr_buf;
  this_start = line;
  
  /* Skip over extra whitespace or commas before addresses. */
  while (*line_end &&
		 (XP_IS_SPACE (*line_end) || *line_end == ','))
	line_end++;

  while (*line_end)
	{
	  uint32 paren_depth = 0;
	  const char *oparen = 0;
	  const char *mailbox_start = 0;
	  const char *mailbox_end = 0;

  	  while (*line_end &&
			 !(*line_end == ',' &&
			   paren_depth <= 0 &&  /* comma is ok inside () */
			   (!mailbox_start || mailbox_end))) /* comma is ok inside <> */
		{
		  if (*line_end == '\\')
			{
			  line_end++;
			  if (!*line_end)	/* otherwise, we walk off end of line, right? */
				  break;
			}
		  else if (*line_end == '"') 
			{
			  int leave_quotes = 0;

			  line_end++;  /* remove open " */

			  /* handle '"John.Van Doe"@space.com' case */
			  if (paren_depth == 0 && !mailbox_start)
			    {
				  char *end_quote = strchr(line_end, '"');
				  char *mailbox   = end_quote ? strchr(end_quote, '<') : (char *)NULL,
					   *comma     = end_quote ? strchr(end_quote, ',') : (char *)NULL;
				  if (!mailbox || (comma && comma < mailbox))
				    {

					  leave_quotes = 1; /* no mailbox for this address */
					  *addr_out++ = '"';
				    }
			    }

			  while (*line_end)
				{
				  if (*line_end == '\\')
				  {
					if (   paren_depth == 0
					    && (*(line_end+1) == '\\' || *(line_end+1) == '"'))
						*addr_out++ = *line_end++;
					else
						line_end++;
				  }
				  else if (*line_end == '"')
					break;

				  if (paren_depth == 0)
					*addr_out++ = *line_end;

				  line_end++;
				}
			  if (*line_end) line_end++;  /* remove close " */
			  if (leave_quotes) *addr_out++ = '"';
			  continue;
			}

		  if (*line_end == '(')
			{
			  if (paren_depth == 0)
				oparen = line_end;
			  paren_depth++;
			}
		  else if (*line_end == '<' && paren_depth == 0)
			{
			  mailbox_start = line_end;
			}
		  else if (*line_end == '>' && mailbox_start && paren_depth == 0)
			{
			  mailbox_end = line_end;
			}
		  else if (*line_end == ')' && paren_depth > 0)
			{
			  paren_depth--;
			  if (paren_depth == 0)
				{
				  const char *s = oparen + 1;
				  /* Copy the characters inside the parens onto the
					 "name" buffer. */

				  /* Push out some whitespace before the paren, if
					 there is non-whitespace there already. */
				  if (name_out > name_start &&
					  !XP_IS_SPACE (name_out [-1]))
					*name_out++ = ' ';

				  /* Skip leading whitespace. */
				  while (XP_IS_SPACE (*s) && s < line_end)
					s++;

				  while (s < line_end)
					{
					  if (*s == '\"')
						{
						  /* Strip out " within () unless backslashed */
						  s++;
						  continue;
						}

					  if (*s == '\\')	/* remove one \ */
						s++;

					  if (XP_IS_SPACE (*s) &&
						  name_out > name_start &&
						  XP_IS_SPACE (name_out[-1]))
						/* collapse consecutive whitespace */;
					  else
						*name_out++ = *s;

					  s++;
					}
				  oparen = 0;
				}
			}
		  else
			{
			  /* If we're not inside parens or a <mailbox>, tack this
				 on to the end of the addr_buf. */
			  if (paren_depth == 0 &&
				  (!mailbox_start || mailbox_end))
				{
				  /* Eat whitespace at the beginning of the line,
					 and eat consecutive whitespace within the line. */
				  if (XP_IS_SPACE (*line_end) &&
					  (addr_out == addr_start ||
					   XP_IS_SPACE (addr_out[-1])))
					/* skip it */;
				  else
					*addr_out++ = *line_end;
				}
			}

		  line_end++;
		}

	  /* Now we have extracted a single address from the comma-separated
		 list of addresses.  The characters have been divided among the
		 various buffers: the parts inside parens have been placed in the
		 name_buf, and everything else has been placed in the addr_buf.
		 Quoted strings and backslashed characters have been `expanded.'

		 If there was a <mailbox> spec in it, we have remembered where it was.
		 Copy that on to the addr_buf, replacing what was there, and copy the
		 characters not inside <> onto the name_buf, replacing what is there
		 now (which was just the parenthesized parts.)  (And we need to do the
		 quote and backslash hacking again, since we're coming from the
		 original source.)

		 Otherwise, we're already done - the addr_buf and name_buf contain
		 the right data already (de-quoted.)
	   */
	  if (mailbox_end)
		{
		  const char *s;
		  XP_ASSERT (*mailbox_start == '<');
		  XP_ASSERT (*mailbox_end == '>');

		  /* First, copy the name.
		   */
		  name_out = name_start;
		  s = this_start;
		  /* Skip leading whitespace. */
		  while (XP_IS_SPACE (*s) && s < mailbox_start)
			s++;
		  /* Copy up to (not including) the < */
		  while (s < mailbox_start)
			{
			  if (*s == '\"')
				{
				  s++;
				  continue;
				}
			  if (*s == '\\')
			    {
				  if (s + 1 < mailbox_start && (*(s+1) == '\\' || *(s+1) == '\"'))
				    *name_out++ = *s++;
				  else
					s++;
				}
			  if (XP_IS_SPACE (*s) &&
				  name_out > name_start &&
				  XP_IS_SPACE (name_out[-1]))
				/* collapse consecutive whitespace */;
			  else
				*name_out++ = *s;
			  s++;
			}
		  /* Trim trailing whitespace. */
		  while (name_out > name_start && XP_IS_SPACE (name_out[-1]))
			name_out--;
		  /* Push out one space. */
		  *name_out++ = ' ';
		  s = mailbox_end+1;
		  /* Skip whitespace after > */
		  while (XP_IS_SPACE (*s) && s < line_end)
			s++;
		  /* Copy from just after > to the end. */
		  while (s < line_end)
			{
			  if (*s == '\"')
				{
				  s++;
				  continue;
				}
			  if (*s == '\\')
			    {
				  if (s + 1 < line_end && (*(s+1) == '\\' || *(s+1) == '\"'))
				    *name_out++ = *s++;
				  else
					s++;
				}
			  if (XP_IS_SPACE (*s) &&
				  name_out > name_start &&
				  XP_IS_SPACE (name_out[-1]))
				/* collapse consecutive whitespace */;
			  else
				*name_out++ = *s;
			  s++;
			}
		  /* Trim trailing whitespace. */
		  while (name_out > name_start && XP_IS_SPACE (name_out[-1]))
			name_out--;
		  /* null-terminate. */
		  *name_out++ = 0;

		  /* Now, copy the address.
		   */
		  mailbox_start++;
		  addr_out = addr_start;
		  s = mailbox_start;
		  /* Skip leading whitespace. */
		  while (XP_IS_SPACE (*s) && s < mailbox_end)
			s++;
		  /* Copy up to (not including) the > */
		  while (s < mailbox_end)
			{
			  if (*s == '\"')
				{
				  s++;
				  continue;
				}
			  if (*s == '\\')
			    {
				  if (s + 1 < mailbox_end && (*(s+1) == '\\' || *(s+1) == '\"'))
				    *addr_out++ = *s++;
				  else
					s++;
				}
			  *addr_out++ = *s++;
			}
		  /* Trim trailing whitespace. */
		  while (addr_out > addr_start && XP_IS_SPACE (addr_out[-1]))
			addr_out--;
		  /* null-terminate. */
		  *addr_out++ = 0;
		}
	  else  /* No component of <mailbox> form. */
		{
		  /* Trim trailing whitespace. */
		  while (addr_out > addr_start && XP_IS_SPACE (addr_out[-1]))
			addr_out--;
		  /* null-terminate. */
		  *addr_out++ = 0;

		  /* Trim trailing whitespace. */
		  while (name_out > name_start && XP_IS_SPACE (name_out[-1]))
			name_out--;
		  /* null-terminate. */
		  *name_out++ = 0;

		  /* Attempt to deal with the simple error case of a missing comma.
			 We can only really deal with this in the non-<> case.
			 If there is no name, and if the address doesn't contain
			 double-quotes, but the address does contain whitespace,
			 then assume that the whitespace is an address delimiter.
		   */
		  if (!name_start || !*name_start)
			{
			  char *s;
			  char *space = 0;
			  for (s = addr_start; s < addr_out; s++)
				if (*s == '\\')
				  s++;
				else if (!space && XP_IS_SPACE (*s))
				  space = s;
				else if (*s == '"')
				  {
					space = 0;
					break;
				  }
			  if (space)
				{
				  for (s = space; s < addr_out; s++)
					if (*s == '\\')
					  s++;
					else if (XP_IS_SPACE (*s))
					  {
						*s = 0;
						*name_out++ = 0;
						addr_count++;
					  }
				}
			}
		}

	  /* Now re-quote the names and addresses if necessary.
	   */
	  if (quote_names_p && names)
		{
		  int L = name_out - name_start - 1;
		  L = msg_quote_phrase_or_addr (name_start, L, FALSE);
		  name_out = name_start + L + 1;
		}

	  if (quote_addrs_p && addresses)
		{
		  int L = addr_out - addr_start - 1;
		  L = msg_quote_phrase_or_addr (addr_start, L, TRUE);
		  addr_out = addr_start + L + 1;
		}

	  addr_count++;

	  if (first_only_p)
		/* If we only want the first address, we can stop now. */
		break;

	  if (*line_end)
		line_end++;

	  /* Skip over extra whitespace or commas between addresses. */
	  while (*line_end &&
			 (XP_IS_SPACE (*line_end) || *line_end == ','))
		line_end++;

	  this_start = line_end;
	  name_start = name_out;
	  addr_start = addr_out;
	}

  /* Make one more pass through and convert all whitespace characters
	 to SPC.  We could do that in the first pass, but this is simpler. */
  {
	char *s;
	for (s = name_buf; s < name_out; s++)
	  if (XP_IS_SPACE (*s) && *s != ' ')
		*s = ' ';
	for (s = addr_buf; s < addr_out; s++)
	  if (XP_IS_SPACE (*s) && *s != ' ')
		*s = ' ';
  }

  /* #### Should we bother realloc'ing them smaller? */

  if (names)
	*names = name_buf;
  else
	XP_FREE (name_buf);

  if (addresses)
	*addresses = addr_buf;
  else
	XP_FREE (addr_buf);

  return addr_count;
}


extern "C" int
MSG_ParseRFC822Addresses (const char *line,
						  char **names,
						  char **addresses)
{
  return msg_parse_rfc822_addresses(line, names, addresses, TRUE, TRUE, FALSE);
}



/* Given a single mailbox, this quotes the characters in it which need
   to be quoted; it writes into `address' and returns a new length.
   `address' is assumed to be long enough; worst case, its size will
   be (N*2)+2.
 */
static int
msg_quote_phrase_or_addr (char *address, int32 length, XP_Bool addr_p)
{
    int quotable_count = 0, in_quote = 0;
    int unquotable_count = 0;
    int32 i, new_length;
    char *in, *out;
    XP_Bool atsign = FALSE;
	XP_Bool user_quote = FALSE;

    /* If the entire address is quoted, fall out now. */
    if (address[0] == '"' && address[length - 1] == '"')
	    return length;

    for (i = 0, in = address; i < length; i++, in++)
    {
        if (*in == 0)
            return length; /* #### horrible kludge... */

        else if (addr_p && *in == '@' && !atsign && !in_quote)
		{
            /* Exactly one unquoted at-sign is allowed in an address. */
            atsign = TRUE;

			/* If address is of the form '"userid"@somewhere.com' don't quote
			 * the quotes around 'userid'.  Also reset the quotable count, since
			 * any quotables we've seen are already inside quotes.
			 */
			if (address[0] == '"' && in > address + 2 && *(in - 1) == '"' && *(in - 2) != '\\')
				unquotable_count -= 2, quotable_count = 0, user_quote = TRUE;
		}

        else if (*in == '\\')
        {
            if (i + 1 < length && (*(in + 1) == '\\' || *(in + 1) == '"'))
                /* If the next character is a backslash or quote, this backslash */
                /* is an escape backslash; ignore it and the next character.     */
                i++, in++;
            else
                /* If the name contains backslashes or quotes, they must be escaped. */
                unquotable_count++;
        }

        else if (*in == '"')
            /* If the name contains quotes, they must be escaped. */
            unquotable_count++, in_quote = !in_quote;

        else if (   *in >= 127 || *in < 0
		         || *in == '[' || *in == ']' || *in == '(' || *in == ')'
                 || *in == '<' || *in == '>' || *in == '@' || *in == ','
                 || *in == ';' || *in == '$') 
            /* If the name contains control chars or RFC822 specials, it needs to
             * be enclosed in quotes.  Double-quotes and backslashes will be dealt
             * with seperately.
             *
             * The ":" character is explicitly not in this list, though RFC822 says
             * it should be quoted, because that has been seen to break VMS
             * systems.  (Rather, it has been seen that there are Unix SMTP servers
             * which accept RCPT TO:<host::user> but not RCPT TO:<"host::user"> or
             * RCPT TO:<host\:\:user>, which is the syntax that VMS/DECNET hosts
             * use.
             *
             * For future reference: it is also claimed that some VMS SMTP servers
             * allow \ quoting but not "" quoting; and that sendmail uses self-
             * contradcitory quoting conventions that violate both RFCs 821 and
             * 822, so any address quoting on a sendmail system will lose badly.
             */
            quotable_count++;

        else if (addr_p && *in == ' ')
            /* Naked spaces are allowed in names, but not addresses. */
            quotable_count++;

        else if (   !addr_p
		         && (*in == '.' || *in == '!' || *in == '$' || *in == '%'))
            /* Naked dots are allowed in addresses, but not in names.
             * The other characters (!$%) are technically allowed in names, but
             * are surely going to cause someone trouble, so we quote them anyway.
             */
            quotable_count++;
    }

    if (quotable_count == 0 && unquotable_count == 0)
        return length;

    /* Add 2 to the length for the quotes, plus one for each character
     * which will need a backslash as well.
     */
    new_length = length + unquotable_count + 2;

    /* Now walk through the string backwards (so that we can use the same
     * block.)  First put on the terminating quote, then push out each
     * character, backslashing as necessary.  Then a final quote.
     * Uh, except, put the second quote just before the last @ if there
     * is one.
     */
    out = address + new_length - 1;
    in = address + length - 1;
    if (!atsign || (user_quote && quotable_count > 0))
        *out-- = '"';
    while (out > address)
    {
        XP_ASSERT(in >= address);

		if (*in == '@' && user_quote && quotable_count > 0)
			*out-- = '"';

        *out-- = *in;

        if (*in == '@' && atsign && !user_quote)
        {
			*out-- = '"';
			atsign = FALSE;
        }
        else if (*in == '\\' || *in == '"')
        {
			if (   user_quote && *in == '"'
				&& (   in == address
				    || (   in < address + length - 1 && in > address
					    && *(in + 1) == '@' && *(in - 1) != '\\')))
				/* Do nothing */;
		    else if (in > address && *(in - 1) == '\\')
			    *out-- = *--in;
			else
			{
                XP_ASSERT(out > address);
                *out-- = '\\';
			}
        }
        in--;
    }
    XP_ASSERT(in == address - 1 || (user_quote && in == address));
    XP_ASSERT(out == address);
	*out = '"';
    address[new_length] = 0;
    return new_length;
}

/* Given a name or address that might have been quoted
 it will take out the escape and double quotes
 The caller is responsible for freeing the resulting
 string.
 */
extern "C" int
MSG_UnquotePhraseOrAddr (char *line, char** lineout)
{
	int outlen = 0;
	char *lineptr = NULL;
	char *tmpLine = NULL;
	char *outptr = NULL;
	int result = 0;

	(*lineout) = NULL;
	if (line) {
		/* if the first character isnt a double quote
		  then there is nothing to do */
		if (*line != '"')
		{
			(*lineout) = XP_STRDUP (line);
			if (!lineout)
				return -1;
			else
				return 0;
		}

		/* dont count the first character that is the double quote */
		lineptr = line;
		lineptr++;
		/* count up how many characters we are going to output */
		while (*lineptr) {
			/* if the character is an '\' then
			output the escaped character */
			if (*lineptr == '\\')
				lineptr++;
			outlen++;
			lineptr++;
		}
		tmpLine = (char *) XP_ALLOC (outlen + 1);
		if (!tmpLine)
			return -1;
		XP_MEMSET(tmpLine, 0, outlen);
		/* dont output the first double quote */
		line++;
		lineptr = line;
		outptr = (tmpLine);
		while ((*lineptr) != '\0') {
			/* if the character is an '\' then
			output the character that was escaped */
			/* if it was part of the quote then don't
			output it */
			if (*lineptr == '\\' || *lineptr == '"') {
				lineptr++;
			}
			*outptr = *lineptr;
			if (*lineptr != '\0') {
				outptr++;
				lineptr++;
			}
		}
		*outptr = '\0';
		if (tmpLine)
			(*lineout) = XP_STRDUP (tmpLine);
		else
			result = -1;
		XP_FREEIF (tmpLine);
	}
	return result;
}
/* Given a string which contains a list of RFC822 addresses, returns a
   comma-seperated list of just the `mailbox' portions.
 */
extern "C" char *
MSG_ExtractRFC822AddressMailboxes (const char *line)
{
  char *addrs = 0;
  char *result, *s, *out;
  uint32 i, size = 0;
  int status = MSG_ParseRFC822Addresses (line, 0, &addrs);
  if (status <= 0)
	return 0;

  s = addrs;
  for (i = 0; (int) i < status; i++)
	{
	  uint32 j = XP_STRLEN (s);
	  s += j + 1;
	  size += j + 2;
	}

  result = (char*)XP_ALLOC (size + 1);
  if (! result)
	{
	  XP_FREE (addrs);
	  return 0;
	}
  out = result;
  s = addrs;
  for (i = 0; (int) i < status; i++)
	{
	  uint32 j = XP_STRLEN (s);
	  XP_MEMCPY (out, s, j);
	  out += j;
	  if ((int) (i+1) < status)
		{
		  *out++ = ',';
		  *out++ = ' ';
		}
	  s += j + 1;
	}
  *out = 0;

  XP_FREE (addrs);
  return result;
}


/* Given a string which contains a list of RFC822 addresses, returns a
   comma-seperated list of just the `user name' portions.  If any of
   the addresses doesn't have a name, then the mailbox is used instead.

   The names are *unquoted* and therefore cannot be re-parsed in any way.
   They are, however, nice and human-readable.
 */
extern "C" char *
MSG_ExtractRFC822AddressNames (const char *line)
{
  char *names = 0;
  char *addrs = 0;
  char *result, *s1, *s2, *out;
  uint32 i, size = 0;
  int status = msg_parse_rfc822_addresses(line, &names, &addrs, FALSE, FALSE,
										  FALSE);
  if (status <= 0)
	return 0;

  s1 = names;
  s2 = addrs;
  for (i = 0; (int) i < status; i++)
	{
	  uint32 j1 = XP_STRLEN (s1);
	  uint32 j2 = XP_STRLEN (s2);
	  s1 += j1 + 1;
	  s2 += j2 + 1;
	  size += (j1 ? j1 : j2) + 2;
	}

  result = (char*)XP_ALLOC (size + 1);
  if (! result)
	{
	  XP_FREE (names);
	  XP_FREE (addrs);
	  return 0;
	}
  out = result;
  s1 = names;
  s2 = addrs;
  for (i = 0; (int) i < status; i++)
	{
	  uint32 j1 = XP_STRLEN (s1);
	  uint32 j2 = XP_STRLEN (s2);

	  if (j1)
		{
		  XP_MEMCPY (out, s1, j1);
		  out += j1;
		}
	  else
		{
		  XP_MEMCPY (out, s2, j2);
		  out += j2;
		}

	  if ((int) (i+1) < status)
		{
		  *out++ = ',';
		  *out++ = ' ';
		}
	  s1 += j1 + 1;
	  s2 += j2 + 1;
	}
  *out = 0;

  XP_FREE (names);
  XP_FREE (addrs);
  return result;
}

/* Like MSG_ExtractRFC822AddressNames(), but only returns the first name
   in the list, if there is more than one. 
 */
extern "C" char *
MSG_ExtractRFC822AddressName (const char *line)
{
  char *name = 0;
  char *addr = 0;
  int status = msg_parse_rfc822_addresses(line, &name, &addr, FALSE, FALSE,
										  TRUE);
  if (status <= 0)
	return 0;
  /* This can happen if there is an address like "From: foo bar" which
	 we parse as two addresses (that's a syntax error.)  In that case,
	 we'll return just the first one (the rest is after the NULL.)
  XP_ASSERT(status == 1);
   */
  if (name && *name)
	{
	  FREEIF(addr);
	  return name;
	}
  else
	{
	  FREEIF(name);
	  return addr;
	}
}


static char *
msg_format_rfc822_addresses (const char *names, const char *addrs,
							 int count, XP_Bool wrap_lines_p)
{
  char *result, *out;
  const char *s1, *s2;
  uint32 i, size = 0;
  uint32 column = 10;

  if (count <= 0)
	return 0;

  s1 = names;
  s2 = addrs;
  for (i = 0; (int) i < count; i++)
	{
	  uint32 j1 = XP_STRLEN (s1);
	  uint32 j2 = XP_STRLEN (s2);
	  s1 += j1 + 1;
	  s2 += j2 + 1;
	  size += j1 + j2 + 10;
	}

  result = (char *) XP_ALLOC (size + 1);
  if (! result) return 0;

  out = result;
  s1 = names;
  s2 = addrs;

  for (i = 0; (int) i < count; i++)
	{
	  char *o;
	  uint32 j1 = XP_STRLEN (s1);
	  uint32 j2 = XP_STRLEN (s2);

	  if (wrap_lines_p && i > 0 &&
		  (column + j1 + j2 + 3 +
		   (((int) (i+1) < count) ? 2 : 0)
		   > 76))
		{
		  if (out > result && out[-1] == ' ')
			out--;
		  *out++ = CR;
		  *out++ = LF;
		  *out++ = '\t';
		  column = 8;
		}

	  o = out;

	  if (j1)
		{
		  XP_MEMCPY (out, s1, j1);
		  out += j1;
		  *out++ = ' ';
		  *out++ = '<';
		}
	  XP_MEMCPY (out, s2, j2);
	  out += j2;
	  if (j1)
		*out++ = '>';

	  if ((int) (i+1) < count)
		{
		  *out++ = ',';
		  *out++ = ' ';
		}
	  s1 += j1 + 1;
	  s2 += j2 + 1;

	  column += (out - o);
	}
  *out = 0;
  return result;
}

/* Given a string which contains a list of RFC822 addresses, returns a new
   string with the same data, but inserts missing commas, parses and reformats
   it, and wraps long lines with newline-tab.
 */
extern "C" char *
MSG_ReformatRFC822Addresses (const char *line)
{
  char *names = 0;
  char *addrs = 0;
  char *result;
  int status = MSG_ParseRFC822Addresses (line, &names, &addrs);
  if (status <= 0)
	return 0;
  result = msg_format_rfc822_addresses (names, addrs, status, TRUE);
  XP_FREE (names);
  XP_FREE (addrs);
  return result;
}

/* Returns a copy of ADDRS which may have had some addresses removed.
   Addresses are removed if they are already in either ADDRS or OTHER_ADDRS.
   (If OTHER_ADDRS contain addresses which are not in ADDRS, they are not
   added.  That argument is for passing in addresses that were already
   mentioned in other header fields.)

   Addresses are considered to be the same if they contain the same mailbox
   part (case-insensitive.)  Real names and other comments are not compared.

   removeAliasesToMe allows the address parser to use the preference which
   contains regular expressions which also mean 'me' for the purpose of
   stripping the user's email address(es) out of addrs
 */
extern "C" char *
MSG_RemoveDuplicateAddresses (const char *addrs,
							  const char *other_addrs,
							  XP_Bool removeAliasesToMe)
{
  /* This is probably way more complicated than it should be... */
  char *s1 = 0, *s2 = 0;
  char *output = 0, *out = 0;
  char *result = 0;
  int count1 = 0, count2 = 0, count3 = 0;
  int size1 = 0, size2 = 0, size3 = 0;
  char *names1 = 0, *names2 = 0;
  char *addrs1 = 0, *addrs2 = 0;
  char **a_array1 = 0, **a_array2 = 0, **a_array3 = 0;
  char **n_array1 = 0,                 **n_array3 = 0;
  int i, j;

  if (!addrs) return 0;

  count1 = MSG_ParseRFC822Addresses (addrs, &names1, &addrs1);
  if (count1 < 0) goto FAIL;
  if (count1 == 0)
	{
	  result = XP_STRDUP("");
	  goto FAIL;
	}
  if (other_addrs)
	count2 = MSG_ParseRFC822Addresses (other_addrs, &names2, &addrs2);
  if (count2 < 0) goto FAIL;

  s1 = names1;
  s2 = addrs1;
  for (i = 0; i < count1; i++)
	{
	  uint32 j1 = XP_STRLEN (s1);
	  uint32 j2 = XP_STRLEN (s2);
	  s1 += j1 + 1;
	  s2 += j2 + 1;
	  size1 += j1 + j2 + 10;
	}

  s1 = names2;
  s2 = addrs2;
  for (i = 0; i < count2; i++)
	{
	  uint32 j1 = XP_STRLEN (s1);
	  uint32 j2 = XP_STRLEN (s2);
	  s1 += j1 + 1;
	  s2 += j2 + 1;
	  size2 += j1 + j2 + 10;
	}

  a_array1 = (char **) XP_ALLOC (count1 * sizeof(char *));
  if (!a_array1) goto FAIL;
  n_array1 = (char **) XP_ALLOC (count1 * sizeof(char *));
  if (!n_array1) goto FAIL;

  if (count2 > 0)
	{
	  a_array2 = (char **) XP_ALLOC (count2 * sizeof(char *));
	  if (!a_array2) goto FAIL;
	  /* don't need an n_array2 */
	}

  a_array3 = (char **) XP_ALLOC (count1 * sizeof(char *));
  if (!a_array3) goto FAIL;
  n_array3 = (char **) XP_ALLOC (count1 * sizeof(char *));
  if (!n_array3) goto FAIL;


  /* fill in the input arrays */
  s1 = names1;
  s2 = addrs1;
  for (i = 0; i < count1; i++)
	{
	  n_array1[i] = s1;
	  a_array1[i] = s2;
	  s1 += XP_STRLEN (s1) + 1;
	  s2 += XP_STRLEN (s2) + 1;
	}

  s2 = addrs2;
  for (i = 0; i < count2; i++)
	{
	  a_array2[i] = s2;
	  s2 += XP_STRLEN (s2) + 1;
	}

  /* Iterate over all addrs in the "1" arrays.
	 If those addrs are not present in "3" or "2", add them to "3".
   */
  for (i = 0; i < count1; i++)			/* iterate over all addrs */
	{
	  XP_Bool found = FALSE;
	  for (j = 0; j < count2; j++)
		if (!strcasecomp (a_array1[i], a_array2[j]))
		  {
			found = TRUE;
			break;
		  }

	  if (!found)
		for (j = 0; j < count3; j++)
		  if (!strcasecomp (a_array1[i], a_array3[j]))
			{
			  found = TRUE;
			  break;
			}

      if (!found && removeAliasesToMe)
	  {
		  found = MSG_Prefs::IsEmailAddressAnAliasForMe (a_array1[i]);
		  if (found)
			  break;
	  }

	  if (!found)
		{
		  n_array3[count3] = n_array1[i];
		  a_array3[count3] = a_array1[i];
		  size3 += (XP_STRLEN(n_array3[count3]) + XP_STRLEN(a_array3[count3])
					+ 10);
		  count3++;
		  XP_ASSERT (count3 <= count1);
		  if (count3 > count1) break;
		}
	}

  output = (char *) XP_ALLOC (size3 + 1);
  if (!output) goto FAIL;

  *output = 0;
  out = output;
  s2 = output;
  for (i = 0; i < count3; i++)
	{
	  XP_STRCPY (out, a_array3[i]);
	  out += XP_STRLEN (out);
	  *out++ = 0;
	}
  s1 = out;
  for (i = 0; i < count3; i++)
	{
	  XP_STRCPY (out, n_array3[i]);
	  out += XP_STRLEN (out);
	  *out++ = 0;
	}
  result = msg_format_rfc822_addresses (s1, s2, count3, FALSE);

 FAIL:
  FREEIF (a_array1);
  FREEIF (a_array2);
  FREEIF (a_array3);
  FREEIF (n_array1);
  FREEIF (n_array3);
  FREEIF (names1);
  FREEIF (names2);
  FREEIF (addrs1);
  FREEIF (addrs2);
  FREEIF (output);
  return result;
}


/* Given an e-mail address and a person's name, cons them together into a
   single string of the form "name <address>", doing all the necessary quoting.
   A new string is returned, which you must free when you're done with it.
 */
extern "C" char *
MSG_MakeFullAddress (const char* name, const char* addr)
{
  int nl = name ? XP_STRLEN (name) : 0;
  int al = addr ? XP_STRLEN (addr) : 0;
  char *buf, *s;
  int L;
  if (al == 0)
	return 0;
  buf = (char *) XP_ALLOC ((nl * 2) + (al * 2) + 20);
  if (!buf)
	return 0;
  if (nl > 0)
	{
	  XP_STRCPY (buf, name);
	  L = msg_quote_phrase_or_addr (buf, nl, FALSE);
	  s = buf + L;
	  *s++ = ' ';
	  *s++ = '<';
	}
  else
	{
	  s = buf;
	}

  XP_STRCPY (s, addr);
  L = msg_quote_phrase_or_addr (s, al, TRUE);
  s += L;
  if (nl > 0)
	*s++ = '>';
  *s = 0;
  L = (s - buf) + 1;
  buf = (char *) XP_REALLOC (buf, L);
  return buf;
}

#if 0
main (int argc, char **argv)
{
  fprintf (stderr, "%s\n",
		   MSG_RemoveDuplicateAddresses (argv[1], argv[2], FALSE));
}
#endif


#if 0
main (int argc, char **argv)
{
  fprintf (stderr, "%s\n", MSG_MakeFullAddress (argv[1], argv[2]));
}
#endif


#if 0
/* Test cases for the above routines.
 */
static void
test1 (const char *line, XP_Bool np, XP_Bool ap,
	   uint32 expect_count, const char *expect_n, const char *expect_a)
{
  char *names = 0, *addrs = 0;
  int result;
  if (! np) expect_n = 0;
  if (! ap) expect_a = 0;
  result = MSG_ParseRFC822Addresses (line,
									 (np ? &names : 0),
									 (ap ? &addrs : 0));
  if (result <= 0)
	printf (" #### error %d\n", result);
  else
	{
	  uint32 i;
	  char *n = names, *a = addrs;
	  if (expect_count != result)
		printf (" #### wrong number of results (%d instead of %d)\n",
				(int) result, (int) expect_count);
	  for (i = 0; i < result; i++)
		{
		  if (((!!n) != (!!expect_n)) ||
			  (n && XP_STRCMP (n, expect_n)))
			{
			  printf (" ####### name got: %s\n"
					  " #### name wanted: %s\n",
					  (n ? n : "<NULL>"),
					  (expect_n ? expect_n : "<NULL>"));
			}
		  if (((!!a) != (!!expect_a)) ||
			  (a && XP_STRCMP (a, expect_a)))
			{
			  printf (" ####### addr got: %s\n"
					  " #### addr wanted: %s\n",
					  (a ? a : "<NULL>"),
					  (expect_a ? expect_a : "<NULL>"));
			}
		  if (n) n += XP_STRLEN (n) + 1;
		  if (a) a += XP_STRLEN (a) + 1;
		  if (expect_n) expect_n += XP_STRLEN (expect_n) + 1;
		  if (expect_a) expect_a += XP_STRLEN (expect_a) + 1;
		}
	}
  FREEIF (names);
  FREEIF (addrs);
}

static void
test (const char *line, uint32 expect_n,
	  const char *expect_names, const char *expect_addrs,
	  const char *expect_all_names, const char *expect_all_addrs,
	  const char *canonical)
{
  char *s;
  printf ("testing %s\n", line);
  test1 (line, TRUE,  TRUE,  expect_n, expect_names, expect_addrs);
  test1 (line, TRUE,  FALSE, expect_n, expect_names, expect_addrs);
  test1 (line, FALSE, TRUE,  expect_n, expect_names, expect_addrs);
  test1 (line, FALSE, FALSE, expect_n, expect_names, expect_addrs);

  s = MSG_ExtractRFC822AddressMailboxes (line);
  if (!s || XP_STRCMP (s, expect_all_addrs))
	printf (" #### expected addrs: %s\n"
			" ######### got addrs: %s\n",
			expect_all_addrs, (s ? s : "<NULL>"));
  FREEIF (s);

  s = MSG_ExtractRFC822AddressNames (line);
  if (!s || XP_STRCMP (s, expect_all_names))
	printf (" #### expected names: %s\n"
			" ######### got names: %s\n",
			expect_all_names, (s ? s : "<NULL>"));
  FREEIF (s);

  s = MSG_ReformatRFC822Addresses (line);
  if (!s || XP_STRCMP (s, canonical))
	printf (" #### expected canonical: %s\n"
			" ######### got canonical: %s\n",
			canonical, (s ? s : "<NULL>"));
  FREEIF (s);
}


void
main ()
{
  test ("spanky",
		1, "", "spanky",
		"spanky", "spanky",
		"spanky");

  test ("<spanky>",
		1, "", "spanky",
		"spanky", "spanky",
		"spanky");

  test ("< spanky>  ",
		1, "", "spanky",
		"spanky", "spanky",
		"spanky");

  test ("Simple Case <simple1>",
		1,
		"Simple Case", "simple1",
		"Simple Case", "simple1",
		"Simple Case <simple1>");

  test ("   Simple    Case    <   simple1   >  ",
		1,
		"Simple Case", "simple1",
		"Simple Case", "simple1",
		"Simple Case <simple1>");

  test ("simple2 (Simple Case)",
		1,
		"Simple Case", "simple2",
		"Simple Case", "simple2",
		"Simple Case <simple2>");

  test ("simple3 (Slightly) (Trickier)",
		1,
		"Slightly Trickier", "simple3",
		"Slightly Trickier", "simple3",
		"Slightly Trickier <simple3>");

  test ("(Slightly) simple3 (Trickier)",
		1,
		"Slightly Trickier", "simple3",
		"Slightly Trickier", "simple3",
		"Slightly Trickier <simple3>");

  test ("(  Slightly  )   simple3   (  Trickier  )  ",
		1,
		"Slightly Trickier", "simple3",
		"Slightly Trickier", "simple3",
		"Slightly Trickier <simple3>");

  test ("(Even) more <trickier> (Trickier\\, I say)",
		1,
		"\"(Even) more (Trickier, I say)\"", "trickier",
		"(Even) more (Trickier, I say)",     "trickier",
		"\"(Even) more (Trickier, I say)\" <trickier>");

  test ("\"this, is\" <\"some loser\"@address> (foo)",
		1,
		"\"this, is (foo)\"", "\"some loser\"@address",
		"this, is (foo)", "\"some loser\"@address",
		"\"this, is (foo)\" <\"some loser\"@address>");

  test ("foo, bar",
		2,
		""    "\000" "",
		"foo" "\000" "bar",
		"foo, bar", "foo, bar",
		"foo, bar");

  test ("<foo>, <bar>",
		2,
		""    "\000" "",
		"foo" "\000" "bar",
		"foo, bar", "foo, bar",
		"foo, bar");

  test ("<  foo  >  , <  bar  >  ",
		2,
		""    "\000" "",
		"foo" "\000" "bar",
		"foo, bar", "foo, bar",
		"foo, bar");

  test ("<  foo  > , , , ,,,,, , <  bar  >  ,",
		2,
		""    "\000" "",
		"foo" "\000" "bar",
		"foo, bar", "foo, bar",
		"foo, bar");

  test ("\"this, is\" <\"some loser\"@address> (foo), <bar>",
		2,
		"\"this, is (foo)\""     "\000" "",
		"\"some loser\"@address" "\000" "bar",
		"this, is (foo), bar",
		"\"some loser\"@address, bar",
		"\"this, is (foo)\" <\"some loser\"@address>, bar");

  test ("\"this, is\" <some\\ loser@address> (foo), bar",
		2,
		"\"this, is (foo)\""     "\000" "",
		"\"some loser\"@address" "\000" "bar",
		"this, is (foo), bar",
		"\"some loser\"@address, bar",
		"\"this, is (foo)\" <\"some loser\"@address>, bar");

  test ("(I'm a (total) loser) \"space address\"",
		1,
		"\"I'm a (total) loser\"", "\"space address\"",
		"I'm a (total) loser", "\"space address\"",
		"\"I'm a (total) loser\" <\"space address\">");

  test ("(I'm a (total) loser) \"space address\"@host",
		1,
		"\"I'm a (total) loser\"", "\"space address\"@host",
		"I'm a (total) loser", "\"space address\"@host",
		"\"I'm a (total) loser\" <\"space address\"@host>");

  test ("It\\'s \"me\" <address>, I'm a (total) loser <\"space address\">",
		2,
		"It's me"  "\000" "\"I'm a (total) loser\"",
		"address"  "\000" "\"space address\"",
		"It's me, I'm a (total) loser",
		"address, \"space address\"",
		"It's me <address>, \"I'm a (total) loser\" <\"space address\">");

  test("It\\'s \"me\" <address>, I'm a (total) loser <\"space address\"@host>",
		2,
		"It's me"  "\000" "\"I'm a (total) loser\"",
		"address"  "\000" "\"space address\"@host",
		"It's me, I'm a (total) loser",
		"address, \"space address\"@host",
	   "It's me <address>, \"I'm a (total) loser\" <\"space address\"@host>");

  test ("(It\\'s \"me\") address, (I'm a (total) loser) \"space address\"",
		2,
		"It's me"  "\000" "\"I'm a (total) loser\"",
		"address"  "\000" "\"space address\"",
		"It's me, I'm a (total) loser",
		"address, \"space address\"",
		"It's me <address>, \"I'm a (total) loser\" <\"space address\">");

  test ("(It\\'s \"me\") address, (I'm a (total) loser) \"space \\\"address\"",
		2,
		"It's me"  "\000" "\"I'm a (total) loser\"",
		"address"  "\000" "\"space \\\"address\"",
		"It's me, I'm a (total) loser",
		"address, \"space \\\"address\"",
		"It's me <address>, \"I'm a (total) loser\" <\"space \\\"address\">");

  test ("(It\\'s \"me\") address, (I'm a (total) loser) \"space @address\"@host",
		2,
		"It's me"  "\000" "\"I'm a (total) loser\"",
		"address"  "\000" "\"space @address\"@host",
		"It's me, I'm a (total) loser",
		"address, \"space @address\"@host",
		"It's me <address>, \"I'm a (total) loser\" <\"space @address\"@host>");

  test ("Probably Bogus <some@loser@somewhere>",
		1,
		"Probably Bogus",
		"\"some@loser\"@somewhere",
		"Probably Bogus",
		"\"some@loser\"@somewhere",
		"Probably Bogus <\"some@loser\"@somewhere>");

  test ("Probably Bogus <\"some$loser,666\"@somewhere>",
		1,
		"Probably Bogus",
		"\"some$loser,666\"@somewhere",
		"Probably Bogus",
		"\"some$loser,666\"@somewhere",
		"Probably Bogus <\"some$loser,666\"@somewhere>");

  test ("Probably Bogus <\"some$loser,666\"@somewhere>",
		1,
		"Probably Bogus",
		"\"some$loser,666\"@somewhere",
		"Probably Bogus",
		"\"some$loser,666\"@somewhere",
		"Probably Bogus <\"some$loser,666\"@somewhere>");

  test ("\"Probably Bogus, Jr.\" <\"some$loser,666\"@somewhere>",
		1,
		"\"Probably Bogus, Jr.\"",
		"\"some$loser,666\"@somewhere",
		"Probably Bogus, Jr.",
		"\"some$loser,666\"@somewhere",
		"\"Probably Bogus, Jr.\" <\"some$loser,666\"@somewhere>");

  test ("Probably Bogus\\, Jr. <\"some$loser,666\"@somewhere>",
		1,
		"\"Probably Bogus, Jr.\"",
		"\"some$loser,666\"@somewhere",
		"Probably Bogus, Jr.",
		"\"some$loser,666\"@somewhere",
		"\"Probably Bogus, Jr.\" <\"some$loser,666\"@somewhere>");

  test ("This isn't legal <some$loser,666@somewhere>",
		1,
		"This isn't legal", "\"some$loser,666\"@somewhere",
		"This isn't legal", "\"some$loser,666\"@somewhere",
		"This isn't legal <\"some$loser,666\"@somewhere>");

  test ("This isn't legal!! <some$loser,666@somewhere>",
		1,
		"\"This isn't legal!!\"", "\"some$loser,666\"@somewhere",
		"This isn't legal!!", "\"some$loser,666\"@somewhere",
		"\"This isn't legal!!\" <\"some$loser,666\"@somewhere>");

  test ("addr1, addr2, \n\taddr3",
		3,
		""       "\000" ""      "\000" "",
		"addr1"  "\000" "addr2" "\000" "addr3",
		"addr1, addr2, addr3",
		"addr1, addr2, addr3",
		"addr1, addr2, addr3");

  test ("addr1 addr2 addr3",
		3,
		""       "\000" ""      "\000" "",
		"addr1"  "\000" "addr2" "\000" "addr3",
		"addr1, addr2, addr3",
		"addr1, addr2, addr3",
		"addr1, addr2, addr3");

  test ("   addr1     addr2    addr3    ,,,,,,    ",
		3,
		""       "\000" ""      "\000" "",
		"addr1"  "\000" "addr2" "\000" "addr3",
		"addr1, addr2, addr3",
		"addr1, addr2, addr3",
		"addr1, addr2, addr3");

  test ("addr1, addr2  \n\t addr3",
		3,
		""       "\000" ""      "\000" "",
		"addr1"  "\000" "addr2" "\000" "addr3",
		"addr1, addr2, addr3",
		"addr1, addr2, addr3",
		"addr1, addr2, addr3");

  test ("addr1, addr2, addr3 addr4, <addr5>, (and) addr6 (yeah)",
		6,
		""      "\000" ""      "\000" ""      "\000" ""      "\000" ""
		  "\000" "and yeah",
		"addr1" "\000" "addr2" "\000" "addr3" "\000" "addr4" "\000" "addr5"
		  "\000" "addr6",
		"addr1, addr2, addr3, addr4, addr5, and yeah",
		"addr1, addr2, addr3, addr4, addr5, addr6",
		"addr1, addr2, addr3, addr4, addr5, and yeah <addr6>");

  test ("addr1 (and some (nested) parens), addr2 <and unbalanced mbox",
		2,
		"\"and some (nested) parens\""  "\000" "",
		"addr1"                     "\000" "addr2",
		"and some (nested) parens, addr2",
		"addr1, addr2",
		"\"and some (nested) parens\" <addr1>, addr2");

  test ("addr1))) ((()))()()()()()()())), addr2 addr3, addr4 (foo, bar)",
		4,
		"\"(())\""        "\000" ""      "\000" ""      "\000" "\"foo, bar\"",
		"\"addr1))) ))\"" "\000" "addr2" "\000" "addr3" "\000" "addr4",
		"(()), addr2, addr3, foo, bar",
		"\"addr1))) ))\", addr2, addr3, addr4",
		"\"(())\" <\"addr1))) ))\">, addr2, addr3, \"foo, bar\" <addr4>");

  test ("avec le quoted quotes <\"a \\\" quote\">",
		1,
		"avec le quoted quotes", "\"a \\\" quote\"",
		"avec le quoted quotes", "\"a \\\" quote\"",
		"avec le quoted quotes <\"a \\\" quote\">");

  test ("avec le quoted quotes <\"a \\\" quote\"@host>",
		1,
		"avec le quoted quotes", "\"a \\\" quote\"@host",
		"avec le quoted quotes", "\"a \\\" quote\"@host",
		"avec le quoted quotes <\"a \\\" quote\"@host>");

  /* bang paths work, right? */
  test ("nelsonb <abit.com!nelsonb@netscape.com>",
		1,
		"nelsonb", "abit.com!nelsonb@netscape.com",
		"nelsonb", "abit.com!nelsonb@netscape.com",
		"nelsonb <abit.com!nelsonb@netscape.com>");

# if 0  /* these tests don't pass, but should. */

  /* a perverse example from RFC822: */
  test ("Muhammed.(I am  the greatest) Ali @(the)Vegas.WBA",
		1,
		"I am the greatest", "Muhammed.Ali@Vegas.WBA",
		"I am the greatest", "Muhammed.Ali@Vegas.WBA",
		"I am the greatest <Muhammed.Ali@Vegas.WBA>");

  /* Oops, this should work but doesn't. */
  test ("nelsonb <@abit.com.tw:nelsonb@netscape.com>",
		1,
		"nelsonb", "@abit.com.tw:nelsonb@netscape.com",
		"nelsonb", "@abit.com.tw:nelsonb@netscape.com",
		"nelsonb <@abit.com.tw:nelsonb@netscape.com>");

  test ("(Sat43Jan@cyberpromo.com) <Ronald.F.Guilmette#$&'*+-/=?^_`|~@monkeys.com> ((#$&'*+-/=?^_`|~)) ((\\)))",
		1,
		"(Sat43Jan@cyberpromo.com)", "\"Ronald.F.Guilmette#$&'*+-/=?^_`|~\"@monkeys.com",
		"(Sat43Jan@cyberpromo.com)", "\"Ronald.F.Guilmette#$&'*+-/=?^_`|~\"@monkeys.com",
		"(Sat43Jan@cyberpromo.com) <\"Ronald.F.Guilmette#$&'*+-/=?^_`|~\"@monkeys.com>");

  /* Intentionally fail this one */
  test("my . name @ my . host . com",
	   1,
	   "", "my.name@my.host.com",
	   "", "my.name@my.host.com",
	   "<my.name@my.host.com>");

  /* but this one should work */
  test("loser < my . name @ my . host . com > ",
	   1,
	   "loser", "my.name@my.host.com",
	   "loser", "my.name@my.host.com",
	   "loser <my.name@my.host.com>");

  test("my(@).(@)name(<)@(>)my(:).(;)host(((\\)))).(@)com",
	   1,
	   "@", "my.name@my.host.com",
	   "@", "my.name@my.host.com",
	   "\"@\" <my.name@my.host.com>");

# endif /* 0 */

  exit (0);
}
#endif /* 0 */
