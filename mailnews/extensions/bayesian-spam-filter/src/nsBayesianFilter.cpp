/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "nsIMsgHdr.h"
#include "nsIMsgFilterHitNotify.h"
#include "nsIByteBuffer.h"
#include "nsNetUtil.h"
#include "nsQuickSort.h"
#include "nsIProfileInternal.h"

static const char* kBayesianFilterTokenDelimiters = " \t\n\r\f!\"#%&()*+,./:;<=>?@[\\]^_`{|}~";

class Token {
public:
    Token(const char* word, PRUint32 count = 0) : mWord(word), mCount(count), mProbability(0) {}
    Token(const Token& token) : mWord(token.mWord), mCount(token.mCount), mProbability(token.mProbability) {}
    ~Token() {}
    
    nsCString mWord;
    PRUint32 mCount;        // TODO:  put good/bad count values in same token object.
    double mProbability;    // TODO:  cache probabilities
};

static void* PR_CALLBACK cloneToken(nsHashKey *aKey, void *aData, void* aClosure)
{
    Token* token = (Token*) aData;
    return new Token(*token);
}

static PRIntn PR_CALLBACK destroyToken(nsHashKey *aKey, void *aData, void* aClosure)
{
    Token* token = (Token*) aData;
    delete token;
    return kHashEnumerateNext;
}

struct VisitClosure {
    bool (*f) (Token*, void*);
    void* data;
};

static PRIntn PR_CALLBACK visitToken(nsHashKey *aKey, void *aData, void* aClosure)
{
    Token* token = (Token*) aData;
    VisitClosure* closure = (VisitClosure*) aClosure;
    return (closure->f(token, closure->data) ?
            kHashEnumerateNext : kHashEnumerateStop);
}

Tokenizer::Tokenizer() : mTokens(cloneToken, NULL, destroyToken, NULL) {}
Tokenizer::~Tokenizer() {}

inline Token* Tokenizer::get(const char* word)
{
    nsCStringKey key(word);
    Token* token = (Token*) mTokens.Get(&key);
    return token;
}

Token* Tokenizer::add(const char* word, PRUint32 count)
{
    nsCStringKey key(word);
    Token* token = (Token*) mTokens.Get(&key);
    if (!token) {
        // FIXME:  share the word string with the hash table key!!!
        // do this by using nsDependentCString?
        token = new Token(word, count);
        if (token)
            mTokens.Put(&key, token);
    } else {
        token->mCount += count;
    }
    return token;
}

void Tokenizer::remove(const char* word, PRUint32 count)
{
    nsCStringKey key(word);
    Token* token = (Token*) mTokens.Get(&key);
    if (token) {
        NS_ASSERTION(token->mCount >= count, "token count underflow");
        token->mCount -= count;
        if (token->mCount == 0)
            mTokens.RemoveAndDelete(&key);
    }
}

static bool isDecimalNumber(const char* word)
{
    const char* p = word;
    if (*p == '-') ++p;
    char c;
    while ((c = *p++)) {
        if (!isdigit(c))
            return false;
    }
    return true;
}

static char* tolowercase(char* str)
{
    char* p = str;
    char c;
    while ((c = *p++)) {
        if (isalpha(c))
            p[-1] = tolower(c);
    }
    return str;
}

