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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Contributor(s): rickg@netscape.com 
 *
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
       
//#define ENABLE_CRC              
//#define RICKG_DEBUG           
     
          
#include "nsDebug.h"      
#include "COtherDTD.h" 
#include "nsHTMLTokens.h"
#include "nsCRT.h"     
#include "nsParser.h"  
#include "nsIParser.h"
#include "nsIHTMLContentSink.h"  
#include "nsScanner.h"
#include "prenv.h"  //this is here for debug reasons...
#include "prtypes.h"  //this is here for debug reasons...
#include "prio.h"
#include "plstr.h"  
#include "nsDTDUtils.h"
#include "nsTagHandler.h" 
#include "nsHTMLTokenizer.h"
#include "nsTime.h"
#include "nsViewSourceHTML.h" 
#include "nsParserNode.h"
#include "nsHTMLEntities.h"
#include "nsLinebreakConverter.h"

#include "prmem.h" 


static NS_DEFINE_IID(kIHTMLContentSinkIID, NS_IHTML_CONTENT_SINK_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kIDTDIID,      NS_IDTD_IID);
static NS_DEFINE_IID(kClassIID,     NS_IOTHERHTML_DTD_IID); 
  static NS_DEFINE_IID(kParserServiceCID, NS_PARSERSERVICE_CID);
static char*        kVerificationDir = "c:/temp";
 

#ifdef  ENABLE_CRC
static char gShowCRC;
#endif

   
 
#ifdef MOZ_PERF_METRICS 
#  define START_TIMER()                    \
    if(mParser) MOZ_TIMER_START(mParser->mParseTime); \
    if(mParser) MOZ_TIMER_START(mParser->mDTDTime); 

#  define STOP_TIMER()                     \
    if(mParser) MOZ_TIMER_STOP(mParser->mParseTime); \
    if(mParser) MOZ_TIMER_STOP(mParser->mDTDTime); 
#else
#  define STOP_TIMER() 
#  define START_TIMER()
#endif


#include "COtherElements.h"


/************************************************************************
  And now for the main class -- COtherDTD...
 ************************************************************************/

/**
 *  This method gets called as part of our COM-like interfaces.
 *  Its purpose is to create an interface to parser object
 *  of some type.
 *  
 *  @update   gess 4/8/98
 *  @param    nsIID  id of object to discover
 *  @param    aInstancePtr ptr to newly discovered interface
 *  @return   NS_xxx result code 
 */  
nsresult COtherDTD::QueryInterface(const nsIID& aIID, void** aInstancePtr)  
{                                                                         
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      

  if(aIID.Equals(kISupportsIID))    {  //do IUnknown...
    *aInstancePtr = (nsIDTD*)(this);                                        
  }
  else if(aIID.Equals(kIDTDIID)) {  //do IParser base class...
    *aInstancePtr = (nsIDTD*)(this);                                        
  }
  else if(aIID.Equals(kClassIID)) {  //do this class...
    *aInstancePtr = (COtherDTD*)(this);                                        
  }                 
  else {
    *aInstancePtr=0;
    return NS_NOINTERFACE;
  }
  NS_ADDREF_THIS();
  return NS_OK;                                                        
}

NS_IMPL_ADDREF(COtherDTD)
NS_IMPL_RELEASE(COtherDTD)

/**
 *  Default constructor
 *   
 *  @update  gess 4/9/98
 *  @param   
 *  @return   
 */ 
