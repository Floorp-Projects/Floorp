/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
  
//#define __INCREMENTAL 1

#include "nsHTMLParser.h"
#include "nsHTMLContentSink.h" 
#include "nsTokenizer.h"
#include "nsHTMLTokens.h"
#include "nsString.h"
#include "nsIURL.h"
#include "nsCRT.h"
#include "COtherDelegate.h"
#include "COtherDTD.h"
#include "CNavDelegate.h"
#include "CNavDTD.h"
#include "prenv.h"  //this is here for debug reasons...
#include "plstr.h"
#ifdef XP_PC
#include <direct.h> //this is here for debug reasons...
#endif

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kClassIID, NS_IHTML_PARSER_IID); 
static NS_DEFINE_IID(kIParserIID, NS_IPARSER_IID);

static const char* kNullURL = "Error: Null URL given";
static const char* kNullTokenizer = "Error: Unable to construct tokenizer";
static const char* kNullToken = "Error: Null token given";
static const char* kInvalidTagStackPos = "Error: invalid tag stack position";

static char* gVerificationOutputDir=0;

/**
 *  This method is defined in nsIParser. It is used to 
 *  cause the COM-like construction of an nsHTMLParser.
 *  
 *  @update  gess 3/25/98
 *  @param   nsIParser** ptr to newly instantiated parser
 *  @return  NS_xxx error result
 */
NS_HTMLPARS nsresult NS_NewHTMLParser(nsIParser** aInstancePtrResult)
{
  nsHTMLParser *it = new nsHTMLParser();

  if (it == 0) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kClassIID, (void **) aInstancePtrResult);
}

/**
 *  init the set of default token handlers...
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
void nsHTMLParser::InitializeDefaultTokenHandlers() {
  AddTokenHandler(new CStartTokenHandler());
  AddTokenHandler(new CEndTokenHandler());
  AddTokenHandler(new CCommentTokenHandler());
  AddTokenHandler(new CEntityTokenHandler());
  AddTokenHandler(new CWhitespaceTokenHandler());
  AddTokenHandler(new CNewlineTokenHandler());
  AddTokenHandler(new CTextTokenHandler());
  AddTokenHandler(new CAttributeTokenHandler());
  AddTokenHandler(new CScriptTokenHandler());
  AddTokenHandler(new CStyleTokenHandler());
}


/**
 *  default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
nsHTMLParser::nsHTMLParser() {
  NS_INIT_REFCNT();
  mSink=0;
  mContextStackPos=0;
  mCurrentPos=0;
  mParseMode=eParseMode_unknown;
  nsCRT::zero(mContextStack,sizeof(mContextStack));
  nsCRT::zero(mTokenHandlers,sizeof(mTokenHandlers));
  mDTD=0;
  mHasOpenForm=PR_FALSE;
  mTokenHandlerCount=0;
  InitializeDefaultTokenHandlers();
  gVerificationOutputDir = PR_GetEnv("VERIFY_PARSER");
}


/**
 *  Default destructor
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
nsHTMLParser::~nsHTMLParser() {
  DeleteTokenHandlers();
  if(mCurrentPos)
    delete mCurrentPos;
  mCurrentPos=0;
  if(mTokenizer)
    delete mTokenizer;
  if(mDTD)
    delete mDTD;
  mTokenizer=0;
  mDTD=0;
}


NS_IMPL_ADDREF(nsHTMLParser)
NS_IMPL_RELEASE(nsHTMLParser)
//NS_IMPL_ISUPPORTS(nsHTMLParser,NS_IHTML_PARSER_IID)


/**
 *  This method gets called as part of our COM-like interfaces.
 *  Its purpose is to create an interface to parser object
 *  of some type.
 *  
 *  @update   gess 3/25/98
 *  @param    nsIID  id of object to discover
 *  @param    aInstancePtr ptr to newly discovered interface
 *  @return   NS_xxx result code
 */
nsresult nsHTMLParser::QueryInterface(const nsIID& aIID, void** aInstancePtr)  
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      

  if(aIID.Equals(kISupportsIID))    {  //do IUnknown...
    *aInstancePtr = (nsIParser*)(this);                                        
  }
  else if(aIID.Equals(kIParserIID)) {  //do IParser base class...
    *aInstancePtr = (nsIParser*)(this);                                        
  }
  else if(aIID.Equals(kClassIID)) {  //do this class...
    *aInstancePtr = (nsHTMLParser*)(this);                                        
  }                 
  else {
    *aInstancePtr=0;
    return NS_NOINTERFACE;
  }
  ((nsISupports*) *aInstancePtr)->AddRef();
  return NS_OK;                                                        
}


