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
  
#include "nsHTMLParser.h"
#include "nsHTMLDelegate.h"
#include "nsHTMLContentSink.h"
#include "nsTokenizer.h"
#include "nsHTMLTokens.h"
#include "nsString.h"
#include "nsIURL.h"
#include "nsDefaultTokenHandler.h"
#include "nsCRT.h"
#include "nsHTMLDTD.h"


static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kClassIID, NS_IHTML_PARSER_IID); 
static NS_DEFINE_IID(kIParserIID, NS_IPARSER_IID);


/**-------------------------------------------------------
 *  This method is defined in nsIParser. It is used to 
 *  cause the COM-like construction of an nsHTMLParser.
 *  
 *  @update  gess 3/25/98
 *  @param   nsIParser** ptr to newly instantiated parser
 *  @return  NS_xxx error result
 *------------------------------------------------------*/
NS_HTMLPARS nsresult NS_NewHTMLParser(nsIParser** aInstancePtrResult)
{
  nsHTMLParser *it = new nsHTMLParser();

  if (it == 0) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kClassIID, (void **) aInstancePtrResult);
}

/**-------------------------------------------------------
 *  init the set of default token handlers...
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
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


/**-------------------------------------------------------
 *  default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
nsHTMLParser::nsHTMLParser() {
  NS_INIT_REFCNT();
  mSink=0;
  mTokenHandlerCount=0;
  mTagStackPos=0;
  mCurrentPos=0;
  nsCRT::zero(mTagStack,sizeof(mTagStack));
  nsCRT::zero(mTokenHandlers,sizeof(mTokenHandlers));
  mDTD=new nsHTMLDTD();
  InitializeDefaultTokenHandlers();
}


/**-------------------------------------------------------
 *  Default destructor
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
nsHTMLParser::~nsHTMLParser() {
  if(mCurrentPos)
    delete mCurrentPos;
  mCurrentPos=0;
  if(mDTD)
    delete mDTD;
  mDTD=0;
}


NS_IMPL_ADDREF(nsHTMLParser)
NS_IMPL_RELEASE(nsHTMLParser)
//NS_IMPL_ISUPPORTS(nsHTMLParser,NS_IHTML_PARSER_IID)


/**-------------------------------------------------------
 *  This method gets called as part of our COM-like interfaces.
 *  Its purpose is to create an interface to parser object
 *  of some type.
 *  
 *  @update   gess 3/25/98
 *  @param    nsIID  id of object to discover
 *  @param    aInstancePtr ptr to newly discovered interface
 *  @return   NS_xxx result code
 *------------------------------------------------------*/
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


/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
eHTMLTags nsHTMLParser::NodeAt(PRInt32 aPos) const {
  NS_PRECONDITION(0 <= aPos, "bad nodeAt");
  if((aPos>-1) && (aPos<mTagStackPos))
    return mTagStack[aPos];
  return (eHTMLTags)kNotFound;
}

/**-------------------------------------------------------
 *  This method retrieves the HTMLTag type of the topmost
 *  container on the stack.
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
eHTMLTags nsHTMLParser::GetTopNode() const {
  if(mTagStackPos) 
    return mTagStack[mTagStackPos-1];
  return (eHTMLTags)kNotFound;
}

/**-------------------------------------------------------
 *  Determine whether the given tag is open anywhere
 *  in our context stack.
 *  
 *  @update  gess 4/2/98
 *  @param   eHTMLTags tag to be searched for in stack
 *  @return  topmost index of tag on stack
 *------------------------------------------------------*/
PRInt32 nsHTMLParser::GetTopmostIndex(eHTMLTags aTag) const {
  for(int i=mTagStackPos-1;i>=0;i--){
    if(mTagStack[i]==aTag)
      return i;
  }
  return kNotFound;
}

/**-------------------------------------------------------
 *  Determine whether the given tag is open anywhere
 *  in our context stack.
 *  
 *  @update  gess 4/2/98
 *  @param   eHTMLTags tag to be searched for in stack
 *  @return  topmost index of tag on stack
 *------------------------------------------------------*/
PRBool nsHTMLParser::IsOpen(eHTMLTags aTag) const {
  PRInt32 pos=GetTopmostIndex(aTag);
  return PRBool(kNotFound!=pos);
}

