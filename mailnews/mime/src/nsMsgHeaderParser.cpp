/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "msgCore.h"    // precompiled header...

#include "nsISupports.h"
#include "nsIMsgHeaderParser.h"
#include "nsMsgHeaderParser.h"	 
#include "libi18n.h"
#include "prmem.h"

// the following three functions are only here for test purposes.....because I18N stuff is not getting built
// yet!!!!
/**********************************************
int16 INTL_DefaultWinCharSetID(iDocumentContext context)
{
	return 0;
}

int INTL_CharLen(int charSetID, unsigned char *pstr) 
char * NextChar_UTF8(char *str); 
About the same functionality. The macros in nsMsgRFC822Parser.cpp need modification.     


char *INTL_Strstr(int16 charSetID, const char *s1, const char *s2) 
char * Strstr_UTF8(const char *s1, const char *s2); 
We need to converted to UTF-8 before using it. I think you can replace now and should 
continue to work for us-ascii. 
**************************************/
int INTL_CharLen(int charSetID, unsigned char *pstr)
{
	return 1;
}

char *INTL_Strstr(int16 charSetID, const char *s1, const char *s2)
{
	return NULL;
}

/*
 * Macros used throughout the RFC-822 parsing code.
 */

#undef FREEIF
#define FREEIF(obj) do { if (obj) { PR_Free (obj); obj = 0; }} while (0)

#define CS_APP_DEFAULT             INTL_DefaultWinCharSetID(NULL)

#define COPY_CHAR(_CSID,_D,_S)      do { if (!_S || !*_S) { *_D++ = 0; }\
                                         else { int _LEN = INTL_CharLen(_CSID,(unsigned char *)_S);\
                                                nsCRT::memcpy(_D,_S,_LEN); _D += _LEN; } } while (0)
#define NEXT_CHAR(_CSID,_STR)       (_STR += MAX(1,INTL_CharLen(_CSID,(unsigned char *)_STR)))
#define TRIM_WHITESPACE(_S,_E,_T)   do { while (_E > _S && IS_SPACE(_E[-1])) _E--;\
                                         *_E++ = _T; } while (0)

/*
 * The following are prototypes for the old "C" functions used to support all of the RFC-822 parsing code
 * We could have made these private functions of nsMsgHeaderParser if we wanted...
 */
static int msg_parse_Header_addresses(PRInt16 csid, const char *line, char **names, char **addresses,
                                      PRBool quote_names_p = PR_TRUE, PRBool quote_addrs_p = PR_TRUE,
                                      PRBool first_only_p = PR_FALSE);
static int msg_quote_phrase_or_addr(PRInt16 csid, char *address, PRInt32 length, PRBool addr_p);
static int msg_unquote_phrase_or_addr(PRInt16 csid, const char *line, char **lineout);
static char *msg_extract_Header_address_mailboxes(PRInt16 csid, const char *line);
static char *msg_extract_Header_address_names(PRInt16 csid, const char *line);
static char *msg_extract_Header_address_name(PRInt16 csid, const char *line);
static char *msg_format_Header_addresses(const char *names, const char *addrs, int count,
                                         PRBool wrap_lines_p);
static char *msg_reformat_Header_addresses(PRInt16 csid, const char *line);
static char *msg_remove_duplicate_addresses(PRInt16 csid, const char *addrs, const char *other_addrs,
                                            PRBool removeAliasesToMe);
static char *msg_make_full_address(PRInt16 csid, const char* name, const char* addr);


/*
 * nsMsgHeaderParser definitions....
 */

nsMsgHeaderParser::nsMsgHeaderParser()
{
  /* the following macro is used to initialize the ref counting data */
  NS_INIT_REFCNT();
}

nsMsgHeaderParser::~nsMsgHeaderParser()
{}

/* the following macros actually implement addref, release and query interface for our component. */
NS_IMPL_ADDREF(nsMsgHeaderParser)
NS_IMPL_RELEASE(nsMsgHeaderParser)
NS_IMPL_QUERY_INTERFACE(nsMsgHeaderParser, nsIMsgHeaderParser::GetIID()); /* we need to pass in the interface ID of this interface */

nsresult nsMsgHeaderParser::ParseHeaderAddresses (const char *charset, const char *line, char **names, char **addresses, PRUint32& numAddresses)
{
	numAddresses = msg_parse_Header_addresses(CS_APP_DEFAULT, line, names, addresses);
	return NS_OK;
}