/**
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 */
eHTMLTags nsHTMLParser::NodeAt(PRInt32 aPos) const {
  NS_PRECONDITION(0 <= aPos, "bad nodeAt");
  if((aPos>-1) && (aPos<mContextStackPos))
    return (eHTMLTags)mContextStack[aPos];
  return eHTMLTag_unknown;
}

/**
 *  This method allows the caller to determine if a form
 *  element is currently open.
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 */
PRBool nsHTMLParser::HasOpenContainer(PRInt32 aContainer) const {
  PRBool result=PR_FALSE;

  switch((eHTMLTags)aContainer) {
    case eHTMLTag_form:
      result=mHasOpenForm; break;

    default:
      result=(kNotFound!=GetTopmostIndex((eHTMLTags)aContainer)); break;
  }
  return result;
}

/**
 *  This method retrieves the HTMLTag type of the topmost
 *  container on the stack.
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 */
eHTMLTags nsHTMLParser::GetTopNode() const {
  if(mContextStackPos) 
    return (eHTMLTags)mContextStack[mContextStackPos-1];
  return eHTMLTag_unknown;
}


/**
 *  Determine whether the given tag is open anywhere
 *  in our context stack.
 *  
 *  @update  gess 4/2/98
 *  @param   eHTMLTags tag to be searched for in stack
 *  @return  topmost index of tag on stack
 */
PRInt32 nsHTMLParser::GetTopmostIndex(eHTMLTags aTag) const {
  for(int i=mContextStackPos-1;i>=0;i--){
    if(mContextStack[i]==aTag)
      return i;
  }
  return kNotFound;
}

/**
 *  Determine whether the given tag is open anywhere
 *  in our context stack.
 *  
 *  @update  gess 4/2/98
 *  @param   eHTMLTags tag to be searched for in stack
 *  @return  topmost index of tag on stack
 */
PRBool nsHTMLParser::IsOpen(eHTMLTags aTag) const {
  PRInt32 pos=GetTopmostIndex(aTag);
  return PRBool(kNotFound!=pos);
}

/**
 *  Gets the number of open containers on the stack.
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 */
PRInt32 nsHTMLParser::GetStackPos() const {
  return mContextStackPos;
}

/**
 *  Finds a tag handler for the given tag type, given in string.
 *  
 *  @update  gess 4/2/98
 *  @param   aString contains name of tag to be handled
 *  @return  valid tag handler (if found) or null
 */
nsHTMLParser& nsHTMLParser::DeleteTokenHandlers(void) {
  for(int i=0;i<mTokenHandlerCount;i++){
    delete mTokenHandlers[i];
  }
  mTokenHandlerCount=0;
  return *this;
}

/**
 *  Finds a tag handler for the given tag type, given in string.
 *  
 *  @update  gess 4/2/98
 *  @param   aString contains name of tag to be handled
 *  @return  valid tag handler (if found) or null
 */
CTokenHandler* nsHTMLParser::GetTokenHandler(const nsString& aString) const{
  eHTMLTokenTypes theType=DetermineTokenType(aString);
  return GetTokenHandler(theType);
}


/**
 *  Finds a tag handler for the given tag type.
 *  
 *  @update  gess 4/2/98
 *  @param   aTagType type of tag to be handled
 *  @return  valid tag handler (if found) or null
 */
CTokenHandler* nsHTMLParser::GetTokenHandler(eHTMLTokenTypes aType) const {
  for(int i=0;i<mTokenHandlerCount;i++) {
    if(mTokenHandlers[i]->CanHandle(aType)) {
      return mTokenHandlers[i];
    }
  }
  return 0;
}


/**
 *  Register a handler.
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 */
CTokenHandler* nsHTMLParser::AddTokenHandler(CTokenHandler* aHandler) {
  NS_ASSERTION(0!=aHandler,"Error: Null handler argument");
  
  if(aHandler) {
    int max=sizeof(mTokenHandlers)/sizeof(mTokenHandlers[0]);
    if(mTokenHandlerCount<max) {
      mTokenHandlers[mTokenHandlerCount++]=aHandler;
    } 
  }
  return 0;
}

/**
 *  This method gets called in order to set the content
 *  sink for this parser to dump nodes to.
 *  
 *  @update  gess 3/25/98
 *  @param   nsIContentSink interface for node receiver
 *  @return  
 */
nsIContentSink* nsHTMLParser::SetContentSink(nsIContentSink* aSink) {
  NS_PRECONDITION(0!=aSink,"sink cannot be null!");
  nsIContentSink* old=mSink;
  if(aSink) {
    mSink=(nsHTMLContentSink*)(aSink);
  }
  return old;
}


