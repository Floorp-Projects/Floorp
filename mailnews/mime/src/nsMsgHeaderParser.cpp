/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "msgCore.h"    // precompiled header...
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsMsgHeaderParser.h"
#include "nsISimpleEnumerator.h"   
#include "comi18n.h"
#include "prmem.h"

nsresult FillResultsArray(const char * aName, const char *aAddress, PRUnichar ** aOutgoingEmailAddress, PRUnichar ** aOutgoingName, 
                          PRUnichar ** aOutgoingFullName, nsIMsgHeaderParser *aParser);

/*
 * Macros used throughout the RFC-822 parsing code.
 */

#undef FREEIF
#define FREEIF(obj) do { if (obj) { PR_Free (obj); obj = 0; }} while (0)

#define COPY_CHAR(_D,_S)            do { if (!_S || !*_S) { *_D++ = 0; }\
                                         else { int _LEN = NextChar_UTF8((char *)_S) - _S;\
                                                memcpy(_D,_S,_LEN); _D += _LEN; } } while (0)
//#define NEXT_CHAR(_STR)             (_STR = (* (char *) _STR < 128) ? (char *) _STR + 1 : NextChar_UTF8((char *)_STR))
#define NEXT_CHAR(_STR)             (_STR = NextChar_UTF8((char *)_STR))
#define TRIM_WHITESPACE(_S,_E,_T)   do { while (_E > _S && nsCRT::IsAsciiSpace(_E[-1])) _E--;\
                                         *_E++ = _T; } while (0)

/*
 * The following are prototypes for the old "C" functions used to support all of the RFC-822 parsing code
 * We could have made these private functions of nsMsgHeaderParser if we wanted...
 */
static int msg_parse_Header_addresses(const char *line, char **names, char **addresses,
                                      PRBool quote_names_p = PR_TRUE, PRBool quote_addrs_p = PR_TRUE,
                                      PRBool first_only_p = PR_FALSE);
static int msg_quote_phrase_or_addr(char *address, PRInt32 length, PRBool addr_p);
static nsresult msg_unquote_phrase_or_addr(const char *line, PRBool strict, char **lineout);
static char *msg_extract_Header_address_mailboxes(const char *line);
static char *msg_extract_Header_address_names(const char *line);
static char *msg_extract_Header_address_name(const char *line);
#if 0
static char *msg_format_Header_addresses(const char *addrs, int count,
                                         PRBool wrap_lines_p);
#endif
static char *msg_reformat_Header_addresses(const char *line);

static char *msg_remove_duplicate_addresses(const char *addrs, const char *other_addrs,
                                            PRBool removeAliasesToMe);
static char *msg_make_full_address(const char* name, const char* addr);

/*
 * nsMsgHeaderParser definitions....
 */

nsMsgHeaderParser::nsMsgHeaderParser()
{
}

nsMsgHeaderParser::~nsMsgHeaderParser()
{
}

NS_IMPL_ISUPPORTS1(nsMsgHeaderParser, nsIMsgHeaderParser)

// helper function called by ParseHeadersWithArray
nsresult FillResultsArray(const char * aName, const char *aAddress, PRUnichar ** aOutgoingEmailAddress, PRUnichar ** aOutgoingName, 
                          PRUnichar ** aOutgoingFullName, nsIMsgHeaderParser *aParser)
{
  NS_ENSURE_ARG(aParser);
  nsresult rv = NS_OK;

  char * result = nsnull;
  if (aAddress)
  {
    result = MIME_DecodeMimeHeader(aAddress, NULL, PR_FALSE, PR_TRUE);
    *aOutgoingEmailAddress = ToNewUnicode(NS_ConvertUTF8toUCS2(result ? result : aAddress));
    PR_FREEIF(result);
  }
  
  if (aName)
  {
    result = MIME_DecodeMimeHeader(aName, NULL, PR_FALSE, PR_TRUE);
    *aOutgoingName = ToNewUnicode(NS_ConvertUTF8toUCS2(result ? result : aName));
    PR_FREEIF(result);
  }

  nsXPIDLCString fullAddress;
  nsXPIDLCString unquotedAddress;
  rv = aParser->MakeFullAddress("UTF-8", aName, aAddress, getter_Copies(fullAddress));
  if (NS_SUCCEEDED(rv) && (const char*)fullAddress)
  {
    result = MIME_DecodeMimeHeader(fullAddress, nsnull, PR_FALSE, PR_TRUE);
    if (result)
      fullAddress.Adopt(result);
    aParser->UnquotePhraseOrAddr(fullAddress, PR_TRUE, getter_Copies(unquotedAddress));
    if (!unquotedAddress.IsEmpty())
      fullAddress = unquotedAddress;
    *aOutgoingFullName = ToNewUnicode(NS_ConvertUTF8toUCS2(fullAddress.get()));
  }
  else
    *aOutgoingFullName = nsnull;

  return rv;
}

