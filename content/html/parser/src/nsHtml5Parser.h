/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   Henri Sivonen <hsivonen@iki.fi>
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
 
#ifndef NS_HTML5_PARSER__
#define NS_HTML5_PARSER__

#include "nsTimer.h"
#include "nsIParser.h"
#include "nsDeque.h"
#include "nsIURL.h"
#include "nsParserCIID.h"
#include "nsITokenizer.h"
#include "nsThreadUtils.h"
#include "nsIContentSink.h"
#include "nsIParserFilter.h"
#include "nsIRequest.h"
#include "nsIChannel.h"
#include "nsCOMArray.h"
#include "nsContentSink.h"
#include "nsIHTMLDocument.h"
#include "nsIUnicharStreamListener.h"
#include "nsCycleCollectionParticipant.h"
#include "nsAutoPtr.h"
#include "nsIInputStream.h"
#include "nsIUnicodeDecoder.h"
#include "nsHtml5UTF16Buffer.h"

#define NS_HTML5_PARSER_CID \
  {0x3113adb0, 0xe56d, 0x459e, \
    {0xb9, 0x5b, 0xf1, 0xf2, 0x4a, 0xba, 0x2a, 0x80}}

#define NS_HTML5_PARSER_READ_BUFFER_SIZE 1024

enum eHtml5ParserLifecycle {
  NOT_STARTED = 0,
  PARSING = 1,
  STREAM_ENDING = 2,
  TERMINATED = 3
};

