/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Patrick C. Beard <beard@netscape.com>
 *   Seth Spitzer <sspitzer@netscape.com>
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

#ifdef MOZ_LOGGING
// sorry, this has to be before the pre-compiled header
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif

#include "nsCRT.h"
#include "nsBayesianFilter.h"
#include "nsIInputStream.h"
#include "nsIStreamListener.h"
#include "nsNetUtil.h"
#include "nsQuickSort.h"
#include "nsIMsgMessageService.h"
#include "nsMsgUtils.h" // for GetMessageServiceFromURI
#include "prnetdb.h"
#include "nsIMsgWindow.h"
#include "prlog.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsUnicharUtils.h"

#include "nsPrintfCString.h"
#include "nsIMIMEHeaderParam.h"
#include "nsNetCID.h"
#include "nsIMimeHeaders.h"
#include "nsMsgMimeCID.h"
#include "nsIMsgMailNewsUrl.h"
#include "nsIMimeMiscStatus.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIStringEnumerator.h"

// needed to mark attachment flag on the db hdr
#include "nsIMsgHdr.h"

// needed to strip html out of the body
#include "nsParserCIID.h"
#include "nsIParser.h"
#include "nsIHTMLContentSink.h"
#include "nsIContentSerializer.h"
#include "nsLayoutCID.h"
#include "nsIHTMLToTextSink.h"
#include "nsIDocumentEncoder.h" 

#include <math.h>

static PRLogModuleInfo *BayesianFilterLogModule = nsnull;

static NS_DEFINE_CID(kParserCID, NS_PARSER_CID);
static NS_DEFINE_CID(kNavDTDCID, NS_CNAVDTD_CID);

#define kDefaultJunkThreshold .99 // we override this value via a pref
static const char* kBayesianFilterTokenDelimiters = " \t\n\r\f.";
static int kMinLengthForToken = 3; // lower bound on the number of characters in a word before we treat it as a token
static int kMaxLengthForToken = 12; // upper bound on the number of characters in a word to be declared as a token

#define FORGED_RECEIVED_HEADER_HINT NS_LITERAL_CSTRING("may be forged")

#ifndef M_LN2
#define M_LN2 0.69314718055994530942
#endif

#ifndef M_E
#define M_E   2.7182818284590452354
#endif

struct Token : public PLDHashEntryHdr {
    const char* mWord;
    PRUint32 mLength;
    PRUint32 mCount;            // TODO:  put good/bad count values in same token object.
    double mProbability;        // TODO:  cache probabilities
    double mDistance;
};

TokenEnumeration::TokenEnumeration(PLDHashTable* table)
    :   mEntrySize(table->entrySize),
        mEntryCount(table->entryCount),
        mEntryOffset(0),
        mEntryAddr(table->entryStore)
{
    PRUint32 capacity = PL_DHASH_TABLE_SIZE(table);
    mEntryLimit = mEntryAddr + capacity * mEntrySize;
}
    
inline PRBool TokenEnumeration::hasMoreTokens()
{
    return (mEntryOffset < mEntryCount);
}

inline Token* TokenEnumeration::nextToken()
{
    Token* token = NULL;
    PRUint32 entrySize = mEntrySize;
    char *entryAddr = mEntryAddr, *entryLimit = mEntryLimit;
    while (entryAddr < entryLimit) {
        PLDHashEntryHdr* entry = (PLDHashEntryHdr*) entryAddr;
        entryAddr += entrySize;
        if (PL_DHASH_ENTRY_IS_LIVE(entry)) {
            token = NS_STATIC_CAST(Token*, entry);
            ++mEntryOffset;
            break;
        }
    }
    mEntryAddr = entryAddr;
    return token;
}

struct VisitClosure {
    PRBool (*f) (Token*, void*);
    void* data;
};

static PLDHashOperator PR_CALLBACK VisitEntry(PLDHashTable* table, PLDHashEntryHdr* entry,
                                              PRUint32 number, void* arg)
{
    VisitClosure* closure = NS_REINTERPRET_CAST(VisitClosure*, arg);
    Token* token = NS_STATIC_CAST(Token*, entry);
    return (closure->f(token, closure->data) ? PL_DHASH_NEXT : PL_DHASH_STOP);
}

// member variables
static const PLDHashTableOps gTokenTableOps = {
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    PL_DHashGetKeyStub,
    PL_DHashStringKey,
    PL_DHashMatchStringKey,
    PL_DHashMoveEntryStub,
    PL_DHashClearEntryStub,
    PL_DHashFinalizeStub
};

Tokenizer::Tokenizer()
{
    PL_INIT_ARENA_POOL(&mWordPool, "Words Arena", 16384);
    PRBool ok = PL_DHashTableInit(&mTokenTable, &gTokenTableOps, nsnull, sizeof(Token), 256);
    NS_ASSERTION(ok, "mTokenTable failed to initialize");
    if (!ok)
      PR_LOG(BayesianFilterLogModule, PR_LOG_ALWAYS, ("mTokenTable failed to initialize"));
}

Tokenizer::~Tokenizer()
{
    if (mTokenTable.entryStore)
        PL_DHashTableFinish(&mTokenTable);
    PL_FinishArenaPool(&mWordPool);
}