/**-------------------------------------------------------
 *  Gets the number of open containers on the stack.
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRInt32 nsHTMLParser::GetStackPos() const {
  return mTagStackPos;
}


/**-------------------------------------------------------
 *  Finds a tag handler for the given tag type, given in string.
 *  
 *  @update  gess 4/2/98
 *  @param   aString contains name of tag to be handled
 *  @return  valid tag handler (if found) or null
 *------------------------------------------------------*/
CDefaultTokenHandler* nsHTMLParser::GetTokenHandler(const nsString& aString) const{
  eHTMLTokenTypes theType=DetermineTokenType(aString);
  return GetTokenHandler(theType);
}


/**-------------------------------------------------------
 *  Finds a tag handler for the given tag type.
 *  
 *  @update  gess 4/2/98
 *  @param   aTagType type of tag to be handled
 *  @return  valid tag handler (if found) or null
 *------------------------------------------------------*/
CDefaultTokenHandler* nsHTMLParser::GetTokenHandler(eHTMLTokenTypes aType) const {
  for(int i=0;i<mTokenHandlerCount;i++) {
    if(mTokenHandlers[i]->CanHandle(aType)) {
      return mTokenHandlers[i];
    }
  }
  return 0;
}


/**-------------------------------------------------------
 *  Register a handler.
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
CDefaultTokenHandler* nsHTMLParser::AddTokenHandler(CDefaultTokenHandler* aHandler) {
  NS_ASSERTION(0!=aHandler,"Error: Null handler argument");
  if(aHandler) {
  }
  return 0;
}

/**-------------------------------------------------------
 *  This method gets called in order to set the content
 *  sink for this parser to dump nodes to.
 *  
 *  @update  gess 3/25/98
 *  @param   nsIContentSink interface for node receiver
 *  @return  
 *------------------------------------------------------*/
nsIContentSink* nsHTMLParser::SetContentSink(nsIContentSink* aSink) {
  NS_PRECONDITION(0!=aSink,"sink cannot be null!");
  nsIContentSink* old=mSink;
  if(aSink) {
    mSink=(nsHTMLContentSink*)(aSink);
  }
  return old;
}

/**-------------------------------------------------------
 *  This is the main controlling routine in the parsing process. 
 *  Note that it may get called multiple times for the same scanner, 
 *  since this is a pushed based system, and all the tokens may 
 *  not have been consumed by the scanner during a given invocation 
 *  of this method. 
 *
 *  @update  gess 3/25/98
 *  @param   aFilename -- const char* containing file to be parsed.
 *  @return  PR_TRUE if parse succeeded, PR_FALSE otherwise.
 *------------------------------------------------------*/
PRBool nsHTMLParser::Parse(nsIURL* aURL){
  NS_PRECONDITION(0!=aURL,"Error: URL cannot be null!");
  
  PRBool result=PR_FALSE;
  if(aURL) {

    result=PR_TRUE;
    CHTMLTokenizerDelegate delegate;
    mTokenizer=new CTokenizer(aURL, delegate);
    mTokenizer->Tokenize();

#ifdef VERBOSE_DEBUG
    mTokenizer->DebugDumpTokens(cout);
#endif

    CDeque& deque=mTokenizer->GetDeque();
    CDequeIterator e=deque.End();

    if(mCurrentPos)
      delete mCurrentPos; //don't leak, now!
    mCurrentPos=new CDequeIterator(deque.Begin());

    CToken* theToken;
    PRBool  done=PR_FALSE;
    PRInt32 iteration=0;

    while((!done) && (result)) {
      theToken=*mCurrentPos;
      eHTMLTokenTypes type=eHTMLTokenTypes(theToken->GetTokenType());
      iteration++; //debug purposes...
      switch(eHTMLTokenTypes(type)){
        case eToken_start: 
          result=HandleStartToken(theToken); break;
        case eToken_end:
          result=HandleEndToken(theToken); break;
        case eToken_entity:
          result=HandleEntityToken(theToken); break;
        case eToken_text:
          result=HandleTextToken(theToken); break;
        case eToken_newline:
          result=HandleNewlineToken(theToken); break;
        case eToken_skippedcontent:  
          //used in cases like <SCRIPT> where we skip over script content.
          result=HandleSkippedContentToken(theToken); break;
        case eToken_attribute:
          result=HandleAttributeToken(theToken); break;
        case eToken_script:
          result=HandleScriptToken(theToken); break;
        case eToken_style:
          result=HandleStyleToken(theToken); break;
        case eToken_comment:
          result=HandleCommentToken(theToken); break;
        case eToken_whitespace:
          result=HandleWhitespaceToken(theToken); break;
        default:
          //for all these non-interesting types, just skip them for now.
          //later you have to Handle them, because they're relevent to certain containers (eg PRE).
          break;
      }
      mDTD->VerifyContextStack(mTagStack,mTagStackPos);
      done=PRBool(++(*mCurrentPos)==e);
    }

    //One last thing...close any open containers.
    if((PR_TRUE==result) && (mTagStackPos>0)) {
      result=CloseContainersTo(0);
    }
  }
  return result;
}