nsresult nsMsgHeaderParser::ExtractHeaderAddressMailboxes (const char *charset, const char *line, char ** mailboxes)
{
	if (mailboxes)
	{
		*mailboxes = msg_extract_Header_address_mailboxes(CS_APP_DEFAULT, line);
		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;
}

nsresult nsMsgHeaderParser::ExtractHeaderAddressNames (const char *charset, const char *line, char ** names)
{
	if (names)
	{
		*names = msg_extract_Header_address_names(CS_APP_DEFAULT, line);
		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;
}


nsresult nsMsgHeaderParser::ExtractHeaderAddressName (const char *charset, const char *line, char ** name)
{
	if (name)
	{
		*name = msg_extract_Header_address_name(CS_APP_DEFAULT, line);
		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;
}

nsresult nsMsgHeaderParser::ReformatHeaderAddresses (const char *charset, const char *line, char ** reformattedAddress)
{
	if (reformattedAddress)
	{
		*reformattedAddress = msg_reformat_Header_addresses(CS_APP_DEFAULT, line);
		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;
}

nsresult nsMsgHeaderParser::RemoveDuplicateAddresses (const char *charset, const char *addrs, const char *other_addrs, PRBool removeAliasesToMe, char ** newOutput)
{
	if (newOutput)
	{
		*newOutput = msg_remove_duplicate_addresses(CS_APP_DEFAULT, addrs, other_addrs, removeAliasesToMe);
		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;
}

nsresult nsMsgHeaderParser::MakeFullAddress (const char *charset, const char* name, const char* addr, char ** fullAddress)
{
	if (fullAddress)
	{
		*fullAddress = msg_make_full_address(CS_APP_DEFAULT, name, addr);
		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;
}

nsresult nsMsgHeaderParser::UnquotePhraseOrAddr (const char *charset, const char *line, char** lineout)
{
	msg_unquote_phrase_or_addr(CS_APP_DEFAULT, line, lineout);
	return NS_OK;
}

 /* this function will be used by the factory to generate an RFC-822 Parser....*/
nsresult NS_NewHeaderParser(nsIMsgHeaderParser ** aInstancePtrResult)
{
	/* note this new macro for assertions...they can take a string describing the assertion */
	NS_PRECONDITION(nsnull != aInstancePtrResult, "nsnull ptr");
	if (nsnull != aInstancePtrResult)
	{
		nsMsgHeaderParser* parser = new nsMsgHeaderParser();
		if (parser)
			return parser->QueryInterface(nsIMsgHeaderParser::GetIID(), (void **)aInstancePtrResult);
		else
			return NS_ERROR_OUT_OF_MEMORY; /* we couldn't allocate the object */
	}
	else
		return NS_ERROR_NULL_POINTER; /* aInstancePtrResult was NULL....*/
}

 
/*
 * The remainder of this file is the actual parsing code and it was extracted verbatim from addrutils.cpp 
 */

/* msg_parse_Header_addresses
 *
 * Given a string which contains a list of Header addresses, parses it into
 * their component names and mailboxes.
 *
 * The returned value is the number of addresses, or a negative error code;
 * the names and addresses are returned into the provided pointers as
 * consecutive null-terminated strings.  It is up to the caller to free them.
 * Note that some of the strings may be zero-length.
 *
 * Either of the provided pointers may be NULL if the caller is not interested
 * in those components.
 *
 * quote_names_p and quote_addrs_p control whether the returned strings should
 * be quoted as Header entities, or returned in a more human-presentable (but
 * not necessarily parsable) form.
 *
 * If first_only_p is true, then only the first element of the list is
 * returned; we don't bother parsing the rest.
 */
static int msg_parse_Header_addresses (PRInt16 csid, const char *line, char **names, char **addresses,
								PRBool quote_names_p, PRBool quote_addrs_p, PRBool first_only_p)
{
	PRUint32 addr_count = 0;
	size_t line_length;
	const char *line_end;
	const char *this_start;
	char *name_buf = 0, *name_out, *name_start;
	char *addr_buf = 0, *addr_out, *addr_start;

	NS_ASSERTION(line, "");
 	if (!line)
		return -1;
	if (names)
		*names = 0;
	if (addresses)
		*addresses = 0;
	line_length = PL_strlen(line);
	if (line_length == 0)
		return 0;

	name_buf = (char *)PR_Malloc(line_length * 2 + 10);
	if (!name_buf)
		return NS_ERROR_OUT_OF_MEMORY;

	addr_buf = (char *)PR_Malloc(line_length * 2 + 10);
	if (!addr_buf)
	{
		FREEIF(name_buf);
		return NS_ERROR_OUT_OF_MEMORY;
	}

	line_end = line;
	addr_out = addr_buf;
	name_out = name_buf;
	name_start = name_buf;
	addr_start = addr_buf;
	this_start = line;
  
	/* Skip over extra whitespace or commas before addresses.
	 */
	while (*line_end && (IS_SPACE(*line_end) || *line_end == ','))
		NEXT_CHAR(csid, line_end);

	while (*line_end)
	{
		PRUint32 paren_depth = 0;
		const char *oparen = 0;
		const char *mailbox_start = 0;
		const char *mailbox_end = 0;

		while (   *line_end
		       && !(   *line_end == ',' && paren_depth <= 0 /* comma is ok inside () */
                    && (!mailbox_start || mailbox_end)))    /* comma is ok inside <> */
		{
			if (*line_end == '\\')
			{
				line_end++;
				if (!*line_end)	/* otherwise, we walk off end of line, right? */
					break;
			}
			else if (*line_end == '\"') 
			{
				int leave_quotes = 0;

				line_end++;  /* remove open " */

				/* handle '"John.Van Doe"@space.com' case */
				if (paren_depth == 0 && !mailbox_start)
				{
					char *end_quote = INTL_Strstr(csid, line_end, "\"");
					char *mailbox   = end_quote ? INTL_Strstr(csid, end_quote, "<") : (char *)NULL,
					     *comma     = end_quote ? INTL_Strstr(csid, end_quote, ",") : (char *)NULL;
					if (!mailbox || (comma && comma < mailbox))
					{
						leave_quotes = 1; /* no mailbox for this address */
						*addr_out++ = '\"';
					}
				}

				while (*line_end)
				{
					if (*line_end == '\\')
					{
						if (   paren_depth == 0
							&& (*(line_end+1) == '\\' || *(line_end+1) == '\"'))
							*addr_out++ = *line_end++;
						else
							line_end++;
					}
					else if (*line_end == '\"')
					{
						line_end++;  /* remove close " */
						break;
					}

					if (paren_depth == 0)
						COPY_CHAR(csid, addr_out, line_end);

					NEXT_CHAR(csid, line_end);
				}
				if (leave_quotes) *addr_out++ = '\"';
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

					/* Copy the chars inside the parens onto the "name" buffer.
					 */

					/* Push out some whitespace before the paren, if
					 * there is non-whitespace there already.
					 */
					if (name_out > name_start && !IS_SPACE(name_out [-1]))
						*name_out++ = ' ';

					/* Skip leading whitespace.
					 */
					while (IS_SPACE(*s) && s < line_end)
						s++;

					while (s < line_end)
					{
						/* Strip out " within () unless backslashed
						 */
						if (*s == '\"')
						{
							s++;
							continue;
						}

						if (*s == '\\')	/* remove one \ */
							s++;

						if (IS_SPACE(*s) && name_out > name_start && IS_SPACE(name_out[-1]))
							/* collapse consecutive whitespace */;
						else
							COPY_CHAR(csid, name_out, s);

						NEXT_CHAR(csid, s);
					}
					oparen = 0;
				}
			}
			else
			{
				/* If we're not inside parens or a <mailbox>, tack this
				 * on to the end of the addr_buf.
				 */
				if (paren_depth == 0 && (!mailbox_start || mailbox_end))
				{
					/* Eat whitespace at the beginning of the line,
					 * and eat consecutive whitespace within the line.
					 */
					if (   IS_SPACE(*line_end)
					    && (addr_out == addr_start || IS_SPACE(addr_out[-1])))
						/* skip it */;
					else
						COPY_CHAR(csid, addr_out, line_end);
				}
			}

		  	NEXT_CHAR(csid, line_end);
		}

		/* Now we have extracted a single address from the comma-separated
		 * list of addresses.  The characters have been divided among the
		 * various buffers: the parts inside parens have been placed in the
		 * name_buf, and everything else has been placed in the addr_buf.
		 * Quoted strings and backslashed characters have been `expanded.'
		 *
		 * If there was a <mailbox> spec in it, we have remembered where it was.
		 * Copy that on to the addr_buf, replacing what was there, and copy the
		 * characters not inside <> onto the name_buf, replacing what is there
		 * now (which was just the parenthesized parts.)  (And we need to do the
		 * quote and backslash hacking again, since we're coming from the
		 * original source.)
		 *
		 * Otherwise, we're already done - the addr_buf and name_buf contain
		 * the right data already (de-quoted.)
		 */
		if (mailbox_end)
		{
			const char *s;
			NS_ASSERTION(*mailbox_start == '<', "");
			NS_ASSERTION(*mailbox_end == '>', "");

			/* First, copy the name.
			 */
			name_out = name_start;
			s = this_start;

			/* Skip leading whitespace.
			 */
			while (IS_SPACE(*s) && s < mailbox_start)
				s++;

			/* Copy up to (not including) the <
			 */
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
				if (IS_SPACE(*s) && name_out > name_start && IS_SPACE(name_out[-1]))
					/* collapse consecutive whitespace */;
				else
					COPY_CHAR(csid, name_out, s);

				NEXT_CHAR(csid, s);
			}

			/* Push out one space.
			 */
			TRIM_WHITESPACE(name_start, name_out, ' ');
			s = mailbox_end + 1;

			/* Skip whitespace after >
			 */
			while (IS_SPACE(*s) && s < line_end)
				s++;

			/* Copy from just after > to the end.
			 */
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
				if (IS_SPACE (*s) && name_out > name_start && IS_SPACE (name_out[-1]))
					/* collapse consecutive whitespace */;
				else
					COPY_CHAR(csid, name_out, s);

				NEXT_CHAR(csid, s);
			}

			TRIM_WHITESPACE(name_start, name_out, 0);

			/* Now, copy the address.
			 */
			mailbox_start++;
			addr_out = addr_start;
			s = mailbox_start;

			/* Skip leading whitespace.
			 */
			while (IS_SPACE(*s) && s < mailbox_end)
				s++;

			/* Copy up to (not including) the >
			 */
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
				COPY_CHAR(csid, addr_out, s);
				NEXT_CHAR(csid, s);
			}

			TRIM_WHITESPACE(addr_start, addr_out, 0);
		}
		/* No component of <mailbox> form.
		 */
		else
		{
			TRIM_WHITESPACE(addr_start, addr_out, 0);
			TRIM_WHITESPACE(name_start, name_out, 0);

			/* Attempt to deal with the simple error case of a missing comma.
			 * We can only really deal with this in the non-<> case.
			 * If there is no name, and if the address doesn't contain
			 * double-quotes, but the address does contain whitespace,
			 * then assume that the whitespace is an address delimiter.
			 */
			if (!name_start || !*name_start)
			{
				char *s;
				char *space = 0;
				for (s = addr_start; s < addr_out; NEXT_CHAR(csid, s))
				{
					if (*s == '\\')
						s++;
					else if (!space && IS_SPACE(*s))
						space = s;
					else if (*s == '\"')
					{
						space = 0;
						break;
					}
				}
				if (space)
				{
					for (s = space; s < addr_out; NEXT_CHAR(csid, s))
					{
						if (*s == '\\')
							s++;
						else if (IS_SPACE(*s))
						{
							*s = 0;
							*name_out++ = 0;
							addr_count++;
						}
					}
				}
			}
		}

		/* Now re-quote the names and addresses if necessary.
		 */
		if (quote_names_p && names)
		{
			int L = name_out - name_start - 1;
			L = msg_quote_phrase_or_addr(csid, name_start, L, PR_FALSE);
			name_out = name_start + L + 1;
		}

		if (quote_addrs_p && addresses)
		{
			int L = addr_out - addr_start - 1;
			L = msg_quote_phrase_or_addr(csid, addr_start, L, PR_TRUE);
			addr_out = addr_start + L + 1;
		}

		addr_count++;

		/* If we only want the first address, we can stop now.
		 */
		if (first_only_p)
			break;

		if (*line_end)
			NEXT_CHAR(csid, line_end);

		/* Skip over extra whitespace or commas between addresses. */
		while (*line_end && (IS_SPACE(*line_end) || *line_end == ','))
			line_end++;

		this_start = line_end;
		name_start = name_out;
		addr_start = addr_out;
	}

	/* Make one more pass through and convert all whitespace characters
	 * to SPC.  We could do that in the first pass, but this is simpler.
	 */
	{
		char *s;
		for (s = name_buf; s < name_out; NEXT_CHAR(csid, s))
			if (IS_SPACE(*s) && *s != ' ')
				*s = ' ';
		for (s = addr_buf; s < addr_out; NEXT_CHAR(csid, s))
			if (IS_SPACE(*s) && *s != ' ')
				*s = ' ';
	}

	if (names)
		*names = name_buf;
	else
		PR_Free(name_buf);

	if (addresses)
		*addresses = addr_buf;
	else
		PR_Free(addr_buf);

	return addr_count;
}

/* msg_quote_phrase_or_addr
 *
 * Given a single mailbox, this quotes the characters in it which need
 * to be quoted; it writes into `address' and returns a new length.
 * `address' is assumed to be long enough; worst case, its size will
 * be (N*2)+2.
 */
static int
msg_quote_phrase_or_addr(PRInt16 csid, char *address, PRInt32 length, PRBool addr_p)
{
    int quotable_count = 0, in_quote = 0;
    int unquotable_count = 0;
    PRInt32 new_length, full_length = length;
    char *in, *out, *orig_out, *atsign = NULL, *orig_address = address;
	PRBool user_quote = PR_FALSE;
	PRBool quote_all = PR_FALSE;

    /* If the entire address is quoted, fall out now. */
    if (address[0] == '\"' && address[length - 1] == '\"')
	    return length;

	/* Check to see if there is a routing prefix.  If there is one, we can
	 * skip quoting it because by definition it can't need to be quoted.
	 */
	if (addr_p && *address && *address == '@')
	{
		for (in = address; *in; NEXT_CHAR(csid, in))
		{
			if (*in == ':')
			{
				length -= ++in - address;
				address = in;
				break;
			}
			else if (!IS_DIGIT(*in) && !IS_ALPHA(*in) && *in != '@' && *in != '.')
				break;
		}
	}

    for (in = address; in < address + length; NEXT_CHAR(csid, in))
    {
        if (*in == 0)
            return full_length; /* #### horrible kludge... */

        else if (*in == '@' && addr_p && !atsign && !in_quote)
		{
            /* Exactly one unquoted at-sign is allowed in an address. */
			if (atsign)
				quotable_count++;
            atsign = in;

			/* If address is of the form '"userid"@somewhere.com' don't quote
			 * the quotes around 'userid'.  Also reset the quotable count, since
			 * any quotables we've seen are already inside quotes.
			 */
			if (address[0] == '\"' && in > address + 2 && *(in - 1) == '\"' && *(in - 2) != '\\')
				unquotable_count -= 2, quotable_count = 0, user_quote = PR_TRUE;
		}

        else if (*in == '\\')
        {
            if (in + 1 < address + length && (*(in + 1) == '\\' || *(in + 1) == '\"'))
                /* If the next character is a backslash or quote, this backslash */
                /* is an escape backslash; ignore it and the next character.     */
                in++;
            else
                /* If the name contains backslashes or quotes, they must be escaped. */
                unquotable_count++;
        }

        else if (*in == '\"')
            /* If the name contains quotes, they must be escaped. */
            unquotable_count++, in_quote = !in_quote;

        else if (   *in >= 127 || *in < 0
		         || *in == ';' || *in == '$' || *in == '(' || *in == ')'
                 || *in == '<' || *in == '>' || *in == '@' || *in == ',') 
            /* If the name contains control chars or Header specials, it needs to
             * be enclosed in quotes.  Double-quotes and backslashes will be dealt
             * with seperately.
             *
             * The ":" character is explicitly not in this list, though Header says
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

		else if (!atsign && (*in == '[' || *in == ']'))
			/* Braces are normally special characters, except when they're
			 * used for domain literals (e.g. johndoe@[127.0.0.1].acme.com).
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
        return full_length;

	/* We must quote the entire string if there are quotables outside the user
	 * quote.
	 */
	if (!atsign || (user_quote && quotable_count > 0))
		quote_all = PR_TRUE, atsign = NULL;

    /* Add 2 to the length for the quotes, plus one for each character
     * which will need a backslash, plus one for a null terminator.
     */
    new_length = length + unquotable_count + 3;

    in = address;
    out = orig_out = (char *)PR_Malloc(new_length);
	if (!out)
	{
		*orig_address = 0;
		return 0;
	}

	/* Start off with a quote.
	 */
	*out++ = '\"';

	while (*in)
	{
		if (*in == '@')
		{
			if (atsign == in)
				*out++ = '\"';
			*out++ = *in++;
			continue;
		}
        else if (*in == '\"')
		{
 			if (!user_quote || (in != address && in != atsign - 1))
				*out++ = '\\';
			*out++ = *in++;
			continue;
		}
        else if (*in == '\\')
        {
		    if (*(in + 1) == '\\' || *(in + 1) == '\"')
			    *out++ = *in++;
			else
                *out++ = '\\';
			*out++ = *in++;
			continue;
        }
		else
			COPY_CHAR(csid, out, in);

		NEXT_CHAR(csid, in);
	}

	/* Add a final quote if we are quoting the entire string.
	 */
	if (quote_all)
		*out++ = '\"';
	*out++ = 0;

	NS_ASSERTION(new_length == (out - orig_out), "");
	nsCRT::memcpy(address, orig_out, new_length);
	PR_FREEIF(orig_out); /* make sure we release the string we allocated */

    return full_length + unquotable_count + 2;
}

/* msg_unquote_phrase_or_addr
 *
 * Given a name or address that might have been quoted
 * it will take out the escape and double quotes
 * The caller is responsible for freeing the resulting
 * string.
 */
static int
msg_unquote_phrase_or_addr(PRInt16 csid, const char *line, char **lineout)
{
	if (!line || !lineout)
		return 0;

	/* If the first character isn't a double quote, there is nothing to do
	 */
	if (*line != '\"')
	{
		*lineout = PL_strdup(line);
		if (!*lineout)
			return -1;
		else
			return 0;
	}
	else
		*lineout = NULL;

	/* Don't copy the first double quote
	 */
	*lineout = PL_strdup(line + 1);
	if (!*lineout)
		return -1;

	const char *lineptr = line + 1; 
	char *outptr = *lineout;
	while (*lineptr != '\0')
	{
		/* If the character is an '\' then output the character that was
		 * escaped.  If it was part of the quote then don't	output it.
		 */
		if (*lineptr == '\\' || *lineptr == '\"')
			lineptr++;
		if (*lineptr)
		{
			COPY_CHAR(csid, outptr, lineptr);
			NEXT_CHAR(csid, lineptr);
		}
	}
	*outptr = '\0';
	
	return 0;
}

/* msg_extract_Header_address_mailboxes
 *
 * Given a string which contains a list of Header addresses, returns a
 * comma-seperated list of just the `mailbox' portions.
 */
static char *
msg_extract_Header_address_mailboxes(PRInt16 csid, const char *line)
{
	char *addrs = 0;
	char *result, *s, *out;
	PRUint32 i, size = 0;
	int status = msg_parse_Header_addresses(csid, line, NULL, &addrs);
	if (status <= 0)
		return NULL;

	s = addrs;
	for (i = 0; (int) i < status; i++)
	{
		PRUint32 j = PL_strlen(s);
		s += j + 1;
		size += j + 2;
	}

	result = (char*)PR_Malloc(size + 1);
	if (!result)
	{
		PR_Free(addrs);
		return 0;
	}
	out = result;
	s = addrs;
	for (i = 0; (int)i < status; i++)
	{
		PRUint32 j = PL_strlen(s);
		nsCRT::memcpy(out, s, j);
		out += j;
		if ((int)(i+1) < status)
		{
			*out++ = ',';
			*out++ = ' ';
		}
		s += j + 1;
	}
	*out = 0;

	PR_Free(addrs);
	return result;
}


/* msg_extract_Header_address_names
 *
 * Given a string which contains a list of Header addresses, returns a
 * comma-seperated list of just the `user name' portions.  If any of
 * the addresses doesn't have a name, then the mailbox is used instead.
 *
 * The names are *unquoted* and therefore cannot be re-parsed in any way.
 * They are, however, nice and human-readable.
 */
static char *
msg_extract_Header_address_names(PRInt16 csid, const char *line)
{
	char *names = 0;
	char *addrs = 0;
	char *result, *s1, *s2, *out;
	PRUint32 i, size = 0;
	int status = msg_parse_Header_addresses(csid, line, &names, &addrs);
	if (status <= 0)
		return 0;

	s1 = names;
	s2 = addrs;
	for (i = 0; (int)i < status; i++)
	{
		PRUint32 j1 = PL_strlen(s1);
		PRUint32 j2 = PL_strlen(s2);
		s1 += j1 + 1;
		s2 += j2 + 1;
		size += (j1 ? j1 : j2) + 2;
	}

	result = (char *)PR_Malloc(size + 1);
	if (!result)
	{
		PR_Free(names);
		PR_Free(addrs);
		return 0;
	}

	out = result;
	s1 = names;
	s2 = addrs;
	for (i = 0; (int)i < status; i++)
	{
		PRUint32 j1 = PL_strlen(s1);
		PRUint32 j2 = PL_strlen(s2);

		if (j1)
		{
			nsCRT::memcpy(out, s1, j1);
			out += j1;
		}
		else
		{
			nsCRT::memcpy(out, s2, j2);
			out += j2;
		}

		if ((int)(i+1) < status)
		{
			*out++ = ',';
			*out++ = ' ';
		}
		s1 += j1 + 1;
		s2 += j2 + 1;
	}
	*out = 0;

	PR_Free(names);
	PR_Free(addrs);
	return result;
}

/* msg_extract_Header_address_name
 *
 * Like MSG_ExtractHeaderAddressNames(), but only returns the first name
 * in the list, if there is more than one. 
 */
static char *
msg_extract_Header_address_name(PRInt16 csid, const char *line)
{
	char *name = 0;
	char *addr = 0;
	int status = msg_parse_Header_addresses(csid, line, &name, &addr, PR_FALSE, PR_FALSE, PR_TRUE);
	if (status <= 0)
		return 0;

	/* This can happen if there is an address like "From: foo bar" which
	 * we parse as two addresses (that's a syntax error.)  In that case,
	 * we'll return just the first one (the rest is after the NULL.)
	 *
	 * NS_ASSERTION(status == 1);
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

/* msg_format_Header_addresses
 */
static char *
msg_format_Header_addresses (const char *names, const char *addrs,
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
	for (i = 0; (int)i < count; i++)
	{
		PRUint32 j1 = PL_strlen(s1);
		PRUint32 j2 = PL_strlen(s2);
		s1 += j1 + 1;
		s2 += j2 + 1;
		size += j1 + j2 + 10;
	}

	result = (char *)PR_Malloc(size + 1);
	if (!result) return 0;

	out = result;
	s1 = names;
	s2 = addrs;

	for (i = 0; (int)i < count; i++)
	{
		char *o;
		PRUint32 j1 = PL_strlen(s1);
		PRUint32 j2 = PL_strlen(s2);

		if (   wrap_lines_p && i > 0
		    && (column + j1 + j2 + 3 + (((int)(i+1) < count) ? 2 : 0) > 76))
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
			nsCRT::memcpy(out, s1, j1);
			out += j1;
			*out++ = ' ';
			*out++ = '<';
		}
		nsCRT::memcpy(out, s2, j2);
		out += j2;
		if (j1)
			*out++ = '>';

		if ((int)(i+1) < count)
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

/* msg_reformat_Header_addresses
 *
 * Given a string which contains a list of Header addresses, returns a new
 * string with the same data, but inserts missing commas, parses and reformats
 * it, and wraps long lines with newline-tab.
 */
static char *
msg_reformat_Header_addresses(PRInt16 csid, const char *line)
{
	char *names = 0;
	char *addrs = 0;
	char *result;
	int status = msg_parse_Header_addresses(csid, line, &names, &addrs);
	if (status <= 0)
		return 0;
	result = msg_format_Header_addresses(names, addrs, status, PR_TRUE);
	PR_Free (names);
	PR_Free (addrs);
	return result;
}

/* msg_remove_duplicate_addresses
 *
 * Returns a copy of ADDRS which may have had some addresses removed.
 * Addresses are removed if they are already in either ADDRS or OTHER_ADDRS.
 * (If OTHER_ADDRS contain addresses which are not in ADDRS, they are not
 * added.  That argument is for passing in addresses that were already
 * mentioned in other header fields.)
 *
 * Addresses are considered to be the same if they contain the same mailbox
 * part (case-insensitive.)  Real names and other comments are not compared.
 *
 * removeAliasesToMe allows the address parser to use the preference which
 * contains regular expressions which also mean 'me' for the purpose of
 * stripping the user's email address(es) out of addrs
 */
static char *
msg_remove_duplicate_addresses(PRInt16 csid, const char *addrs, const char *other_addrs,
                               PRBool removeAliasesToMe)
{
	if (!addrs) return 0;

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

	count1 = msg_parse_Header_addresses(csid, addrs, &names1, &addrs1);
	if (count1 < 0) goto FAIL;
	if (count1 == 0)
	{
		result = PL_strdup("");
		goto FAIL;
	}
	if (other_addrs)
		count2 = msg_parse_Header_addresses(csid, other_addrs, &names2, &addrs2);
	if (count2 < 0) goto FAIL;

	s1 = names1;
	s2 = addrs1;
	for (i = 0; i < count1; i++)
	{
		PRUint32 j1 = PL_strlen(s1);
		PRUint32 j2 = PL_strlen(s2);
		s1 += j1 + 1;
		s2 += j2 + 1;
		size1 += j1 + j2 + 10;
	}

	s1 = names2;
	s2 = addrs2;
	for (i = 0; i < count2; i++)
	{
		PRUint32 j1 = PL_strlen(s1);
		PRUint32 j2 = PL_strlen(s2);
		s1 += j1 + 1;
		s2 += j2 + 1;
		size2 += j1 + j2 + 10;
	}

	a_array1 = (char **)PR_Malloc(count1 * sizeof(char *));
	if (!a_array1) goto FAIL;
	n_array1 = (char **)PR_Malloc(count1 * sizeof(char *));
	if (!n_array1) goto FAIL;

	if (count2 > 0)
	{
		a_array2 = (char **)PR_Malloc(count2 * sizeof(char *));
		if (!a_array2) goto FAIL;
		/* don't need an n_array2 */
	}

	a_array3 = (char **)PR_Malloc(count1 * sizeof(char *));
	if (!a_array3) goto FAIL;
	n_array3 = (char **)PR_Malloc(count1 * sizeof(char *));
	if (!n_array3) goto FAIL;


	/* fill in the input arrays */
	s1 = names1;
	s2 = addrs1;
	for (i = 0; i < count1; i++)
	{
		n_array1[i] = s1;
		a_array1[i] = s2;
		s1 += PL_strlen(s1) + 1;
		s2 += PL_strlen(s2) + 1;
	}

	s2 = addrs2;
	for (i = 0; i < count2; i++)
	{
		a_array2[i] = s2;
		s2 += PL_strlen(s2) + 1;
	}

	/* Iterate over all addrs in the "1" arrays.
	 * If those addrs are not present in "3" or "2", add them to "3".
	 */
	for (i = 0; i < count1; i++)
	{
		PRBool found = PR_FALSE;
		for (j = 0; j < count2; j++)
			if (!PL_strcasecmp (a_array1[i], a_array2[j]))
			{
				found = PR_TRUE;
				break;
			}

		if (!found)
			for (j = 0; j < count3; j++)
				if (!PL_strcasecmp(a_array1[i], a_array3[j]))
				{
					found = PR_TRUE;
					break;
				}
/* HACK ALERT!!!! TEMPORARILY COMMENTING OUT UNTIL WE PORT MSG_PREFS INTO THE MOZILLA TREE!!!!!! */
#if 0
		if (!found && removeAliasesToMe)
		{
			found = MSG_Prefs::IsEmailAddressAnAliasForMe(a_array1[i]);
			if (found)
				break;
		}
#endif
		if (!found)
		{
			n_array3[count3] = n_array1[i];
			a_array3[count3] = a_array1[i];
			size3 += (PL_strlen(n_array3[count3]) + PL_strlen(a_array3[count3])	+ 10);
			count3++;
			NS_ASSERTION (count3 <= count1, "");
			if (count3 > count1) break;
		}
	}

	output = (char *)PR_Malloc(size3 + 1);
	if (!output) goto FAIL;

	*output = 0;
	out = output;
	s2 = output;
	for (i = 0; i < count3; i++)
	{
		PL_strcpy(out, a_array3[i]);
		out += PL_strlen(out);
		*out++ = 0;
	}
	s1 = out;
	for (i = 0; i < count3; i++)
	{
		PL_strcpy(out, n_array3[i]);
		out += PL_strlen(out);
		*out++ = 0;
	}
	result = msg_format_Header_addresses(s1, s2, count3, PR_FALSE);

 FAIL:
	FREEIF(a_array1);
	FREEIF(a_array2);
	FREEIF(a_array3);
	FREEIF(n_array1);
	FREEIF(n_array3);
	FREEIF(names1);
	FREEIF(names2);
	FREEIF(addrs1);
	FREEIF(addrs2);
	FREEIF(output);
	return result;
}


/* msg_make_full_address
 *
 * Given an e-mail address and a person's name, cons them together into a
 * single string of the form "name <address>", doing all the necessary quoting.
 * A new string is returned, which you must free when you're done with it.
 */
static char *
msg_make_full_address(PRInt16 csid, const char* name, const char* addr)
{
	int nl = name ? PL_strlen (name) : 0;
	int al = addr ? PL_strlen (addr) : 0;
	char *buf, *s;
	int L;
	if (al == 0)
		return 0;
	buf = (char *)PR_Malloc((nl * 2) + (al * 2) + 20);
	if (!buf)
		return 0;
	if (nl > 0)
	{
		PL_strcpy(buf, name);
		L = msg_quote_phrase_or_addr(csid, buf, nl, PR_FALSE);
		s = buf + L;
		*s++ = ' ';
		*s++ = '<';
	}
	else
	{
		s = buf;
	}

	PL_strcpy(s, addr);
	L = msg_quote_phrase_or_addr(csid, s, al, PR_TRUE);
	s += L;
	if (nl > 0)
		*s++ = '>';
	*s = 0;
	L = (s - buf) + 1;
	buf = (char *)PR_Realloc (buf, L);
	return buf;
}
