/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   John G Myers   <jgmyers@netscape.com>
 *   Takayuki Tei   <taka@netscape.com>
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

#include "nsICharsetConverterManager.h"
#include "nsICharsetAlias.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "prtypes.h"
#include "prmem.h"
#include "prlog.h"
#include "prprf.h"
#include "plstr.h"
#include "nsCRT.h"
#include "comi18n.h"
#include "nsIServiceManager.h"
#include "nsIStringCharsetDetector.h"
#include "nsIPref.h"
#include "mimebuf.h"
#include "nsMsgI18N.h"
#include "nsMimeTypes.h"
#include "nsICharsetConverterManager.h"
#include "nsISaveAsCharset.h"
#include "nsHankakuToZenkakuCID.h"
#include "nsReadableUtils.h"
#include "mimehdrs.h"
#include "nsIMIMEHeaderParam.h"
#include "nsNetCID.h"


////////////////////////////////////////////////////////////////////////////////

static char basis_64[] =
   "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

#define NO_Q_ENCODING_NEEDED(ch)  \
  (((ch) >= 'a' && (ch) <= 'z') || ((ch) >= 'A' && (ch) <= 'Z') || \
   ((ch) >= '0' && (ch) <= '9') ||  (ch) == '!' || (ch) == '*' ||  \
    (ch) == '+' || (ch) == '-' || (ch) == '/')

static const char hexdigits[] = "0123456789ABCDEF";

static PRInt32
intlmime_encode_q(const unsigned char *src, PRInt32 srcsize, char *out)
{
  const unsigned char *in = (unsigned char *) src;
  const unsigned char *end = in + srcsize;
  char *head = out;

  for (; in < end; in++) {
    if (NO_Q_ENCODING_NEEDED(*in)) {
      *out++ = *in;
    }
    else if (*in == ' ') {
      *out++ = '_';
    }
    else {
      *out++ = '=';
      *out++ = hexdigits[*in >> 4];
      *out++ = hexdigits[*in & 0xF];
    }
  }
  *out = '\0';
  return (out - head);
}

static void
encodeChunk(const unsigned char* chunk, char* output)
{
  register PRInt32 offset;

  offset = *chunk >> 2;
  *output++ = basis_64[offset];

  offset = ((*chunk << 4) & 0x30) + (*(chunk+1) >> 4);
  *output++ = basis_64[offset];

  if (*(chunk+1)) {
    offset = ((*(chunk+1) & 0x0f) << 2) + ((*(chunk+2) & 0xc0) >> 6);
    *output++ = basis_64[offset];
  }
  else
    *output++ = '=';

  if (*(chunk+2)) {
    offset = *(chunk+2) & 0x3f;
    *output++ = basis_64[offset];
  }
  else
    *output++ = '=';
}

static PRInt32
intlmime_encode_b(const unsigned char* input, PRInt32 inlen, char* output)
{
  unsigned char  chunk[3];
  PRInt32   i, len;
  char *head = output;

  for (len = 0; inlen >=3 ; inlen -= 3) {
    for (i = 0; i < 3; i++)
      chunk[i] = *input++;
    encodeChunk(chunk, output);
    output += 4;
    len += 4;
  }

  if (inlen > 0) {
    for (i = 0; i < inlen; i++)
      chunk[i] = *input++;
    for (; i < 3; i++)
      chunk[i] = 0;
    encodeChunk(chunk, output);
    output += 4;
  }

  *output = 0;
  return (output - head);
}


/*      some utility function used by this file */
static PRBool intlmime_only_ascii_str(const char *s)
{
  for(; *s; s++)
    if(*s & 0x80)
      return PR_FALSE;
  return PR_TRUE;
}

static unsigned char * utf8_nextchar(unsigned char *str)
{
  if (*str < 128) {
    return (str+1);
  }
  int len = strlen((char *) str);
  // RFC 2279 defines more than 3 bytes sequences (0xF0, 0xF8, 0xFC),
  // but I think we won't encounter those cases as long as we're supporting UCS-2 and no surrogate.
  if ((len >= 3) && (*str >= 0xE0)) {
    return (str+3);
  }
  else if ((len >= 2) && (*str >= 0xC0)) {
    return (str+2);
  }

  return (str+1); // error, return +1 to avoid infinite loop
}

/* -------------- */