/**
 * This debug method allows us to determine whether or not 
 * we've seen (and can handle) the given context vector.
 *
 * @update  gess4/22/98
 * @param   tags is an array of eHTMLTags
 * @param   count represents the number of items in the tags array
 * @param   aDTD is the DTD we plan to ask for verification
 * @return  TRUE if we know how to handle it, else false
 */
PRBool VerifyContextVector(PRInt32 aTags[],PRInt32 count,nsIDTD* aDTD) {

  PRBool  result=PR_TRUE;

  if(0!=gVerificationOutputDir) {
  
    if(aDTD){

#ifdef XP_PC
      char    path[_MAX_PATH+1];
      strcpy(path,gVerificationOutputDir);
#endif

      for(int i=0;i<count;i++){

#ifdef NS_WIN32
        strcat(path,"/");
        const char* name=GetTagName(aTags[i]);
        strcat(path,name);
        mkdir(path);
#endif
      }
      //ok, now see if we understand this vector
      result=aDTD->VerifyContextVector(aTags,count);
    }
    if(PR_FALSE==result){
      //add debugging code here to record the fact that we just encountered
      //a context vector we don't know how to handle.
    }
  }

  return result;
}


/**
 *  This is where we loop over the tokens created in the 
 *  tokenization phase, and try to make sense out of them. 
 *
 *  @update  gess 3/25/98
 *  @param   
 *  @return  PR_TRUE if parse succeeded, PR_FALSE otherwise.
 */
PRBool nsHTMLParser::IterateTokens() {
  nsDeque& deque=mTokenizer->GetDeque();
  nsDequeIterator e=deque.End(); 

  if(mCurrentPos)
    delete mCurrentPos; //don't leak, now!
  mCurrentPos=new nsDequeIterator(deque.Begin());

  CToken* theToken;
  PRBool  done=PR_FALSE;
  PRBool  result=PR_TRUE;
  PRInt32 iteration=0;

  while((!done) && (result)) {
    theToken=(CToken*)mCurrentPos->GetCurrent();
    eHTMLTokenTypes type=eHTMLTokenTypes(theToken->GetTokenType());
    iteration++; //debug purposes...

    switch(type){
  
      case eToken_start: 
      case eToken_text:
      case eToken_newline:
      case eToken_whitespace:
        result=HandleStartToken(theToken); break;
      
      case eToken_end:
        result=HandleEndToken(theToken); break;
      
      case eToken_entity:
        result=HandleEntityToken(theToken); break;
    
      case eToken_skippedcontent:  
        //used in cases like <SCRIPT> where we skip over script content.
        result=HandleSkippedContentToken(theToken); break;
      
      case eToken_attribute:
        result=HandleAttributeToken(theToken); break;
      
      case eToken_style:
        result=HandleStyleToken(theToken); break;
      
      case eToken_comment:
        result=HandleCommentToken(theToken); break;
      
      default:
        //for all these non-interesting types, just skip them for now.
        //later you have to Handle them, because they're relevent to certain containers (eg PRE).
        break;
    }
    VerifyContextVector(mContextStack,mContextStackPos,mDTD);
    ++(*mCurrentPos);
    done=PRBool(!(*mCurrentPos<e));
  }

  //One last thing...close any open containers.
  if((PR_TRUE==result) && (mContextStackPos>0)) {
    result=CloseContainersTo(0);
  }
  return result;
}


/**
 *  This is the main controlling routine in the parsing process. 
 *  Note that it may get called multiple times for the same scanner, 
 *  since this is a pushed based system, and all the tokens may 
 *  not have been consumed by the scanner during a given invocation 
 *  of this method. 
 *
 *  @update  gess 3/25/98
 *  @param   aFilename -- const char* containing file to be parsed.
 *  @return  PR_TRUE if parse succeeded, PR_FALSE otherwise.
 */
PRBool nsHTMLParser::Parse(nsIURL* aURL){
  eParseMode  theMode=eParseMode_navigator;
  const char* theModeStr= PR_GetEnv("PARSE_MODE");
  const char* other="other";

  if(theModeStr) 
    if(0==nsCRT::strcasecmp(other,theModeStr))
      theMode=eParseMode_other;

  return Parse(aURL,theMode);
}

/**
 *  This is the main controlling routine in the parsing process. 
 *  Note that it may get called multiple times for the same scanner, 
 *  since this is a pushed based system, and all the tokens may 
 *  not have been consumed by the scanner during a given invocation 
 *  of this method. 
 *
 *  @update  gess 3/25/98
 *  @param   aFilename -- const char* containing file to be parsed.
 *  @return  PR_TRUE if parse succeeded, PR_FALSE otherwise.
 */