COtherDTD::COtherDTD() : nsIDTD() {
  NS_INIT_REFCNT();
  mSink = 0; 
  mParser=0;        
  mLineNumber=1;  
  mHasOpenBody=PR_FALSE;
  mHasOpenHead=0;
  mHasOpenForm=PR_FALSE;
  mHasOpenMap=PR_FALSE;
  mTokenizer=0;
  mTokenAllocator=0;
  mComputedCRC32=0;
  mExpectedCRC32=0;
  mDTDState=NS_OK;
  mDocType=eHTML_Strict;
  mHadFrameset=PR_FALSE;
  mHadBody=PR_FALSE;
  mHasOpenScript=PR_FALSE;
  mParserCommand=eViewNormal;
  mNodeAllocator=new nsNodeAllocator();
  mBodyContext=new nsDTDContext();

#if 1 //set this to 1 if you want strictDTD to be based on the environment setting.
  char* theEnvString = PR_GetEnv("MOZ_DISABLE_STRICT"); 
  mEnableStrict=PRBool(0==theEnvString);
#else
  mEnableStrict=PR_TRUE;
#endif

  if(!gElementTable) {
    gElementTable = new CElementTable();
  }

#ifdef  RICKG_DEBUG
  //DebugDumpContainmentRules2(*this,"c:/temp/DTDRules.new","New COtherDTD Containment Rules");
  nsHTMLElement::DebugDumpContainment("c:/temp/contain.new","ElementTable Rules");
  nsHTMLElement::DebugDumpMembership("c:/temp/membership.out");
  nsHTMLElement::DebugDumpContainType("c:/temp/ctnrules.out");
#endif

}

/**
 * 
 * @update  gess1/8/99
 * @param 
 * @return
 */
const nsIID& COtherDTD::GetMostDerivedIID(void)const {
  return kClassIID;
}
 
/**
 *  Default destructor
 *  
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
COtherDTD::~COtherDTD(){
  delete mBodyContext;

  if(mNodeAllocator) {
    delete mNodeAllocator;
    mNodeAllocator=nsnull;
  }
  
  NS_IF_RELEASE(mTokenizer);
  NS_IF_RELEASE(mSink);
}
 
/**
 *  This method is defined in nsIParser. It is used to 
 *  cause the COM-like construction of an nsParser.
 *  
 *  @update  gess 4/8/98
 *  @param   nsIParser** ptr to newly instantiated parser
 *  @return  NS_xxx error result
 */
nsresult NS_NewOtherHTMLDTD(nsIDTD** aInstancePtrResult) {
  COtherDTD* it = new COtherDTD();

  if (it == 0) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kClassIID, (void **) aInstancePtrResult);
}

/**
 * Call this method if you want the DTD to construct a fresh 
 * instance of itself. 
 * @update  gess7/23/98
 * @param 
 * @return
 */
nsresult COtherDTD::CreateNewInstance(nsIDTD** aInstancePtrResult){
  nsresult result=NS_NewOtherHTMLDTD(aInstancePtrResult);

  if(aInstancePtrResult) {
    COtherDTD *theOtherDTD=(COtherDTD*)*aInstancePtrResult;
    if(theOtherDTD) {
      theOtherDTD->mDTDMode=mDTDMode;
      theOtherDTD->mParserCommand=mParserCommand;
      theOtherDTD->mDocType=mDocType;
      theOtherDTD->mEnableStrict=mEnableStrict;
    }
  }

  return result;
}

/** 
 * This method is called to determine if the given DTD can parse
 * a document in a given source-type. 
 * NOTE: Parsing always assumes that the end result will involve
 *       storing the result in the main content model.
 * @update  gess6/24/98
 * @param    
 * @return  TRUE if this DTD can satisfy the request; FALSE otherwise.
 */
