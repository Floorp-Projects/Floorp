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

#ifndef nsBayesianFilter_h__
#define nsBayesianFilter_h__

#include "nsCOMPtr.h"
#include "nsHashtable.h"
#include "nsIMsgFilterPlugin.h"

class Token;
class TokenAnalyzer;

class Tokenizer {
public:
    Tokenizer();
    ~Tokenizer();
    
    Token* get(const char* word);
    Token* add(const char* word, PRUint32 count = 1);
    void remove(const char* word, PRUint32 count = 1);
    
    PRUint32 countTokens() { return mTokens.Count(); }
    Token** getTokens();

    /**
     * Assumes that text is mutable and
     * can be nsCRT::strtok'd.
     */
    void tokenize(char* text);
    
    /**
     * Copies the string before tokenizing.
     */
    void tokenize(const char* str);
    
    /**
     * Calls passed-in function for each token in the table.
     */
    void visit(bool (*f) (Token*, void*), void* data);

private:
    nsObjectHashtable mTokens;
};

class nsBayesianFilter : public nsIJunkMailPlugin {
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIMSGFILTERPLUGIN
    NS_DECL_NSIJUNKMAILPLUGIN
    
    nsBayesianFilter();
    virtual ~nsBayesianFilter();
    
    nsresult tokenizeMessage(const char* messageURI, TokenAnalyzer* analyzer);
    void classifyMessage(Tokenizer& messageTokens, const char* messageURI, nsIJunkMailClassificationListener* listener);
    void observeMessage(Tokenizer& messageTokens, const char* messageURI, PRInt32 oldClassification, PRInt32 newClassification, nsIJunkMailClassificationListener* listener);

    void writeTrainingData();
    void readTrainingData();
    
protected:
    Tokenizer mGoodTokens, mBadTokens;
    PRUint32 mGoodCount, mBadCount;
    nsACString* mServerPrefsKey;
    PRPackedBool mBatchUpdate;
    PRPackedBool mTrainingDataDirty;
};

#endif // _nsBayesianFilter_h__