PRBool nsHTMLParser::Parse(nsIURL* aURL,eParseMode aMode){
  NS_PRECONDITION(0!=aURL,kNullURL);
  
  PRBool result=PR_FALSE;
  if(aURL) {

    result=PR_TRUE;
    mParseMode=aMode;
    ITokenizerDelegate* theDelegate=0;
    
    mDTD=0;
    switch(mParseMode) {
      case eParseMode_navigator:
        theDelegate=new CNavDelegate();
        if(theDelegate)
          mDTD=theDelegate->GetDTD();
        break;
      case eParseMode_other:
        theDelegate=new COtherDelegate();
        if(theDelegate)
          mDTD=theDelegate->GetDTD();
        break;
      default:
        break;
    }
    if(!theDelegate) {
      NS_ERROR(kNullTokenizer);
      return PR_FALSE;
    }

    if(mDTD)
      mDTD->SetParser(this);
    mTokenizer=new CTokenizer(aURL, theDelegate, mParseMode);

    mSink->WillBuildModel();
#ifdef __INCREMENTAL 
    int iter=-1;
    for(;;){
      mSink->WillResume();
      mTokenizer->TokenizeAvailable(++iter);
      mSink->WillInterrupt();
    }
#else
    mTokenizer->Tokenize();
#endif
    result=IterateTokens();
    mSink->DidBuildModel();
  }
  return result;
}


/**
 *  This routine is called to cause the parser to continue
 *  parsing it's underling stream. This call allows the
 *  parse process to happen in chunks, such as when the
 *  content is push based, and we need to parse in pieces.
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  PR_TRUE if parsing concluded successfully.
 */
PRBool nsHTMLParser::ResumeParse() {
  mSink->WillResume();
  int iter=0;
  PRInt32 errcode=mTokenizer->TokenizeAvailable(iter);
  if(kInterrupted==errcode)
    mSink->WillInterrupt();
  PRBool result=IterateTokens();
  return result;
}


/**
 * 
 * @update  gess4/22/98
 * @param 
 * @return
 */
PRInt32 nsHTMLParser::GetStack(PRInt32* aStackPtr) {
  aStackPtr=&mContextStack[0];
  return mContextStackPos;
}


/**
 * 
 * @update  gess4/22/98
 * @param 
 * @return
 */
PRInt32 nsHTMLParser::CollectAttributes(nsCParserNode& aNode){
  eHTMLTokenTypes subtype=eToken_attribute;
  nsDeque&         deque=mTokenizer->GetDeque();
  nsDequeIterator  end=deque.End();
  
  while((*mCurrentPos<end) && (eToken_attribute==subtype)) {
    CHTMLToken* tkn=(CHTMLToken*)(++(*mCurrentPos));
    if(tkn){
      subtype=eHTMLTokenTypes(tkn->GetTokenType());
      if(eToken_attribute==subtype) {
        aNode.AddAttribute(tkn);
      } 
      else (*mCurrentPos)--;
    }
  }
  return aNode.GetAttributeCount();

}


/**
 * 
 * @update  gess4/22/98
 * @param 
 * @return
 */