eAutoDetectResult COtherDTD::CanParse(CParserContext& aParserContext,nsString& aBuffer, PRInt32 aVersion) {
  eAutoDetectResult result=eUnknownDetect;

  if(mEnableStrict) {
    if(eViewSource==aParserContext.mParserCommand) {
      if(PR_TRUE==aParserContext.mMimeType.EqualsWithConversion(kPlainTextContentType)) {
        result=eValidDetect;
      }
    }
    else {
      if(PR_TRUE==aParserContext.mMimeType.EqualsWithConversion(kPlainTextContentType)) {
        result=eValidDetect;
      }
      else if(PR_TRUE==aParserContext.mMimeType.EqualsWithConversion(kHTMLTextContentType)) {
        switch(aParserContext.mDTDMode) {
          case eDTDMode_strict:
            result=ePrimaryDetect;
            break;
          default:
            result=eValidDetect;
            break;
        }
      }
      else {
        //otherwise, look into the buffer to see if you recognize anything...
        PRBool theBufHasXML=PR_FALSE;
        if(BufferContainsHTML(aBuffer,theBufHasXML)){
          result = eValidDetect ;
          if(0==aParserContext.mMimeType.Length()) {
            aParserContext.SetMimeType(NS_ConvertASCIItoUCS2(kHTMLTextContentType));
            if(!theBufHasXML) {
              switch(aParserContext.mDTDMode) {
                case eDTDMode_strict:
                  result=ePrimaryDetect;
                  break;
                default:
                  result=eValidDetect;
                  break;
              }
            }
            else result=eValidDetect;
          }
        }
      }
    } 
  } 
  return result; 
}

 
/**
  * The parser uses a code sandwich to wrap the parsing process. Before
  * the process begins, WillBuildModel() is called. Afterwards the parser
  * calls DidBuildModel(). 
  * @update	rickg 03.20.2000
  * @param	aParserContext
  * @param	aSink
  * @return	error code (almost always 0)
  */
nsresult COtherDTD::WillBuildModel(  const CParserContext& aParserContext,nsIContentSink* aSink){
  nsresult result=NS_OK;

  mFilename=aParserContext.mScanner->GetFilename();
  mHasOpenBody=PR_FALSE;
  mHadFrameset=PR_FALSE;
  mLineNumber=1;
  mHasOpenScript=PR_FALSE;
  mDTDMode=aParserContext.mDTDMode;
  mParserCommand=aParserContext.mParserCommand;

  if((!aParserContext.mPrevContext) && (aSink)) {

    STOP_TIMER();
    MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: COtherDTD::WillBuildModel(), this=%p\n", this));

    mDocType=aParserContext.mDocType;
    mBodyContext->mFlags.mTransitional=PR_FALSE;

    if(aSink && (!mSink)) {
      result=aSink->QueryInterface(kIHTMLContentSinkIID, (void **)&mSink);
    }

    if(result==NS_OK) {
      result = aSink->WillBuildModel();

      mBodyContext->ResetCounters();

      MOZ_TIMER_DEBUGLOG(("Start: Parse Time: COtherDTD::WillBuildModel(), this=%p\n", this));
      START_TIMER();

      mSkipTarget=eHTMLTag_unknown;
      mComputedCRC32=0;
      mExpectedCRC32=0;
    }
  }

  return result;
}


/**
  * This is called when it's time to read as many tokens from the tokenizer
  * as you can. Not all tokens may make sense, so you may not be able to
  * read them all (until more come in later).
  *
  * @update gess5/18/98
  * @param  aParser is the parser object that's driving this process
  * @return error code (almost always NS_OK)
  */
nsresult COtherDTD::BuildModel(nsIParser* aParser,nsITokenizer* aTokenizer,nsITokenObserver* anObserver,nsIContentSink* aSink) {
  nsresult result=NS_OK;

  if(aTokenizer) {
    nsITokenizer*  oldTokenizer=mTokenizer;
    mTokenizer=aTokenizer;
    mParser=(nsParser*)aParser;

    if(mTokenizer) {

      mTokenAllocator=mTokenizer->GetTokenAllocator();
      
      mBodyContext->SetTokenAllocator(mTokenAllocator);
      mBodyContext->SetNodeAllocator(mNodeAllocator);

      if(mSink) {

        if(!mBodyContext->GetCount()) {
            //if the content model is empty, then begin by opening <html>...
          CStartToken *theToken=(CStartToken*)mTokenAllocator->CreateTokenOfType(eToken_start,eHTMLTag_html,NS_LITERAL_STRING("html"));
          HandleStartToken(theToken); //this token should get pushed on the context stack, don't recycle it.
        }

        while(NS_SUCCEEDED(result)){

          if(mDTDState!=NS_ERROR_HTMLPARSER_STOPPARSING) {
            CToken* theToken=mTokenizer->PopToken();
            if(theToken) { 
              result=HandleToken(theToken,aParser);
            }
            else break;
          }
          else {
            result=mDTDState;
            break;
          }
        }//while
        mTokenizer=oldTokenizer; 
      }
    } 
  }
  else result=NS_ERROR_HTMLPARSER_BADTOKENIZER;
  return result;
}
 