/**-------------------------------------------------------
 *  This routine is called to cause the parser to continue
 *  parsing it's underling stream. This call allows the
 *  parse process to happen in chunks, such as when the
 *  content is push based, and we need to parse in pieces.
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  PR_TRUE if parsing concluded successfully.
 *------------------------------------------------------*/
PRBool nsHTMLParser::ResumeParse() {
  PRBool result=PR_TRUE;
  return result;
}


/*-------------------------------------------------------
 * 
 * @update	gess4/6/98
 * @param 
 * @return
 *------------------------------------------------------*/
PRInt32 nsHTMLParser::CollectAttributes(nsCParserNode& aNode){
  eHTMLTokenTypes subtype=eToken_attribute;
  CDeque&         deque=mTokenizer->GetDeque();
  CDequeIterator  end=deque.End();
  
  while((*mCurrentPos!=end) && (eToken_attribute==subtype)) {
    CHTMLToken* tkn=(CHTMLToken*)(++(*mCurrentPos));
    subtype=eHTMLTokenTypes(tkn->GetTokenType());
    if(eToken_attribute==subtype) {
      aNode.AddAttribute(tkn);
    } 
    else (*mCurrentPos)--;
  }
  return aNode.GetAttributeCount();
}

/*-------------------------------------------------------
 * 
 * @update	gess4/6/98
 * @param 
 * @return
 *------------------------------------------------------*/
PRInt32 nsHTMLParser::CollectSkippedContent(nsCParserNode& aNode){
  eHTMLTokenTypes subtype=eToken_attribute;
  CDeque&         deque=mTokenizer->GetDeque();
  CDequeIterator  end=deque.End();
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


/**-------------------------------------------------------
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
 *------------------------------------------------------*/
PRBool nsHTMLParser::HandleStartToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,"token cannot be null!");

  PRBool        result=PR_FALSE;
  CStartToken*  st= (CStartToken*)(aToken);
  eHTMLTags     tokenTagType=st->GetHTMLTag();


  //Begin by gathering up attributes...
  nsCParserNode   attrNode((CHTMLToken*)aToken);
  PRInt32 attrCount=CollectAttributes(attrNode);
  switch(tokenTagType) {
    case eHTMLTag_html:
      result=OpenHTML(attrNode); break;
    case eHTMLTag_head:
      result=OpenHead(attrNode); break;
    case eHTMLTag_body:
      result=OpenBody(attrNode); break;
    case eHTMLTag_title:
      {
        nsCParserNode theNode((CHTMLToken*)aToken);
        result=OpenHead(theNode); //open the title...
        CollectSkippedContent(attrNode);
        result=mSink->SetTitle(attrNode.GetSkippedContent());
      }
      break;
    case eHTMLTag_form:
      result = mSink->OpenForm(attrNode);
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
    case eHTMLTag_map:
      // Put map into the head section
      result=OpenHead(attrNode);
      result=OpenContainer(attrNode);
      break;
    default:
      {
        eHTMLTags parentTag=(eHTMLTags)GetTopNode();
        if(kNotFound==parentTag) {
          CreateContextStackFor(tokenTagType);
        }
        else {
          if(PR_FALSE==mDTD->CanContain(parentTag,tokenTagType)){
            //if you're here, then the new topmost container can't contain aToken.
            //You must determine what container hierarchy you need to hold aToken,
            //and create that on the parsestack.
            ReduceContextStackFor(tokenTagType);
            if(PR_FALSE==mDTD->CanContain((eHTMLTags)GetTopNode(),tokenTagType)) {
              //we unwound too far; now we have to recreate a valid context stack.
              result=CreateContextStackFor(tokenTagType);
            }
          }
        }
        //now we think the stack is correct, so fall through 
        //and push our newest token onto the stack...

        if(mDTD->IsContainer(tokenTagType)){
          result=OpenContainer(attrNode);
        }
        else {
          result=AddLeaf(attrNode);
        }
        result=PR_TRUE;
        //actually, you want to push the start token and ALL it's attributes...
      }
      break;
  }

  return result;
}

