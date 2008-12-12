/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=79: */
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

#include "nsCompatibility.h"
#include "nsScriptLoader.h"
#include "nsNetUtil.h"
#include "nsIStyleSheetLinkingElement.h"
#include "nsICharsetConverterManager.h"

#include "nsHtml5DocumentMode.h"
#include "nsHtml5Tokenizer.h"
#include "nsHtml5UTF16Buffer.h"
#include "nsHtml5TreeBuilder.h"

#include "nsHtml5Parser.h"

static NS_DEFINE_CID(kHtml5ParserCID, NS_HTML5_PARSER_CID);

//-------------- Begin ParseContinue Event Definition ------------------------
/*
The parser can be explicitly interrupted by passing a return value of
NS_ERROR_HTMLPARSER_INTERRUPTED from BuildModel on the DTD. This will cause
the parser to stop processing and allow the application to return to the event
loop. The data which was left at the time of interruption will be processed
the next time OnDataAvailable is called. If the parser has received its final
chunk of data then OnDataAvailable will no longer be called by the networking
module, so the parser will schedule a nsHtml5ParserContinueEvent which will call
the parser to process the remaining data after returning to the event loop.
If the parser is interrupted while processing the remaining data it will
schedule another ParseContinueEvent. The processing of data followed by
scheduling of the continue events will proceed until either:

  1) All of the remaining data can be processed without interrupting
  2) The parser has been cancelled.


This capability is currently used in CNavDTD and nsHTMLContentSink. The
nsHTMLContentSink is notified by CNavDTD when a chunk of tokens is going to be
processed and when each token is processed. The nsHTML content sink records
the time when the chunk has started processing and will return
NS_ERROR_HTMLPARSER_INTERRUPTED if the token processing time has exceeded a
threshold called max tokenizing processing time. This allows the content sink
to limit how much data is processed in a single chunk which in turn gates how
much time is spent away from the event loop. Processing smaller chunks of data
also reduces the time spent in subsequent reflows.

This capability is most apparent when loading large documents. If the maximum
token processing time is set small enough the application will remain
responsive during document load.

A side-effect of this capability is that document load is not complete when
the last chunk of data is passed to OnDataAvailable since  the parser may have
been interrupted when the last chunk of data arrived. The document is complete
when all of the document has been tokenized and there aren't any pending
nsHtml5ParserContinueEvents. This can cause problems if the application assumes
that it can monitor the load requests to determine when the document load has
been completed. This is what happens in Mozilla. The document is considered
completely loaded when all of the load requests have been satisfied. To delay
the document load until all of the parsing has been completed the
nsHTMLContentSink adds a dummy parser load request which is not removed until
the nsHTMLContentSink's DidBuildModel is called. The CNavDTD will not call
DidBuildModel until the final chunk of data has been passed to the parser
through the OnDataAvailable and there aren't any pending
nsHtml5ParserContineEvents.

Currently the parser is ignores requests to be interrupted during the
processing of script.  This is because a document.write followed by JavaScript
calls to manipulate the DOM may fail if the parser was interrupted during the
document.write.

For more details @see bugzilla bug 76722
*/


class nsHtml5ParserContinueEvent : public nsRunnable
{
public:
  nsRefPtr<nsHtml5Parser> mParser;

  nsHtml5ParserContinueEvent(nsHtml5Parser* aParser)
    : mParser(aParser)
  {}

  NS_IMETHODIMP Run()
  {
    mParser->HandleParserContinueEvent(this);
    return NS_OK;
  }
};

//-------------- End ParseContinue Event Definition ------------------------

NS_IMPL_ISUPPORTS_INHERITED3(nsHtml5Parser, nsContentSink, nsIParser, nsIStreamListener, nsIContentSink)

/**
 *  default constructor
 */
nsHtml5Parser::nsHtml5Parser()
  : mRequest(nsnull),
    mObserver(nsnull),
    mUnicodeDecoder(nsnull),
    mFirstBuffer(new nsHtml5UTF16Buffer(NS_HTML5_PARSER_READ_BUFFER_SIZE)), // XXX allocate elsewhere for fragment parser?
    mLastBuffer(mFirstBuffer),
    mTreeBuilder(new nsHtml5TreeBuilder(this)),
    mTokenizer(new nsHtml5Tokenizer(mTreeBuilder, this))
{
  // There's a zeroing operator new for everything else
}