NS_IMETHODIMP nsMsgHeaderParser::ParseHeadersWithArray(const PRUnichar * aLine, PRUnichar *** aEmailAddresses,
                                                       PRUnichar *** aNames, PRUnichar *** aFullNames, PRUint32 * aNumAddresses)
{
  char * names = nsnull;
  char * addresses = nsnull;
  PRUint32 numAddresses = 0;
  nsresult rv = NS_OK;

  // need to convert unicode to UTF-8...
  nsAutoString tempString (aLine);
  char * utf8String = ToNewUTF8String(tempString);

  rv = ParseHeaderAddresses("UTF-8", utf8String, &names, &addresses, &numAddresses);
  nsCRT::free(utf8String);
  if (NS_SUCCEEDED(rv) && numAddresses)
  {
    // allocate space for our arrays....
    *aEmailAddresses = (PRUnichar **) PR_MALLOC(sizeof(PRUnichar *) * numAddresses);
    *aNames = (PRUnichar **) PR_MALLOC(sizeof(PRUnichar *) * numAddresses);
    *aFullNames = (PRUnichar **) PR_MALLOC(sizeof(PRUnichar *) * numAddresses);

    // for simplicities sake...
    PRUnichar ** outgoingEmailAddresses = *aEmailAddresses;
    PRUnichar ** outgoingNames = *aNames;
    PRUnichar ** outgoingFullNames = *aFullNames;

    // iterate over the results and fill in our arrays....
    PRUint32 index = 0; 
    const char * currentName = names;
    const char * currentAddress = addresses;
    char * unquotedName = nsnull;
    while (index < numAddresses)
    {
      if (NS_SUCCEEDED(UnquotePhraseOrAddr(currentName, PR_TRUE, &unquotedName)))
        rv = FillResultsArray(unquotedName, currentAddress, &outgoingEmailAddresses[index], &outgoingNames[index], &outgoingFullNames[index], this);
      else
        rv = FillResultsArray(currentName, currentAddress, &outgoingEmailAddresses[index], &outgoingNames[index], &outgoingFullNames[index], this);
      
      PR_FREEIF(unquotedName);
      currentName += strlen(currentName) + 1;
      currentAddress += strlen(currentAddress) + 1;
      index++;
    }
  }

  *aNumAddresses = numAddresses;
  PR_FREEIF(names);
  PR_FREEIF(addresses);
  return rv;
}

nsresult nsMsgHeaderParser::ParseHeaderAddresses (const char *charset, const char *line, char **names, char **addresses, PRUint32 *numAddresses)
{
  *numAddresses = msg_parse_Header_addresses(line, names, addresses);

  return NS_OK;
}

nsresult nsMsgHeaderParser::ExtractHeaderAddressMailboxes (const char *charset, const char *line, char ** mailboxes)
{
  if (mailboxes)
  {
    *mailboxes = msg_extract_Header_address_mailboxes(line);
    return NS_OK;
  }
  else
    return NS_ERROR_NULL_POINTER;
}

nsresult nsMsgHeaderParser::ExtractHeaderAddressNames (const char *charset, const char *line, char ** names)
{
  if (names)
  {
    *names = msg_extract_Header_address_names(line);
    return NS_OK;
  }
  else
    return NS_ERROR_NULL_POINTER;
}