nsresult Tokenizer::clearTokens()
{
    // we re-use the tokenizer when classifying multiple messages, 
    // so this gets called after every message classification.
    PRBool ok = PR_TRUE;
    if (mTokenTable.entryStore)
    {
        PL_DHashTableFinish(&mTokenTable);
        PL_FreeArenaPool(&mWordPool);
        ok = PL_DHashTableInit(&mTokenTable, &gTokenTableOps, nsnull, sizeof(Token), 256);
        NS_ASSERTION(ok, "mTokenTable failed to initialize");
        if (!ok)
          PR_LOG(BayesianFilterLogModule, PR_LOG_ALWAYS, ("mTokenTable failed to initialize in clearTokens()"));
    }
    return (ok) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

char* Tokenizer::copyWord(const char* word, PRUint32 len)
{
    void* result;
    PRUint32 size = 1 + len;
    PL_ARENA_ALLOCATE(result, &mWordPool, size);
    if (result)
        memcpy(result, word, size);
    return NS_REINTERPRET_CAST(char*, result);
}

inline Token* Tokenizer::get(const char* word)
{
    PLDHashEntryHdr* entry = PL_DHashTableOperate(&mTokenTable, word, PL_DHASH_LOOKUP);
    if (PL_DHASH_ENTRY_IS_BUSY(entry))
        return NS_STATIC_CAST(Token*, entry);
    return NULL;
}

Token* Tokenizer::add(const char* word, PRUint32 count)
{
    PR_LOG(BayesianFilterLogModule, PR_LOG_ALWAYS, ("add word: %s (count=%d)", word, count));

    PLDHashEntryHdr* entry = PL_DHashTableOperate(&mTokenTable, word, PL_DHASH_ADD);
    Token* token = NS_STATIC_CAST(Token*, entry);
    if (token) {
        if (token->mWord == NULL) {
            PRUint32 len = strlen(word);
            NS_ASSERTION(len != 0, "adding zero length word to tokenizer");
            if (!len)
              PR_LOG(BayesianFilterLogModule, PR_LOG_ALWAYS, ("adding zero length word to tokenizer"));
            token->mWord = copyWord(word, len);
            NS_ASSERTION(token->mWord, "copyWord failed");
            if (!token->mWord) {
                PR_LOG(BayesianFilterLogModule, PR_LOG_ALWAYS, ("copyWord failed: %s (%d)", word, len));
                PL_DHashTableRawRemove(&mTokenTable, entry);
                return NULL;
            }
            token->mLength = len;
            token->mCount = count;
            token->mProbability = 0;
            PR_LOG(BayesianFilterLogModule, PR_LOG_ALWAYS, ("adding word to tokenizer: %s (len=%d) (count=%d)", word, len, count));
        } else {
            token->mCount += count;
            PR_LOG(BayesianFilterLogModule, PR_LOG_ALWAYS, ("adding word to tokenizer: %s (count=%d) (mCount=%d)", word, count, token->mCount));
        }
    }
    return token;
}

void Tokenizer::remove(const char* word, PRUint32 count)
{
    PR_LOG(BayesianFilterLogModule, PR_LOG_ALWAYS, ("remove word: %s (count=%d)", word, count));
    Token* token = get(word);
    if (token) {
        NS_ASSERTION(token->mCount >= count, "token count underflow");
        if (token->mCount >= count) {
            PR_LOG(BayesianFilterLogModule, PR_LOG_ALWAYS, ("remove word: %s (count=%d) (mCount=%d)", word, count, token->mCount));
            token->mCount -= count;
            if (token->mCount == 0)
                PL_DHashTableRawRemove(&mTokenTable, token);
        }
        else {
          PR_LOG(BayesianFilterLogModule, PR_LOG_ALWAYS, ("token count underflow: %s (count=%d) (mCount=%d)", word, count, token->mCount));
        }
    }
}

static PRBool isDecimalNumber(const char* word)
{
    const char* p = word;
    if (*p == '-') ++p;
    char c;
    while ((c = *p++)) {
        if (!isdigit(c))
            return PR_FALSE;
    }
    return PR_TRUE;
}

static PRBool isASCII(const char* word)
{
    const unsigned char* p = (const unsigned char*)word;
    unsigned char c;
    while ((c = *p++)) {
        if (c > 127)
            return PR_FALSE;
    }
    return PR_TRUE;
}

inline PRBool isUpperCase(char c) { return ('A' <= c) && (c <= 'Z'); }

static char* toLowerCase(char* str)
{
    char c, *p = str;
    while ((c = *p++)) {
        if (isUpperCase(c))
            p[-1] = c + ('a' - 'A');
    }
    return str;
}

void Tokenizer::addTokenForHeader(const char * aTokenPrefix, nsACString& aValue, PRBool aTokenizeValue)
{
  if (aValue.Length())
  {
    ToLowerCase(aValue);
    if (!aTokenizeValue)
      add(PromiseFlatCString(nsDependentCString(aTokenPrefix) + NS_LITERAL_CSTRING(":") + aValue).get());
    else 
    {
      char* word;
      const nsPromiseFlatCString &flatValue = PromiseFlatCString(aValue);
      char* next = (char *) flatValue.get();
      while ((word = nsCRT::strtok(next, kBayesianFilterTokenDelimiters, &next)) != NULL) 
      {
          if (word[0] == '\0') continue;
          if (isDecimalNumber(word)) continue;
          if (isASCII(word))
              add(PromiseFlatCString(nsDependentCString(aTokenPrefix) + NS_LITERAL_CSTRING(":") + nsDependentCString(word)).get());
      }
    }
  }
}

void Tokenizer::tokenizeAttachment(const char * aContentType, const char * aFileName)
{
  nsCAutoString contentType;
  nsCAutoString fileName;
  fileName.Assign(aFileName);
  contentType.Assign(aContentType);

  // normalize the content type and the file name
  ToLowerCase(fileName);
  ToLowerCase(contentType);
  addTokenForHeader("attachment/filename", fileName);

  addTokenForHeader("attachment/content-type", contentType);
}

void Tokenizer::tokenizeHeaders(nsIUTF8StringEnumerator * aHeaderNames, nsIUTF8StringEnumerator * aHeaderValues)
{
  nsCOMPtr<nsIMIMEHeaderParam> mimehdrpar = do_GetService(NS_MIMEHEADERPARAM_CONTRACTID);

  nsCString headerValue;
  nsCAutoString headerName; // we'll be normalizing all header names to lower case
  PRBool hasMore = PR_TRUE;

  while (hasMore)
  {
    aHeaderNames->GetNext(headerName);
    ToLowerCase(headerName); 
    aHeaderValues->GetNext(headerValue);

    switch (headerName.First())
    {
    case 'c':
        if (headerName.Equals("content-type"))
        {
          // extract the charset parameter
          nsXPIDLCString parameterValue;
          mimehdrpar->GetParameterInternal(headerValue.get(), "charset", nsnull, nsnull, getter_Copies(parameterValue));
          addTokenForHeader("charset", parameterValue);

          // create a token containing just the content type 
          mimehdrpar->GetParameterInternal(headerValue.get(), "type", nsnull, nsnull, getter_Copies(parameterValue));
          if (!parameterValue.Length())
            mimehdrpar->GetParameterInternal(headerValue.get(), nsnull /* use first unnamed param */, nsnull, nsnull, getter_Copies(parameterValue));
          addTokenForHeader("content-type/type", parameterValue);

          // XXX: should we add a token for the entire content-type header as well or just these parts we have extracted?
        }
        break;
    case 'r':
      if (headerName.Equals("received"))
      {
        // look for the string "may be forged" in the received headers. sendmail sometimes adds this hint
        // This does not compile on linux yet. Need to figure out why. Commenting out for now
        // if (FindInReadable(FORGED_RECEIVED_HEADER_HINT, headerValue))
        //   addTokenForHeader(headerName.get(), FORGED_RECEIVED_HEADER_HINT);
        // for now 
      }
      
      // leave out reply-to
      break;
    case 's':
        if (headerName.Equals("subject"))
        { 
          // we want to tokenize the subject
          addTokenForHeader(headerName.get(), headerValue, PR_TRUE);
        }

        // important: leave out sender field. Too strong of an indicator
        break;
    case 'x': // (2) X-Mailer / user-agent works best if it is untokenized, just fold the case and any leading/trailing white space
    case 'u': 
        addTokenForHeader(headerName.get(), headerValue); 
        break;
    default:
        addTokenForHeader(headerName.get(), headerValue); 
        break;
    } // end switch

    aHeaderNames->HasMore(&hasMore);
  }
}

void Tokenizer::tokenize_ascii_word(char * aWord)
{
  // always deal with normalized lower case strings
  toLowerCase(aWord);
  PRInt32 wordLength = strlen(aWord);

  // if the wordLength is within our accepted token limit, then add it
  if (wordLength >= kMinLengthForToken && wordLength <= kMaxLengthForToken)
    add(aWord);
  else if (wordLength > kMaxLengthForToken)
  {
    // don't skip over the word if it looks like an email address,
    // there is value in adding tokens for addresses
    nsDependentCString word (aWord, wordLength); // CHEAP, no allocation occurs here...

    // XXX: i think the 40 byte check is just for perf reasons...if the email address is longer than that then forget about it.
    if (wordLength < 40 && strchr(aWord, '.') && word.CountChar('@') == 1)
    {
      PRInt32 numBytesToSep = word.FindChar('@'); 
      if (numBytesToSep < wordLength - 1) // if the @ sign is the last character, it must not be an email address
      {
        // split the john@foo.com into john and foo.com, treat them as separate tokens
        // if i did my string foo correctly, none of this string magic should cause a heap based allocation...
        add(nsPrintfCString(256, "email name:%s", PromiseFlatCString(Substring(word, 0, numBytesToSep++)).get()).get());
        add(nsPrintfCString(256, "email addr:%s", PromiseFlatCString(Substring(word, numBytesToSep, wordLength - numBytesToSep)).get()).get());
        return;
      }
    }

    // there is value in generating a token indicating the number
    // of characters we are skipping. We'll round to the nearest 10
    add(nsPrintfCString("skip:%c %d", word[0], (wordLength/10) * 10).get()); 
  } 
}

nsresult Tokenizer::stripHTML(const nsAString& inString, nsAString& outString)
{
  nsresult rv = NS_OK;
  // Create a parser
  nsCOMPtr<nsIParser> parser = do_CreateInstance(kParserCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Create the appropriate output sink
  nsCOMPtr<nsIContentSink> sink = do_CreateInstance(NS_PLAINTEXTSINK_CONTRACTID,&rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIHTMLToTextSink> textSink(do_QueryInterface(sink));
  NS_ENSURE_TRUE(textSink, NS_ERROR_FAILURE);
  PRUint32 flags = nsIDocumentEncoder::OutputLFLineBreak 
                 | nsIDocumentEncoder::OutputNoScriptContent
                 | nsIDocumentEncoder::OutputNoFramesContent
                 | nsIDocumentEncoder::OutputBodyOnly;

  textSink->Initialize(&outString, flags, 80);

  parser->SetContentSink(sink);
  nsCOMPtr<nsIDTD> dtd = do_CreateInstance(kNavDTDCID,&rv);
  NS_ENSURE_SUCCESS(rv, rv);

  parser->RegisterDTD(dtd);

  return parser->Parse(inString, 0, NS_LITERAL_CSTRING("text/html"), PR_FALSE, PR_TRUE);
}

void Tokenizer::tokenize(char* aText)
{
    PR_LOG(BayesianFilterLogModule, PR_LOG_ALWAYS, ("tokenize: %s", aText));

    // strip out HTML tags before we begin processing
    // uggh but first we have to blow up our string into UCS2
    // since that's what the document encoder wants. UTF8/UCS2, I wish we all
    // spoke the same language here..
    nsString text = NS_ConvertUTF8toUCS2(aText);
    nsString strippedUCS2;
    stripHTML(text, strippedUCS2);

    nsCString strippedStr = NS_ConvertUCS2toUTF8(strippedUCS2);
    char * strippedText = (char *) strippedStr.get(); // bleh
    PR_LOG(BayesianFilterLogModule, PR_LOG_ALWAYS, ("tokenize stripped html: %s", strippedText));

    char* word;
    char* next = strippedText;
    while ((word = nsCRT::strtok(next, kBayesianFilterTokenDelimiters, &next)) != NULL) {
        if (!*word) continue;
        if (isDecimalNumber(word)) continue;
        if (isASCII(word))
            tokenize_ascii_word(word);
        else {
            nsresult rv;
            // use I18N  scanner to break this word into meaningful semantic units.
            if (!mScanner) {
                mScanner = do_CreateInstance(NS_SEMANTICUNITSCANNER_CONTRACTID, &rv);
                NS_ASSERTION(NS_SUCCEEDED(rv), "couldn't create semantic unit scanner!");
                if (NS_FAILED(rv)) {
                    return;
                }
            }
            if (mScanner) {
                mScanner->Start("UTF-8");
                // convert this word from UTF-8 into UCS2.
                NS_ConvertUTF8toUCS2 uword(word);
                ToLowerCase(uword);
                const PRUnichar* utext = uword.get();
                PRInt32 len = uword.Length(), pos = 0, begin, end;
                PRBool gotUnit;
                while (pos < len) {
                    rv = mScanner->Next(utext, len, pos, PR_TRUE, &begin, &end, &gotUnit);
                    if (NS_SUCCEEDED(rv) && gotUnit) {
                        NS_ConvertUCS2toUTF8 utfUnit(utext + begin, end - begin);
                        add(utfUnit.get());
                        // advance to end of current unit.
                        pos = end;
                    } else {
                        break;
                    }
                }
            }
        }
    }
}

void Tokenizer::tokenize(const char* str)
{
    char* text = nsCRT::strdup(str);
    if (text) {
        tokenize(text);
        nsCRT::free(text);
    }
}

void Tokenizer::visit(PRBool (*f) (Token*, void*), void* data)
{
    VisitClosure closure = { f, data };
    PRUint32 visitCount = PL_DHashTableEnumerate(&mTokenTable, VisitEntry, &closure);
    NS_ASSERTION(visitCount == mTokenTable.entryCount, "visitCount != entryCount!");
    if (visitCount != mTokenTable.entryCount) {
      PR_LOG(BayesianFilterLogModule, PR_LOG_ALWAYS, ("visitCount != entryCount!: %d vs %d", visitCount, mTokenTable.entryCount));
    }
}

inline PRUint32 Tokenizer::countTokens()
{
    return mTokenTable.entryCount;
}

Token* Tokenizer::copyTokens()
{
    PRUint32 count = countTokens();
    if (count > 0) {
        Token* tokens = new Token[count];
        if (tokens) {
            Token* tp = tokens;
            TokenEnumeration e(&mTokenTable);
            while (e.hasMoreTokens())
                *tp++ = *e.nextToken();
        }
        return tokens;
    }
    return NULL;
}

inline TokenEnumeration Tokenizer::getTokens()
{
    return TokenEnumeration(&mTokenTable);
}

class TokenAnalyzer {
public:
    virtual ~TokenAnalyzer() {}
    
    virtual void analyzeTokens(Tokenizer& tokenizer) = 0;
    void setTokenListener(nsIStreamListener *aTokenListener)
    {
      mTokenListener = aTokenListener;
    }

    void setSource(const char *sourceURI) {mTokenSource = sourceURI;}

    nsCOMPtr<nsIStreamListener> mTokenListener;
    nsCString mTokenSource;

};

/**
 * This class downloads the raw content of an email message, buffering until
 * complete segments are seen, that is until a linefeed is seen, although
 * any of the valid token separators would do. This could be a further
 * refinement.
 */
class TokenStreamListener : public nsIStreamListener, nsIMsgHeaderSink {
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIMSGHEADERSINK
    
    TokenStreamListener(TokenAnalyzer* analyzer);
    virtual ~TokenStreamListener();
protected:
    TokenAnalyzer* mAnalyzer;
    char* mBuffer;
    PRUint32 mBufferSize;
    PRUint32 mLeftOverCount;
    Tokenizer mTokenizer;
    PRBool mSetAttachmentFlag;
};

const PRUint32 kBufferSize = 16384;

TokenStreamListener::TokenStreamListener(TokenAnalyzer* analyzer)
    :   mAnalyzer(analyzer),
        mBuffer(NULL), mBufferSize(kBufferSize), mLeftOverCount(0),
        mSetAttachmentFlag(PR_FALSE)
{
}

TokenStreamListener::~TokenStreamListener()
{
    delete[] mBuffer;
    delete mAnalyzer;
}

NS_IMPL_ISUPPORTS3(TokenStreamListener, nsIRequestObserver, nsIStreamListener, nsIMsgHeaderSink)

NS_IMETHODIMP TokenStreamListener::ProcessHeaders(nsIUTF8StringEnumerator *aHeaderNames, nsIUTF8StringEnumerator *aHeaderValues, PRBool dontCollectAddress)
{
    mTokenizer.tokenizeHeaders(aHeaderNames, aHeaderValues);
    return NS_OK;
}

NS_IMETHODIMP TokenStreamListener::HandleAttachment(const char *contentType, const char *url, const PRUnichar *displayName, const char *uri, PRBool aNotDownloaded)
{
    // UE Improvement, if we are downloading the message for the junk filter, and the junk filter is told the msg has attachments
    // then go ahead and mark the message as having attachments instead of waiting for the user to display the message
    // before making such a notation. 
    // XXX Unfortunately this code causes us to be over agressive in marking messages as having attachments
#if 0
    if (!mSetAttachmentFlag)
    {
      nsCOMPtr <nsIMsgMessageService> messageService;
      nsresult rv = GetMessageServiceFromURI(uri, getter_AddRefs(messageService));
      if (messageService)
      {
        nsCOMPtr <nsIMsgDBHdr> msgHdr;
        rv = messageService->MessageURIToMsgHdr(uri, getter_AddRefs(msgHdr));
        if (msgHdr) 
          msgHdr->MarkHasAttachments(PR_TRUE);
      }

      mSetAttachmentFlag = PR_TRUE;
    }
#endif

    mTokenizer.tokenizeAttachment(contentType, NS_ConvertUCS2toUTF8(displayName).get());
    return NS_OK;
}

NS_IMETHODIMP TokenStreamListener::OnEndAllAttachments()
{
    return NS_OK;
}

NS_IMETHODIMP TokenStreamListener::OnEndMsgDownload(nsIMsgMailNewsUrl *url)
{
    return NS_OK;
}


NS_IMETHODIMP TokenStreamListener::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
    return NS_OK;
}
NS_IMETHODIMP TokenStreamListener::SetSecurityInfo(nsISupports * aSecurityInfo)
{
    return NS_OK;
}

/* void onStartRequest (in nsIRequest aRequest, in nsISupports aContext); */
NS_IMETHODIMP TokenStreamListener::OnStartRequest(nsIRequest *aRequest, nsISupports *aContext)
{
    mLeftOverCount = 0;
    if (!mTokenizer)
        return NS_ERROR_OUT_OF_MEMORY;
    if (!mBuffer)
    {
        mBuffer = new char[mBufferSize];
        if (!mBuffer)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    // get the url for the channel and set our nsIMsgHeaderSink on it so we get notified 
    // about the headers and attachments

    nsCOMPtr<nsIChannel> channel (do_QueryInterface(aRequest));
    if (channel)
    {
        nsCOMPtr<nsIURI> uri;
        channel->GetURI(getter_AddRefs(uri));
        nsCOMPtr<nsIMsgMailNewsUrl> mailUrl = do_QueryInterface(uri);
        if (mailUrl)
            mailUrl->SetMsgHeaderSink(NS_STATIC_CAST(nsIMsgHeaderSink*, this));
    }

    return NS_OK;
}

/* void onDataAvailable (in nsIRequest aRequest, in nsISupports aContext, in nsIInputStream aInputStream, in unsigned long aOffset, in unsigned long aCount); */
NS_IMETHODIMP TokenStreamListener::OnDataAvailable(nsIRequest *aRequest, nsISupports *aContext, nsIInputStream *aInputStream, PRUint32 aOffset, PRUint32 aCount)
{
    nsresult rv = NS_OK;

    while (aCount > 0) {
        PRUint32 readCount, totalCount = (aCount + mLeftOverCount);
        if (totalCount >= mBufferSize) {
            readCount = mBufferSize - mLeftOverCount - 1;
        } else {
            readCount = aCount;
        }
        
        char* buffer = mBuffer;
        rv = aInputStream->Read(buffer + mLeftOverCount, readCount, &readCount);
        if (NS_FAILED(rv))
            break;

        if (readCount == 0) {
            rv = NS_ERROR_UNEXPECTED;
            NS_WARNING("failed to tokenize");
            break;
        }
            
        aCount -= readCount;
        
        /* consume the tokens up to the last legal token delimiter in the buffer. */
        totalCount = (readCount + mLeftOverCount);
        buffer[totalCount] = '\0';
        char* lastDelimiter = NULL;
        char* scan = buffer + totalCount;
        while (scan > buffer) {
            if (strchr(kBayesianFilterTokenDelimiters, *--scan)) {
                lastDelimiter = scan;
                break;
            }
        }
        
        if (lastDelimiter) {
            *lastDelimiter = '\0';
            mTokenizer.tokenize(buffer);

            PRUint32 consumedCount = 1 + (lastDelimiter - buffer);
            mLeftOverCount = totalCount - consumedCount;
            if (mLeftOverCount)
                memmove(buffer, buffer + consumedCount, mLeftOverCount);
        } else {
            /* didn't find a delimiter, keep the whole buffer around. */
            mLeftOverCount = totalCount;
            if (totalCount >= (mBufferSize / 2)) {
                PRUint32 newBufferSize = mBufferSize * 2;
                char* newBuffer = new char[newBufferSize];
                if (!newBuffer) return NS_ERROR_OUT_OF_MEMORY;
                memcpy(newBuffer, mBuffer, mLeftOverCount);
                delete[] mBuffer;
                mBuffer = newBuffer;
                mBufferSize = newBufferSize;
            }
        }
    }
    
    return rv;
}

/* void onStopRequest (in nsIRequest aRequest, in nsISupports aContext, in nsresult aStatusCode); */
NS_IMETHODIMP TokenStreamListener::OnStopRequest(nsIRequest *aRequest, nsISupports *aContext, nsresult aStatusCode)
{
    if (mLeftOverCount) {
        /* assume final buffer is complete. */
        char* buffer = mBuffer;
        buffer[mLeftOverCount] = '\0';
        mTokenizer.tokenize(buffer);
    }
    
    /* finally, analyze the tokenized message. */
    PR_LOG(BayesianFilterLogModule, PR_LOG_ALWAYS, ("analyze the tokenized message"));
    if (mAnalyzer)
        mAnalyzer->analyzeTokens(mTokenizer);
    
    return NS_OK;
}

/* Implementation file */
NS_IMPL_ISUPPORTS2(nsBayesianFilter, nsIMsgFilterPlugin, nsIJunkMailPlugin)

nsBayesianFilter::nsBayesianFilter()
    :   mGoodCount(0), mBadCount(0),
        mBatchLevel(0), mTrainingDataDirty(PR_FALSE)
{
    if (!BayesianFilterLogModule)
      BayesianFilterLogModule = PR_NewLogModule("BayesianFilter");

    PRInt32 junkThreshold = 0;
    nsresult rv;
    nsCOMPtr<nsIPrefBranch> pPrefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
    if (pPrefBranch)
      pPrefBranch->GetIntPref("mail.adaptivefilters.junk_threshold", &junkThreshold);

    mJunkProbabilityThreshold = ((double) junkThreshold) / 100;
    if (mJunkProbabilityThreshold == 0 || mJunkProbabilityThreshold >= 1)
      mJunkProbabilityThreshold = kDefaultJunkThreshold;

    PR_LOG(BayesianFilterLogModule, PR_LOG_ALWAYS, ("junk probabilty threshold: %f", mJunkProbabilityThreshold));

    PRBool ok = (mGoodTokens && mBadTokens);
    NS_ASSERTION(ok, "error allocating tokenizers");
    if (ok)
        readTrainingData();
    else {
      PR_LOG(BayesianFilterLogModule, PR_LOG_ALWAYS, ("error allocating tokenizers"));
    }
}

nsBayesianFilter::~nsBayesianFilter() 
{
  // call shutdown when we are going away in case we need
  // to flush the training set to disk
  Shutdown();
}

// this object is used for one call to classifyMessage or classifyMessages(). 
// So if we're classifying multiple messages, this object will be used for each message.
// It's going to hold a reference to itself, basically, to stay in memory.
class MessageClassifier : public TokenAnalyzer {
public:
    MessageClassifier(nsBayesianFilter* aFilter, nsIJunkMailClassificationListener* aListener, 
      nsIMsgWindow *aMsgWindow,
      PRUint32 aNumMessagesToClassify, const char **aMessageURIs)
        :   mFilter(aFilter), mSupports(aFilter), mListener(aListener), mMsgWindow(aMsgWindow)
    {
      mCurMessageToClassify = 0;
      mNumMessagesToClassify = aNumMessagesToClassify;
      mMessageURIs = (char **) nsMemory::Alloc(sizeof(char *) * aNumMessagesToClassify);
      for (PRUint32 i = 0; i < aNumMessagesToClassify; i++)
        mMessageURIs[i] = PL_strdup(aMessageURIs[i]);

    }
    
    virtual ~MessageClassifier()
    {
       if (mMessageURIs)
       {
         NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(mNumMessagesToClassify, mMessageURIs);
       }
    }
    virtual void analyzeTokens(Tokenizer& tokenizer)
    {
        mFilter->classifyMessage(tokenizer, mTokenSource.get(), mListener);
        tokenizer.clearTokens();
        classifyNextMessage();
    }

    virtual void classifyNextMessage()
    {
      
      if (++mCurMessageToClassify < mNumMessagesToClassify && mMessageURIs[mCurMessageToClassify]) {
        PR_LOG(BayesianFilterLogModule, PR_LOG_ALWAYS, ("classifyNextMessage(%s)", mMessageURIs[mCurMessageToClassify]));
        mFilter->tokenizeMessage(mMessageURIs[mCurMessageToClassify], mMsgWindow, this);
      }
      else
      {
        mTokenListener = nsnull; // this breaks the circular ref that keeps this object alive
                                 // so we will be destroyed as a result.
      }
    }

private:
    nsBayesianFilter* mFilter;
    nsCOMPtr<nsISupports> mSupports;
    nsCOMPtr<nsIJunkMailClassificationListener> mListener;
    nsCOMPtr<nsIMsgWindow> mMsgWindow;
    PRInt32 mNumMessagesToClassify;
    PRInt32 mCurMessageToClassify; // 0-based index
    char **mMessageURIs;
};

nsresult nsBayesianFilter::tokenizeMessage(const char* aMessageURI, nsIMsgWindow *aMsgWindow, TokenAnalyzer* aAnalyzer)
{

    nsCOMPtr <nsIMsgMessageService> msgService;
    nsresult rv = GetMessageServiceFromURI(aMessageURI, getter_AddRefs(msgService));
    NS_ENSURE_SUCCESS(rv, rv);

    aAnalyzer->setSource(aMessageURI);
    return msgService->StreamMessage(aMessageURI, aAnalyzer->mTokenListener, aMsgWindow,
						nsnull, PR_TRUE /* convert data */, 
                                                "filter", nsnull);
}

PR_STATIC_CALLBACK(int) compareTokens(const void* p1, const void* p2, void* /* data */)
{
    Token *t1 = (Token*) p1, *t2 = (Token*) p2;
    double delta = t1->mDistance - t2->mDistance;
    return (delta == 0.0 ? 0 : (delta > 0.0 ? 1 : -1));
}

inline double dmax(double x, double y) { return (x > y ? x : y); }
inline double dmin(double x, double y) { return (x < y ? x : y); }

double chi2Q (double x2, PRUint32 v) 
{
  PRUint32 i;
  double m = x2 / 2.0;
  double sum = exp(-m);
  double term = sum;

  NS_ASSERTION(!(v & 1), "chi2Q called with odd value");
  for (i=1; i < v/2; ++i) 
  {
    term *= m / i;
    sum += term;
  }

  return dmin(sum,1.0);
}

void nsBayesianFilter::classifyMessage(Tokenizer& tokenizer, const char* messageURI,
                                       nsIJunkMailClassificationListener* listener)
{
    Token* tokens = tokenizer.copyTokens();
    if (!tokens) return;
  
    // the algorithm in "A Plan For Spam" assumes that you have a large good
    // corpus and a large junk corpus.
    // that won't be the case with users who first use the junk mail feature
    // so, we do certain things to encourage them to train.
    //
    // if there are no good tokens, assume the message is junk
    // this will "encourage" the user to train
    // and if there are no bad tokens, assume the message is not junk
    // this will also "encourage" the user to train
    // see bug #194238
    if (listener && !mGoodCount && !mGoodTokens.countTokens()) {
      PR_LOG(BayesianFilterLogModule, PR_LOG_ALWAYS, ("no good tokens, assume junk"));
      listener->OnMessageClassified(messageURI, nsMsgJunkStatus(nsIJunkMailPlugin::JUNK));
      return;
    }
    if (listener && !mBadCount && !mBadTokens.countTokens()) {
      PR_LOG(BayesianFilterLogModule, PR_LOG_ALWAYS, ("no bad tokens, assume good"));
      listener->OnMessageClassified(messageURI, nsMsgJunkStatus(nsIJunkMailPlugin::GOOD));
      return;
    }

    /* this part is similar to the Graham algorithm with some adjustments. */
    PRUint32 i, goodclues=0, count = tokenizer.countTokens();
    double ngood = mGoodCount, nbad = mBadCount, prob;

    for (i = 0; i < count; ++i) 
    {
        Token& token = tokens[i];
        const char* word = token.mWord;
        Token* t = mGoodTokens.get(word);
      double hamcount = ((t != NULL) ? t->mCount : 0);
        t = mBadTokens.get(word);
       double spamcount = ((t != NULL) ? t->mCount : 0);
       prob = (spamcount / nbad) / ( hamcount / ngood + spamcount / nbad);
       double n = hamcount + spamcount;
       prob =  (0.225 + n * prob) / (.45 + n);
       double distance = PR_ABS(prob - 0.5);
       if (distance >= .1) 
       {
         goodclues++;
         token.mDistance = distance;
         token.mProbability = prob;
            PR_LOG(BayesianFilterLogModule, PR_LOG_ALWAYS, ("token.mProbability (%s) is %f", word, token.mProbability));
        }
      else 
        token.mDistance = -1; //ignore clue
    }
    
    // sort the array by the token distances
        NS_QuickSort(tokens, count, sizeof(Token), compareTokens, NULL);
    PRUint32 first, last = count;
    first = (goodclues > 150) ? count - 150 : 0;

    double H = 1.0, S = 1.0;
    PRUint32 Hexp = 0, Sexp = 0;
    goodclues=0;
    int e;

    for (i = first; i < last; ++i) 
    {
      if (tokens[i].mDistance != -1) 
      {
        goodclues++;
        double value = tokens[i].mProbability;
        S *= (1.0 - value);
        H *= value;
        if ( S < 1e-200 ) 
        {
          S = frexp(S, &e);
          Sexp += e;
        }
        if ( H < 1e-200 ) 
        {
          H = frexp(H, &e);
          Hexp += e;
    }
    }
    }

    S = log(S) + Sexp * M_LN2;
    H = log(H) + Hexp * M_LN2;

    if (goodclues > 0) 
    {
      S = 1.0 - chi2Q(-2.0 * S, 2 * goodclues);
      H = 1.0 - chi2Q(-2.0 * H, 2 * goodclues);
      prob = (S-H +1.0) / 2.0;
    } 
    else 
      prob = 0.5;

    PRBool isJunk = (prob >= mJunkProbabilityThreshold);
    PR_LOG(BayesianFilterLogModule, PR_LOG_ALWAYS, ("%s is junk probability = (%f)  HAM SCORE:%f SPAM SCORE:%f", messageURI, prob,H,S));

    delete[] tokens;

    if (listener)
        listener->OnMessageClassified(messageURI, isJunk ? nsMsgJunkStatus(nsIJunkMailPlugin::JUNK) : nsMsgJunkStatus(nsIJunkMailPlugin::GOOD));
}

/* void shutdown (); */
NS_IMETHODIMP nsBayesianFilter::Shutdown()
{
    if (mTrainingDataDirty)
        writeTrainingData();
    return NS_OK;
}

/* readonly attribute boolean shouldDownloadAllHeaders; */
NS_IMETHODIMP nsBayesianFilter::GetShouldDownloadAllHeaders(PRBool *aShouldDownloadAllHeaders)
{
    // bayesian filters work on the whole msg body currently.
    *aShouldDownloadAllHeaders = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP nsBayesianFilter::StartBatch(void)
{
  PR_LOG(BayesianFilterLogModule, PR_LOG_ALWAYS, ("StartBatch() entered with mBatchLevel=%d", mBatchLevel));
  ++mBatchLevel;
  return NS_OK;
}

NS_IMETHODIMP nsBayesianFilter::EndBatch(void)
{
    PR_LOG(BayesianFilterLogModule, PR_LOG_ALWAYS, ("EndBatch() entered with mBatchLevel=%d", mBatchLevel));
    NS_ASSERTION(mBatchLevel > 0, "nsBayesianFilter::EndBatch() called with"
                 " mBatchLevel <= 0");
    --mBatchLevel;
    
    if (!mBatchLevel && mTrainingDataDirty)
        writeTrainingData();
    
    return NS_OK;
}

/* void classifyMessage (in string aMsgURL, in nsIJunkMailClassificationListener aListener); */
NS_IMETHODIMP nsBayesianFilter::ClassifyMessage(const char *aMessageURL, nsIMsgWindow *aMsgWindow, nsIJunkMailClassificationListener *aListener)
{
    MessageClassifier* analyzer = new MessageClassifier(this, aListener, aMsgWindow, 1, &aMessageURL);
    if (!analyzer) return NS_ERROR_OUT_OF_MEMORY;
    TokenStreamListener *tokenListener = new TokenStreamListener(analyzer);
    analyzer->setTokenListener(tokenListener);
    return tokenizeMessage(aMessageURL, aMsgWindow, analyzer);
}

/* void classifyMessages (in unsigned long aCount, [array, size_is (aCount)] in string aMsgURLs, in nsIJunkMailClassificationListener aListener); */
NS_IMETHODIMP nsBayesianFilter::ClassifyMessages(PRUint32 aCount, const char **aMsgURLs, nsIMsgWindow *aMsgWindow, nsIJunkMailClassificationListener *aListener)
{
    TokenAnalyzer* analyzer = new MessageClassifier(this, aListener, aMsgWindow, aCount, aMsgURLs);
    if (!analyzer) return NS_ERROR_OUT_OF_MEMORY;
    TokenStreamListener *tokenListener = new TokenStreamListener(analyzer);
    analyzer->setTokenListener(tokenListener);
    return tokenizeMessage(aMsgURLs[0], aMsgWindow, analyzer);
}

class MessageObserver : public TokenAnalyzer {
public:
    MessageObserver(nsBayesianFilter* filter,
                    nsMsgJunkStatus oldClassification,
                    nsMsgJunkStatus newClassification,
                    nsIJunkMailClassificationListener* listener)
        :   mFilter(filter), mSupports(filter), mListener(listener),
            mOldClassification(oldClassification),
            mNewClassification(newClassification)
    {
    }
    
    virtual void analyzeTokens(Tokenizer& tokenizer)
    {
        mFilter->observeMessage(tokenizer, mTokenSource.get(), mOldClassification,
                                mNewClassification, mListener);
        // release reference to listener, which will allow us to go away as well.
        mTokenListener = nsnull;

    }

private:
    nsBayesianFilter* mFilter;
    nsCOMPtr<nsISupports> mSupports;
    nsCOMPtr<nsIJunkMailClassificationListener> mListener;
    nsMsgJunkStatus mOldClassification;
    nsMsgJunkStatus mNewClassification;
};

static void forgetTokens(Tokenizer& corpus, TokenEnumeration tokens)
{
    // if we are forgetting the tokens for a message, should only 
    // subtract 1 from the occurrence count for that token in the training set
    // because we assume we only bumped the training set count once per messages
    // containing the token. 
    while (tokens.hasMoreTokens()) {
        Token* token = tokens.nextToken();
        corpus.remove(token->mWord);
    }
}

static void rememberTokens(Tokenizer& corpus, TokenEnumeration tokens)
{
    while (tokens.hasMoreTokens()) {
        Token* token = tokens.nextToken();
        corpus.add(token->mWord);
    }
}

void nsBayesianFilter::observeMessage(Tokenizer& tokenizer, const char* messageURL,
                                      nsMsgJunkStatus oldClassification, nsMsgJunkStatus newClassification,
                                      nsIJunkMailClassificationListener* listener)
{
    PR_LOG(BayesianFilterLogModule, PR_LOG_ALWAYS, ("observeMessage(%s) old=%d new=%d", messageURL, oldClassification, newClassification));
    TokenEnumeration tokens = tokenizer.getTokens();

    // Uhoh...if the user is re-training then the message may already be classified and we are classifying it again with the same classification.
    // the old code would have removed the tokens for this message then added them back. But this really hurts the message occurrence
    // count for tokens if you just removed training.dat and are re-training. See Bug #237095 for more details.
    // What can we do here? Well we can skip the token removal step if the classifications are the same and assume the user is
    // just re-training. But this then allows users to re-classify the same message on the same training set over and over again
    // leading to data skew. But that's all I can think to do right now to address this.....
    if (oldClassification != newClassification) 
    {
      // remove the tokens from the token set it is currently in
    switch (oldClassification) {
    case nsIJunkMailPlugin::JUNK:
        // remove tokens from junk corpus.
        if (mBadCount > 0) {
            --mBadCount;
            forgetTokens(mBadTokens, tokens);
            mTrainingDataDirty = PR_TRUE;
        }
        break;
    case nsIJunkMailPlugin::GOOD:
        // remove tokens from good corpus.
        if (mGoodCount > 0) {
            --mGoodCount;
            forgetTokens(mGoodTokens, tokens);
            mTrainingDataDirty = PR_TRUE;
        }
        break;
    }
    }

    
    switch (newClassification) {
    case nsIJunkMailPlugin::JUNK:
        // put tokens into junk corpus.
        ++mBadCount;
        rememberTokens(mBadTokens, tokens);
        mTrainingDataDirty = PR_TRUE;
        break;
    case nsIJunkMailPlugin::GOOD:
        // put tokens into good corpus.
        ++mGoodCount;
        rememberTokens(mGoodTokens, tokens);
        mTrainingDataDirty = PR_TRUE;
        break;
    }
    
    if (listener)
        listener->OnMessageClassified(messageURL, newClassification);

    if (mTrainingDataDirty && !mBatchLevel)
        writeTrainingData();
}

static nsresult getTrainingFile(nsCOMPtr<nsILocalFile>& file)
{
    // should we cache the profile manager's directory?
    nsCOMPtr<nsIFile> profileDir;

    nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(profileDir));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = profileDir->Append(NS_LITERAL_STRING("training.dat"));
    if (NS_FAILED(rv)) return rv;
    
    file = do_QueryInterface(profileDir, &rv);
    return rv;
}

/*
    Format of the training file for version 1:
    [0xFEEDFACE]
    [number good messages][number bad messages]
    [number good tokens]
    [count][length of word]word
    ...
    [number bad tokens]
    [count][length of word]word
    ...
 */

inline int writeUInt32(FILE* stream, PRUint32 value)
{
    value = PR_htonl(value);
    return fwrite(&value, sizeof(PRUint32), 1, stream);
}

inline int readUInt32(FILE* stream, PRUint32* value)
{
    int n = fread(value, sizeof(PRUint32), 1, stream);
    if (n == 1) {
        *value = PR_ntohl(*value);
    }
    return n;
}

static PRBool writeTokens(FILE* stream, Tokenizer& tokenizer)
{
    PRUint32 tokenCount = tokenizer.countTokens();
    if (writeUInt32(stream, tokenCount) != 1)
        return PR_FALSE;

    if (tokenCount > 0) {
        TokenEnumeration tokens = tokenizer.getTokens();
        for (PRUint32 i = 0; i < tokenCount; ++i) {
            Token* token = tokens.nextToken();
            if (writeUInt32(stream, token->mCount) != 1)
                break;
            PRUint32 tokenLength = token->mLength;
            if (writeUInt32(stream, tokenLength) != 1)
                break;
            if (fwrite(token->mWord, tokenLength, 1, stream) != 1)
                break;
        }
    }
    
    return PR_TRUE;
}

static PRBool readTokens(FILE* stream, Tokenizer& tokenizer)
{
    PRUint32 tokenCount;
    if (readUInt32(stream, &tokenCount) != 1)
        return PR_FALSE;

    PRUint32 bufferSize = 4096;
    char* buffer = new char[bufferSize];
    if (!buffer) return PR_FALSE;

    for (PRUint32 i = 0; i < tokenCount; ++i) {
        PRUint32 count;
        if (readUInt32(stream, &count) != 1)
            break;
        PRUint32 size;
        if (readUInt32(stream, &size) != 1)
            break;
        if (size >= bufferSize) {
            delete[] buffer;
            PRUint32 newBufferSize = 2 * bufferSize;
            while (size >= newBufferSize)
                newBufferSize *= 2;
            buffer = new char[newBufferSize];
            if (!buffer) return PR_FALSE;
            bufferSize = newBufferSize;
        }
        if (fread(buffer, size, 1, stream) != 1)
            break;
        buffer[size] = '\0';
        tokenizer.add(buffer, count);
    }
    
    delete[] buffer;
    
    return PR_TRUE;
}

static const char kMagicCookie[] = { '\xFE', '\xED', '\xFA', '\xCE' };

void nsBayesianFilter::writeTrainingData()
{
    PR_LOG(BayesianFilterLogModule, PR_LOG_ALWAYS, ("writeTrainingData() entered"));
    nsCOMPtr<nsILocalFile> file;
    nsresult rv = getTrainingFile(file);
    if (NS_FAILED(rv)) return;
 
    // open the file, and write out training data using fprintf for now.
    FILE* stream;
    rv = file->OpenANSIFileDesc("wb", &stream);
    if (NS_FAILED(rv)) return;

    if (!((fwrite(kMagicCookie, sizeof(kMagicCookie), 1, stream) == 1) &&
          (writeUInt32(stream, mGoodCount) == 1) &&
          (writeUInt32(stream, mBadCount) == 1) &&
           writeTokens(stream, mGoodTokens) &&
           writeTokens(stream, mBadTokens))) {
        NS_WARNING("failed to write training data.");
        fclose(stream);
        // delete the training data file, since it is potentially corrupt.
        file->Remove(PR_FALSE);
    } else {
        fclose(stream);
        mTrainingDataDirty = PR_FALSE;
    }
}

void nsBayesianFilter::readTrainingData()
{
    nsCOMPtr<nsILocalFile> file;
    nsresult rv = getTrainingFile(file);
    if (NS_FAILED(rv)) return;
    
    PRBool exists;
    rv = file->Exists(&exists);
    if (NS_FAILED(rv) || !exists) return;

    // open the file, and write out training data using fprintf for now.
    FILE* stream;
    rv = file->OpenANSIFileDesc("rb", &stream);
    if (NS_FAILED(rv)) return;

    // FIXME:  should make sure that the tokenizers are empty.
    char cookie[4];
    if (!((fread(cookie, sizeof(cookie), 1, stream) == 1) &&
          (memcmp(cookie, kMagicCookie, sizeof(cookie)) == 0) &&
          (readUInt32(stream, &mGoodCount) == 1) &&
          (readUInt32(stream, &mBadCount) == 1) &&
           readTokens(stream, mGoodTokens) &&
           readTokens(stream, mBadTokens))) {
        NS_WARNING("failed to read training data.");
        PR_LOG(BayesianFilterLogModule, PR_LOG_ALWAYS, ("failed to read training data."));
    }
    
    fclose(stream);
}

NS_IMETHODIMP nsBayesianFilter::GetUserHasClassified(PRBool *aResult)
{
  *aResult = (mGoodCount && mGoodTokens.countTokens() ||
              mBadCount && mBadTokens.countTokens());
  return NS_OK;
}

/* void setMessageClassification (in string aMsgURL, in long aOldClassification, in long aNewClassification); */
NS_IMETHODIMP nsBayesianFilter::SetMessageClassification(const char *aMsgURL,
                                                         nsMsgJunkStatus aOldClassification,
                                                         nsMsgJunkStatus aNewClassification,
                                                         nsIMsgWindow *aMsgWindow,
                                                         nsIJunkMailClassificationListener *aListener)
{
    MessageObserver* analyzer = new MessageObserver(this, aOldClassification, aNewClassification, aListener);
    if (!analyzer) return NS_ERROR_OUT_OF_MEMORY;
    TokenStreamListener *tokenListener = new TokenStreamListener(analyzer);
    analyzer->setTokenListener(tokenListener);
    return tokenizeMessage(aMsgURL, aMsgWindow, analyzer);
}

NS_IMETHODIMP nsBayesianFilter::ResetTrainingData()
{
  // clear out our in memory training tokens...
  if (mGoodCount && mGoodTokens.countTokens())
  {
    mGoodTokens.clearTokens();
    mGoodCount = 0;
  }

  if (mBadCount && mBadTokens.countTokens())
  {
    mBadTokens.clearTokens();
    mBadCount = 0;
  }

  // now remove training.dat
  nsCOMPtr<nsILocalFile> file;
  nsresult rv = getTrainingFile(file);
  if (file)
    file->Remove(PR_FALSE);

  return NS_OK;
}