nsHtml5Parser::~nsHtml5Parser()
{
  while (mFirstBuffer->next) {
    nsHtml5UTF16Buffer* oldBuf = mFirstBuffer;
    mFirstBuffer = mFirstBuffer->next;
    delete oldBuf;
  }
  delete mFirstBuffer;
  
  delete mTokenizer;
  delete mTreeBuilder;
}

NS_IMETHODIMP_(void) 
nsHtml5Parser::SetContentSink(nsIContentSink* aSink)
{
  NS_ASSERTION((aSink == static_cast<nsIContentSink*> (this)), "Attempt to set a foreign sink.");
}

NS_IMETHODIMP_(nsIContentSink*)
nsHtml5Parser::GetContentSink(void)
{
  return static_cast<nsIContentSink*> (this);
}

NS_IMETHODIMP_(void) 
nsHtml5Parser::GetCommand(nsCString& aCommand)
{
  aCommand.Assign("loadAsData");
}

NS_IMETHODIMP_(void) 
nsHtml5Parser::SetCommand(const char* aCommand)
{
  NS_ASSERTION((!strcmp(aCommand, "view")), "Parser command was not view");
}

NS_IMETHODIMP_(void) 
nsHtml5Parser::SetCommand(eParserCommands aParserCommand)
{
  NS_ASSERTION((aParserCommand == eViewNormal), "Parser command was not eViewNormal.");
}

NS_IMETHODIMP_(void)
nsHtml5Parser::SetDocumentCharset(const nsACString& aCharset, PRInt32 aCharsetSource)
{
  mCharset = aCharset;
  mCharsetSource = aCharsetSource;
}

NS_IMETHODIMP_(void)
nsHtml5Parser::SetParserFilter(nsIParserFilter* aFilter)
{
  NS_ASSERTION(PR_TRUE, "Attempt to set a parser filter on HTML5 parser.");
}

NS_IMETHODIMP
nsHtml5Parser::GetChannel(nsIChannel** aChannel)
{
  return CallQueryInterface(mRequest, aChannel);
}

NS_IMETHODIMP
nsHtml5Parser::GetDTD(nsIDTD** aDTD)
{
  *aDTD = nsnull;
  return NS_OK;
}
    
NS_IMETHODIMP
nsHtml5Parser::ContinueParsing()
{
  mBlocked = PR_FALSE;
  return ContinueInterruptedParsing();
}

NS_IMETHODIMP
nsHtml5Parser::ContinueInterruptedParsing()
{
  // If the stream has already finished, there's a good chance
  // that we might start closing things down when the parser
  // is reenabled. To make sure that we're not deleted across
  // the reenabling process, hold a reference to ourselves.
  // XXX is this really necessary? -- hsivonen?
  nsCOMPtr<nsIParser> kungFuDeathGrip(this);

  // XXX Stop speculative script thread but why?
  
  ParseUntilSuspend();

  return NS_OK;
}

/**
 * This method isn't really useful as a method, but it's in nsIParser.
 */
NS_IMETHODIMP_(void)
nsHtml5Parser::BlockParser()
{
  NS_PRECONDITION((!mFragmentMode), "Must not block in fragment mode.");
  mBlocked = PR_TRUE;
}

NS_IMETHODIMP_(void)
nsHtml5Parser::UnblockParser()
{
  mBlocked = PR_FALSE;
}

NS_IMETHODIMP_(PRBool)
nsHtml5Parser::IsParserEnabled()
{
  return !mBlocked;
}

NS_IMETHODIMP_(PRBool)
nsHtml5Parser::IsComplete()
{
  // XXX old parser says 
  // return !(mFlags & NS_PARSER_FLAG_PENDING_CONTINUE_EVENT);
  return (mLifeCycle == TERMINATED);
}

NS_IMETHODIMP
nsHtml5Parser::Parse(nsIURI* aURL, // legacy parameter; ignored
                     nsIRequestObserver* aObserver,
                     void* aKey, 
                     nsDTDMode aMode) // legacy; ignored
{
  mObserver = aObserver;
  mRootContextKey = aKey;
  NS_ASSERTION((mLifeCycle == NOT_STARTED), "Tried to start parse without initializing the parser properly.");
  mTokenizer->start();
  mLifeCycle = PARSING;
  mParser = this;
  return NS_OK;
}