PRInt32 nsHTMLParser::CollectSkippedContent(nsCParserNode& aNode){
  eHTMLTokenTypes subtype=eToken_attribute;
  nsDeque&         deque=mTokenizer->GetDeque();
  nsDequeIterator  end=deque.End();
  PRInt32         count=0;

  while((*mCurrentPos!=end) && (eToken_attribute==subtype)) {
    CHTMLToken* tkn=(CHTMLToken*)(++(*mCurrentPos));
    subtype=eHTMLTokenTypes(tkn->GetTokenType());
    if(eToken_skippedcontent==subtype) {
      aNode.SetSkippedContent(tkn);
      count++;
    } 
    else (*mCurrentPos)--;
  }
  return count;
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
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @param   aNode -- CParserNode representing this start token
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
PRBool nsHTMLParser::HandleDefaultStartToken(CToken* aToken,eHTMLTags aChildTag,nsCParserNode& aNode) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  eHTMLTags parentTag=(eHTMLTags)GetTopNode();
  PRBool    result=mDTD->CanContain(parentTag,aChildTag);

  if(PR_FALSE==result){
    result=CreateContextStackFor(aChildTag);
    if(PR_FALSE==result) {
      //if you're here, then the new topmost container can't contain aToken.
      //You must determine what container hierarchy you need to hold aToken,
      //and create that on the parsestack.
      result=ReduceContextStackFor(aChildTag);
      if(PR_FALSE==mDTD->CanContain(GetTopNode(),aChildTag)) {
        //we unwound too far; now we have to recreate a valid context stack.
        result=CreateContextStackFor(aChildTag);
      }
    }
  }

  if(mDTD->IsContainer(aChildTag)){
    result=OpenContainer(aNode);
  }
  else {
    result=AddLeaf(aNode);
  }
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
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @param   aNode -- CParserNode representing this start token
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
PRBool nsHTMLParser::HandleStartToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  PRBool        result=PR_FALSE;
  CStartToken*  st= (CStartToken*)(aToken);
  eHTMLTags     tokenTagType=st->GetHTMLTag();


  //Begin by gathering up attributes...
  nsCParserNode   attrNode((CHTMLToken*)aToken);
  PRInt32 attrCount=CollectAttributes(attrNode);

    //now check to see if this token should be omitted...
  if(PR_TRUE==mDTD->CanOmit(GetTopNode(),tokenTagType)) {
    return PR_TRUE;
  }
  
  switch(tokenTagType) {

    case eHTMLTag_html:
      result=OpenHTML(attrNode); break;

    case eHTMLTag_title:
      {
        nsCParserNode theNode(st);
        result=OpenHead(theNode); //open the head...
        CollectSkippedContent(attrNode);
        result=mSink->SetTitle(attrNode.GetSkippedContent());
        result=CloseHead(theNode); //close the head...
      }
      break;

    case eHTMLTag_textarea:
      {
        CollectSkippedContent(attrNode);
        result=AddLeaf(attrNode);
      }
      break;

    case eHTMLTag_form:
      result = OpenForm(attrNode);
      break;

    case eHTMLTag_meta:
    case eHTMLTag_link:
      {
        nsCParserNode theNode((CHTMLToken*)aToken);
        result=OpenHead(theNode);
        result=AddLeaf(theNode);
        result=CloseHead(theNode);
      }
      break;

    case eHTMLTag_style:
      {
        nsCParserNode theNode((CHTMLToken*)aToken);
        result=OpenHead(theNode);
        CollectSkippedContent(attrNode);
        result=AddLeaf(attrNode);
        result=CloseHead(theNode);
      }
      break;

    case eHTMLTag_script:
      result=HandleScriptToken(st); break;
      

    case eHTMLTag_map:
      // Put map into the head section
      result=OpenHead(attrNode);
      result=OpenContainer(attrNode);
      break;

    case eHTMLTag_head:
      result=PR_TRUE; break; //ignore head tags...

    case eHTMLTag_select:
      result=PR_FALSE;

    default:
      result=HandleDefaultStartToken(aToken,tokenTagType,attrNode);
      break;
  }

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
PRBool nsHTMLParser::HandleEndToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  PRBool      result=PR_FALSE;
  CEndToken*  et = (CEndToken*)(aToken);
  eHTMLTags   tokenTagType=et->GetHTMLTag();

    //now check to see if this token should be omitted...
  if(PR_TRUE==mDTD->CanOmitEndTag(GetTopNode(),tokenTagType)) {
    return PR_TRUE;
  }

  nsCParserNode theNode((CHTMLToken*)aToken);
  switch(tokenTagType) {

    case eHTMLTag_style:
    case eHTMLTag_link:
    case eHTMLTag_meta:
    case eHTMLTag_textarea:
    case eHTMLTag_title:
    case eHTMLTag_head:
    case eHTMLTag_script:
      result=PR_TRUE; 
      break;

    case eHTMLTag_map:
      result=CloseContainer(theNode);
      result=CloseHead(theNode);
      break;

    case eHTMLTag_form:
      {
        nsCParserNode aNode((CHTMLToken*)aToken);
        result=CloseForm(aNode);
      }
      break;

    default:
      if(mDTD->IsContainer(tokenTagType)){
        result=CloseContainersTo(tokenTagType); 
      }
      result=PR_TRUE;
      //
      break;
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
PRBool nsHTMLParser::HandleEntityToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);
  CEntityToken*  et = (CEntityToken*)(aToken);
  PRBool result=PR_TRUE;
  nsCParserNode aNode((CHTMLToken*)aToken);
  result=AddLeaf(aNode);
  return result;
}