static
PRInt32 generate_encodedwords(char *pUTF8, const char *charset, char method, char *output, PRInt32 outlen, PRInt32 output_carryoverlen, PRInt32 foldlen, PRBool foldingonly)
{
  NS_ASSERTION(output_carryoverlen > 0, "output_carryoverlen must be > 0"); 

  nsCOMPtr <nsISaveAsCharset> conv;
  PRUnichar *_pUCS2 = nsnull, *pUCS2 = nsnull, *pUCS2Head = nsnull, cUCS2Tmp = 0;
  char  *ibuf, *o = output;
  char  encodedword_head[kMAX_CSNAME+4+1];
  nsCAutoString _charset;
  char  *pUTF8Head = nsnull, cUTF8Tmp = 0;
  PRInt32   olen = 0, offset, linelen = output_carryoverlen, convlen = 0;
  PRInt32   encodedword_headlen = 0, encodedword_taillen = foldingonly ? 0 : 2; // "?="
  nsresult rv;

  encodedword_head[0] = 0;

  if (!foldingonly) {

    // Deal with UCS2 pointer
    pUCS2 = _pUCS2 = ToNewUnicode(NS_ConvertUTF8toUCS2(pUTF8));
    if (!pUCS2)
      return -1;

    // Resolve charset alias
    {
      nsCOMPtr <nsICharsetAlias> calias = do_GetService(NS_CHARSETALIAS_CONTRACTID, &rv);
      nsCOMPtr <nsIAtom> charsetAtom;
      charset = !nsCRT::strcasecmp(charset, "us-ascii") ? "ISO-8859-1" : charset;
      rv = calias->GetPreferred(nsDependentCString(charset),
                                _charset);
      if (NS_FAILED(rv)) {
        if (_pUCS2)
          nsMemory::Free(_pUCS2);
        return -1;
      }

      // this is so nasty, but _charset won't be going away..
      charset = _charset.get();
    }

    // Prepare MIME encoded-word head with official charset name
    if (PR_snprintf(encodedword_head, sizeof(encodedword_head)-1, "=?%s?%c?", charset, method) < 0) {
      if (_pUCS2)
        nsMemory::Free(_pUCS2);
      return -1;
    }
    encodedword_headlen = strlen(encodedword_head);

    // Load HANKAKU-KANA prefrence and cache it.
    if (!nsCRT::strcasecmp("ISO-2022-JP", charset)) {
      static PRInt32  conv_kana = -1;
      if (conv_kana < 0) {
        nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
        if (nsnull != prefs && NS_SUCCEEDED(rv)) {
          PRBool val;
          if (NS_FAILED(prefs->GetBoolPref("mailnews.send_hankaku_kana", &val)))
            val = PR_FALSE;  // no pref means need the mapping
          conv_kana = val ? 0 : 1;
        }
      }
      // Do nsITextTransform if needed
      if (conv_kana > 0) {
        nsCOMPtr <nsITextTransform> textTransform =
            do_CreateInstance(NS_HANKAKUTOZENKAKU_CONTRACTID, &rv);
        if (NS_SUCCEEDED(rv)) {
          nsString text(pUCS2), result;
          rv = textTransform->Change(pUCS2, nsCRT::strlen(pUCS2), result);
          if (NS_SUCCEEDED(rv)) {
            if (_pUCS2)
              nsMemory::Free(_pUCS2);
            pUCS2 = _pUCS2 = ToNewUnicode(result);
            if (!pUCS2)
              return -1;
          }
        }
      }
    }

    // Create an instance of charset converter and initialize it
    conv = do_CreateInstance(NS_SAVEASCHARSET_CONTRACTID, &rv);
    if(NS_SUCCEEDED(rv)) {
      rv = conv->Init(charset, 
                       nsISaveAsCharset::attr_FallbackQuestionMark + nsISaveAsCharset::attr_EntityAfterCharsetConv, 
                       nsIEntityConverter::transliterate);
    }
    if (NS_FAILED(rv)) {
      if (_pUCS2)
        nsMemory::Free(_pUCS2);
      return -1;
    }
  } // if (!foldingonly)

  /*
     See if folding needs to happen.
     If carried over length is already too long to hold
     encoded-word header and trailer length, it's too obvious
     that we need to fold.  So, don't waste time for calculation.
  */
  if (linelen + encodedword_headlen + encodedword_taillen < foldlen) {
    if (foldingonly) {
      offset = strlen(pUTF8Head = pUTF8);
      pUTF8 += offset;
    }
    else {
      rv = conv->Convert(pUCS2, &ibuf);
      if (NS_FAILED(rv) || ibuf == nsnull) {
        if (_pUCS2)
          nsMemory::Free(_pUCS2);
        return -1; //error
      }
      if (method == 'B') {
        /* 4 / 3 = B encoding multiplier */
        offset = strlen(ibuf) * 4 / 3;
      }
      else {
        /* 3 = Q encoding multiplier */
        offset = 0;
        for (PRInt32 i = 0; *(ibuf+i); i++)
          offset += NO_Q_ENCODING_NEEDED(*(ibuf+i)) ? 1 : 3;
      }
    }
    if (linelen + encodedword_headlen + offset + encodedword_taillen > foldlen) {
      /* folding has to happen */
      if (foldingonly) {
        pUTF8 = pUTF8Head;
        pUTF8Head = nsnull;
      }
      else
        PR_Free(ibuf);
    }
    else {
      /* no folding needed, let's fall thru */
      strcpy(o, encodedword_head);
      olen += encodedword_headlen;
      linelen += encodedword_headlen;
      o += encodedword_headlen;
      if (!foldingonly)
        *pUCS2 = 0;
    }
  }
  else {
    strcpy(o, "\r\n ");
    olen += 3;
    o += 3;
    linelen = 1;
  }

  /*
    Let's do the nasty RFC-2047 & folding stuff
  */

  while ((foldingonly ? *pUTF8 : *pUCS2) && (olen < outlen)) {
    strcpy(o, encodedword_head);
    olen += encodedword_headlen;
    linelen += encodedword_headlen;
    o += encodedword_headlen;
    olen += encodedword_taillen;
    if (foldingonly)
      pUTF8Head = pUTF8;
    else
      pUCS2Head = pUCS2;

    while ((foldingonly ? *pUTF8 : *pUCS2) && (olen < outlen)) {
      if (foldingonly) {
        ++pUTF8;
        for (;;) {
          if (*pUTF8 == ' ' || *pUTF8 == TAB || !*pUTF8) {
            offset = pUTF8 - pUTF8Head;
            cUTF8Tmp = *pUTF8;
            *pUTF8 = 0;
            break;
          }
          ++pUTF8;
        }
      }
      else {
        convlen = ++pUCS2 - pUCS2Head;
        cUCS2Tmp = *(pUCS2Head + convlen);
        *(pUCS2Head + convlen) = 0;
        rv = conv->Convert(pUCS2Head, &ibuf);
        *(pUCS2Head + convlen) = cUCS2Tmp;
        if (NS_FAILED(rv) || ibuf == nsnull) {
          if (_pUCS2)
            nsMemory::Free(_pUCS2);
          return -1; //error
        }
        if (method == 'B') {
          /* 4 / 3 = B encoding multiplier */
          offset = strlen(ibuf) * 4 / 3;
        }
        else {
          /* 3 = Q encoding multiplier */
          offset = 0;
          for (PRInt32 i = 0; *(ibuf+i); i++)
            offset += NO_Q_ENCODING_NEEDED(*(ibuf+i)) ? 1 : 3;
        }
      }
      if (linelen + offset > foldlen) {
process_lastline:
        PRInt32 enclen;
        if (foldingonly) {
          strcpy(o, pUTF8Head);
          enclen = strlen(o);
          o += enclen;
          *pUTF8 = cUTF8Tmp;
        }
        else {
          if (method == 'B')
            enclen = intlmime_encode_b((const unsigned char *)ibuf, strlen(ibuf), o);
          else
            enclen = intlmime_encode_q((const unsigned char *)ibuf, strlen(ibuf), o);
          PR_Free(ibuf);
          o += enclen;
          strcpy(o, "?=");
        }
        o += encodedword_taillen;
        olen += enclen;
        if (foldingonly ? *pUTF8 : *pUCS2) { /* not last line */
          strcpy(o, "\r\n ");
          o += 3;
          olen += 3;
          linelen = 1;
          if (foldingonly) {
            pUTF8Head = nsnull;
            if (*pUTF8 == ' ' || *pUTF8 == TAB) {
              ++pUTF8;
              if (!*pUTF8)
                return 0;
            }
          }
        }
        else {
          if (_pUCS2)
            nsMemory::Free(_pUCS2);
          return linelen + enclen + encodedword_taillen;
        }
        break;
      }
      else {
        if (foldingonly)
          *pUTF8 = cUTF8Tmp;
        else {
          if (*pUCS2)
            PR_Free(ibuf);
        }
      }
    }
  }

  goto process_lastline;
}