NS_IMETHODIMP
nsHtml5Parser::Parse(const nsAString& aSourceBuffer,
                     void* aKey,
                     const nsACString& aContentType, // ignored
                     PRBool aLastCall,
                     nsDTDMode aMode) // ignored
{
  NS_PRECONDITION((!mFragmentMode), "Document.write called in fragment mode!");
  // Return early if the parser has processed EOF
  switch (mLifeCycle) {
    case TERMINATED:
      return NS_OK;
    case NOT_STARTED:
      mTokenizer->start();
      mLifeCycle = PARSING;
      mParser = this;
      break;
    default:
      break;
  }
  
  if (aLastCall && aSourceBuffer.IsEmpty() && aKey == GetRootContextKey()) {
    // document.close()
    mLifeCycle = STREAM_ENDING;
    MaybePostContinueEvent();
    return NS_OK;
  }

  // XXX stop speculative script thread here
  
  // Maintain a reference to ourselves so we don't go away
  // till we're completely done.
  // XXX is this still necessary? -- hsivonen
  nsCOMPtr<nsIParser> kungFuDeathGrip(this);
  
  if (!aSourceBuffer.IsEmpty()) {
    nsHtml5UTF16Buffer* buffer = new nsHtml5UTF16Buffer(aSourceBuffer.Length());
    memcpy(buffer->getBuffer(), aSourceBuffer.BeginReading(), aSourceBuffer.Length() * sizeof(PRUnichar));
    buffer->setEnd(aSourceBuffer.Length());
    if (!mBlocked) {
      WillResumeImpl();
      WillProcessTokensImpl();
      while (buffer->hasMore()) {
        buffer->adjust(mLastWasCR);
        mLastWasCR = PR_FALSE;
        if (buffer->hasMore()) {
          mLastWasCR = mTokenizer->tokenizeBuffer(buffer);
          if (mScriptElement) {
            ExecuteScript();
          }
          if (mNeedsCharsetSwitch) {
            // XXX setup immediate reparse
            delete buffer;
            WillInterruptImpl();
            return NS_OK;
          } else if (mBlocked) {
            // XXX is the tail insertion and script exec in the wrong order?
            WillInterruptImpl();
            break;
          } else {
            // Ignore suspensions
            continue;
          }
        }
      }
    }
    if (buffer->hasMore()) {
      nsHtml5UTF16Buffer* prevSearchBuf = nsnull;
      nsHtml5UTF16Buffer* searchBuf = mFirstBuffer;
      if (aKey) { // after document.open, the first level of document.write has null key
        while (searchBuf != mLastBuffer) {
          if (searchBuf->key == aKey) {
            buffer->next = searchBuf;
            if (prevSearchBuf) {
              prevSearchBuf->next = buffer;
            } else {
              mFirstBuffer = buffer;
            }
            break;
          }
          prevSearchBuf = searchBuf;
          searchBuf = searchBuf->next;
        }
      }
      if (searchBuf == mLastBuffer || !aKey) {
        // key was not found or we have a first-level write after document.open
        // we'll insert to the head of the queue
        nsHtml5UTF16Buffer* keyHolder = new nsHtml5UTF16Buffer(aKey);
        keyHolder->next = mFirstBuffer;
        buffer->next = keyHolder;
        mFirstBuffer = buffer;
      }
      MaybePostContinueEvent();
    } else {
      delete buffer;
    }
  }
  return NS_OK;
}

/**
 * This magic value is passed to the previous method on document.close()
 */
NS_IMETHODIMP_(void *)
nsHtml5Parser::GetRootContextKey()
{
  return mRootContextKey;
}

NS_IMETHODIMP
nsHtml5Parser::Terminate(void)
{
  // We should only call DidBuildModel once, so don't do anything if this is
  // the second time that Terminate has been called.
  if (mTerminated) {
    return NS_OK;
  }
  
  // XXX - [ until we figure out a way to break parser-sink circularity ]
  // Hack - Hold a reference until we are completely done...
  nsCOMPtr<nsIParser> kungFuDeathGrip(this);
  
  // CancelParsingEvents must be called to avoid leaking the nsParser object
  // @see bug 108049
  CancelParsingEvents();

  return DidBuildModel(); // nsIContentSink
}