/**-------------------------------------------------------
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
 *------------------------------------------------------*/
PRBool nsHTMLParser::HandleEndToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,"token cannot be null!");

  PRBool      result=PR_FALSE;
  CEndToken*  st = (CEndToken*)(aToken);
  eHTMLTags   tokenTagType=st->GetHTMLTag();

  nsCParserNode theNode((CHTMLToken*)aToken);
  switch(tokenTagType) {
    case eHTMLTag_html:
      result=CloseContainersTo(tokenTagType); break;

    case eHTMLTag_style:
    case eHTMLTag_link:
    case eHTMLTag_meta:
      result=PR_TRUE;
      break;

    case eHTMLTag_map:
      result=CloseContainer(theNode);
      result=CloseHead(theNode);
      break;

    case eHTMLTag_head:
      result=CloseHead(theNode); break;
    case eHTMLTag_body:
      result=CloseContainersTo(eHTMLTag_body); break;
    case eHTMLTag_title:
      result=CloseHead(theNode); break; //close the title...
    case eHTMLTag_form:
      {
        nsCParserNode aNode((CHTMLToken*)aToken);
        result=mSink->CloseForm(aNode);
      }
      break;
    default:
      if(mDTD->IsContainer(tokenTagType)){
        result=CloseContainer(theNode);
      }
      result=PR_TRUE;
      //actually, you want to push the start token and ALL it's attributes...
      break;
  }
  return result;
}

/**-------------------------------------------------------
 *  This method gets called when an entity token has been 
 *  encountered in the parse process. 
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 *------------------------------------------------------*/
PRBool nsHTMLParser::HandleEntityToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,"token cannot be null!");
  CEntityToken*  et = (CEntityToken*)(aToken);
  PRBool result=PR_TRUE;
  nsCParserNode aNode((CHTMLToken*)aToken);
  result=AddLeaf(aNode);
  return result;
}

/**-------------------------------------------------------
 *  This method gets called when a comment token has been 
 *  encountered in the parse process. After making sure
 *  we're somewhere in the body, we handle the comment
 *  in the same code that we use for text.
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 *------------------------------------------------------*/
PRBool nsHTMLParser::HandleCommentToken(CToken* aToken) {
  return PR_TRUE;
}

/**-------------------------------------------------------
 *  This method gets called when a ws token has been 
 *  encountered in the parse process. After making sure
 *  we're somewhere in the body, we handle the ws
 *  in the same code that we use for text.
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 *------------------------------------------------------*/
PRBool nsHTMLParser::HandleWhitespaceToken(CToken* aToken) {
  PRBool result=PR_TRUE;
  if(PR_TRUE==IsWithinBody()) {
    //now we know we're in the body <i>somewhere</i>. 
    //let's see if the current tag can contain a ws.
    result=mDTD->CanDisregard(mTagStack[mTagStackPos-1],eHTMLTag_whitespace);
    if(PR_FALSE==result)
      result=HandleTextToken(aToken);
  }
  return result;
}

/**-------------------------------------------------------
 *  This method gets called when a newline token has been 
 *  encountered in the parse process. After making sure
 *  we're somewhere in the body, we handle the newline
 *  in the same code that we use for text.
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 *------------------------------------------------------*/
PRBool nsHTMLParser::HandleNewlineToken(CToken* aToken) {
  PRBool result=PR_TRUE;
  if(PR_TRUE==IsWithinBody()) {
    //now we know we're in the body <i>somewhere</i>. 
    //let's see if the current tag can contain a ws.
    result=mDTD->CanDisregard(mTagStack[mTagStackPos-1],eHTMLTag_newline);
    if(PR_FALSE==result)
      result=HandleTextToken(aToken);
  }
  return result;
}