typedef struct _RFC822AddressList {
  char        *displayname;
  PRBool      asciionly;
  char        *addrspec;
  struct _RFC822AddressList *next;
} RFC822AddressList;

static
void destroy_addresslist(RFC822AddressList *p)
{
  if (p->next)
    destroy_addresslist(p->next);
  PR_FREEIF(p->displayname);
  PR_FREEIF(p->addrspec);
  PR_Free(p);
  return;
}

static
RFC822AddressList * construct_addresslist(char *s)
{
  PRBool  quoted = PR_FALSE, angle_addr = PR_FALSE;
  PRInt32  comment = 0;
  char *displayname = nsnull, *addrspec = nsnull;
  static RFC822AddressList  listinit;
  RFC822AddressList  *listhead = (RFC822AddressList *)PR_Malloc(sizeof(RFC822AddressList));
  RFC822AddressList  *list = listhead;

  if (!list)
    return nsnull;

  while (*s == ' ' || *s == TAB)
    ++s;

  for (*list = listinit; *s; ++s) {
    if (*s == '\\') {
      if (quoted || comment) {
        ++s;
        continue;
      }
    }
    else if (*s == '(' || *s == ')') {
      if (!quoted) {
        if (*s == '(') {
          if (comment++ == 0)
            displayname = s + 1;
        }
        else {
          if (--comment == 0) {
            *s = '\0';
            PR_FREEIF(list->displayname);
            list->displayname = nsCRT::strdup(displayname);
            list->asciionly = intlmime_only_ascii_str(displayname);
            *s = ')';
          }
        }
        continue;
      }
    }
    else if (*s == '"') {
      if (!comment && !angle_addr) {
        quoted = !quoted;
        if (quoted)
          displayname = s;
        else {
          char tmp = *(s+1);
          *(s+1) = '\0';
          PR_FREEIF(list->displayname);
          list->displayname = nsCRT::strdup(displayname);
          list->asciionly = intlmime_only_ascii_str(displayname);
          *(s+1) = tmp;
        }
        continue;
      }
    }
    else if (*s == '<' || *s == '>') {
      if (!quoted && !comment) {
        if (*s == '<') {
          angle_addr = PR_TRUE;
          addrspec = s;
          if (displayname) {
            char *e = s - 1, tmp;
            while (*e == TAB || *e == ' ')
              --e;
            tmp = *++e;
            *e = '\0';
            PR_FREEIF(list->displayname);
            list->displayname = nsCRT::strdup(displayname);
            list->asciionly = intlmime_only_ascii_str(displayname);
            *e = tmp;
          }
        }
        else {
          char tmp;
          angle_addr = PR_FALSE;
          tmp = *(s+1);
          *(s+1) = '\0';
          PR_FREEIF(list->addrspec);
          list->addrspec = nsCRT::strdup(addrspec);
          *(s+1) = tmp;
        }
        continue;
      }
    }
    if (!quoted && !comment && !angle_addr) {
      /* address-list separator. end of this address */
      if (*s == ',') {
        /* deal with addr-spec only address */
        if (!addrspec && displayname) {
          *s = '\0';
          list->addrspec = nsCRT::strdup(displayname);
          /* and don't forget to free the display name in the list, in that case it's bogus */
          PR_FREEIF(list->displayname);
        }
        /* prepare for next address */
        addrspec = displayname = nsnull;
        list->next = (RFC822AddressList *)PR_Malloc(sizeof(RFC822AddressList));
        list = list->next;
        *list = listinit;
        /* eat spaces */
        ++s;
        while (*s == ' ' || *s == TAB)
          ++s;
        if (*s == '\r' && *(s+1) == '\n' && (*(s+2) == ' ' || *(s+2) == TAB))
          s += 2;
        else
          --s;
      }
      else if (!displayname && *s != ' ' && *s != TAB)
        displayname = s;
    }
  }

  /* deal with addr-spec only address comes at last */
  if (!addrspec && displayname)
  {
    list->addrspec = nsCRT::strdup(displayname);
    /* and don't forget to free the display name in the list, in that case it's bogus */
    PR_FREEIF(list->displayname);
  }

  return listhead;
}