NS_IMETHODIMP
nsHtml5Parser::ParseFragment(const nsAString& aSourceBuffer,
                             void* aKey,
                             nsTArray<nsString>& aTagStack,
                             PRBool aXMLMode,
                             const nsACString& aContentType,
                             nsDTDMode aMode)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHtml5Parser::BuildModel(void)
{
  // XXX who calls this? Should this be a no-op?
  ParseUntilSuspend();
  return NS_OK;
}

/**
 * This method is dead code. Here only for interface compat.
 */
NS_IMETHODIMP_(nsDTDMode)
nsHtml5Parser::GetParseMode(void)
{
  NS_NOTREACHED("No one is supposed to call GetParseMode!");
  return eDTDMode_unknown;
}

NS_IMETHODIMP
nsHtml5Parser::CancelParsingEvents()
{
  mContinueEvent = nsnull;
  return NS_OK;
}

void
nsHtml5Parser::Reset()
{
  NS_NOTREACHED("Can't reset.");
}
    
/* End nsIParser  */

// nsIRequestObserver methods:

nsresult
nsHtml5Parser::OnStartRequest(nsIRequest* aRequest, nsISupports* aContext)
{
  NS_PRECONDITION(eNone == mStreamListenerState,
                  "Parser's nsIStreamListener API was not setup "
                  "correctly in constructor.");
  if (mObserver) {
    mObserver->OnStartRequest(aRequest, aContext);
  }
  // XXX figure out
  // mParserContext->mAutoDetectStatus = eUnknownDetect;
  mStreamListenerState = eOnStart;
  mRequest = aRequest;

  nsresult rv = NS_OK;

//  if (sParserDataListeners) {
//    nsISupports *ctx = GetTarget();
//    PRInt32 count = sParserDataListeners->Count();
//
//    while (count--) {
//      rv |= sParserDataListeners->ObjectAt(count)->
//              OnStartRequest(request, ctx);
//    }
//  }

  nsCOMPtr<nsICharsetConverterManager> convManager = do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = convManager->GetUnicodeDecoder(mCharset.get(), getter_AddRefs(mUnicodeDecoder));
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
}

/**
 *  This is called by the networking library once the last block of data
 *  has been collected from the net.
 */
nsresult
nsHtml5Parser::OnStopRequest(nsIRequest* aRequest, nsISupports* aContext,
                        nsresult status)
{
  NS_ASSERTION((mRequest == aRequest), "Got Stop on wrong stream.");
  nsresult rv = NS_OK;

  mLifeCycle = STREAM_ENDING;

//  if (eOnStart == mStreamListenerState) {
    // If you're here, then OnDataAvailable() never got called.  Prior
    // to necko, we never dealt with this case, but the problem may
    // have existed.  Everybody can live with an empty input stream, so
    // just resume parsing.
//    ParseUntilSuspend();
//  }
//  mStreamStatus = status;

//  if (mParserFilter)
//    mParserFilter->Finish();

  mStreamListenerState = eOnStop;

  ParseUntilSuspend();

  // If the parser isn't enabled, we don't finish parsing till
  // it is reenabled.

  // XXX Should we wait to notify our observers as well if the
  // parser isn't yet enabled?
  if (mObserver) {
    mObserver->OnStopRequest(aRequest, aContext, status);
  }

//  if (sParserDataListeners) {
//    nsISupports *ctx = GetTarget();
//    PRInt32 count = sParserDataListeners->Count();
//
//    while (count--) {
//      rv |= sParserDataListeners->ObjectAt(count)->OnStopRequest(aRequest, ctx,
//                                                                 status);
//    }
//  }

  return rv;
}

// nsIStreamListener method:


/*
 * This function is invoked as a result of a call to a stream's
 * ReadSegments() method. It is called for each contiguous buffer
 * of data in the underlying stream or pipe. Using ReadSegments
 * allows us to avoid copying data to read out of the stream.
 */
static NS_METHOD
ParserWriteFunc(nsIInputStream* aInStream,
                void* aHtml5Parser,
                const char* aFromSegment,
                PRUint32 aToOffset,
                PRUint32 aCount,
                PRUint32* aWriteCount)
{
  nsHtml5Parser* parser = static_cast<nsHtml5Parser*> (aHtml5Parser);
  return parser->WriteStreamBytes(aFromSegment, aCount, aWriteCount);
}