/**
 * 
 * @update  gess5/18/98
 * @param 
 * @return 
 */
nsresult COtherDTD::DidBuildModel(nsresult anErrorCode,PRBool aNotifySink,nsIParser* aParser,nsIContentSink* aSink){
  nsresult result=NS_OK;

  if(aSink) {  

    if(aParser && (NS_OK==result)){ 
      if(aNotifySink){ 
        if((NS_OK==anErrorCode) && (mBodyContext->GetCount()>0)) {

          PRInt32 theIndex=mBodyContext->GetCount()-1;
          eHTMLTags theChild = mBodyContext->TagAt(theIndex); 
          while(theIndex>0) {             
            eHTMLTags theParent= mBodyContext->TagAt(--theIndex);
            CElement *theElement=gElementTable->mElements[theParent];
            nsCParserNode *theNode=mBodyContext->PeekNode();
            theElement->HandleEndToken(theNode,theChild,mBodyContext,mSink);
            theChild=theParent;
          }

          nsEntryStack *theChildStyles=0;
          nsCParserNode* theNode=(nsCParserNode*)mBodyContext->Pop(theChildStyles);   
          if(theNode) {
            mSink->CloseHTML(*theNode);
          }

        }       
        else {
          //If you're here, then an error occured, but we still have nodes on the stack.
          //At a minimum, we should grab the nodes and recycle them.
          //Just to be correct, we'll also recycle the nodes. 
  
          while(mBodyContext->GetCount() > 0) {  
 
            nsEntryStack *theChildStyles=0;
            nsCParserNode* theNode=(nsCParserNode*)mBodyContext->Pop(theChildStyles);
            if(theNode) {
              theNode->mUseCount=0;
              if(theChildStyles) {
                delete theChildStyles;
              } 
              IF_FREE(theNode, mNodeAllocator);
            }
          }    
        }    
  
      } 
    } //if aparser  

      //No matter what, you need to call did build model.
    result=aSink->DidBuildModel(0); 

  } //if asink
  return result; 
}  

/** 
 *  This big dispatch method is used to route token handler calls to the right place.
 *  What's wrong with it? This table, and the dispatch methods themselves need to be 
 *  moved over to the delegate. Ah, so much to do...
 *  
 *  @update  gess 12/1/99
 *  @param   aToken 
 *  @param   aParser   
 *  @return    
 */
nsresult COtherDTD::HandleToken(CToken* aToken,nsIParser* aParser){
  nsresult  result=NS_OK;

  if(aToken) {
    CHTMLToken*     theToken= (CHTMLToken*)(aToken);
    eHTMLTokenTypes theType=eHTMLTokenTypes(theToken->GetTokenType());

//    theToken->mUseCount=0;  //assume every token coming into this system needs recycling.

    mParser=(nsParser*)aParser;

    switch(theType) {
      case eToken_text:
      case eToken_start:
      case eToken_whitespace: 
      case eToken_newline:
      case eToken_doctypeDecl:
      case eToken_markupDecl:
        result=HandleStartToken(theToken); break;

      case eToken_entity:
        result=HandleEntityToken(theToken); break;

      case eToken_end:
        result=HandleEndToken(theToken); break;
       
      default: 
        break; 
    }//switch


    if(NS_SUCCEEDED(result) || (NS_ERROR_HTMLPARSER_BLOCK==result)) {
      IF_FREE(theToken, mTokenAllocator);
    }
    else if(result==NS_ERROR_HTMLPARSER_STOPPARSING)
      mDTDState=result; 
    else return NS_OK;

  }//if
  return result;
}

  
/**
 * This gets called after we've handled a given start tag.
 * It's a generic hook to let us to post processing.
 * @param   aToken contains the tag in question
 * @param   aChildTag is the tag itself.
 * @return  status
 */