/**-------------------------------------------------------
 *  This method gets called when a text token has been 
 *  encountered in the parse process. After verifying that
 *  the topmost container can contain text, we call AddLeaf
 *  to store this token in the top container.
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 *------------------------------------------------------*/
PRBool nsHTMLParser::HandleTextToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,"token cannot be null!");
  PRBool result=PR_TRUE;
  nsCParserNode aNode((CHTMLToken*)aToken);
  result=AddLeaf(aNode);
  return result;
}

/**-------------------------------------------------------
 *  This method gets called when a skippedcontent token has 
 *  been encountered in the parse process. After verifying 
 *  that the topmost container can contain text, we call 
 *  AddLeaf to store this token in the top container.
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 *------------------------------------------------------*/
PRBool nsHTMLParser::HandleSkippedContentToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,"token cannot be null!");
  PRBool result=PR_TRUE;

  if(IsWithinBody()) {
    nsCParserNode aNode((CHTMLToken*)aToken);
    result=AddLeaf(aNode);
  }
  return result;
}

/**-------------------------------------------------------
 *  This method gets called when an attribute token has been 
 *  encountered in the parse process. This is an error, since
 *  all attributes should have been accounted for in the prior
 *  start or end tokens
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 *------------------------------------------------------*/
PRBool nsHTMLParser::HandleAttributeToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,"token cannot be null!");
  NS_ERROR("attribute encountered -- this shouldn't happen!");

  CAttributeToken*  at = (CAttributeToken*)(aToken);
  PRBool result=PR_TRUE;
  return result;
}

/**-------------------------------------------------------
 *  This method gets called when a script token has been 
 *  encountered in the parse process. 
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 *------------------------------------------------------*/
PRBool nsHTMLParser::HandleScriptToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,"token cannot be null!");

  CScriptToken*  st = (CScriptToken*)(aToken);
  PRBool result=PR_TRUE;
  return result;
}

/**-------------------------------------------------------
 *  This method gets called when a style token has been 
 *  encountered in the parse process. 
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 *------------------------------------------------------*/
PRBool nsHTMLParser::HandleStyleToken(CToken* aToken){
  NS_PRECONDITION(0!=aToken,"token cannot be null!");

  CStyleToken*  st = (CStyleToken*)(aToken);
  PRBool result=PR_TRUE;
  return result;
}

/**-------------------------------------------------------
 *  This method gets called to determine whether a given 
 *  tag is itself a container
 *  
 *  @update  gess 3/25/98
 *  @param   aTag -- tag to test for containership
 *  @return  PR_TRUE if given tag can contain other tags
 *------------------------------------------------------*/
PRBool nsHTMLParser::IsWithinBody(void) const {
  for(int i=0;i<mTagStackPos;i++) {
    if(eHTMLTag_body==mTagStack[i])
      return PR_TRUE;
  }
  return PR_FALSE;
}

/*-------------------------------------------------------
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 *
 * @update	gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 *------------------------------------------------------*/
PRBool nsHTMLParser::OpenHTML(const nsIParserNode& aNode){
  NS_PRECONDITION(mTagStackPos >= 0, "invalid tag stack pos");
  PRBool result=mSink->OpenHTML(aNode); 
  mTagStack[mTagStackPos++]=(eHTMLTags)aNode.GetNodeType();
  return result;
}

/*-------------------------------------------------------
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 *
 * @update	gess4/6/98
 * @param   aNode -- next node to be removed from our model
 * @return  TRUE if ok, FALSE if error
 *------------------------------------------------------*/
PRBool nsHTMLParser::CloseHTML(const nsIParserNode& aNode){
  NS_PRECONDITION(mTagStackPos > 0, "invalid tag stack pos");
  PRBool result=mSink->CloseHTML(aNode); 
  mTagStack[--mTagStackPos]=eHTMLTag_unknown;
  return result;
}


/*-------------------------------------------------------
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update	gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 *------------------------------------------------------*/
PRBool nsHTMLParser::OpenHead(const nsIParserNode& aNode){
//  NS_PRECONDITION(mTagStackPos >= 0, "invalid tag stack pos");
  PRBool result=mSink->OpenHead(aNode); 
//  mTagStack[mTagStackPos++]=(eHTMLTags)aNode.GetNodeType();
  return result;
}

/*-------------------------------------------------------
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update	gess4/6/98
 * @param   aNode -- next node to be removed from our model
 * @return  TRUE if ok, FALSE if error
 *------------------------------------------------------*/
