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

/* -*- Mode: C; tab-width: 4 -*-
   addr.c --- parsing RFC822 addresses.
 */

#include "oldmsg.h"
#include "plstr.h"
#include "prmem.h"

extern int MK_OUT_OF_MEMORY;


static int msg_quote_phrase_or_addr (char *address, PRInt32 length,
									 PRBool addr_p);
static int msg_parse_rfc822_addresses (const char *line,
							char **names,
							char **addresses,
							PRBool quote_names_p,
							PRBool quote_addrs_p,
							PRBool first_only_p);


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
							PRBool quote_names_p,
							PRBool quote_addrs_p,
							PRBool first_only_p)
{
  PRUint32 addr_count = 0;
  PRUint32 line_length;
  const char *line_end;
  const char *this_start;
  char *name_buf = 0, *name_out, *name_start;
  char *addr_buf = 0, *addr_out, *addr_start;
  PR_ASSERT (line);
  if (! line)
	return -1;
  if (names)
	*names = 0;
  if (addresses)
	*addresses = 0;
  line_length = PL_strlen (line);
  if (line_length == 0)
	return 0;

  name_buf = (char *) PR_Malloc (line_length * 2 + 10);
  if (! name_buf)
	return MK_OUT_OF_MEMORY;

  addr_buf = (char *) PR_Malloc (line_length * 2 + 10);
  if (! addr_buf)
	{
	  PR_FREEIF (name_buf);
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
	  PRUint32 paren_depth = 0;
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
			  if (!mailbox_start)
			    {
				  char *end_quote = strchr(line_end, '"');
				  char *mailbox   = end_quote ? strchr(end_quote, '<') : NULL,
					   *comma     = end_quote ? strchr(end_quote, ',') : NULL;
				  if (!mailbox || (comma && comma < mailbox))
				    {

					  leave_quotes = 1; /* no mailbox for this address */
					  *addr_out++ = '"';
				    }
			    }

			  while (*line_end && *line_end != '"')
				{
				  if (*line_end == '\\')
					line_end++;

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
		  PR_ASSERT (*mailbox_start == '<');
		  PR_ASSERT (*mailbox_end == '>');

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
				s++;
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
				s++;
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
				s++;
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
		  L = msg_quote_phrase_or_addr (name_start, L, PR_FALSE);
		  name_out = name_start + L + 1;
		}

	  if (quote_addrs_p && addresses)
		{
		  int L = addr_out - addr_start - 1;
		  L = msg_quote_phrase_or_addr (addr_start, L, PR_TRUE);
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
	PR_Free (name_buf);

  if (addresses)
	*addresses = addr_buf;
  else
	PR_Free (addr_buf);

  return addr_count;
}


int
MSG_ParseRFC822Addresses (const char *line,
						  char **names,
						  char **addresses)
{
  return msg_parse_rfc822_addresses(line, names, addresses, PR_TRUE, PR_TRUE, PR_FALSE);
}



/* Given a single mailbox, this quotes the characters in it which need
   to be quoted; it writes into `address' and returns a new length.
   `address' is assumed to be long enough; worst case, its size will
   be (N*2)+2.
 */
static int
msg_quote_phrase_or_addr (char *address, PRInt32 length, PRBool addr_p)
{
  int quotable_count = 0;
  int unquotable_count = 0;
  PRInt32 i, new_length;
  char *in, *out;
  PRBool atsign = PR_FALSE;

  /* If the entire address is quoted, fall out now. */
  if (address[0] == '"' && address[length - 1] == '"')
	  return length;

  for (i = 0, in = address; i < length; i++, in++)
	{
	  if (*in == 0)
		return length; /* #### horrible kludge... */

	  else if (*in == '@' && !atsign && addr_p)
		/* Exactly one unquoted at-sign is allowed in an address. */
		atsign = PR_TRUE;

	  else if (*in == '\\' || *in == '"')
		/* If the name contains backslashes or quotes, they must be escaped. */
		unquotable_count++;

	  else if (*in >= 127 || *in < 0 ||
			   *in == '[' || *in == ']' || *in == '(' || *in == ')' ||
			   *in == '<' || *in == '>' || *in == '@' || *in == ',' ||
			   *in == ';' || *in == '$') 
		/* If the name contains control chars or RFC822 specials, it needs to
		   be enclosed in quotes.  Double-quotes and backslashes will be dealt
		   with seperately.

		   The ":" character is explicitly not in this list, though RFC822 says
		   it should be quoted, because that has been seen to break VMS
		   systems.  (Rather, it has been seen that there are Unix SMTP servers
		   which accept RCPT TO:<host::user> but not RCPT TO:<"host::user"> or
		   RCPT TO:<host\:\:user>, which is the syntax that VMS/DECNET hosts
		   use.

		   For future reference: it is also claimed that some VMS SMTP servers
		   allow \ quoting but not "" quoting; and that sendmail uses self-
		   contradcitory quoting conventions that violate both RFCs 821 and
		   822, so any address quoting on a sendmail system will lose badly.
		 */
		quotable_count++;

	  else if (addr_p && *in == ' ')
		/* Naked spaces are allowed in names, but not addresses. */
		quotable_count++;

      else if (!addr_p &&
			   (*in == '.' || *in == '!' || *in == '$' || *in == '%'))
		/* Naked dots are allowed in addresses, but not in names.
		   The other characters (!$%) are technically allowed in names, but
		   are surely going to cause someone trouble, so we quote them anyway.
		 */
		quotable_count++;
	}

  if (quotable_count == 0 && unquotable_count == 0)
	return length;

  /* Add 2 to the length for the quotes, plus one for each character
	 which will need a backslash as well. */
  new_length = length + unquotable_count + 2;

  /* Now walk through the string backwards (so that we can use the same
	 block.)  First put on the terminating quote, then push out each
	 character, backslashing as necessary.  Then a final quote.
	 Uh, except, put the second quote just before the last @ if there
	 is one.
   */
  out = address + new_length - 1;
  in = address + length - 1;
  if (!atsign)
	*out-- = '"';
  while (out > address)
	{
	  PR_ASSERT(in >= address);
	  *out-- = *in;
	  if (atsign && *in == '@')
		{
		  *out-- = '"';
		  atsign = PR_FALSE;
		}
	  if (*in == '\\' || *in == '"')
		{
		  PR_ASSERT(out > address);
		  *out-- = '\\';
		}
	  in--;
	}
  PR_ASSERT(in == address - 1);
  PR_ASSERT(out == address);
  *out = '"';
  address[new_length] = 0;
  return new_length;
}

/* Given a name or address that might have been quoted
 it will take out the escape and double quotes
 The caller is responsible for freeing the resulting
 string.
 */
int
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
			(*lineout) = PL_strdup (line);
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
		tmpLine = (char *) PR_Malloc (outlen + 1);
		if (!tmpLine)
			return -1;
		memset(tmpLine, 0, outlen);
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
			(*lineout) = PL_strdup (tmpLine);
		else
			result = -1;
		PR_FREEIF (tmpLine);
	}
	return result;
}
/* Given a string which contains a list of RFC822 addresses, returns a
   comma-seperated list of just the `mailbox' portions.
 */