static 
char * apply_rfc2047_encoding(const char *_src, PRBool structured, const char *charset, PRInt32 cursor, PRInt32 foldlen)
{
  RFC822AddressList  *listhead, *list;
  PRInt32   outputlen, usedlen;
  char  *src, *src_head, *output, *outputtail;
  char  method = nsMsgI18Nmultibyte_charset(charset) ? 'B' : 'Q';

  if (!_src)
    return nsnull;
  if ((src = src_head = nsCRT::strdup(_src)) == nsnull)
    return nsnull;

  /* allocate enough buffer for conversion, this way it can avoid
     do another memory allocation which is expensive
   */
  outputlen = strlen(src) * 4 + kMAX_CSNAME + 8;
  if ((outputtail = output = (char *)PR_Malloc(outputlen)) == nsnull) {
    PR_Free(src_head);
    return nsnull;
  }

  if (structured) {
    listhead = list = construct_addresslist(src);
    if (!list) {
      PR_Free(output);
      output = nsnull;
    }
    else {
      for (; list && (outputlen > 0); list = list->next) {
        if (list->displayname) {
          if (list->asciionly && list->addrspec) {
            PRInt32 len = cursor + strlen(list->displayname) + strlen(list->addrspec);
            if (foldlen < len && len < 998) { /* see RFC 2822 for magic number 998 */
              PR_snprintf(outputtail, outputlen - 1, (list == listhead || cursor == 1) ? "%s %s%s" : "\r\n %s %s%s", list->displayname, list->addrspec, list->next ? ",\r\n " : "");
              usedlen = strlen(outputtail);
              outputtail += usedlen;
              outputlen -= usedlen;
              cursor = 1;
              continue;
            }
          }
          cursor = generate_encodedwords(list->displayname, charset, method, outputtail, outputlen, cursor, foldlen, list->asciionly);
          if (cursor < 0) {
            PR_Free(output);
            output = nsnull;
            break;
          }
          usedlen = strlen(outputtail);
          outputtail += usedlen;
          outputlen -= usedlen;
        }
        if (list->addrspec) {
          usedlen = strlen(list->addrspec);
          if (cursor + usedlen > foldlen) {
            if (PR_snprintf(outputtail, outputlen - 1, "\r\n %s", list->addrspec) < 0) {
              PR_Free(output);
              destroy_addresslist(listhead);
              return nsnull;
            }
            usedlen += 3;         /* FWS + addr-spec */
            cursor = usedlen - 2; /* \r\n */
          }
          else {
            if (PR_snprintf(outputtail, outputlen - 1, " %s", list->addrspec) < 0) {
              PR_Free(output);
              destroy_addresslist(listhead);
              return nsnull;
            }
            usedlen++;
            cursor += usedlen;
          }
          outputtail += usedlen;
          outputlen -= usedlen;
        }
        else {
          PR_Free(output);
          output = nsnull;
          break;
        }
        if (list->next) {
          strcpy(outputtail, ", ");
          cursor += 2;
          outputtail += 2;
          outputlen -= 2;
        }
      }
      destroy_addresslist(listhead);
    }
  }
  else {
    char *spacepos = nsnull, *org_output = output;
    /* show some mercy to stupid ML systems which don't know 
       how to respect MIME encoded subject */
    for (char *p = src; *p && !(*p & 0x80); p++) {
      if (*p == 0x20 || *p == TAB)
        spacepos = p;
    }
    if (spacepos) {
      char head[kMAX_CSNAME+4+1];
      PRInt32  overhead, skiplen;
      if (PR_snprintf(head, sizeof(head) - 1, "=?%s?%c?", charset, method) < 0) {
        PR_Free(output);
        return nsnull;
      }
      overhead = strlen(head) + 2 + 4; // 2 : "?=", 4 : a B-encoded chunk
      skiplen = spacepos + 1 - src;
      if (cursor + skiplen + overhead < foldlen) {
        char tmp = *(spacepos + 1);
        *(spacepos + 1) = '\0';
        strcpy(output, src);
        output += skiplen;
        outputlen -= skiplen;
        cursor += skiplen;
        src += skiplen;
        *src = tmp;
      }
    }
    PRBool asciionly = intlmime_only_ascii_str(src);
    if (generate_encodedwords(src, charset, method, output, outputlen, cursor, foldlen, asciionly) < 0) {
      PR_Free(org_output);
      org_output = nsnull;
    }
    output = org_output;
  }

  PR_Free(src_head);

  return output;
}