/**
 *  This method gets called when a comment token has been 
 *  encountered in the parse process. After making sure
 *  we're somewhere in the body, we handle the comment
 *  in the same code that we use for text.
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
PRBool nsHTMLParser::HandleCommentToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);
  return PR_TRUE;
}

/**
 *  This method gets called when a skippedcontent token has 
 *  been encountered in the parse process. After verifying 
 *  that the topmost container can contain text, we call 
 *  AddLeaf to store this token in the top container.
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
PRBool nsHTMLParser::HandleSkippedContentToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  PRBool result=PR_TRUE;

  if(IsWithinBody()) {
    nsCParserNode aNode((CHTMLToken*)aToken);
    result=AddLeaf(aNode);
  }
  return result;
}

/**
 *  This method gets called when an attribute token has been 
 *  encountered in the parse process. This is an error, since
 *  all attributes should have been accounted for in the prior
 *  start or end tokens
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
PRBool nsHTMLParser::HandleAttributeToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);
  NS_ERROR("attribute encountered -- this shouldn't happen!");

  CAttributeToken*  at = (CAttributeToken*)(aToken);
  PRBool result=PR_TRUE;
  return result;
}

/**
 *  This method gets called when a script token has been 
 *  encountered in the parse process. 
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
PRBool nsHTMLParser::HandleScriptToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  CScriptToken*  st = (CScriptToken*)(aToken);
  PRBool result=PR_TRUE;
  return result;
}

/**
 *  This method gets called when a style token has been 
 *  encountered in the parse process. 
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
PRBool nsHTMLParser::HandleStyleToken(CToken* aToken){
  NS_PRECONDITION(0!=aToken,kNullToken);

  CStyleToken*  st = (CStyleToken*)(aToken);
  PRBool result=PR_TRUE;
  return result;
}

/**
 *  This method gets called to determine whether a given 
 *  tag is itself a container
 *  
 *  @update  gess 3/25/98
 *  @param   aTag -- tag to test for containership
 *  @return  PR_TRUE if given tag can contain other tags
 */
PRBool nsHTMLParser::IsWithinBody(void) const {
  for(int i=0;i<mContextStackPos;i++) {
    if(eHTMLTag_body==mContextStack[i])
      return PR_TRUE;
  }
  return PR_FALSE;
}


/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * 
 * @update  gess4/22/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