nsresult
nsHtml5Parser::OnDataAvailable(nsIRequest* aRequest, 
                               nsISupports* aContext,
                               nsIInputStream* aInStream, 
                               PRUint32 aSourceOffset,
                               PRUint32 aLength)
{
  NS_PRECONDITION((eOnStart == mStreamListenerState ||
                   eOnDataAvail == mStreamListenerState),
            "Error: OnStartRequest() must be called before OnDataAvailable()");
  NS_ASSERTION((mRequest == aRequest), "Got data on wrong stream.");
  PRUint32 totalRead;
  nsresult rv = aInStream->ReadSegments(ParserWriteFunc, static_cast<void*> (this), aLength, &totalRead);
  NS_ASSERTION((totalRead == aLength), "ReadSegments read the wrong number of bytes.");
  ParseUntilSuspend();
  return rv;
}

// EncodingDeclarationHandler

void
nsHtml5Parser::internalEncodingDeclaration(nsString* aEncoding)
{
  // XXX implement
}

// DocumentModeHandler

void 
nsHtml5Parser::documentMode(nsHtml5DocumentMode m)
{
#if 0
  nsCompatibility mode = eCompatibility_NavQuirks;
  switch (m) {
    case STANDARDS_MODE:
      mode = eCompatibility_FullStandards;
      break;
    case ALMOST_STANDARDS_MODE:
      mode = eCompatibility_AlmostStandards;
      break;
    case QUIRKS_MODE:
      mode = eCompatibility_NavQuirks;
      break;
  }
  nsCOMPtr<nsIHTMLDocument> htmlDocument = do_QueryInterface(mDocument);
  NS_ASSERTION(htmlDocument, "Document didn't QI into HTML document.");
  if (htmlDocument) {
    htmlDocument->SetCompatibilityMode(mode);  
  }
#endif
}

// nsIContentSink