////////////////////////////////////////////////////////////////////////////////
// BEGIN PUBLIC INTERFACE
extern "C" {
#define PUBLIC


extern "C" char *MIME_DecodeMimeHeader(const char *header, 
                                       const char *default_charset,
                                       PRBool override_charset,
                                       PRBool eatContinuations)
{
  nsresult rv;
  nsCOMPtr <nsIMIMEHeaderParam> mimehdrpar = do_GetService(NS_MIMEHEADERPARAM_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return nsnull;
  nsCAutoString result;
  rv = mimehdrpar->DecodeRFC2047Header(header, default_charset, override_charset, 
                                       eatContinuations, result);
  if (NS_SUCCEEDED(rv))
    return nsCRT::strdup(result.get());
  return nsnull;
}  

char *MIME_EncodeMimePartIIStr(const char* header, PRBool structured, const char* mailCharset, const PRInt32 fieldNameLen, const PRInt32 encodedWordSize)
{
  return apply_rfc2047_encoding(header, structured, mailCharset, fieldNameLen, encodedWordSize);
}

// UTF-8 utility functions.

char * NextChar_UTF8(char *str)
{
  return (char *) utf8_nextchar((unsigned char *) str);
}

//detect charset soly based on aBuf. return in aCharset
nsresult
MIME_detect_charset(const char *aBuf, PRInt32 aLength, const char** aCharset)
{
  nsresult res;
  char theBuffer[128];
  nsFixedCString detector_contractid(theBuffer, sizeof(theBuffer), 0);
  nsXPIDLString detector_name;
  nsCOMPtr<nsIStringCharsetDetector> detector;
  *aCharset = nsnull;

  detector_contractid.Assign(NS_STRCDETECTOR_CONTRACTID_BASE);

  nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &res)); 
  if (NS_SUCCEEDED(res)) {
    if (NS_SUCCEEDED(prefs->GetLocalizedUnicharPref("intl.charset.detector", getter_Copies(detector_name)))) {
      AppendUTF16toUTF8(detector_name, detector_contractid);
    }
  }

  if (detector_contractid.Length() > sizeof(NS_STRCDETECTOR_CONTRACTID_BASE)) {
    detector = do_CreateInstance(detector_contractid.get(), &res);
    if (NS_SUCCEEDED(res)) {
      nsDetectionConfident oConfident;
      res = detector->DoIt(aBuf, aLength, aCharset, oConfident);
      if (NS_SUCCEEDED(res) && (eBestAnswer == oConfident || eSureAnswer == oConfident)) {
        return NS_OK;
      }
    }
  }
  return res;
}