char *
MSG_ExtractRFC822AddressMailboxes (const char *line)
{
  char *addrs = 0;
  char *result, *s, *out;
  PRUint32 i, size = 0;
  int status = MSG_ParseRFC822Addresses (line, 0, &addrs);
  if (status <= 0)
	return 0;

  s = addrs;
  for (i = 0; (int) i < status; i++)
	{
	  PRUint32 j = PL_strlen (s);
	  s += j + 1;
	  size += j + 2;
	}

  result = (char*)PR_Malloc (size + 1);
  if (! result)
	{
	  PR_Free (addrs);
	  return 0;
	}
  out = result;
  s = addrs;
  for (i = 0; (int) i < status; i++)
	{
	  PRUint32 j = PL_strlen (s);
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

  PR_Free (addrs);
  return result;
}


/* Given a string which contains a list of RFC822 addresses, returns a
   comma-seperated list of just the `user name' portions.  If any of
   the addresses doesn't have a name, then the mailbox is used instead.

   The names are *unquoted* and therefore cannot be re-parsed in any way.
   They are, however, nice and human-readable.
 */
char *
MSG_ExtractRFC822AddressNames (const char *line)
{
  char *names = 0;
  char *addrs = 0;
  char *result, *s1, *s2, *out;
  PRUint32 i, size = 0;
  int status = msg_parse_rfc822_addresses(line, &names, &addrs, PR_FALSE, PR_FALSE,
										  PR_FALSE);
  if (status <= 0)
	return 0;

  s1 = names;
  s2 = addrs;
  for (i = 0; (int) i < status; i++)
	{
	  PRUint32 j1 = PL_strlen (s1);
	  PRUint32 j2 = PL_strlen (s2);
	  s1 += j1 + 1;
	  s2 += j2 + 1;
	  size += (j1 ? j1 : j2) + 2;
	}

  result = (char*)PR_Malloc (size + 1);
  if (! result)
	{
	  PR_Free (names);
	  PR_Free (addrs);
	  return 0;
	}
  out = result;
  s1 = names;
  s2 = addrs;
  for (i = 0; (int) i < status; i++)
	{
	  PRUint32 j1 = PL_strlen (s1);
	  PRUint32 j2 = PL_strlen (s2);

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

  PR_Free (names);
  PR_Free (addrs);
  return result;
}

/* Like MSG_ExtractRFC822AddressNames(), but only returns the first name
   in the list, if there is more than one. 
 */
char *
MSG_ExtractRFC822AddressName (const char *line)
{
  char *name = 0;
  char *addr = 0;
  int status = msg_parse_rfc822_addresses(line, &name, &addr, PR_FALSE, PR_FALSE,
										  PR_TRUE);
  if (status <= 0)
	return 0;
  /* This can happen if there is an address like "From: foo bar" which
	 we parse as two addresses (that's a syntax error.)  In that case,
	 we'll return just the first one (the rest is after the NULL.)
  PR_ASSERT(status == 1);
   */
  if (name && *name)
	{
	  PR_FREEIF(addr);
	  return name;
	}
  else
	{
	  PR_FREEIF(name);
	  return addr;
	}
}


static char *
msg_format_rfc822_addresses (const char *names, const char *addrs,
							 int count, PRBool wrap_lines_p)
{
  char *result, *out;
  const char *s1, *s2;
  PRUint32 i, size = 0;
  PRUint32 column = 10;

  if (count <= 0)
	return 0;

  s1 = names;
  s2 = addrs;
  for (i = 0; (int) i < count; i++)
	{
	  PRUint32 j1 = PL_strlen (s1);
	  PRUint32 j2 = PL_strlen (s2);
	  s1 += j1 + 1;
	  s2 += j2 + 1;
	  size += j1 + j2 + 10;
	}

  result = (char *) PR_Malloc (size + 1);
  if (! result) return 0;

  out = result;
  s1 = names;
  s2 = addrs;

  for (i = 0; (int) i < count; i++)
	{
	  char *o;
	  PRUint32 j1 = PL_strlen (s1);
	  PRUint32 j2 = PL_strlen (s2);

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
char *
MSG_ReformatRFC822Addresses (const char *line)
{
  char *names = 0;
  char *addrs = 0;
  char *result;
  int status = MSG_ParseRFC822Addresses (line, &names, &addrs);
  if (status <= 0)
	return 0;
  result = msg_format_rfc822_addresses (names, addrs, status, PR_TRUE);
  PR_Free (names);
  PR_Free (addrs);
  return result;
}