nsresult nsMsgHeaderParser::ExtractHeaderAddressName (const char *charset, const char *line, char ** name)
{
  if (name)
  {
    *name = msg_extract_Header_address_name(line);
    return NS_OK;
  }
  else
    return NS_ERROR_NULL_POINTER;
}

nsresult nsMsgHeaderParser::ReformatHeaderAddresses (const char *charset, const char *line, char ** reformattedAddress)
{
  if (reformattedAddress)
  {
    *reformattedAddress = msg_reformat_Header_addresses(line);
    return NS_OK;
  }
  else
    return NS_ERROR_NULL_POINTER;
}

nsresult nsMsgHeaderParser::RemoveDuplicateAddresses (const char *charset, const char *addrs, const char *other_addrs, PRBool removeAliasesToMe, char ** newOutput)
{
  if (newOutput)
  {
    *newOutput = msg_remove_duplicate_addresses(addrs, other_addrs, removeAliasesToMe);
    return NS_OK;
  }
  else
    return NS_ERROR_NULL_POINTER;
}

nsresult nsMsgHeaderParser::MakeFullAddress (const char *charset, const char* name, const char* addr, char ** fullAddress)
{
  NS_ENSURE_ARG_POINTER(fullAddress);
  
  *fullAddress = msg_make_full_address(name, addr);
  return NS_OK;
}

nsresult nsMsgHeaderParser::MakeFullAddressWString (const PRUnichar* name, const PRUnichar* addr, PRUnichar ** fullAddress)
{
  nsXPIDLCString utf8Str;
  nsresult rv = MakeFullAddress(nsnull, NS_ConvertUCS2toUTF8(name).get(), 
                                NS_ConvertUCS2toUTF8(addr).get(), getter_Copies(utf8Str));
  if (NS_SUCCEEDED(rv))
  {
    *fullAddress = ToNewUnicode(NS_ConvertUTF8toUCS2(utf8Str.get()));
    if (*fullAddress == nsnull)
      rv = NS_ERROR_OUT_OF_MEMORY;
  }

  return rv;
}

nsresult nsMsgHeaderParser::UnquotePhraseOrAddr (const char *line, PRBool preserveIntegrity, char** result)
{
  NS_ENSURE_ARG_POINTER(result);
  return msg_unquote_phrase_or_addr(line, preserveIntegrity, result);
}

nsresult nsMsgHeaderParser::UnquotePhraseOrAddrWString (const PRUnichar *line, PRBool preserveIntegrity, PRUnichar ** result)
{
  nsXPIDLCString utf8Str;
  nsresult rv = msg_unquote_phrase_or_addr(NS_ConvertUCS2toUTF8(line).get(), preserveIntegrity, getter_Copies(utf8Str));
  if (NS_SUCCEEDED(rv))
  {
    *result = ToNewUnicode(NS_ConvertUTF8toUCS2(utf8Str.get()));
    if (*result == nsnull)
      rv = NS_ERROR_OUT_OF_MEMORY;
  }

  return rv;
}