class nsHtml5Parser : public nsIParser,
                      public nsIStreamListener,
                      public nsIContentSink,
                      public nsContentSink {

  
  public:
    NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW
    NS_DECL_ISUPPORTS
    
    nsHtml5Parser();

    virtual ~nsHtml5Parser();

    /* Start nsIParser */

    /**
     * No-op for backwards compat.
     */
    NS_IMETHOD_(void) SetContentSink(nsIContentSink* aSink);

    /**
     * Returns |this| for backwards compat.
     */
    NS_IMETHOD_(nsIContentSink*) GetContentSink(void);
    
    /**
     * Methods for backwards compat.
     */
    NS_IMETHOD_(void) GetCommand(nsCString& aCommand);
    NS_IMETHOD_(void) SetCommand(const char* aCommand);
    NS_IMETHOD_(void) SetCommand(eParserCommands aParserCommand);

    /**
     *  Call this method once you've created a parser, and want to instruct it
     *  about what charset to load
     *  
     *  @update  ftang 4/23/99
     *  @param   aCharset- the charset of a document
     *  @param   aCharsetSource- the source of the charset
     *  @return	 nada
     */
    NS_IMETHOD_(void) SetDocumentCharset(const nsACString& aCharset, PRInt32 aSource);

    NS_IMETHOD_(void) GetDocumentCharset(nsACString& aCharset, PRInt32& aSource)
    {
         aCharset = mCharset;
         aSource = mCharsetSource;
    }

    /**
     * No-op for backwards compat.
     */
    NS_IMETHOD_(void) SetParserFilter(nsIParserFilter* aFilter);


    /** 
     * Get the channel associated with this parser
     * @update harishd,gagan 07/17/01
     * @param aChannel out param that will contain the result
     * @return NS_OK if successful
     */
    NS_IMETHOD GetChannel(nsIChannel** aChannel);
    
    /** 
     * Return |this| for backwards compat.
     */
    NS_IMETHOD GetDTD(nsIDTD** aDTD);
    
    NS_IMETHOD        ContinueParsing();
    NS_IMETHOD        ContinueInterruptedParsing();
    NS_IMETHOD_(void) BlockParser();
    NS_IMETHOD_(void) UnblockParser();
    
    /**
     * Call this to query whether the parser is enabled or not.
     *
     *  @update  vidur 4/12/99
     *  @return  current state
     */
    NS_IMETHOD_(PRBool) IsParserEnabled();

    /**
     * Call this to query whether the parser thinks it's done with parsing.
     *
     *  @update  rickg 5/12/01
     *  @return  complete state
     */
    NS_IMETHOD_(PRBool) IsComplete();
    
    /**
     * Cause parser to parse input from given URL 
     * @update	gess5/11/98
     * @param   aURL is a descriptor for source document
     * @param   aListener is a listener to forward notifications to
     * @return  TRUE if all went well -- FALSE otherwise
     */
    NS_IMETHOD Parse(nsIURI* aURL,
                     nsIRequestObserver* aListener = nsnull,
                     void* aKey = 0,
                     nsDTDMode aMode = eDTDMode_autodetect);

    /**
     * @update	gess5/11/98
     * @param   anHTMLString contains a string-full of real HTML
     * @param   appendTokens tells us whether we should insert tokens inline, or append them.
     * @return  TRUE if all went well -- FALSE otherwise
     */
    NS_IMETHOD Parse(const nsAString& aSourceBuffer,
                     void* aKey,
                     const nsACString& aContentType,
                     PRBool aLastCall,
                     nsDTDMode aMode = eDTDMode_autodetect);

    NS_IMETHOD_(void *) GetRootContextKey();

    NS_IMETHOD        Terminate(void);

    /**
     * This method needs documentation
     */
    NS_IMETHOD ParseFragment(const nsAString& aSourceBuffer,
                             void* aKey,
                             nsTArray<nsString>& aTagStack,
                             PRBool aXMLMode,
                             const nsACString& aContentType,
                             nsDTDMode aMode = eDTDMode_autodetect);


    /**
     * This method gets called when the tokens have been consumed, and it's time
     * to build the model via the content sink.
     * @update	gess5/11/98
     * @return  YES if model building went well -- NO otherwise.
     */
    NS_IMETHOD BuildModel(void);

    /**
     *  Retrieve the scanner from the topmost parser context
     *  
     *  @update  gess 6/9/98
     *  @return  ptr to scanner
     */
    NS_IMETHOD_(nsDTDMode) GetParseMode(void);

    /**
     *  Removes continue parsing events
     *  @update  kmcclusk 5/18/98
     */

    NS_IMETHODIMP CancelParsingEvents();
    
    virtual void Reset();
    
    /**
     * Tells the parser that a script is now executing. The only data we
     * should resume parsing for is document.written data. We'll deal with any
     * data that comes in over the network later.
     */
    virtual void ScriptExecuting();

    /**
     * Tells the parser that the script is done executing. We should now
     * continue the regular parsing process.
     */
    virtual void ScriptDidExecute();
    
    /* End nsIParser  */

     //*********************************************
      // These methods are callback methods used by
      // net lib to let us know about our inputstream.
      //*********************************************
    // nsIRequestObserver methods:
    NS_DECL_NSIREQUESTOBSERVER

    // nsIStreamListener methods:
    NS_DECL_NSISTREAMLISTENER
  
    /**
     *  Fired when the continue parse event is triggered.
     *  @update  kmcclusk 5/18/98
     */
    void HandleParserContinueEvent(class nsHtml5ParserContinueEvent *);
    
    // EncodingDeclarationHandler
    
    void internalEncodingDeclaration(nsString* aEncoding);
    
    // DocumentModeHandler

    void documentMode(nsHtml5DocumentMode m);

    // nsIContentSink

    NS_IMETHOD WillParse();
    NS_IMETHOD WillBuildModel();
    NS_IMETHOD DidBuildModel();
    NS_IMETHOD WillInterrupt();
    NS_IMETHOD WillResume();
    NS_IMETHOD SetParser(nsIParser* aParser);
    virtual void FlushPendingNotifications(mozFlushType aType);
    NS_IMETHOD SetDocumentCharset(nsACString& aCharset);
    virtual nsISupports *GetTarget();
  
    // Not from an external interface
  public:
    // nsContentSink methods
    virtual nsresult Initialize(nsIDocument* aDoc,
                        nsIURI* aURI,
                        nsISupports* aContainer,
                        nsIChannel* aChannel);
    virtual nsresult ProcessBASETag(nsIContent* aContent);
    virtual void UpdateChildCounts();
    virtual nsresult FlushTags();
    
    // Non-inherited methods
    NS_IMETHOD WriteStreamBytes(const char* aFromSegment,
                                PRUint32 aCount,
                                PRUint32* aWriteCount);
    void Suspend();
    void SetScriptElement(nsIContent* aScript);
    void UpdateStyleSheet(nsIContent* aElement);
    // Getters and setters for fields from nsContentSink 
    nsIDocument* GetDocument() {
      return mDocument;
    }
    nsNodeInfoManager* GetNodeInfoManager() {
      return mNodeInfoManager;
    }
    nsIDocShell* GetDocShell() {
      return mDocShell;
    }
  private:
    void ExecuteScript();
    void MaybePostContinueEvent();
    nsresult PerformCharsetSwitch();
    /**
     * Parse until pending data is exhausted or tree builder suspends
     */
    void ParseUntilSuspend();
    void Cleanup();
  
  private:
    // State variables
    PRBool                       mNeedsCharsetSwitch;
    PRBool                       mLastWasCR;
    PRBool                       mTerminated;
    PRBool                       mLayoutStarted;
    PRBool                       mFragmentMode;
    PRBool                       mBlocked;
    PRBool                       mSuspending;
    eHtml5ParserLifecycle        mLifeCycle;
    eStreamState                 mStreamListenerState;

    // script execution
    nsCOMPtr<nsIContent>         mScriptElement;
    PRUint32                     mScriptsExecuting;
     
    // Gecko integration
    void*                        mRootContextKey;
    nsCOMPtr<nsIRequest>         mRequest; 
    nsCOMPtr<nsIRequestObserver> mObserver;
    nsIRunnable*                 mContinueEvent;  // weak ref
 
    // tree-related stuff
    nsIContent*                  mDocElement; // weak ref 
  
    // encoding-related stuff
    PRInt32                      mCharsetSource;
    nsCString                    mCharset;
    nsCString                    mPendingCharset;
    nsCOMPtr<nsIUnicodeDecoder>  mUnicodeDecoder;
        
    // Portable parser objects    
    nsHtml5UTF16Buffer*          mFirstBuffer; // manually managed strong ref
    nsHtml5UTF16Buffer*          mLastBuffer; // weak ref; always points to 
                      // a buffer of the size NS_HTML5_PARSER_READ_BUFFER_SIZE
    nsHtml5TreeBuilder*          mTreeBuilder; // manually managed strong ref
    nsHtml5Tokenizer*            mTokenizer; // manually managed strong ref
};

#endif 