NS_IMETHODIMP
nsHtml5Parser::WillTokenize()
{
  NS_NOTREACHED("No one shuld call this");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHtml5Parser::WillBuildModel()
{
  NS_NOTREACHED("No one shuld call this");
  return NS_ERROR_NOT_IMPLEMENTED;
}

// XXX should this live in TreeBuilder::end?
// This is called when the tree construction has ended
NS_IMETHODIMP
nsHtml5Parser::DidBuildModel()
{
  NS_ASSERTION((mLifeCycle == STREAM_ENDING), "Bad life cycle.");
  mTokenizer->eof();
  mTokenizer->end();
  mLifeCycle = TERMINATED;
  // This is comes from nsXMLContentSink
  DidBuildModelImpl();

  mDocument->ScriptLoader()->RemoveObserver(this);

  StartLayout(PR_FALSE);

  ScrollToRef();

  mDocument->RemoveObserver(this);

  mDocument->EndLoad();

  DropParserAndPerfHint();

  return NS_OK;
}

NS_IMETHODIMP
nsHtml5Parser::WillInterrupt()
{
  return WillInterruptImpl();
}

NS_IMETHODIMP
nsHtml5Parser::WillResume()
{
  NS_NOTREACHED("No one should call this.");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHtml5Parser::SetParser(nsIParser* aParser)
{
  NS_NOTREACHED("No one should be setting a parser on the HTML5 pseudosink.");
  return NS_ERROR_NOT_IMPLEMENTED;
}

void
nsHtml5Parser::FlushPendingNotifications(mozFlushType aType)
{
  // Only flush tags if we're not doing the notification ourselves
  // (since we aren't reentrant)
  if (!mInNotification) {
    mTreeBuilder->Flush();
    if (aType >= Flush_Layout) {
      // Make sure that layout has started so that the reflow flush
      // will actually happen.
      StartLayout(PR_TRUE);
    }
  }
}

NS_IMETHODIMP
nsHtml5Parser::SetDocumentCharset(nsACString& aCharset)
{
  // XXX who calls this anyway?
  // XXX keep in sync with parser???
  if (mDocument) {
    mDocument->SetDocumentCharacterSet(aCharset);
  }
  
  return NS_OK;
}

nsISupports*
nsHtml5Parser::GetTarget()
{
  return mDocument;
}

// not from interface

void
nsHtml5Parser::HandleParserContinueEvent(nsHtml5ParserContinueEvent* ev)
{
  // Ignore any revoked continue events...
  if (mContinueEvent != ev)
    return;

  mContinueEvent = nsnull;

  ContinueInterruptedParsing();  
}

NS_IMETHODIMP
nsHtml5Parser::WriteStreamBytes(const char* aFromSegment,
                                PRUint32 aCount,
                                PRUint32* aWriteCount)
{
  // mLastBuffer always points to a buffer of the size NS_HTML5_PARSER_READ_BUFFER_SIZE.
  if (mLastBuffer->getEnd() == NS_HTML5_PARSER_READ_BUFFER_SIZE) {
      mLastBuffer = (mLastBuffer->next = new nsHtml5UTF16Buffer(NS_HTML5_PARSER_READ_BUFFER_SIZE));
  }  
  PRInt32 totalByteCount = 0;

  for (;;) {
    PRInt32 end = mLastBuffer->getEnd();
    PRInt32 byteCount = aCount - totalByteCount;
    PRInt32 utf16Count = NS_HTML5_PARSER_READ_BUFFER_SIZE - end;

    nsresult convResult = mUnicodeDecoder->Convert(aFromSegment, &byteCount, mLastBuffer->getBuffer() + end, &utf16Count);  
  
    mLastBuffer->setEnd(end + utf16Count);
    totalByteCount += byteCount;
    aFromSegment += byteCount;
    
    NS_ASSERTION((mLastBuffer->getEnd() <= NS_HTML5_PARSER_READ_BUFFER_SIZE), "The Unicode decoder wrote too much data.");
  
    if (convResult == NS_PARTIAL_MORE_OUTPUT) {
      mLastBuffer = (mLastBuffer->next = new nsHtml5UTF16Buffer(NS_HTML5_PARSER_READ_BUFFER_SIZE));
      NS_ASSERTION(((PRUint32)totalByteCount < aCount), "The Unicode has consumed too many bytes.");
    } else {
      NS_ASSERTION(((PRUint32)totalByteCount == aCount), "The Unicode decoder consumed the wrong number of bytes.");
      *aWriteCount = totalByteCount;
      return NS_OK;      
    }
  }
}

void
nsHtml5Parser::ParseUntilSuspend()
{
  NS_PRECONDITION((!mNeedsCharsetSwitch), "ParseUntilSuspend called when charset switch needed.");
//  NS_PRECONDITION((!mTerminated), "ParseUntilSuspend called when parser had been terminated.");
  if (mBlocked) {
    return;
  }
  
  WillResumeImpl();
  WillProcessTokensImpl();
  
  mSuspending = PR_FALSE;
  for (;;) {
    if (!mFirstBuffer->hasMore()) {
      if (mFirstBuffer == mLastBuffer) {
        switch (mLifeCycle) {
          case PARSING:
            // never release the last buffer. instead just zero its indeces for refill
            mFirstBuffer->setStart(0);
            mFirstBuffer->setEnd(0);
            return; // no more data for now but expecting more
          case STREAM_ENDING:
            DidBuildModel();
            return; // no more data and not expecting more
          default:
            NS_NOTREACHED("ParseUntilSuspended should only be called in PARSING and STREAM_ENDING life cycle states.");
            return;          
        }
      } else {
        nsHtml5UTF16Buffer* oldBuf = mFirstBuffer;
        mFirstBuffer = mFirstBuffer->next;
        delete oldBuf;
        continue;
      }
    }
    // now we have a non-empty buffer
    mFirstBuffer->adjust(mLastWasCR);
    mLastWasCR = PR_FALSE;
    if (mFirstBuffer->hasMore()) {
      mLastWasCR = mTokenizer->tokenizeBuffer(mFirstBuffer);
      if (mScriptElement) {
        ExecuteScript();
      }
      if (mNeedsCharsetSwitch) {
        // XXX setup immediate reparse
        return;
      }
      if (mBlocked) {
        NS_ASSERTION((!mFragmentMode), "Script blocked the parser but we are in the fragment mode.");
        WillInterruptImpl();
        return;
      }
      if (mSuspending && !mFragmentMode) {
        // We never suspend in the fragment mode.
        MaybePostContinueEvent();
        WillInterruptImpl();
        return;
      }
      continue;
    } else {
      continue;
    }
  }
}

/**
 * This method executes a script element set by nsHtml5TreeBuilder. The reason 
 * why this code is here and not in the tree builder is to allow the control 
 * to return from the tokenizer before scripts run. This way, the tokenizer 
 * is not invoked re-entrantly although the parser is.
 */
void
nsHtml5Parser::ExecuteScript()
{
  NS_PRECONDITION(mScriptElement, "Trying to run a script without having one!");
 
   // Copied from nsXMLContentSink
  
  // Now tell the script that it's ready to go. This may execute the script
  // or return NS_ERROR_HTMLPARSER_BLOCK. Or neither if the script doesn't
  // need executing.
  nsresult rv = mScriptElement->DoneAddingChildren(PR_TRUE);

  // If the act of insertion evaluated the script, we're fine.
  // Else, block the parser till the script has loaded.
  if (rv == NS_ERROR_HTMLPARSER_BLOCK) {
    nsCOMPtr<nsIScriptElement> sele = do_QueryInterface(mScriptElement);
    mScriptElements.AppendObject(sele);
    BlockParser();
  }
  mScriptElement = nsnull;
}

void
nsHtml5Parser::MaybePostContinueEvent()
{
  NS_PRECONDITION((mLifeCycle != TERMINATED), "Tried to post continue event when the parser is done.");
  if (mContinueEvent) {
    return; // we already have a pending event
  }
  if (mStreamListenerState == eOnStart || mStreamListenerState == eOnDataAvail) {
    return; // we are expecting stream events
  }
  
  // This creates a reference cycle between this and the event that is
  // broken when the event fires.
  nsCOMPtr<nsIRunnable> event = new nsHtml5ParserContinueEvent(this);
  if (NS_FAILED(NS_DispatchToCurrentThread(event))) {
      NS_WARNING("failed to dispatch parser continuation event");
  } else {
      mContinueEvent = event;
  }
}

void
nsHtml5Parser::Suspend()
{
  mSuspending = PR_TRUE;
}


void
nsHtml5Parser::Cleanup()
{
}

nsresult
nsHtml5Parser::Initialize(nsIDocument* aDoc,
                          nsIURI* aURI,
                          nsISupports* aContainer,
                          nsIChannel* aChannel)
{
  MOZ_TIMER_DEBUGLOG(("Reset and start: nsXMLContentSink::Init(), this=%p\n",
                      this));
  MOZ_TIMER_RESET(mWatch);
  MOZ_TIMER_START(mWatch);
	
  nsresult rv = nsContentSink::Init(aDoc, aURI, aContainer, aChannel);
  NS_ENSURE_SUCCESS(rv, rv);

  aDoc->AddObserver(this);

  MOZ_TIMER_DEBUGLOG(("Stop: nsXMLContentSink::Init()\n"));
  MOZ_TIMER_STOP(mWatch);
  return NS_OK;
}

nsresult
nsHtml5Parser::ProcessBASETag(nsIContent* aContent)
{
  NS_ASSERTION(aContent, "missing base-element");

  nsresult rv = NS_OK;

  if (mDocument) {
    nsAutoString value;
  
    if (aContent->GetAttr(kNameSpaceID_None, nsHtml5Atoms::target, value)) {
      mDocument->SetBaseTarget(value);
    }

    if (aContent->GetAttr(kNameSpaceID_None, nsHtml5Atoms::href, value)) {
      nsCOMPtr<nsIURI> baseURI;
      rv = NS_NewURI(getter_AddRefs(baseURI), value);
      if (NS_SUCCEEDED(rv)) {
        rv = mDocument->SetBaseURI(baseURI); // The document checks if it is legal to set this base
        if (NS_SUCCEEDED(rv)) {
          mDocumentBaseURI = mDocument->GetBaseURI();
        }
      }
    }
  }

  return rv;
}

void
nsHtml5Parser::UpdateStyleSheet(nsIContent* aElement)
{
  nsCOMPtr<nsIStyleSheetLinkingElement> ssle(do_QueryInterface(aElement));
  if (ssle) {
    ssle->SetEnableUpdates(PR_TRUE);
    PRBool willNotify;
    PRBool isAlternate;
    nsresult result = ssle->UpdateStyleSheet(this, &willNotify, &isAlternate);
    if (NS_SUCCEEDED(result) && willNotify && !isAlternate) {
      ++mPendingSheetCount;
      mScriptLoader->AddExecuteBlocker();
    }
  }
}

void
nsHtml5Parser::SetScriptElement(nsIContent* aScript)
{
  mScriptElement = aScript;
}

void 
nsHtml5Parser::UpdateChildCounts()
{
  // No-op
}

nsresult
nsHtml5Parser::FlushTags()
{
    mTreeBuilder->Flush(); 
    return NS_OK; 
}


