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

/**
 * MODULE NOTES:
 * @update  gess 4/8/98
 *  
 *         
 */

/**
 * TRANSIENT STYLE-HANDLING NOTES:
 * @update  gess 6/15/98
 * 
 * See notes about transient style handling
 * in the nsNavDTD.h file.
 *         
 */

#include "COtherDTD.h"
#include "nsHTMLTokens.h"
#include "nsCRT.h"
#include "nsParser.h"
#include "nsIHTMLContentSink.h" 
#include "nsScanner.h"
#include "nsIParser.h"

#include "prenv.h"  //this is here for debug reasons...
#include "prtypes.h"
#include "prio.h"
#include "plstr.h"
#include <fstream.h>


#ifdef XP_PC
#include <direct.h> //this is here for debug reasons...
#endif
#include <time.h> 
#include "prmem.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kIDTDIID,      NS_IDTD_IID);
static NS_DEFINE_IID(kClassIID,     NS_IOtherHTML_DTD_IID); 
static NS_DEFINE_IID(kBaseClassIID, NS_INAVHTML_DTD_IID); 



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
  else if(aIID.Equals(kBaseClassIID)) {  //do nav dtd base class...
    *aInstancePtr = (CNavDTD*)(this);                                        
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

/**
 *  This method is defined in nsIParser. It is used to 
 *  cause the COM-like construction of an nsParser.
 *  
 *  @update  gess 4/8/98
 *  @param   nsIParser** ptr to newly instantiated parser
 *  @return  NS_xxx error result
 */
NS_HTMLPARS nsresult NS_NewOtherHTMLDTD(nsIDTD** aInstancePtrResult)
{
  COtherDTD* it = new COtherDTD();

  if (it == 0) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kClassIID, (void **) aInstancePtrResult);
}


NS_IMPL_ADDREF(COtherDTD)
NS_IMPL_RELEASE(COtherDTD)

/**
 *  Default constructor; parent does all the real work.
 *  
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
COtherDTD::COtherDTD() : CNavDTD() {
}

/**
 *  Default destructor
 *  
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
COtherDTD::~COtherDTD(){
  //parent does all the real work of destruction.
}

/**
 * 
 * @update	gess1/8/99
 * @param 
 * @return
 */
const nsIID& COtherDTD::GetMostDerivedIID(void) const{
  return kClassIID;
}

/**
 * Call this method if you want the DTD to construct a fresh instance
 * of itself
 * @update	gess7/23/98
 * @param 
 * @return
 */
nsresult COtherDTD::CreateNewInstance(nsIDTD** aInstancePtrResult){
  return NS_NewOtherHTMLDTD(aInstancePtrResult);
}

/**
 * This method is called to determine if the given DTD can parse
 * a document in a given source-type. 
 * NOTE: Parsing always assumes that the end result will involve
 *       storing the result in the main content model.
 * @update	gess6/24/98
 * @param   
 * @return  TRUE if this DTD can satisfy the request; FALSE otherwise.
 */