nsresult COtherDTD::DidHandleStartTag(nsIParserNode& aNode,eHTMLTags aChildTag){
  nsresult result=NS_OK;

  switch(aChildTag){ 

    case eHTMLTag_script: 
      mHasOpenScript=PR_TRUE; 
      break;   

    case eHTMLTag_pre:
    case eHTMLTag_listing:
      {
        CToken* theNextToken=mTokenizer->PeekToken();
        if(theNextToken)  {
          eHTMLTokenTypes theType=eHTMLTokenTypes(theNextToken->GetTokenType());
          if(eToken_newline==theType){
            mLineNumber++;
            mTokenizer->PopToken();  //skip 1st newline inside PRE and LISTING
          }//if
        }//if
      }
      break;

    case eHTMLTag_meta:
      {
          //we should only enable user-defined entities in debug builds...

        PRInt32 theCount=aNode.GetAttributeCount();
        const nsString* theNamePtr=0;
        const nsString* theValuePtr=0;

        if(theCount) {
          PRInt32 theIndex=0;
          for(theIndex=0;theIndex<theCount;theIndex++){
            nsAutoString theKey(aNode.GetKeyAt(theIndex));
            if(theKey.EqualsWithConversion("ENTITY",PR_TRUE)) {
              const nsString& theName=aNode.GetValueAt(theIndex);
              theNamePtr=&theName;
            }
            else if(theKey.EqualsWithConversion("VALUE",PR_TRUE)) {
              //store the named enity with the context...
              const nsString& theValue=aNode.GetValueAt(theIndex);
              theValuePtr=&theValue;
            }
          }
        }
        if(theNamePtr && theValuePtr) {
          mBodyContext->RegisterEntity(*theNamePtr,*theValuePtr);
        }
      }
      break; 

    default:
      break;
  }//switch 

  return result;
} 
 

#ifdef  RICKG_DEBUG
void WriteTokenToLog(CToken* aToken) {

  static nsFileSpec fileSpec("c:\\temp\\tokenlog.html");
  static nsOutputFileStream outputStream(fileSpec);
  aToken->DebugDumpSource(outputStream); //write token without close bracket...
}
#endif
 
/**
 * This gets called before we've handled a given start tag.
 * It's a generic hook to let us do pre processing.
 * @param   aToken contains the tag in question
 * @param   aChildTag is the tag itself.
 * @param   aNode is the node (tag) with associated attributes.
 * @return  TRUE if tag processing should continue; FALSE if the tag has been handled.
 */
nsresult COtherDTD::WillHandleStartTag(CToken* aToken,eHTMLTags aTag,nsIParserNode& aNode){
  nsresult result=NS_OK; 

    //first let's see if there's some skipped content to deal with... 
#if 0
  PRInt32 theAttrCount  = aNode.GetAttributeCount(); 
  if(*gElementTable->mElements[aTag].mSkipTarget) { 
    result=CollectSkippedContent(aNode,theAttrCount); 
  } 
#endif
  
  STOP_TIMER()
  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: COtherDTD::WillHandleStartTag(), this=%p\n", this));

  if(mParser) {

    switch(aTag) {
      case eHTMLTag_newline:
        mLineNumber++;
        break;
      default:
        break; 
    }
    mSink->NotifyTagObservers(&aNode);
  }

  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: COtherDTD::WillHandleStartTag(), this=%p\n", this));
  START_TIMER()

  return result; 
}
  