nsresult nsMsgHeaderParser::ReformatUnquotedAddresses (const PRUnichar *line, PRUnichar ** result)
{
  NS_ENSURE_ARG_POINTER(result);
  *result = nsnull;

  NS_ConvertUCS2toUTF8 convertedLine(line);

  PRInt32 lineLen = convertedLine.Length();
  PRInt32 outputMaxLen = (lineLen * sizeof (char) * 2) + 2; //Let's presume we need to escape every char
  char * outputStr = (char *) PR_Malloc(outputMaxLen);
  if (!outputStr)
    return NS_ERROR_OUT_OF_MEMORY;

  const char *readPtr = convertedLine.get();
  char *writePtr = outputStr;
  const char *endOutputPtr = outputStr + outputMaxLen;
  const char *endPtr = readPtr + lineLen;
 
  const char *startRecipient = readPtr;
  char* reformated;
  PRBool openQuoteLevel1 = PR_FALSE;
  PRBool openQuoteLevel2 = PR_FALSE;

  while (readPtr <= endPtr && writePtr < endOutputPtr)
  {
    if (*readPtr == '\\')
    {
      if (*(readPtr + 1) == '"')
      {
        openQuoteLevel2 = !openQuoteLevel2;
        readPtr ++;
      }
    }
    else if (*readPtr == '"')
    {
      openQuoteLevel1 = !openQuoteLevel1;
      openQuoteLevel2 &= openQuoteLevel1;
    }
    else if (*readPtr == ',')
    {
      if (!openQuoteLevel1 && !openQuoteLevel2)
      {
        /* Ok, we found an address delimiter, temporary replace the delimiter with a \0
           and quote the full address
        */
        char * tempEnd = NS_CONST_CAST(char *, readPtr);
        *tempEnd = 0;
        
        reformated = msg_reformat_Header_addresses(startRecipient);
        if (reformated)
        {
          strncpy(writePtr, reformated, endOutputPtr - writePtr);
          writePtr += strlen(reformated);
          PR_Free(reformated);
        }
        else
        {
          strncpy(writePtr, startRecipient, endOutputPtr - writePtr);
          writePtr += strlen(startRecipient);
        }
        *writePtr = ',';
        writePtr ++;

        *tempEnd = ',';
        startRecipient = readPtr + 1;
      }
    }

    readPtr ++;
  }

  /* write off the last recipient */
  reformated = msg_reformat_Header_addresses(startRecipient);
  if (reformated)
  {
    strncpy(writePtr, reformated, endOutputPtr - writePtr);
    PR_Free(reformated);
  }
  else
    strncpy(writePtr, startRecipient, endOutputPtr - writePtr);

  *result = ToNewUnicode(NS_ConvertUTF8toUCS2(outputStr));
  PR_Free(outputStr);
  if (*result == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

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
      return parser->QueryInterface(NS_GET_IID(nsIMsgHeaderParser), (void **)aInstancePtrResult);
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
static int msg_parse_Header_addresses (const char *line, char **names, char **addresses,
                PRBool quote_names_p, PRBool quote_addrs_p, PRBool first_only_p)
{
  PRUint32 addr_count = 0;
  size_t line_length;
  const char *line_end;
  const char *this_start;
  char *name_buf = 0, *name_out, *name_start;
  char *addr_buf = 0, *addr_out, *addr_start;

  if (names)
    *names = 0;
  if (addresses)
    *addresses = 0;
  NS_ASSERTION(line, "");
  if (!line)
    return -1;
  line_length = strlen(line);
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
  while (*line_end && (nsCRT::IsAsciiSpace(*line_end) || *line_end == ','))
    NEXT_CHAR(line_end);

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
        if (!*line_end) /* otherwise, we walk off end of line, right? */
          break;
      }
      else if (*line_end == '\"') 
      {
        int leave_quotes = 0;

        line_end++;  /* remove open " */

        /* handle '"John.Van Doe"@space.com' case */
        if (paren_depth == 0 && !mailbox_start)
        {
          const char *end_quote;
          /* search for the closing quote but ignored escaped quote \" */
          for (end_quote = line_end;; end_quote++)
          {
            end_quote = PL_strstr(end_quote, "\"");
            if (!end_quote || *(end_quote - 1) != '\\')
              break;
          }
          const char *mailbox = end_quote ? PL_strstr(end_quote, "<") : (char *)NULL;
          const char *comma = end_quote ? PL_strstr(end_quote, ",") : (char *)NULL;
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
            line_end++;
            if (paren_depth == 0 && (*line_end == '\\' || *line_end == '\"'))
              *addr_out++ = *line_end++;
            continue;
          }
          else if (*line_end == '\"')
          {
            line_end++;  /* remove close " */
            break;
          }

          if (paren_depth == 0)
            COPY_CHAR(addr_out, line_end);

          NEXT_CHAR(line_end);
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
          if (name_out > name_start && !nsCRT::IsAsciiSpace(name_out [-1]))
            *name_out++ = ' ';

          /* Skip leading whitespace.
           */
          while (nsCRT::IsAsciiSpace(*s) && s < line_end)
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

            if (*s == '\\') /* remove one \ */
              s++;

            if (nsCRT::IsAsciiSpace(*s) && name_out > name_start && nsCRT::IsAsciiSpace(name_out[-1]))
              /* collapse consecutive whitespace */;
            else
              COPY_CHAR(name_out, s);

            NEXT_CHAR(s);
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
          if (   nsCRT::IsAsciiSpace(*line_end)
              && (addr_out == addr_start || nsCRT::IsAsciiSpace(addr_out[-1])))
            /* skip it */;
          else
            COPY_CHAR(addr_out, line_end);
        }
      }

        NEXT_CHAR(line_end);
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
      while (nsCRT::IsAsciiSpace(*s) && s < mailbox_start)
        s++;

      /* Copy up to (not including) the <
       */
      while (s < mailbox_start)
      {
        if (*s == '\"' && !quote_names_p)
        {
          s++;
          continue;
        }
        if (*s == '\\' && !quote_names_p)
        {
          s++;
          if (s < mailbox_start && (*s == '\\' || *s == '\"'))
            *name_out++ = *s++;
          continue;
        }
        if (nsCRT::IsAsciiSpace(*s) && name_out > name_start && nsCRT::IsAsciiSpace(name_out[-1]))
          /* collapse consecutive whitespace */;
        else
          COPY_CHAR(name_out, s);

        NEXT_CHAR(s);
      }

      /* Push out one space.
       */
      TRIM_WHITESPACE(name_start, name_out, ' ');
      s = mailbox_end + 1;

      /* Skip whitespace after >
       */
      while (nsCRT::IsAsciiSpace(*s) && s < line_end)
        s++;

      /* Copy from just after > to the end.
       */
      while (s < line_end)
      {
        if (*s == '\"' && !quote_names_p)
        {
          s++;
          continue;
        }
        if (*s == '\\' && !quote_names_p)
        {
          s++;
          if (s  < line_end && (*s == '\\' || *s == '\"'))
            *name_out++ = *s++;
          continue;
        }
        if (nsCRT::IsAsciiSpace (*s) && name_out > name_start && nsCRT::IsAsciiSpace (name_out[-1]))
          /* collapse consecutive whitespace */;
        else
          COPY_CHAR(name_out, s);

        NEXT_CHAR(s);
      }

      TRIM_WHITESPACE(name_start, name_out, 0);

      /* Now, copy the address.
       */
      mailbox_start++;
      addr_out = addr_start;
      s = mailbox_start;

      /* Skip leading whitespace.
       */
      while (nsCRT::IsAsciiSpace(*s) && s < mailbox_end)
        s++;

      /* Copy up to (not including) the >
       */
      while (s < mailbox_end)
      {
        if (*s == '\"' && !quote_addrs_p)
        {
          s++;
          continue;
        }
        if (*s == '\\' && !quote_addrs_p)
        {
          s++;
          if (s < mailbox_end && (*s == '\\' || *s == '\"'))
            *addr_out++ = *s++;
          continue;
        }
        COPY_CHAR(addr_out, s);
        NEXT_CHAR(s);
      }

      TRIM_WHITESPACE(addr_start, addr_out, 0);
    }
    /* No component of <mailbox> form.
     */
    else
    {
      TRIM_WHITESPACE(addr_start, addr_out, 0);
      TRIM_WHITESPACE(name_start, name_out, 0);
    }

    /* Now re-quote the names and addresses if necessary.
     */