void Tokenizer::tokenize(char* text)
{
    char* word;
    char* next = text;
    while ((word = nsCRT::strtok(next, kBayesianFilterTokenDelimiters, &next)) != NULL) {
        if (isDecimalNumber(word)) continue;
        add(tolowercase(word));
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

void Tokenizer::visit(bool (*f) (Token*, void*), void* data)
{
    VisitClosure closure = { f, data };
    mTokens.Enumerate(visitToken, &closure);
}

struct GatherClosure {
    Token** tokens;
};
static bool gatherTokens(Token* token, void* data)
{
    GatherClosure* closure = (GatherClosure*) data;
    *closure->tokens++ = token;
    return true;
}

Token** Tokenizer::getTokens()
{
    PRUint32 count = countTokens();
    Token** tokens = new Token*[count];
    if (tokens) {
        GatherClosure closure = { tokens };
        visit(gatherTokens, &closure);
    }
    return tokens;
}

class TokenAnalyzer {
public:
    virtual ~TokenAnalyzer() {}
    
    virtual void analyzeTokens(const char* source, Tokenizer& tokens) = 0;
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
    
    TokenStreamListener(const char* tokenSource, TokenAnalyzer* analyzer);
    virtual ~TokenStreamListener();
    
protected:
    nsCString mTokenSource;
    TokenAnalyzer* mAnalyzer;
    nsCOMPtr<nsIByteBuffer> mBuffer;
    PRUint32 mBufferSize;
    PRUint32 mLeftOverCount;
    Tokenizer mTokenizer;
};

TokenStreamListener::TokenStreamListener(const char* tokenSource, TokenAnalyzer* analyzer)
    :   mTokenSource(tokenSource), mAnalyzer(analyzer),
        mBufferSize(8192), mLeftOverCount(0)
{
    NS_INIT_ISUPPORTS();
}

TokenStreamListener::~TokenStreamListener()
{
    delete mAnalyzer;
}

NS_IMPL_ISUPPORTS2(TokenStreamListener, nsIRequestObserver, nsIStreamListener)

/* void onStartRequest (in nsIRequest aRequest, in nsISupports aContext); */
NS_IMETHODIMP TokenStreamListener::OnStartRequest(nsIRequest *aRequest, nsISupports *aContext)
{
    return NS_NewByteBuffer(getter_AddRefs(mBuffer), NULL, mBufferSize);
}

/* void onDataAvailable (in nsIRequest aRequest, in nsISupports aContext, in nsIInputStream aInputStream, in unsigned long aOffset, in unsigned long aCount); */
NS_IMETHODIMP TokenStreamListener::OnDataAvailable(nsIRequest *aRequest, nsISupports *aContext, nsIInputStream *aInputStream, PRUint32 aOffset, PRUint32 aCount)
{
    nsresult rv;

    PRUint32 newBufferLength = (aCount + mLeftOverCount);
    if (newBufferLength > mBufferSize) {
        PRUint32 newBufferSize = newBufferLength * 2;
        rv = mBuffer->Grow(newBufferSize);
        if (NS_FAILED(rv))
            return rv;
        mBufferSize = newBufferSize;
    }
    
    char* buffer = mBuffer->GetBuffer();
    rv = aInputStream->Read(buffer + mLeftOverCount, aCount, &aCount);

    /* consume the tokens up to the last legal token delimiter in the buffer. */
    newBufferLength = (aCount + mLeftOverCount);
    buffer[newBufferLength] = '\0';
    char* last_delimiter = NULL;
    char* scan = buffer + newBufferLength;
    while (scan > buffer) {
        if (strchr(kBayesianFilterTokenDelimiters, *--scan)) {
            last_delimiter = scan;
            break;
        }
    }
    
    if (last_delimiter) {
        *last_delimiter = '\0';
        mTokenizer.tokenize(buffer);

        PRUint32 consumedCount = 1 + (last_delimiter - buffer);
        mLeftOverCount = newBufferLength - consumedCount;
        if (mLeftOverCount)
            memmove(buffer, buffer + consumedCount, mLeftOverCount);
    }

    return rv;
}

/* void onStopRequest (in nsIRequest aRequest, in nsISupports aContext, in nsresult aStatusCode); */
NS_IMETHODIMP TokenStreamListener::OnStopRequest(nsIRequest *aRequest, nsISupports *aContext, nsresult aStatusCode)
{
    if (mLeftOverCount) {
        /* assume final buffer is complete. */
        char* buffer = mBuffer->GetBuffer();
        buffer[mLeftOverCount] = '\0';
        mTokenizer.tokenize(buffer);
    }
    
    /* finally, analyze the tokenized message. */
    if (mAnalyzer)
        mAnalyzer->analyzeTokens(mTokenSource.get(), mTokenizer);
    
    return NS_OK;
}

/* Implementation file */
NS_IMPL_ISUPPORTS2(nsBayesianFilter, nsIMsgFilterPlugin, nsIJunkMailPlugin)

nsBayesianFilter::nsBayesianFilter()
    :   mGoodCount(0), mBadCount(0),
        mServerPrefsKey(NULL), mBatchUpdate(PR_FALSE), mTrainingDataDirty(PR_FALSE)
{
    NS_INIT_ISUPPORTS();
    
    // should probably wait until Init() is called to do this.
    readTrainingData();
}

nsBayesianFilter::~nsBayesianFilter()
{
    delete mServerPrefsKey;
}

/* void init (in ACString aServerPrefsKey); */
NS_IMETHODIMP nsBayesianFilter::Init(const nsACString & aServerPrefsKey)
{
    mServerPrefsKey = new nsCString(aServerPrefsKey);
    return NS_OK;
}

class MessageClassifier : public TokenAnalyzer {
public:
    MessageClassifier(nsBayesianFilter* filter, nsIJunkMailClassificationListener* listener)
        :   mFilter(filter), mSupports(filter), mListener(listener)
    {
    }
    
    virtual void analyzeTokens(const char* source, Tokenizer& tokens)
    {
        mFilter->classifyMessage(tokens, source, mListener);
    }

private:
    nsBayesianFilter* mFilter;
    nsCOMPtr<nsISupports> mSupports;
    nsCOMPtr<nsIJunkMailClassificationListener> mListener;
};

/* void filterMessage (in string aMsgURL, in nsIMsgDBHdr aMsgHdr, in unsigned long aCount, [array, size_is (aCount)] in string aHeaders, in nsIMsgFilterHitNotify aListener, in nsIMsgWindow aMsgWindow); */
NS_IMETHODIMP nsBayesianFilter::FilterMessage(const char *aMsgURL, nsIMsgDBHdr *aMsgHdr, PRUint32 aCount,
                                              const char **aHeaders, nsIMsgFilterHitNotify *aListener, nsIMsgWindow *aMsgWindow)
{   
    TokenAnalyzer* analyzer = new MessageClassifier(this, NULL);
    return tokenizeMessage(aMsgURL, analyzer);
}

nsresult nsBayesianFilter::tokenizeMessage(const char* messageURL, TokenAnalyzer* analyzer)
{
    nsresult rv;
    nsCOMPtr<nsIIOService> ioService = do_GetIOService(&rv);
    if (NS_FAILED(rv)) return rv;
    
    nsCOMPtr<nsIChannel> channel;
    rv = ioService->NewChannel(nsCString(messageURL), NULL, NULL, getter_AddRefs(channel));
    if (NS_FAILED(rv)) return rv;
    
    nsCOMPtr<nsIStreamListener> tokenListener = new TokenStreamListener(messageURL, analyzer);
    rv = channel->AsyncOpen(tokenListener, NULL);
    return rv;
}

inline double abs(double x) { return (x >= 0 ? x : -x); }

static int compareTokens(const void* p1, const void* p2, void* /* data */)
{
    Token *t1 = *(Token**) p1, *t2 = *(Token**) p2;
    double delta = abs(t1->mProbability - 0.5) - abs(t2->mProbability - 0.5);
    return (delta == 0.0 ? 0 : (delta > 0.0 ? 1 : -1));
}

inline double max(double x, double y) { return (x > y ? x : y); }
inline double min(double x, double y) { return (x < y ? x : y); }

void nsBayesianFilter::classifyMessage(Tokenizer& messageTokens, const char* messageURL,
                                       nsIJunkMailClassificationListener* listener)
{
    /* run the kernel of the Graham filter algorithm here. */
    Token ** tokens = messageTokens.getTokens();
    if (!tokens) return;
    
    PRUint32 i, count = messageTokens.countTokens();
    double ngood = mGoodCount, nbad = mBadCount;
    for (i = 0; i < count; ++i) {
        Token* token = tokens[i];
        // ((g (* 2 (or (gethash word good) 0)))
        Token* t = mGoodTokens.get(token->mWord.get());
        double g = 2.0 * ((t != NULL) ? t->mCount : 0);
        // (b (or (gethash word bad) 0)))
        t = mBadTokens.get(token->mWord.get());
        double b = ((t != NULL) ? t->mCount : 0);
        if ((g + b) > 5) {
            // (max .01
            //      (min .99 (float (/ (min 1 (/ b nbad))
            //                         (+ (min 1 (/ g ngood))
            //                            (min 1 (/ b nbad)))))))
            token->mProbability = max(.01,
                                     min(.99,
                                         (min(1.0, (b / nbad)) /
                                              (min(1.0, (g / ngood)) +
                                               min(1.0, (b / nbad))))));
        } else {
            token->mProbability = 0.4;
        }
    }
    
    // sort the array by the distance of the token probabilities from a 50-50 value of 0.5.
    PRUint32 first, last = count;
    if (count > 15) {
        first = count - 15;
        NS_QuickSort(tokens, count, sizeof(Token*), compareTokens, NULL);
    } else {
        first = 0;
    }

    double prod1 = 1.0, prod2 = 1.0;
    for (i = first; i < last; ++i) {
        double value = tokens[i]->mProbability;
        prod1 *= value;
        prod2 *= (1.0 - value);
    }
    double prob = (prod1 / (prod1 + prod2));
    bool isJunk = (prob >= 0.90);
    
    if (listener)
        listener->OnMessageClassified(messageURL, isJunk ? nsIJunkMailPlugin::JUNK : nsIJunkMailPlugin::GOOD);
    
    delete[] tokens;
}

/* void shutdown (); */
NS_IMETHODIMP nsBayesianFilter::Shutdown()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute boolean shouldDownloadAllHeaders; */
NS_IMETHODIMP nsBayesianFilter::GetShouldDownloadAllHeaders(PRBool *aShouldDownloadAllHeaders)
{
    *aShouldDownloadAllHeaders = PR_TRUE;
    return NS_OK;
}

/* attribute boolean batchUpdate; */
NS_IMETHODIMP nsBayesianFilter::GetBatchUpdate(PRBool *aBatchUpdate)
{
    *aBatchUpdate = mBatchUpdate;
    return NS_OK;
}
NS_IMETHODIMP nsBayesianFilter::SetBatchUpdate(PRBool aBatchUpdate)
{
    mBatchUpdate = aBatchUpdate;
    
    if (mBatchUpdate && mTrainingDataDirty)
        writeTrainingData();
        
    return NS_OK;
}

/* void classifyMessage (in string aMsgURL, in nsIJunkMailClassificationListener aListener); */
NS_IMETHODIMP nsBayesianFilter::ClassifyMessage(const char *aMessageURL, nsIJunkMailClassificationListener *aListener)
{
    TokenAnalyzer* analyzer = new MessageClassifier(this, aListener);
    return tokenizeMessage(aMessageURL, analyzer);
}

/* void classifyMessages (in unsigned long aCount, [array, size_is (aCount)] in string aMsgURLs, in nsIJunkMailClassificationListener aListener); */
NS_IMETHODIMP nsBayesianFilter::ClassifyMessages(PRUint32 aCount, const char **aMsgURLs, nsIJunkMailClassificationListener *aListener)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

class MessageObserver : public TokenAnalyzer {
public:
    MessageObserver(nsBayesianFilter* filter, PRInt32 oldClassification, PRInt32 newClassification, nsIJunkMailClassificationListener* listener)
        :   mFilter(filter), mSupports(filter), mOldClassification(oldClassification),
            mNewClassification(newClassification), mListener(listener)
    {
    }
    
    virtual void analyzeTokens(const char* source, Tokenizer& tokens)
    {
        mFilter->observeMessage(tokens, source, mOldClassification, mNewClassification, mListener);
    }

private:
    nsBayesianFilter* mFilter;
    nsCOMPtr<nsISupports> mSupports;
    nsCOMPtr<nsIJunkMailClassificationListener> mListener;
    PRInt32 mOldClassification;
    PRInt32 mNewClassification;
};

static void forgetTokens(Tokenizer& corpus, Token* tokens[], PRUint32 count)
{
    for (PRUint32 i = 0; i < count; ++i) {
        Token* token = tokens[i];
        corpus.remove(token->mWord.get(), token->mCount);
    }
}

static void rememberTokens(Tokenizer& corpus, Token* tokens[], PRUint32 count)
{
    for (PRUint32 i = 0; i < count; ++i) {
        Token* token = tokens[i];
        corpus.add(token->mWord.get(), token->mCount);
    }
}

void nsBayesianFilter::observeMessage(Tokenizer& messageTokens, const char* messageURL, PRInt32 oldClassification,
                                      PRInt32 newClassification, nsIJunkMailClassificationListener* listener)
{
    Token** tokens = messageTokens.getTokens();
    if (!tokens) return;
    
    PRUint32 count = messageTokens.countTokens();
    switch (oldClassification) {
    case nsIJunkMailPlugin::JUNK:
        // remove tokens from junk corpus.
        if (mBadCount > 0) {
            --mBadCount;
            forgetTokens(mBadTokens, tokens, count);
            mTrainingDataDirty = PR_TRUE;
        }
        break;
    case nsIJunkMailPlugin::GOOD:
        // remove tokens from good corpus.
        if (mGoodCount > 0) {
            --mGoodCount;
            forgetTokens(mGoodTokens, tokens, count);
            mTrainingDataDirty = PR_TRUE;
        }
        break;
    }
    switch (newClassification) {
    case nsIJunkMailPlugin::JUNK:
        // put tokens into junk corpus.
        ++mBadCount;
        rememberTokens(mBadTokens, tokens, count);
        mTrainingDataDirty = PR_TRUE;
        break;
    case nsIJunkMailPlugin::GOOD:
        // put tokens into good corpus.
        ++mGoodCount;
        rememberTokens(mGoodTokens, tokens, count);
        mTrainingDataDirty = PR_TRUE;
        break;
    }
    
    delete[] tokens;
    
    if (listener)
        listener->OnMessageClassified(messageURL, newClassification);
        
    if (mTrainingDataDirty && !mBatchUpdate)
        writeTrainingData();
}

/*
        var profileMgr = do_GetService(PROFILEMGR_CTRID, nsIProfileInternal);
        var outFile = profileMgr.getProfileDir(profileMgr.currentProfile);
        outFile.append("spam.db");
        var fileTransportService = do_GetService(FILETPTSVC_CTRID, nsIFileTransportService);
        const ioFlags = NS_WRONLY | NS_CREATE_FILE | NS_TRUNCATE;
        var trans = fileTransportService.createTransport(outFile, ioFlags, 'w', true);
        var out = trans.openOutputStream(0, -1, 0);

        var totals = this.mHamCount.toString() + "\t" +
                     this.mSpamCount.toString() + "\n";
        out.write(totals, totals.length);
        for (token in this.mHash) {
            var record = this.mHash[token];
            var tokenString = token + "\t" + record[kHamCount].toString() +
                              "\t" + record[kSpamCount].toString() +
                              "\n";
//                              "\t" + record[kTime].toString() + "\n";
            out.write(tokenString, tokenString.length);
        }

        out.close();
*/

static nsresult getTrainingFile(nsCOMPtr<nsIFile>& file)
{
    // should we cache the profile manager's directory?
    nsresult rv;
    nsCOMPtr<nsIProfileInternal> profileManager = do_GetService("@mozilla.org/profile/manager;1", &rv);
    if (NS_FAILED(rv)) return rv;
    
    nsXPIDLString currentProfile;
    rv = profileManager->GetCurrentProfile(getter_Copies(currentProfile));
    if (NS_FAILED(rv)) return rv;
    
    rv = profileManager->GetProfileDir(currentProfile.get(), getter_AddRefs(file));
    if (NS_FAILED(rv)) return rv;
    
    return file->Append(NS_LITERAL_STRING("training.txt"));
}

static void writeTokens(FILE* stream, Tokenizer& tokenizer)
{
    PRUint32 i, count = tokenizer.countTokens();
    Token ** tokens = tokenizer.getTokens();
    if (!tokens) return;

    // compute the maximum word length, so we can use a fixed buffer size when reading back in.
    PRUint32 maxWordLength = 0;
    for (i = 0; i < count; ++i) {
        PRUint32 wordLength = tokens[i]->mWord.Length();
        if (wordLength > maxWordLength)
            maxWordLength = wordLength;
    }

    fprintf(stream, "count = %lu, maxWordLength = %lu\n", count, maxWordLength);
    
    for (PRUint32 i = 0; i < count; ++i) {
        Token* token = tokens[i];
        fprintf(stream, "%s : %lu\n", token->mWord.get(), token->mCount);
    }
    
    delete[] tokens;
}

static void readTokens(FILE* stream, Tokenizer& tokenizer)
{
    PRUint32 count, maxWordLength;
    fscanf(stream, "count = %lu, maxWordLength = %lu\n", &count, &maxWordLength);

    char* wordBuffer = new char[maxWordLength + 1];
    if (!wordBuffer) return;

    PRUint32 wordCount;
    
    for (PRUint32 i = 0; i < count; ++i) {
        if (fscanf(stream, "%s : %lu\n", wordBuffer, &wordCount) > 0)
            tokenizer.add(wordBuffer, wordCount);
    }
    
    delete[] wordBuffer;
}

void nsBayesianFilter::writeTrainingData()
{
    nsCOMPtr<nsIFile> file;
    nsresult rv = getTrainingFile(file);
    if (NS_FAILED(rv)) return;
 
    // open the file, and write out training data using fprintf for now.
    nsCOMPtr<nsILocalFile> localFile = do_QueryInterface(file, &rv);
    if (NS_FAILED(rv)) return;
    
    FILE* stream;
    rv = localFile->OpenANSIFileDesc("w", &stream);
    if (NS_FAILED(rv)) return;

    fprintf(stream, "ngood = %lu, nbad = %lu\n", mGoodCount, mBadCount);
    writeTokens(stream, mGoodTokens);
    writeTokens(stream, mBadTokens);
    
    fclose(stream);
    
    mTrainingDataDirty = PR_FALSE;
}

void nsBayesianFilter::readTrainingData()
{
    nsCOMPtr<nsIFile> file;
    nsresult rv = getTrainingFile(file);
    if (NS_FAILED(rv)) return;

    // open the file, and write out training data using fprintf for now.
    nsCOMPtr<nsILocalFile> localFile = do_QueryInterface(file, &rv);
    if (NS_FAILED(rv)) return;
    
    FILE* stream;
    rv = localFile->OpenANSIFileDesc("r", &stream);
    if (NS_FAILED(rv)) return;

    // FIXME:  should make sure that the tokenizers are empty.
    fscanf(stream, "ngood = %lu, nbad = %lu\n", &mGoodCount, &mBadCount);
    readTokens(stream, mGoodTokens);
    readTokens(stream, mBadTokens);
    
    fclose(stream);
}

/* void setMessageClassification (in string aMsgURL, in long aOldClassification, in long aNewClassification); */
NS_IMETHODIMP nsBayesianFilter::SetMessageClassification(const char *aMsgURL, PRInt32 aOldClassification,
                                                         PRInt32 aNewClassification, nsIJunkMailClassificationListener *aListener)
{
    MessageObserver* analyzer = new MessageObserver(this, aOldClassification, aNewClassification, aListener);
    return tokenizeMessage(aMsgURL, analyzer);
}