/**
 *  This method gets called when a start token has been 
 *  encountered in the parse process. If the current container
 *  can contain this tag, then add it. Otherwise, you have
 *  two choices: 1) create an implicit container for this tag
 *                  to be stored in
 *               2) close the top container, and add this to
 *                  whatever container ends up on top.
 *   
 *  @update  gess 1/04/99
 *  @param   aToken -- next (start) token to be handled
 *  @param   aNode -- CParserNode representing this start token
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */      
nsresult COtherDTD::HandleStartToken(CToken* aToken) {  
  #ifdef  RICKG_DEBUG
    WriteTokenToLog(aToken);
  #endif

  //Begin by gathering up attributes...  
 
  nsresult  result=NS_OK;
  nsCParserNode* theNode=mNodeAllocator->CreateNode(aToken,mLineNumber,mTokenAllocator);
  if(theNode) {
   
    eHTMLTags     theChildTag=(eHTMLTags)aToken->GetTypeID();
    PRInt16       attrCount=aToken->GetAttributeCount();
    eHTMLTags     theParent=mBodyContext->Last();
    
    result=(0==attrCount) ? NS_OK : CollectAttributes(*theNode,theChildTag,attrCount);
 
    if(NS_OK==result) {
      result=WillHandleStartTag(aToken,theChildTag,*theNode);
      if(NS_OK==result) {
 
        mLineNumber += aToken->mNewlineCount;
 
        PRBool theTagWasHandled=PR_FALSE; 
 
        switch(theChildTag) {        
            
          case eHTMLTag_html:  
            if(!mBodyContext->HasOpenContainer(theChildTag)){
              mSink->OpenHTML(*theNode);
              mBodyContext->Push(theNode,0);
            } 
            theTagWasHandled=PR_TRUE;   
            break;         
                 
          default:    
            CElement* theElement=gElementTable->mElements[theParent];
            if(theElement) {
              result=theElement->HandleStartToken(theNode,theChildTag,mBodyContext,mSink);  
              theTagWasHandled=PR_TRUE;  
            }    
            break;   
        }//switch              
         
        if(theTagWasHandled) {
          DidHandleStartTag(*theNode,theChildTag);  
        }
   
      } //if             
    }//if           
    IF_FREE(theNode, mNodeAllocator);
  }
  else result=NS_ERROR_OUT_OF_MEMORY;
          
  return result;        
}      
  
/**
 *  This method gets called when an end token has been   
 *  encountered in the parse process. If the end tag matches
 *  the start tag on the stack, then simply close it. Otherwise,
 *  we have a erroneous state condition. This can be because we
 *  have a close tag with no prior open tag (user error) or because
 *  we screwed something up in the parse process. I'm not sure
 *  yet how to tell the difference.
 *    
 *  @update  gess 3/25/98  
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */ 
nsresult COtherDTD::HandleEndToken(CToken* aToken) { 
  nsresult    result=NS_OK;
  eHTMLTags   theChildTag=(eHTMLTags)aToken->GetTypeID();
 
  #ifdef  RICKG_DEBUG    
    WriteTokenToLog(aToken); 
  #endif  
  
  switch(theChildTag) {    
 
    case eHTMLTag_body: //we intentionally don't let the user close HTML or BODY
    case eHTMLTag_html:   
      break;    
        
    case eHTMLTag_script:       
      mHasOpenScript=PR_FALSE;         
      
    default: 
      PRInt32 theCount=mBodyContext->GetCount();
      eHTMLTags theParent=mBodyContext->TagAt(theCount-1);
      if(theChildTag==theParent) {
        theParent=mBodyContext->TagAt(theCount-2);
      }
      CElement* theElement=gElementTable->mElements[theParent];
      if(theElement) { 
        nsCParserNode* theNode=mNodeAllocator->CreateNode(aToken,mLineNumber,mTokenAllocator);
        if(theNode) {
          result=theElement->HandleEndToken(theNode,theChildTag,mBodyContext,mSink);
          IF_FREE(theNode, mNodeAllocator);
        }
      }   
      break; 
  }     
   
  return result;            
}          
          
/**
 * Retrieve the attributes for this node, and add then into
 * the node.
 *
 * @update  gess4/22/98
 * @param   aNode is the node you want to collect attributes for 
 * @param   aCount is the # of attributes you're expecting
 * @return error code (should be 0)
 */