#ifdef BUG11892
    // **** jefft - we don't want and shouldn't to requtoe the name, this
    // violate the RFC822 spec
    if (quote_names_p && names)
    {
      int L = name_out - name_start - 1;
      L = msg_quote_phrase_or_addr(name_start, L, PR_FALSE);
      name_out = name_start + L + 1;
    }
#endif 

    if (quote_addrs_p && addresses)
    {
      int L = addr_out - addr_start - 1;
      L = msg_quote_phrase_or_addr(addr_start, L, PR_TRUE);
      addr_out = addr_start + L + 1;
    }

    addr_count++;

    /* If we only want the first address, we can stop now.
     */
    if (first_only_p)
      break;

    if (*line_end)
      NEXT_CHAR(line_end);

    /* Skip over extra whitespace or commas between addresses. */
    while (*line_end && (nsCRT::IsAsciiSpace(*line_end) || *line_end == ','))
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
    for (s = name_buf; s < name_out; NEXT_CHAR(s))
      if (nsCRT::IsAsciiSpace(*s) && *s != ' ')
        *s = ' ';
    for (s = addr_buf; s < addr_out; NEXT_CHAR(s))
      if (nsCRT::IsAsciiSpace(*s) && *s != ' ')
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
msg_quote_phrase_or_addr(char *address, PRInt32 length, PRBool addr_p)
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
    for (in = address; *in; NEXT_CHAR(in))
    {
      if (*in == ':')
      {
        length -= ++in - address;
        address = in;
        break;
      }
      else if (!nsCRT::IsAsciiDigit(*in) && !nsCRT::IsAsciiAlpha(*in) && *in != '@' && *in != '.')
        break;
    }
  }

    for (in = address; in < address + length; NEXT_CHAR(in))
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

        else if (  /* *in >= 127 || *in < 0  ducarroz: 8bits characters will be mime encoded therefore they are not a problem
             ||*/ (*in == ';' && !addr_p) || *in == '$' || *in == '(' || *in == ')'
                 || *in == '<' || *in == '>' || *in == '@' || *in == ',') 
            /* If the name contains control chars or Header specials, it needs to
             * be enclosed in quotes.  Double-quotes and backslashes will be dealt
             * with separately.
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
             *
             * The ";" character in an address is a group delimiter, therefore it
             * should not be quoted in that case.
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
    new_length = length + quotable_count + unquotable_count + 3;

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
      COPY_CHAR(out, in);

    NEXT_CHAR(in);
  }

  /* Add a final quote if we are quoting the entire string.
   */
  if (quote_all)
    *out++ = '\"';
  *out++ = 0;

  NS_ASSERTION(new_length >= (out - orig_out), "");
  memcpy(address, orig_out, new_length);
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
static nsresult
msg_unquote_phrase_or_addr(const char *line, PRBool preserveIntegrity, char **lineout)
{
  if (!line || !lineout)
    return NS_OK;

  /* If the first character isn't a double quote, there is nothing to do
   */
  if (*line != '\"')
  {
    *lineout = nsCRT::strdup(line);
    if (!*lineout)
      return NS_ERROR_OUT_OF_MEMORY;
    else
      return NS_OK;
  }

  /* in preserveIntegrity mode, we must preserve the quote if the name contains a comma */
  if (preserveIntegrity)
  {
    const char * open_quote = nsnull;
    const char * comma = nsnull;;
    const char * at_sign = nsnull;
    const char * readPos = line + 1;

    while (*readPos)
    {
      if (*readPos == ',')
      {
        if (!open_quote)
          comma = readPos;
      }
      else if (*readPos == '@')
      {
        at_sign = readPos;
        break;
      }
      else if (*readPos == '"')
      {
        if (!open_quote)
          open_quote = readPos;
        else
          open_quote = nsnull;
      }

      readPos ++;
    }

    if (comma && at_sign)
    {
      *lineout = nsCRT::strdup(line);
      if (!*lineout)
        return NS_ERROR_OUT_OF_MEMORY;
      else
        return NS_OK;
    }
  }

  /* Don't copy the first double quote
   */
  *lineout = nsCRT::strdup(line + 1);
  if (!*lineout)
    return NS_ERROR_OUT_OF_MEMORY;

  const char *lineptr = line + 1; 
  char *outptr = *lineout;
  PRBool escaped = PR_FALSE;

  while (*lineptr)
  {
    /* If the character is an '\' then output the character that was
     * escaped.  If it was part of the quote then don't output it.
     */
    if (*lineptr == '\\')
    {
      escaped = PR_TRUE;
      lineptr++;
    }
    if (*lineptr == '\"' && !escaped)
      lineptr++;
    escaped = PR_FALSE;

    if (*lineptr)
    {
      COPY_CHAR(outptr, lineptr);
      NEXT_CHAR(lineptr);
    }
  }
  *outptr = '\0';
  
  return NS_OK;
}