eAutoDetectResult COtherDTD::CanParse(nsString& aContentType, nsString& aCommand, nsString& aBuffer, PRInt32 aVersion) {
  return CNavDTD::CanParse(aContentType,aCommand,aBuffer,aVersion);
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
nsresult COtherDTD::HandleDefaultStartToken(CToken* aToken,eHTMLTags aChildTag,nsIParserNode& aNode) {
  return CNavDTD::HandleDefaultStartToken(aToken,aChildTag,aNode);
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
nsresult COtherDTD::HandleStartToken(CToken* aToken) {
  return CNavDTD::HandleStartToken(aToken);
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
  return CNavDTD::HandleEndToken(aToken);
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
  return CNavDTD::HandleEntityToken(aToken);
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
nsresult COtherDTD::HandleCommentToken(CToken* aToken) {
  return CNavDTD::HandleCommentToken(aToken);
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
nsresult COtherDTD::HandleSkippedContentToken(CToken* aToken) {
  return CNavDTD::HandleSkippedContentToken(aToken);
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
nsresult COtherDTD::HandleAttributeToken(CToken* aToken) {
  return CNavDTD::HandleAttributeToken(aToken);
}

/**
 *  This method gets called when a script token has been 
 *  encountered in the parse process. 
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
nsresult COtherDTD::HandleScriptToken(nsCParserNode& aNode) {
  return CNavDTD::HandleScriptToken(aNode);
}

/**
 *  This method gets called when a style token has been 
 *  encountered in the parse process. 
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
nsresult COtherDTD::HandleStyleToken(CToken* aToken){
  return CNavDTD::HandleStyleToken(aToken);
}
 

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
  return CNavDTD::CanContain(aParent,aChild);
}


/**
 *  This method gets called to determine whether a given 
 *  tag can contain newlines. Most do not.
 *  
 *  @update  gess 3/25/98
 *  @param   aTag -- tag to test for containership
 *  @return  PR_TRUE if given tag can contain other tags
 */
PRBool COtherDTD::CanOmit(eHTMLTags aParent,eHTMLTags aChild) const {
  return CNavDTD::CanOmit(aParent,aChild);
}


/**
 *  This method gets called to determine whether a given
 *  ENDtag can be omitted. Admittedly,this is a gross simplification.
 *  
 *  @update  gess 3/25/98
 *  @param   aTag -- tag to test for containership
 *  @return  PR_TRUE if given tag can contain other tags
 */
PRBool COtherDTD::CanOmitEndTag(eHTMLTags aParent,eHTMLTags aChild) const {
  return CNavDTD::CanOmitEndTag(aParent,aChild);
}


/*********************************************
  Here comes code that handles the interface
  to our content sink.
 *********************************************/


/**
 * It is with great trepidation that I offer this method (privately of course).
 * The gets called whenever a container gets opened. This methods job is to 
 * take a look at the (transient) style stack, and open any style containers that
 * are there. Of course, we shouldn't bother to open styles that are incompatible
 * with our parent container.
 *
 * @update	gess6/4/98
 * @param   tag of the container just opened
 * @return  0 (for now)
 */
nsresult COtherDTD::OpenTransientStyles(eHTMLTags aTag){
  return CNavDTD::OpenTransientStyles(aTag);
}

/**
 * It is with great trepidation that I offer this method (privately of course).
 * The gets called just prior when a container gets opened. This methods job is to 
 * take a look at the (transient) style stack, and <i>close</i> any style containers 
 * that are there. Of course, we shouldn't bother to open styles that are incompatible
 * with our parent container.
 * SEE THE TOP OF THIS FILE for more information about how the transient style stack works.
 *
 * @update	gess6/4/98
 * @param   tag of the container just opened
 * @return  0 (for now)
 */
nsresult COtherDTD::CloseTransientStyles(eHTMLTags aTag){
  return CNavDTD::CloseTransientStyles(aTag);
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
nsresult COtherDTD::OpenHTML(const nsIParserNode& aNode){
  return CNavDTD::OpenHTML(aNode);
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
nsresult COtherDTD::CloseHTML(const nsIParserNode& aNode){
  return CNavDTD::CloseHTML(aNode);
}


/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
nsresult COtherDTD::OpenHead(const nsIParserNode& aNode){
  return CNavDTD::OpenHead(aNode);
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be removed from our model
 * @return  TRUE if ok, FALSE if error
 */
nsresult COtherDTD::CloseHead(const nsIParserNode& aNode){
  return CNavDTD::CloseHead(aNode);
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
nsresult COtherDTD::OpenBody(const nsIParserNode& aNode){
  return CNavDTD::OpenBody(aNode);
}

/**
 * This method does two things: 1st, help close
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be removed from our model
 * @return  TRUE if ok, FALSE if error
 */
nsresult COtherDTD::CloseBody(const nsIParserNode& aNode){
  return CNavDTD::CloseBody(aNode);
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
nsresult COtherDTD::OpenForm(const nsIParserNode& aNode){
  return CNavDTD::OpenForm(aNode);
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be removed from our model
 * @return  TRUE if ok, FALSE if error
 */
nsresult COtherDTD::CloseForm(const nsIParserNode& aNode){
  return CNavDTD::CloseForm(aNode);
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
nsresult COtherDTD::OpenMap(const nsIParserNode& aNode){
  return CNavDTD::OpenMap(aNode);
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be removed from our model
 * @return  TRUE if ok, FALSE if error
 */
nsresult COtherDTD::CloseMap(const nsIParserNode& aNode){
  return CNavDTD::CloseMap(aNode);
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
nsresult COtherDTD::OpenFrameset(const nsIParserNode& aNode){
  return CNavDTD::OpenFrameset(aNode);
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be removed from our model
 * @return  TRUE if ok, FALSE if error
 */
nsresult COtherDTD::CloseFrameset(const nsIParserNode& aNode){
  return CNavDTD::CloseFrameset(aNode);
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
nsresult COtherDTD::OpenContainer(const nsIParserNode& aNode,PRBool aUpdateStyleStack){
  return CNavDTD::OpenContainer(aNode,aUpdateStyleStack);
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be removed from our model
 * @return  TRUE if ok, FALSE if error
 */
nsresult COtherDTD::CloseContainer(const nsIParserNode& aNode,eHTMLTags aTag,PRBool aUpdateStyles){
  return CNavDTD::CloseContainer(aNode,aTag,aUpdateStyles);
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   
 * @return  TRUE if ok, FALSE if error
 */
nsresult COtherDTD::CloseContainersTo(PRInt32 anIndex,eHTMLTags aTag,PRBool aUpdateStyles){
  return CNavDTD::CloseContainersTo(anIndex,aTag,aUpdateStyles);
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   
 * @return  TRUE if ok, FALSE if error
 */
nsresult COtherDTD::CloseContainersTo(eHTMLTags aTag,PRBool aUpdateStyles){
  return CNavDTD::CloseContainersTo(aTag,aUpdateStyles);
}

/**
 * This method causes the topmost container on the stack
 * to be closed. 
 * @update  gess4/6/98
 * @see     CloseContainer()
 * @param   
 * @return  TRUE if ok, FALSE if error
 */
nsresult COtherDTD::CloseTopmostContainer(){
  return CNavDTD::CloseTopmostContainer();
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
nsresult COtherDTD::AddLeaf(const nsIParserNode& aNode){
  return CNavDTD::AddLeaf(aNode);
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
nsresult COtherDTD::CreateContextStackFor(eHTMLTags aChildTag){
  return CNavDTD::CreateContextStackFor(aChildTag);
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
nsresult COtherDTD::ReduceContextStackFor(eHTMLTags aChildTag){
  return CNavDTD::ReduceContextStackFor(aChildTag);
}


/**
 * This method causes all explicit style-tag containers that
 * are opened to be reflected on our internal style-stack.
 *
 * @update	gess6/4/98
 * @param   aTag is the id of the html container being opened
 * @return  0 if all is well.
 */
nsresult COtherDTD::UpdateStyleStackForOpenTag(eHTMLTags aTag,eHTMLTags anActualTag){
  return CNavDTD::UpdateStyleStackForOpenTag(aTag,anActualTag);
} //update...

/**
 * This method gets called when an explicit style close-tag is encountered.
 * It results in the style tag id being popped from our internal style stack.
 *
 * @update	gess6/4/98
 * @param 
 * @return  0 if all went well (which it always does)
 */
nsresult COtherDTD::UpdateStyleStackForCloseTag(eHTMLTags aTag,eHTMLTags anActualTag){
  return CNavDTD::UpdateStyleStackForCloseTag(aTag,anActualTag);
} //update...

/**
 * 
 * @update	gess5/18/98
 * @param 
 * @return
 */
nsresult COtherDTD::WillResumeParse(void){
  return CNavDTD::WillResumeParse();
}

/**
 * 
 * @update	gess5/18/98
 * @param 
 * @return
 */
nsresult COtherDTD::WillInterruptParse(void){
  return CNavDTD::WillInterruptParse();
}


/**
 * 
 * @update	gpk03/14/99
 * @param 
 * @return
 */
nsresult COtherDTD::DoFragment(PRBool aFlag) 
{
  return CNavDTD::DoFragment(aFlag);
}