PRBool nsHTMLParser::CloseHead(const nsIParserNode& aNode){
//  NS_PRECONDITION(mTagStackPos > 0, "invalid tag stack pos");
  PRBool result=mSink->CloseHead(aNode); 
//  mTagStack[--mTagStackPos]=eHTMLTag_unknown;
  return result;
}

/*-------------------------------------------------------
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update	gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 *------------------------------------------------------*/
PRBool nsHTMLParser::OpenBody(const nsIParserNode& aNode){
  NS_PRECONDITION(mTagStackPos >= 0, "invalid tag stack pos");

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
    mTagStack[mTagStackPos++]=(eHTMLTags)aNode.GetNodeType();
  }
  return result;
}

/*-------------------------------------------------------
 * This method does two things: 1st, help close
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update	gess4/6/98
 * @param   aNode -- next node to be removed from our model
 * @return  TRUE if ok, FALSE if error
 *------------------------------------------------------*/
PRBool nsHTMLParser::CloseBody(const nsIParserNode& aNode){
  NS_PRECONDITION(mTagStackPos >= 0, "invalid tag stack pos");
  PRBool result=mSink->CloseBody(aNode); 
  mTagStack[--mTagStackPos]=eHTMLTag_unknown;
  return result;
}

/*-------------------------------------------------------
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update	gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 *------------------------------------------------------*/
PRBool nsHTMLParser::OpenForm(const nsIParserNode& aNode){
  NS_PRECONDITION(mTagStackPos >= 0, "invalid tag stack pos");
  PRBool result=mSink->OpenForm(aNode); 
  mTagStack[mTagStackPos++]=(eHTMLTags)aNode.GetNodeType();
  return result;
}

/*-------------------------------------------------------
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update	gess4/6/98
 * @param   aNode -- next node to be removed from our model
 * @return  TRUE if ok, FALSE if error
 *------------------------------------------------------*/
PRBool nsHTMLParser::CloseForm(const nsIParserNode& aNode){
  NS_PRECONDITION(mTagStackPos > 0, "invalid tag stack pos");
  PRBool result=mSink->CloseContainer(aNode); 
  mTagStack[--mTagStackPos]=eHTMLTag_unknown;
  return result;
}

/*-------------------------------------------------------
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update	gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 *------------------------------------------------------*/
PRBool nsHTMLParser::OpenFrameset(const nsIParserNode& aNode){
  NS_PRECONDITION(mTagStackPos >= 0, "invalid tag stack pos");
  PRBool result=mSink->OpenFrameset(aNode); 
  mTagStack[mTagStackPos++]=(eHTMLTags)aNode.GetNodeType();
  return result;
}

/*-------------------------------------------------------
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update	gess4/6/98
 * @param   aNode -- next node to be removed from our model
 * @return  TRUE if ok, FALSE if error
 *------------------------------------------------------*/
PRBool nsHTMLParser::CloseFrameset(const nsIParserNode& aNode){
  NS_PRECONDITION(mTagStackPos > 0, "invalid tag stack pos");
  PRBool result=mSink->CloseFrameset(aNode); 
  mTagStack[--mTagStackPos]=eHTMLTag_unknown;
  return result;
}

/*-------------------------------------------------------
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update	gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 *------------------------------------------------------*/
PRBool nsHTMLParser::OpenContainer(const nsIParserNode& aNode){
  NS_PRECONDITION(mTagStackPos >= 0, "invalid tag stack pos");
  PRBool result=mSink->OpenContainer(aNode); 
  mTagStack[mTagStackPos++]=(eHTMLTags)aNode.GetNodeType();
  return result;
}

/*-------------------------------------------------------
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update	gess4/6/98
 * @param   aNode -- next node to be removed from our model
 * @return  TRUE if ok, FALSE if error
 *------------------------------------------------------*/
PRBool nsHTMLParser::CloseContainer(const nsIParserNode& aNode){
  NS_PRECONDITION(mTagStackPos > 0, "invalid tag stack pos");
  PRBool result=PR_FALSE;
  
  //XXX Hack! We know this is wrong, but it works
  //for the general case until we get it right.
  switch(aNode.GetNodeType()) {

    case eHTMLTag_html:
      result=CloseHTML(aNode); break;

    case eHTMLTag_style:
      break;
    case eHTMLTag_head:
      result=CloseHead(aNode); break;

    case eHTMLTag_body:
      result=CloseBody(aNode); break;

    case eHTMLTag_form:
      result=CloseForm(aNode); break;

    case eHTMLTag_title:
    default:
      result=mSink->CloseContainer(aNode); 
      mTagStack[--mTagStackPos]=eHTMLTag_unknown;
      break;
  }
  return result;
}