/* msg_extract_Header_address_mailboxes
 *
 * Given a string which contains a list of Header addresses, returns a
 * comma-separated list of just the `mailbox' portions.
 */
static char *
msg_extract_Header_address_mailboxes(const char *line)
{
  char *addrs = 0;
  char *result, *s, *out;
  PRUint32 i, size = 0;
  int status = msg_parse_Header_addresses(line, NULL, &addrs);
  if (status <= 0)
    return NULL;

  s = addrs;
  for (i = 0; (int) i < status; i++)
  {
    PRUint32 j = strlen(s);
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
    PRUint32 j = strlen(s);
    memcpy(out, s, j);
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
 * comma-separated list of just the `user name' portions.  If any of
 * the addresses doesn't have a name, then the mailbox is used instead.
 *
 * The names are *unquoted* and therefore cannot be re-parsed in any way.
 * They are, however, nice and human-readable.
 */
static char *
msg_extract_Header_address_names(const char *line)
{
  char *names = 0;
  char *addrs = 0;
  char *result, *s1, *s2, *out;
  PRUint32 i, size = 0;
  int status = msg_parse_Header_addresses(line, &names, &addrs);
  if (status <= 0)
    return 0;
  PRUint32 len1, len2;

  s1 = names;
  s2 = addrs;
  for (i = 0; (int)i < status; i++)
  {
    len1 = strlen(s1);
    len2 = strlen(s2);
    s1 += len1 + 1;
    s2 += len2 + 1;
    size += (len1 ? len1 : len2) + 2;
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
    len1 = strlen(s1);
    len2 = strlen(s2);

    if (len1)
    {
      memcpy(out, s1, len1);
      out += len1;
    }
    else
    {
      memcpy(out, s2, len2);
      out += len2;
    }

    if ((int)(i+1) < status)
    {
      *out++ = ',';
      *out++ = ' ';
    }
    s1 += len1 + 1;
    s2 += len2 + 1;
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
msg_extract_Header_address_name(const char *line)
{
  char *name = 0;
  char *addr = 0;
  int status = msg_parse_Header_addresses(line, &name, &addr, PR_FALSE, PR_FALSE, PR_TRUE);
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
  PRUint32 len1, len2;
  PRUint32 name_maxlen = 0;
  PRUint32 addr_maxlen = 0;

  if (count <= 0)
    return 0;

  s1 = names;
  s2 = addrs;
  for (i = 0; (int)i < count; i++)
  {
    len1 = strlen(s1);
    len2 = strlen(s2);
    s1 += len1 + 1;
    s2 += len2 + 1;
    
    len1 = (len1 * 2) + 2;  //(N*2) + 2 in case we need to quote it
    len2 = (len2 * 2) + 2;
    name_maxlen = PR_MAX(name_maxlen, len1);
    addr_maxlen = PR_MAX(addr_maxlen, len2);
    size += len1 + len2 + 10;
  }

  result = (char *)PR_Malloc(size + 1);
  char *aName = (char *)PR_Malloc(name_maxlen + 1);
  char *anAddr = (char *)PR_Malloc(addr_maxlen + 1);
  if (!result || !aName || !anAddr)
  {
    PR_FREEIF(result);
    PR_FREEIF(aName);
    PR_FREEIF(anAddr);
    return 0;
  }

  out = result;
  s1 = names;
  s2 = addrs;

  for (i = 0; (int)i < count; i++)
  {
    char *o;
    
    PL_strncpy(aName, s1, name_maxlen);
    PL_strncpy(anAddr, s2, addr_maxlen);
    len1 = msg_quote_phrase_or_addr(aName, strlen(s1), PR_FALSE);
    len2 = msg_quote_phrase_or_addr(anAddr, strlen(s2), PR_TRUE);

    if (   wrap_lines_p && i > 0
        && (column + len1 + len2 + 3 + (((int)(i+1) < count) ? 2 : 0) > 76))
    {
      if (out > result && out[-1] == ' ')
        out--;
      *out++ = nsCRT::CR;
      *out++ = nsCRT::LF;
      *out++ = '\t';
      column = 8;
    }

    o = out;

    if (len1)
    {
      memcpy(out, aName, len1);
      out += len1;
      *out++ = ' ';
      *out++ = '<';
    }
    memcpy(out, anAddr, len2);
    out += len2;
    if (len1)
      *out++ = '>';

    if ((int)(i+1) < count)
    {
      *out++ = ',';
      *out++ = ' ';
    }
    s1 += strlen(s1) + 1;
    s2 += strlen(s2) + 1;

    column += (out - o);
  }
  *out = 0;
  
  PR_FREEIF(aName);
  PR_FREEIF(anAddr);
  
  return result;
}

/* msg_reformat_Header_addresses
 *
 * Given a string which contains a list of Header addresses, returns a new
 * string with the same data, but inserts missing commas, parses and reformats
 * it, and wraps long lines with newline-tab.
 */
static char *
msg_reformat_Header_addresses(const char *line)
{
  char *names = 0;
  char *addrs = 0;
  char *result;
  int status = msg_parse_Header_addresses(line, &names, &addrs);
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
msg_remove_duplicate_addresses(const char *addrs, const char *other_addrs,
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

  count1 = msg_parse_Header_addresses(addrs, &names1, &addrs1);
  if (count1 < 0) goto FAIL;
  if (count1 == 0)
  {
    result = nsCRT::strdup("");
    goto FAIL;
  }
  if (other_addrs)
    count2 = msg_parse_Header_addresses(other_addrs, &names2, &addrs2);
  if (count2 < 0) goto FAIL;

  s1 = names1;
  s2 = addrs1;
  for (i = 0; i < count1; i++)
  {
    PRUint32 len1 = strlen(s1);
    PRUint32 len2 = strlen(s2);
    s1 += len1 + 1;
    s2 += len2 + 1;
    size1 += len1 + len2 + 10;
  }

  s1 = names2;
  s2 = addrs2;
  for (i = 0; i < count2; i++)
  {
    PRUint32 len1 = strlen(s1);
    PRUint32 len2 = strlen(s2);
    s1 += len1 + 1;
    s2 += len2 + 1;
    size2 += len1 + len2 + 10;
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
    s1 += strlen(s1) + 1;
    s2 += strlen(s2) + 1;
  }

  s2 = addrs2;
  for (i = 0; i < count2; i++)
  {
    a_array2[i] = s2;
    s2 += strlen(s2) + 1;
  }

  /* Iterate over all addrs in the "1" arrays.
   * If those addrs are not present in "3" or "2", add them to "3".
   */
  for (i = 0; i < count1; i++)
  {
    PRBool found = PR_FALSE;
    for (j = 0; j < count2; j++)
      if (!nsCRT::strcasecmp (a_array1[i], a_array2[j]))
      {
        found = PR_TRUE;
        break;
      }

    if (!found)
      for (j = 0; j < count3; j++)
        if (!nsCRT::strcasecmp(a_array1[i], a_array3[j]))
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
      size3 += (strlen(n_array3[count3]) + strlen(a_array3[count3]) + 10);
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
    out += strlen(out);
    *out++ = 0;
  }
  s1 = out;
  for (i = 0; i < count3; i++)
  {
    PL_strcpy(out, n_array3[i]);
    out += strlen(out);
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
msg_make_full_address(const char* name, const char* addr)
{
  int nl = name ? strlen (name) : 0;
  int al = addr ? strlen (addr) : 0;
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
    L = msg_quote_phrase_or_addr(buf, nl, PR_FALSE);
    s = buf + L;
    *s++ = ' ';
    *s++ = '<';
  }
  else
  {
    s = buf;
  }

  PL_strcpy(s, addr);
  L = msg_quote_phrase_or_addr(s, al, PR_TRUE);
  s += L;
  if (nl > 0)
    *s++ = '>';
  *s = 0;
  L = (s - buf) + 1;
  buf = (char *)PR_Realloc (buf, L);
  return buf;
}