nsresult COtherDTD::CollectAttributes(nsIParserNode& aNode,eHTMLTags aTag,PRInt32 aCount){
  int attr=0;

  nsresult result=NS_OK;
  int theAvailTokenCount=mTokenizer->GetCount();
  if(aCount<=theAvailTokenCount) {
    //gElementTable->mElements[aTag]->GetSkipTarget();
    CToken* theToken=0; 
    for(attr=0;attr<aCount;attr++){  
      theToken=mTokenizer->PopToken(); 
      if(theToken)  {
        // Sanitize the key for it might contain some non-alpha-non-digit characters
        // at its end.  Ex. <OPTION SELECTED/> - This will be tokenized as "<" "OPTION",
        // "SELECTED/", and ">". In this case the "SELECTED/" key will be sanitized to
        // a legitimate "SELECTED" key.
        ((CAttributeToken*)theToken)->SanitizeKey();
 
  #ifdef  RICKG_DEBUG
    WriteTokenToLog(theToken);
  #endif
 
        aNode.AddAttribute(theToken); 
      }
    }
  }
  else { 
    result=kEOF; 
  }
  return result; 
} 

/**
 *  This method gets called when an entity token has been 
 *  encountered in the parse process. 
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
nsresult COtherDTD::HandleEntityToken(CToken* aToken) {
  nsresult  result=NS_OK;

  nsAutoString theStr;
  aToken->GetSource(theStr);
  PRUnichar theChar=theStr.CharAt(0);
  CToken    *theToken=0;

  if((kHashsign!=theChar) && (-1==nsHTMLEntities::EntityToUnicode(theStr))){

    //before we just toss this away as a bogus entity, let's check...
    CNamedEntity *theEntity=mBodyContext->GetEntity(theStr);
    if(theEntity) {
      theToken=(CTextToken*)mTokenAllocator->CreateTokenOfType(eToken_text,eHTMLTag_text,theEntity->mValue);
    }
    else {
      //if you're here we have a bogus entity.
      //convert it into a text token.
      nsAutoString entityName;
      entityName.AssignWithConversion("&");
      entityName.Append(theStr); //should append the entity name; fix bug 51161.
      theToken=(CTextToken*)mTokenAllocator->CreateTokenOfType(eToken_text,eHTMLTag_text,entityName);
    }
    result=HandleStartToken(theToken);
  }
  else {

    //add this code to fix bug 42629 (entities were getting dropped).
    eHTMLTags theParent=mBodyContext->Last();
    CElement* theElement=gElementTable->mElements[theParent];
    if(theElement) {
      nsCParserNode theNode(aToken,mLineNumber,0);
      result=theElement->HandleStartToken(&theNode,eHTMLTag_text,mBodyContext,mSink);  
    }
  }
  return result;
} 
            
 /***********************************************************************************
   The preceeding tables determine the set of elements each tag can contain...
  ***********************************************************************************/
     
/** 
 *  This method is called to determine whether or not a tag
 *  of one type can contain a tag of another type.
 *  
 *  @update  gess 4/8/98
 *  @param   aParent -- tag enum of parent container
 *  @param   aChild -- tag enum of child container
 *  @return  PR_TRUE if parent can contain child
 */
PRBool COtherDTD::CanContain(PRInt32 aParent,PRInt32 aChild) const {
  CElement *theParent=gElementTable->mElements[eHTMLTags(aParent)];
  if(theParent) {
    CElement *theChild=gElementTable->mElements[eHTMLTags(aChild)];
    if(aChild) {
      if(eHTMLTag_userdefined == aChild)//bug #67007, dont strip userdefined tags
        return PR_TRUE;                 
      else
        return theParent->CanContain(theChild,mBodyContext);
    }
  }
  return PR_FALSE;
} 

/** 
 * Give rest of world access to our tag enums, so that CanContain(), etc,
 * become useful.
 */ 
NS_IMETHODIMP COtherDTD::StringTagToIntTag(nsString &aTag, PRInt32* aIntTag) const {
  *aIntTag = nsHTMLTags::LookupTag(aTag);
  return NS_OK;
}

NS_IMETHODIMP COtherDTD::IntTagToStringTag(PRInt32 aIntTag, nsString& aTag) const {
  aTag.AssignWithConversion(nsHTMLTags::GetStringValue((nsHTMLTag)aIntTag).get());
  return NS_OK;
}  