/*-------------------------------------------------------
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update	gess4/6/98
 * @param   
 * @return  TRUE if ok, FALSE if error
 *------------------------------------------------------*/
PRBool nsHTMLParser::CloseContainersTo(PRInt32 anIndex){
  NS_PRECONDITION(mTagStackPos >= 0, "invalid tag stack pos");
  PRBool result=PR_TRUE;

  nsAutoString empty;
  CHTMLToken aToken(empty);
  nsCParserNode theNode(&aToken);

  if((anIndex<mTagStackPos) && (anIndex>=0)) {
    while(mTagStackPos>anIndex) {
      aToken.SetHTMLTag(mTagStack[mTagStackPos-1]);
      result=CloseContainer(theNode);
    }
  }
  return result;
}

/*-------------------------------------------------------
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update	gess4/6/98
 * @param   
 * @return  TRUE if ok, FALSE if error
 *------------------------------------------------------*/
PRBool nsHTMLParser::CloseContainersTo(eHTMLTags aTag){
  NS_PRECONDITION(mTagStackPos > 0, "invalid tag stack pos");

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

/*-------------------------------------------------------
 * This method causes the topmost container on the stack
 * to be closed. 
 * @update	gess4/6/98
 * @see     CloseContainer()
 * @param   
 * @return  TRUE if ok, FALSE if error
 *------------------------------------------------------*/
PRBool nsHTMLParser::CloseTopmostContainer(){
  NS_PRECONDITION(mTagStackPos > 0, "invalid tag stack pos");

  nsAutoString empty;
  CEndToken aToken(empty);
  aToken.SetHTMLTag(mTagStack[mTagStackPos-1]);
  nsCParserNode theNode(&aToken);

  return CloseContainer(theNode);
}

/*-------------------------------------------------------
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update	gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 *------------------------------------------------------*/
PRBool nsHTMLParser::AddLeaf(const nsIParserNode& aNode){
  PRBool result=mSink->AddLeaf(aNode); 
  return result;
}

/** -------------------------------------------------------
 *  This method gets called to create a valid context stack
 *  for the given child. We compare the current stack to the
 *  default needs of the child, and push new guys onto the
 *  stack until the child can be properly placed.
 *
 *  @update  gess 4/8/98
 *  @param   
 *  @return  
 */ //----------------------------------------------------
PRBool nsHTMLParser::CreateContextStackFor(PRInt32 aChildTag){
  PRBool    result=PR_TRUE;
  eHTMLTags tags[50];
  eHTMLTags theParentTag;
  PRInt32   tagCount=0;

  nsCRT::zero(tags,sizeof(tags));

    //create the necessary stack of parent tags...
  theParentTag=(eHTMLTags)mDTD->GetDefaultParentTagFor(aChildTag);
  while(theParentTag!=kNotFound) {
    tags[tagCount++]=theParentTag;  
    theParentTag=(eHTMLTags)mDTD->GetDefaultParentTagFor(theParentTag);
  }

    //now, compare requested stack against existing stack...
  PRInt32 pos=0;
  while(pos<mTagStackPos) {
    if(mTagStack[pos]==tags[tagCount-1-pos]) {
      pos++;
    }
    else {
      //if you're here, you have something on the stack
      //that doesn't match your needed tags order.
      result=CloseContainersTo(pos);
      break;
    }
  }

    //now, build up the stack according to the tags 
    //you have that aren't in the stack...
  nsAutoString  empty;
  for(int i=pos;i<tagCount;i++) {
    CStartToken* st=new CStartToken(empty);
    st->SetHTMLTag(tags[tagCount-1-i]);
    HandleStartToken(st);
  }
  return result;
}


/** ------------------------------------------------------
 *  This method gets called to ensure that the context
 *  stack is properly set up for the given child. 
 *  We pop containers off the stack (all the way down 
 *  html) until we get a container that can contain
 *  the given child.
 *  
 *  @update  gess 4/8/98
 *  @param   
 *  @return  
 */ //-----------------------------------------------------
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


