/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Patrick C. Beard <beard@netscape.com>
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

#include "nsCRT.h"
#include "nsBayesianFilter.h"
#include "nsIInputStream.h"
#include "nsIStreamListener.h"
#include "nsNetUtil.h"
#include "nsQuickSort.h"
#include "nsIProfileInternal.h"
#include "nsIMsgMessageService.h"
#include "nsMsgUtils.h" // for GetMessageServiceFromURI
#include "prnetdb.h"
#include "nsIMsgWindow.h"

static const char* kBayesianFilterTokenDelimiters = " \t\n\r\f!\"#%&()*+,./:;<=>?@[\\]^_`{|}~";

struct Token : public PLDHashEntryHdr {
    const char* mWord;
    PRUint32 mLength;
    PRUint32 mCount;            // TODO:  put good/bad count values in same token object.
    double mProbability;        // TODO:  cache probabilities
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

// PLDHashTable operation callbacks

static const void* PR_CALLBACK GetKey(PLDHashTable* table, PLDHashEntryHdr* entry)
{
    return NS_STATIC_CAST(Token*, entry)->mWord;
}

static PRBool PR_CALLBACK MatchEntry(PLDHashTable* table,
                                     const PLDHashEntryHdr* entry,
                                     const void* key)
{
    const Token* token = NS_STATIC_CAST(const Token*, entry);
    return (strcmp(token->mWord, NS_REINTERPRET_CAST(const char*, key)) == 0);
}

static void PR_CALLBACK MoveEntry(PLDHashTable* table,
                                  const PLDHashEntryHdr* from,
                                  PLDHashEntryHdr* to)
{
    const Token* fromToken = NS_STATIC_CAST(const Token*, from);
    Token* toToken = NS_STATIC_CAST(Token*, to);
    NS_ASSERTION(fromToken->mLength != 0, "zero length token in table!");
    *toToken = *fromToken;
}

static void PR_CALLBACK ClearEntry(PLDHashTable* table, PLDHashEntryHdr* entry)
{
    // We use the mWord field to further indicate liveness when using PL_DHASH_ADD.
    // So we simply clear it when an entry is removed.
    Token* token = NS_STATIC_CAST(Token*, entry);
    token->mWord = NULL;
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
static PLDHashTableOps gTokenTableOps = {
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    GetKey,
    PL_DHashStringKey,      // Save the extra layer of function call to PL_DHashStringKey.
    MatchEntry,
    MoveEntry,
    ClearEntry,
    PL_DHashFinalizeStub
};

Tokenizer::Tokenizer()
{
    PL_INIT_ARENA_POOL(&mWordPool, "Words Arena", 16384);
    PRBool ok = PL_DHashTableInit(&mTokenTable, &gTokenTableOps, nsnull, sizeof(Token), 256);
    NS_ASSERTION(ok, "mTokenTable failed to initialize");
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
    PLDHashEntryHdr* entry = PL_DHashTableOperate(&mTokenTable, word, PL_DHASH_ADD);
    Token* token = NS_STATIC_CAST(Token*, entry);
    if (token) {
        if (token->mWord == NULL) {
            PRUint32 len = strlen(word);
            NS_ASSERTION(len != 0, "adding zero length word to tokenizer");
            token->mWord = copyWord(word, len);
            NS_ASSERTION(token->mWord, "copyWord failed");
            if (!token->mWord) {
                PL_DHashTableRawRemove(&mTokenTable, entry);
                return NULL;
            }
            token->mLength = len;
            token->mCount = count;
            token->mProbability = 0;
        } else {
            token->mCount += count;
        }
    }
    return token;
}

void Tokenizer::remove(const char* word, PRUint32 count)
{
    Token* token = get(word);
    if (token) {
        NS_ASSERTION(token->mCount >= count, "token count underflow");
        if (token->mCount >= count) {
            token->mCount -= count;
            if (token->mCount == 0)
                PL_DHashTableRawRemove(&mTokenTable, token);
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

void Tokenizer::tokenize(char* text)
{
    char* word;
    char* next = text;
    while ((word = nsCRT::strtok(next, kBayesianFilterTokenDelimiters, &next)) != NULL) {
        if (word[0] == '\0') continue;
        if (isDecimalNumber(word)) continue;
        if (isASCII(word))
            add(toLowerCase(word));
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
class TokenStreamListener : public nsIStreamListener {
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER
    
    TokenStreamListener(TokenAnalyzer* analyzer);
    virtual ~TokenStreamListener();
protected:
    TokenAnalyzer* mAnalyzer;
    char* mBuffer;
    PRUint32 mBufferSize;
    PRUint32 mLeftOverCount;
    Tokenizer mTokenizer;
};

const PRUint32 kBufferSize = 16384;

TokenStreamListener::TokenStreamListener(TokenAnalyzer* analyzer)
    :   mAnalyzer(analyzer),
        mBuffer(NULL), mBufferSize(kBufferSize), mLeftOverCount(0)
{
}

TokenStreamListener::~TokenStreamListener()
{
    delete[] mBuffer;
    delete mAnalyzer;
}

NS_IMPL_ISUPPORTS2(TokenStreamListener, nsIRequestObserver, nsIStreamListener)

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
    PRBool ok = (mGoodTokens && mBadTokens);
    NS_ASSERTION(ok, "error allocating tokenizers");
    if (ok)
        readTrainingData();
}

nsBayesianFilter::~nsBayesianFilter() {}

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
      for (PRInt32 i = 0; i < aNumMessagesToClassify; i++)
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
      if (++mCurMessageToClassify < mNumMessagesToClassify && mMessageURIs[mCurMessageToClassify])
        mFilter->tokenizeMessage(mMessageURIs[mCurMessageToClassify], mMsgWindow, this);
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

inline double abs(double x) { return (x >= 0 ? x : -x); }

PR_STATIC_CALLBACK(int) compareTokens(const void* p1, const void* p2, void* /* data */)
{
    Token *t1 = (Token*) p1, *t2 = (Token*) p2;
    double delta = abs(t1->mProbability - 0.5) - abs(t2->mProbability - 0.5);
    return (delta == 0.0 ? 0 : (delta > 0.0 ? 1 : -1));
}

inline double dmax(double x, double y) { return (x > y ? x : y); }
inline double dmin(double x, double y) { return (x < y ? x : y); }

void nsBayesianFilter::classifyMessage(Tokenizer& tokenizer, const char* messageURI,
                                       nsIJunkMailClassificationListener* listener)
{
    Token* tokens = tokenizer.copyTokens();
    if (!tokens) return;
    
    /* run the kernel of the Graham filter algorithm here. */
    PRUint32 i, count = tokenizer.countTokens();
    double ngood = mGoodCount, nbad = mBadCount;
    for (i = 0; i < count; ++i) {
        Token& token = tokens[i];
        const char* word = token.mWord;
        // ((g (* 2 (or (gethash word good) 0)))
        Token* t = mGoodTokens.get(word);
        double g = 2.0 * ((t != NULL) ? t->mCount : 0);
        // (b (or (gethash word bad) 0)))
        t = mBadTokens.get(word);
        double b = ((t != NULL) ? t->mCount : 0);
        if ((g + b) > 5) {
            // (max .01
            //      (min .99 (float (/ (min 1 (/ b nbad))
            //                         (+ (min 1 (/ g ngood))
            //                            (min 1 (/ b nbad)))))))
            token.mProbability = dmax(.01,
                                     dmin(.99,
                                         (dmin(1.0, (b / nbad)) /
                                              (dmin(1.0, (g / ngood)) +
                                               dmin(1.0, (b / nbad))))));
        } else {
            token.mProbability = 0.4;
        }
    }
    
    // sort the array by the distance of the token probabilities from a 50-50 value of 0.5.
    PRUint32 first, last = count;
    if (count > 15) {
        first = count - 15;
        NS_QuickSort(tokens, count, sizeof(Token), compareTokens, NULL);
    } else {
        first = 0;
    }

    double prod1 = 1.0, prod2 = 1.0;
    for (i = first; i < last; ++i) {
        double value = tokens[i].mProbability;
        prod1 *= value;
        prod2 *= (1.0 - value);
    }
    double prob = (prod1 / (prod1 + prod2));
    PRBool isJunk = (prob >= 0.90);

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
#ifdef DEBUG_dmose
    printf("StartBatch() entered with mBatchLevel=%d\n", mBatchLevel);
#endif
    ++mBatchLevel;
    return NS_OK;
}

NS_IMETHODIMP nsBayesianFilter::EndBatch(void)
{
#ifdef DEBUG_dmose
    printf("EndBatch() entered with mBatchLevel=%d\n", mBatchLevel);
#endif
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
    while (tokens.hasMoreTokens()) {
        Token* token = tokens.nextToken();
        corpus.remove(token->mWord, token->mCount);
    }
}

static void rememberTokens(Tokenizer& corpus, TokenEnumeration tokens)
{
    while (tokens.hasMoreTokens()) {
        Token* token = tokens.nextToken();
        corpus.add(token->mWord, token->mCount);
    }
}

void nsBayesianFilter::observeMessage(Tokenizer& tokenizer, const char* messageURL,
                                      nsMsgJunkStatus oldClassification, nsMsgJunkStatus newClassification,
                                      nsIJunkMailClassificationListener* listener)
{
    TokenEnumeration tokens = tokenizer.getTokens();
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
    nsresult rv;
    nsCOMPtr<nsIProfileInternal> profileManager = do_GetService("@mozilla.org/profile/manager;1", &rv);
    if (NS_FAILED(rv)) return rv;
    
    nsXPIDLString currentProfile;
    rv = profileManager->GetCurrentProfile(getter_Copies(currentProfile));
    if (NS_FAILED(rv)) return rv;
    
    nsCOMPtr<nsIFile> profileDir;
    rv = profileManager->GetProfileDir(currentProfile.get(), getter_AddRefs(profileDir));
    if (NS_FAILED(rv)) return rv;
    
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
#ifdef DEBUG_dmose
    printf("writeTrainingData() entered\n");
#endif
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
    }
    
    fclose(stream);
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