NS_IMETHODIMP COtherDTD::ConvertEntityToUnicode(const nsString& aEntity, PRInt32* aUnicode) const {
  *aUnicode = nsHTMLEntities::EntityToUnicode(aEntity);
  return NS_OK;
}
 
/**
 *  This method is called to determine whether or not
 *  the given childtag is a block element.
 *
 *  @update  gess 6June2000
 *  @param   aChildID -- tag id of child 
 *  @param   aParentID -- tag id of parent (or eHTMLTag_unknown)
 *  @return  PR_TRUE if this tag is a block tag
 */
PRBool COtherDTD::IsBlockElement(PRInt32 aChildID,PRInt32 aParentID) const {
  PRBool result=PR_FALSE;

  if(gElementTable) {
    CElement *theElement=gElementTable->GetElement((eHTMLTags)aChildID);
    result = (theElement) ? theElement->IsBlockElement((eHTMLTags)aParentID) : PR_FALSE;
  }
  return result;
}

/**
 *  This method is called to determine whether or not
 *  the given childtag is an inline element.
 *
 *  @update  gess 6June2000
 *  @param   aChildID -- tag id of child 
 *  @param   aParentID -- tag id of parent (or eHTMLTag_unknown)
 *  @return  PR_TRUE if this tag is an inline element
 */
PRBool COtherDTD::IsInlineElement(PRInt32 aChildID,PRInt32 aParentID) const {
  PRBool result=PR_FALSE;

  if(gElementTable) {
    CElement *theElement=gElementTable->GetElement((eHTMLTags)aChildID);
    result = (theElement) ? theElement->IsInlineElement((eHTMLTags)aParentID) : PR_FALSE;
  }
  return result;
}
     
/**
 *  This method gets called to determine whether a given 
 *  tag is itself a container 
 *   
 *  @update  gess 4/8/98
 *  @param   aTag -- tag to test as a container
 *  @return  PR_TRUE if given tag can contain other tags
 */
PRBool COtherDTD::IsContainer(PRInt32 aTag) const {
  return gElementTable->mElements[eHTMLTags(aTag)]->IsContainer();
}

 
/**
 * Retrieve the preferred tokenizer for use by this DTD.
 * @update  gess12/28/98
 * @param   none
 * @return  ptr to tokenizer
 */
nsresult COtherDTD::GetTokenizer(nsITokenizer*& aTokenizer) {
  nsresult result=NS_OK;
  if(!mTokenizer) {
    result=NS_NewHTMLTokenizer(&mTokenizer,mDTDMode,mDocType,mParserCommand);
  }
  aTokenizer=mTokenizer;
  return result;
}
 
 
/**
 * 
 * @update  gess5/18/98 
 * @param 
 * @return 
 */
nsresult COtherDTD::WillResumeParse(void) {

  STOP_TIMER();
  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: COtherDTD::WillResumeParse(), this=%p\n", this));

  nsresult result=(mSink) ? mSink->WillResume() : NS_OK; 

  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: COtherDTD::WillResumeParse(), this=%p\n", this));
  START_TIMER();

  return result;
}

/**
 * This method gets called when the parsing process is interrupted
 * due to lack of data (waiting for netlib).
 * @update  gess5/18/98
 * @return  error code
 */ 
nsresult COtherDTD::WillInterruptParse(void){ 

  STOP_TIMER();
  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: COtherDTD::WillInterruptParse(), this=%p\n", this));

  nsresult result=(mSink) ? mSink->WillInterrupt() : NS_OK; 

  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: COtherDTD::WillInterruptParse(), this=%p\n", this));
  START_TIMER();

  return result;
}

// CTransitionalDTD is a subclass of COtherDTD that defaults to transitional mode.  
// Used by the editor

CTransitionalDTD::CTransitionalDTD()
{
  if (mBodyContext) mBodyContext->mFlags.mTransitional = PR_TRUE;
}

CTransitionalDTD::~CTransitionalDTD() {}