PRBool nsHTMLParser::OpenHTML(const nsIParserNode& aNode){
  NS_PRECONDITION(mContextStackPos >= 0, kInvalidTagStackPos);

  PRBool result=mSink->OpenHTML(aNode); 
  mContextStack[mContextStackPos++]=(eHTMLTags)aNode.GetNodeType();
  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 *
 * @update  gess4/6/98
 * @param   aNode -- next node to be removed from our model
 * @return  TRUE if ok, FALSE if error
 */
PRBool nsHTMLParser::CloseHTML(const nsIParserNode& aNode){
  NS_PRECONDITION(mContextStackPos > 0, kInvalidTagStackPos);
  PRBool result=mSink->CloseHTML(aNode); 
  mContextStack[--mContextStackPos]=eHTMLTag_unknown;
  return result;
}


/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
PRBool nsHTMLParser::OpenHead(const nsIParserNode& aNode){
  mContextStack[mContextStackPos++]=eHTMLTag_head;
  PRBool result=mSink->OpenHead(aNode); 
  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be removed from our model
 * @return  TRUE if ok, FALSE if error
 */
PRBool nsHTMLParser::CloseHead(const nsIParserNode& aNode){
  PRBool result=mSink->CloseHead(aNode); 
  mContextStack[--mContextStackPos]=eHTMLTag_unknown;
  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
PRBool nsHTMLParser::OpenBody(const nsIParserNode& aNode){
  NS_PRECONDITION(mContextStackPos >= 0, kInvalidTagStackPos);

  PRBool    result=PR_TRUE;
  eHTMLTags topTag=(eHTMLTags)nsHTMLParser::GetTopNode();

  if(eHTMLTag_html!=topTag) {
    
    //ok, there are two cases:
    //  1. Nobody opened the html container
    //  2. Someone left the head (or other) open
    PRInt32 pos=GetTopmostIndex(eHTMLTag_html);
    if(kNotFound!=pos) {
      //if you're here, it means html is open, 
      //but some other tag(s) are in the way.
      //So close other tag(s).
      result=CloseContainersTo(pos+1);
    } else {
      //if you're here, it means that there is
      //no HTML tag in document. Let's open it.

      result=CloseContainersTo(0);  //close current stack containers.

      nsAutoString  empty;
      CHTMLToken    token(empty);
      nsCParserNode htmlNode(&token);

      token.SetHTMLTag(eHTMLTag_html);  //open the html container...
      result=OpenHTML(htmlNode);
    }
  }

  if(PR_TRUE==result) {
    result=mSink->OpenBody(aNode); 
    mContextStack[mContextStackPos++]=(eHTMLTags)aNode.GetNodeType();
  }
  return result;
}

/**
 * This method does two things: 1st, help close
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be removed from our model
 * @return  TRUE if ok, FALSE if error
 */
PRBool nsHTMLParser::CloseBody(const nsIParserNode& aNode){
  NS_PRECONDITION(mContextStackPos >= 0, kInvalidTagStackPos);
  PRBool result=mSink->CloseBody(aNode); 
  mContextStack[--mContextStackPos]=eHTMLTag_unknown;
  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
PRBool nsHTMLParser::OpenForm(const nsIParserNode& aNode){
  if(mHasOpenForm)
    CloseForm(aNode);
  mHasOpenForm=PR_TRUE;
  return mSink->OpenForm(aNode); 
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be removed from our model
 * @return  TRUE if ok, FALSE if error
 */
PRBool nsHTMLParser::CloseForm(const nsIParserNode& aNode){
  PRBool result=PR_TRUE;
  if(mHasOpenForm) {
    mHasOpenForm=PR_FALSE;
    result=mSink->CloseForm(aNode); 
  }
  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
PRBool nsHTMLParser::OpenFrameset(const nsIParserNode& aNode){
  NS_PRECONDITION(mContextStackPos >= 0, kInvalidTagStackPos);
  PRBool result=mSink->OpenFrameset(aNode); 
  mContextStack[mContextStackPos++]=(eHTMLTags)aNode.GetNodeType();
  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be removed from our model
 * @return  TRUE if ok, FALSE if error
 */
PRBool nsHTMLParser::CloseFrameset(const nsIParserNode& aNode){
  NS_PRECONDITION(mContextStackPos > 0, kInvalidTagStackPos);
  PRBool result=mSink->CloseFrameset(aNode); 
  mContextStack[--mContextStackPos]=eHTMLTag_unknown;
  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
PRBool nsHTMLParser::OpenContainer(const nsIParserNode& aNode){
  NS_PRECONDITION(mContextStackPos > 0, kInvalidTagStackPos);
  PRBool result=PR_FALSE;
  
  //XXX Hack! We know this is wrong, but it works
  //for the general case until we get it right.
  switch(aNode.GetNodeType()) {

    case eHTMLTag_html:
      result=OpenHTML(aNode); break;

    case eHTMLTag_body:
      result=OpenBody(aNode); break;

    case eHTMLTag_style:
    case eHTMLTag_textarea:
    case eHTMLTag_head:
    case eHTMLTag_title:
      result=PR_TRUE;
      break;

    case eHTMLTag_form:
      result=OpenForm(aNode); break;

    default:
      result=mSink->OpenContainer(aNode); 
      mContextStack[mContextStackPos++]=(eHTMLTags)aNode.GetNodeType();
      break;
  }
  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be removed from our model
 * @return  TRUE if ok, FALSE if error
 */
PRBool nsHTMLParser::CloseContainer(const nsIParserNode& aNode){
  NS_PRECONDITION(mContextStackPos > 0, kInvalidTagStackPos);
  PRBool result=PR_FALSE;
  
  //XXX Hack! We know this is wrong, but it works
  //for the general case until we get it right.
  switch(aNode.GetNodeType()) {

    case eHTMLTag_html:
      result=CloseHTML(aNode); break;

    case eHTMLTag_style:
    case eHTMLTag_textarea:
      break;

    case eHTMLTag_head:
      //result=CloseHead(aNode); 
      break;

    case eHTMLTag_body:
      result=CloseBody(aNode); break;

    case eHTMLTag_form:
      result=CloseForm(aNode); break;

    case eHTMLTag_title:
    default:
      result=mSink->CloseContainer(aNode); 
      mContextStack[--mContextStackPos]=eHTMLTag_unknown;
      break;
  }
  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   
 * @return  TRUE if ok, FALSE if error
 */
PRBool nsHTMLParser::CloseContainersTo(PRInt32 anIndex){
  NS_PRECONDITION(mContextStackPos > 0, kInvalidTagStackPos);
  PRBool result=PR_TRUE;

  nsAutoString empty;
  CEndToken aToken(empty);
  nsCParserNode theNode(&aToken);

  if((anIndex<mContextStackPos) && (anIndex>=0)) {
    while(mContextStackPos>anIndex) {
      aToken.SetHTMLTag((eHTMLTags)mContextStack[mContextStackPos-1]);
      result=CloseContainer(theNode);
    }
  }
  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   
 * @return  TRUE if ok, FALSE if error
 */
PRBool nsHTMLParser::CloseContainersTo(eHTMLTags aTag){
  NS_PRECONDITION(mContextStackPos > 0, kInvalidTagStackPos);

  PRInt32 pos=GetTopmostIndex(aTag);
  PRBool result=PR_FALSE;

  if(kNotFound!=pos) {
    //the tag is indeed open, so close it.
    result=CloseContainersTo(pos);
  }
  else {
    eHTMLTags theParentTag=(eHTMLTags)mDTD->GetDefaultParentTagFor(aTag);
    pos=GetTopmostIndex(theParentTag);
    if(kNotFound!=pos) {
      //the parent container is open, so close it instead
      result=CloseContainersTo(pos+1);
    }
    else {
      //XXX HACK! This is a real problem -- the unhandled case.!
    }
  }
  return result;
}

/**
 * This method causes the topmost container on the stack
 * to be closed. 
 * @update  gess4/6/98
 * @see     CloseContainer()
 * @param   
 * @return  TRUE if ok, FALSE if error
 */
PRBool nsHTMLParser::CloseTopmostContainer(){
  NS_PRECONDITION(mContextStackPos > 0, kInvalidTagStackPos);

  nsAutoString empty;
  CEndToken aToken(empty);
  aToken.SetHTMLTag((eHTMLTags)mContextStack[mContextStackPos-1]);
  nsCParserNode theNode(&aToken);

  return CloseContainer(theNode);
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
PRBool nsHTMLParser::AddLeaf(const nsIParserNode& aNode){
  PRBool result=mSink->AddLeaf(aNode); 
  return result;
}

/**
 *  This method gets called to create a valid context stack
 *  for the given child. We compare the current stack to the
 *  default needs of the child, and push new guys onto the
 *  stack until the child can be properly placed.
 *
 *  @update  gess 4/8/98
 *  @param   aChildTag is the child for whom we need to 
 *           create a new context vector
 *  @return  true if we succeeded, otherwise false
 */
PRBool nsHTMLParser::CreateContextStackFor(PRInt32 aChildTag){
  nsAutoString  theVector;
  
  PRBool result=PR_FALSE;
  PRInt32 pos=0;
  PRInt32 cnt=0;
  PRInt32 theTop=GetTopNode();
  
  if(PR_TRUE==mDTD->ForwardPropagate(theVector,theTop,aChildTag)){
    //add code here to build up context stack based on forward propagated context vector...
    pos=0;
    cnt=theVector.Length()-1;
    result=PRBool(mContextStack[mContextStackPos-1]==theVector[cnt]);
  }
  else {
    PRBool tempResult;
    if(eHTMLTag_unknown!=theTop) {
      tempResult=mDTD->BackwardPropagate(theVector,theTop,aChildTag);
      if(eHTMLTag_html!=theTop)
        mDTD->BackwardPropagate(theVector,eHTMLTag_html,theTop);
    }
    else tempResult=mDTD->BackwardPropagate(theVector,eHTMLTag_html,aChildTag);

    if(PR_TRUE==tempResult) {

      //propagation worked, so pop unwanted containers, push new ones, then exit...
      pos=0;
      cnt=theVector.Length();
      result=PR_TRUE;
      while(pos<mContextStackPos) {
        if(mContextStack[pos]==theVector[cnt-1-pos]) {
          pos++;
        }
        else {
          //if you're here, you have something on the stack
          //that doesn't match your needed tags order.
          result=CloseContainersTo(pos);
          break;
        }
      } //while
    } //elseif
  } //elseif

    //now, build up the stack according to the tags 
    //you have that aren't in the stack...
  if(PR_TRUE==result){
    nsAutoString  empty;
    for(int i=pos;i<cnt;i++) {
      CStartToken* st=new CStartToken(empty);
      st->SetHTMLTag((eHTMLTags)theVector[cnt-1-i]);
      HandleStartToken(st);
    }
  }
  return result;
}


/**
 *  This method gets called to ensure that the context
 *  stack is properly set up for the given child. 
 *  We pop containers off the stack (all the way down 
 *  html) until we get a container that can contain
 *  the given child.
 *  
 *  @update  gess 4/8/98
 *  @param   
 *  @return  
 */
PRBool nsHTMLParser::ReduceContextStackFor(PRInt32 aChildTag){
  PRBool    result=PR_TRUE;
  eHTMLTags topTag=(eHTMLTags)nsHTMLParser::GetTopNode();

  while( (topTag!=kNotFound) && 
         (PR_FALSE==mDTD->CanContain(topTag,aChildTag)) &&
         (PR_FALSE==mDTD->CanContainIndirect(topTag,aChildTag))) {
    CloseTopmostContainer();
    topTag=(eHTMLTags)nsHTMLParser::GetTopNode();
  }
  return result;
}