//Get unicode decoder(from inputcharset to unicode) for aInputCharset
nsresult 
MIME_get_unicode_decoder(const char* aInputCharset, nsIUnicodeDecoder **aDecoder)
{
  nsresult res;

  // get charset converters.
  nsCOMPtr<nsICharsetConverterManager> ccm = 
           do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &res); 
  if (NS_SUCCEEDED(res)) {

    // create a decoder (conv to unicode), ok if failed if we do auto detection
    if (!*aInputCharset || !nsCRT::strcasecmp("us-ascii", aInputCharset))
      res = ccm->GetUnicodeDecoderRaw("ISO-8859-1", aDecoder);
    else
      res = ccm->GetUnicodeDecoder(aInputCharset, aDecoder);
  }
   
  return res;
}

//Get unicode encoder(from unicode to inputcharset) for aOutputCharset
nsresult 
MIME_get_unicode_encoder(const char* aOutputCharset, nsIUnicodeEncoder **aEncoder)
{
  nsresult res;

  // get charset converters.
  nsCOMPtr<nsICharsetConverterManager> ccm = 
           do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &res); 
  if (NS_SUCCEEDED(res) && *aOutputCharset) {
      // create a encoder (conv from unicode)
      res = ccm->GetUnicodeEncoder(aOutputCharset, aEncoder);
  }
   
  return res;
}

} /* end of extern "C" */
// END PUBLIC INTERFACE

