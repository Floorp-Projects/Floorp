/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#include "nsIHTMLContentSink.h"
#include "nsHTMLTokens.h"
#include "nsIParser.h"
#include "prtypes.h" 
#include <iostream.h>  

#define VERBOSE_DEBUG

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kIContentSinkIID, NS_ICONTENT_SINK_IID);
static NS_DEFINE_IID(kIHTMLContentSinkIID, NS_IHTML_CONTENT_SINK_IID);

class nsHTMLNullSink : public nsIHTMLContentSink {
public:
  nsHTMLNullSink();
  virtual ~nsHTMLNullSink();

  enum eSection {eNone=0,eHTML,eHead,eBody,eContainer};

  // nsISupports
  NS_DECL_ISUPPORTS
 
  // nsIContentSink
  NS_IMETHOD WillBuildModel(void);
  NS_IMETHOD DidBuildModel(PRInt32 aQualityLevel);
  NS_IMETHOD WillInterrupt(void);
  NS_IMETHOD WillResume(void);
  NS_IMETHOD SetParser(nsIParser* aParser);
  NS_IMETHOD OpenContainer(const nsIParserNode& aNode);
  NS_IMETHOD CloseContainer(const nsIParserNode& aNode);
  NS_IMETHOD AddLeaf(const nsIParserNode& aNode);
  NS_IMETHOD NotifyError(const nsParserError* aError);

  NS_IMETHOD AddProcessingInstruction(const nsIParserNode& aNode);
  NS_IMETHOD AddComment(const nsIParserNode& aNode);
  
  // nsIHTMLContentSink
  NS_IMETHOD SetTitle(const nsString& aValue);
  NS_IMETHOD OpenHTML(const nsIParserNode& aNode);
  NS_IMETHOD CloseHTML(const nsIParserNode& aNode);
  NS_IMETHOD OpenHead(const nsIParserNode& aNode);
  NS_IMETHOD CloseHead(const nsIParserNode& aNode);
  NS_IMETHOD OpenBody(const nsIParserNode& aNode); 
  NS_IMETHOD CloseBody(const nsIParserNode& aNode);
  NS_IMETHOD OpenForm(const nsIParserNode& aNode);
  NS_IMETHOD CloseForm(const nsIParserNode& aNode);
  NS_IMETHOD OpenMap(const nsIParserNode& aNode);
  NS_IMETHOD CloseMap(const nsIParserNode& aNode);
  NS_IMETHOD OpenFrameset(const nsIParserNode& aNode);
  NS_IMETHOD CloseFrameset(const nsIParserNode& aNode);

  NS_IMETHOD DoFragment(PRBool aFlag);

protected:
  PRInt32     mNodeStack[100];
  PRInt32     mNodeStackPos;
  nsString    mTitle;
};


/**
 *  "Fakey" factory method used to create an instance of
 *  this class.
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
nsresult
NS_NewHTMLNullSink(nsIContentSink** aInstancePtrResult)
{
  nsHTMLNullSink *it = new nsHTMLNullSink();
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIContentSinkIID, (void **) aInstancePtrResult);
}


/**
 *  Default constructor
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
nsHTMLNullSink::nsHTMLNullSink() : nsIHTMLContentSink(), mTitle("") {
  mNodeStackPos=0;
  memset(mNodeStack,0,sizeof(mNodeStack));
}


/**
 *  Default destructor. Probably not a good idea to call 
 *  this if you created your instance via the factor method.
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
nsHTMLNullSink::~nsHTMLNullSink() {
}

#ifdef VERBOSE_DEBUG
static void DebugDump(const char* str1,const nsString& str2,PRInt32 tabs) {
  for(PRInt32 i=0;i<tabs;i++)
    cout << " "; //add some tabbing to debug output...
  char* cp = str2.ToNewCString();
  cout << str1 << cp << ">" << endl;
  delete[] cp;
}
#endif


/**
 *  This bit of magic creates the addref and release 
 *  methods for this class.
 *
 *  @updated gess 3/25/98
 *  @param  
 *  @return 
 */
NS_IMPL_ADDREF(nsHTMLNullSink)
NS_IMPL_RELEASE(nsHTMLNullSink)



/**
 *  Standard XPCOM query interface implementation. I used
 *  my own version because this class is a subclass of both
 *  ISupports and IContentSink. Perhaps there's a macro for
 *  this, but I didn't see it.
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
nsresult
nsHTMLNullSink::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  if(aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (nsIContentSink*)(this);
  }
  else if(aIID.Equals(kIContentSinkIID)) {
    *aInstancePtr = (nsIContentSink*)(this);
  }
  else if(aIID.Equals(kIHTMLContentSinkIID)) {
    *aInstancePtr = (nsIHTMLContentSink*)(this);
  }
  else {
    *aInstancePtr=0;
    return NS_NOINTERFACE;
  }
  NS_ADDREF_THIS();
  return NS_OK;                                                        
}

/**
 *  This method gets called by the parser when a <HTML> 
 *  tag has been consumed.
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
NS_IMETHODIMP
nsHTMLNullSink::OpenHTML(const nsIParserNode& aNode){
  mNodeStack[mNodeStackPos++]=(eHTMLTags)aNode.GetNodeType();

#ifdef VERBOSE_DEBUG
  DebugDump("<",aNode.GetText(),(mNodeStackPos-1)*2);
#endif
  return NS_OK;
}

/**
 *  This method gets called by the parser when a </HTML> 
 *  tag has been consumed.
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
NS_IMETHODIMP
nsHTMLNullSink::CloseHTML(const nsIParserNode& aNode){

  NS_PRECONDITION(mNodeStackPos > 0, "node stack empty");

  mNodeStack[--mNodeStackPos]=eHTMLTag_unknown;

#ifdef VERBOSE_DEBUG
  DebugDump("</",aNode.GetText(),(mNodeStackPos-1)*2);
#endif

  return NS_OK;
}

/**
 *  This method gets called by the parser <i>any time</i>
 *  head data gets consumed by the parser. Currently, that
 *  list includes <META>, <ISINDEX>, <LINK>, <SCRIPT>,
 *  <STYLE>, <TITLE>.
 *
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
NS_IMETHODIMP
nsHTMLNullSink::OpenHead(const nsIParserNode& aNode) {
  mNodeStack[mNodeStackPos++]=(eHTMLTags)aNode.GetNodeType();

#ifdef VERBOSE_DEBUG
  DebugDump("<",aNode.GetText(),(mNodeStackPos-1)*2);
#endif

  return NS_OK;
}

/**
 *  This method gets called by the parser when a </HEAD>
 *  tag has been seen (either implicitly or explicitly).
 *
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
NS_IMETHODIMP
nsHTMLNullSink::CloseHead(const nsIParserNode& aNode) {
  NS_PRECONDITION(mNodeStackPos > 0, "node stack empty");

  mNodeStack[--mNodeStackPos]=eHTMLTag_unknown;

#ifdef VERBOSE_DEBUG
  DebugDump("</",aNode.GetText(),(mNodeStackPos)*2);
#endif

  return NS_OK;
}

/**
 *  This gets called by the parser when a <TITLE> tag 
 *  gets consumed.
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
NS_IMETHODIMP
nsHTMLNullSink::SetTitle(const nsString& aValue){
  mTitle=aValue;
  return NS_OK;
}

/**
 *  This method gets called by the parser when a <BODY> 
 *  tag has been consumed.
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
NS_IMETHODIMP
nsHTMLNullSink::OpenBody(const nsIParserNode& aNode) {
  mNodeStack[mNodeStackPos++]=(eHTMLTags)aNode.GetNodeType();

#ifdef VERBOSE_DEBUG
  DebugDump("<",aNode.GetText(),(mNodeStackPos-1)*2);
#endif

  return NS_OK;
}

/**
 *  This method gets called by the parser when a </BODY> 
 *  tag has been consumed.
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
NS_IMETHODIMP
nsHTMLNullSink::CloseBody(const nsIParserNode& aNode){
  NS_PRECONDITION(mNodeStackPos > 0, "node stack empty");
  mNodeStack[--mNodeStackPos]=eHTMLTag_unknown;

#ifdef VERBOSE_DEBUG
  DebugDump("</",aNode.GetText(),(mNodeStackPos)*2);
#endif

  return NS_OK;
}

/**
 *  This method gets called by the parser when a <FORM> 
 *  tag has been consumed.
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
NS_IMETHODIMP
nsHTMLNullSink::OpenForm(const nsIParserNode& aNode) {
  mNodeStack[mNodeStackPos++]=(eHTMLTags)aNode.GetNodeType();

#ifdef VERBOSE_DEBUG
  DebugDump("<",aNode.GetText(),(mNodeStackPos-1)*2);
#endif

  return NS_OK;
}

/**
 *  This method gets called by the parser when a </FORM> 
 *  tag has been consumed.
 *   
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
NS_IMETHODIMP
nsHTMLNullSink::CloseForm(const nsIParserNode& aNode){
  NS_PRECONDITION(mNodeStackPos > 0, "node stack empty");

  mNodeStack[--mNodeStackPos]=eHTMLTag_unknown;

#ifdef VERBOSE_DEBUG
  DebugDump("</",aNode.GetText(),(mNodeStackPos)*2);
#endif

  return NS_OK;
}

/**
 *  This method gets called by the parser when a <FORM> 
 *  tag has been consumed.
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
NS_IMETHODIMP
nsHTMLNullSink::OpenMap(const nsIParserNode& aNode) {
  mNodeStack[mNodeStackPos++]=(eHTMLTags)aNode.GetNodeType();

#ifdef VERBOSE_DEBUG
  DebugDump("<",aNode.GetText(),(mNodeStackPos-1)*2);
#endif

  return NS_OK;
}

/**
 *  This method gets called by the parser when a </FORM> 
 *  tag has been consumed.
 *   
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
NS_IMETHODIMP
nsHTMLNullSink::CloseMap(const nsIParserNode& aNode){
  NS_PRECONDITION(mNodeStackPos > 0, "node stack empty");

  mNodeStack[--mNodeStackPos]=eHTMLTag_unknown;

#ifdef VERBOSE_DEBUG
  DebugDump("</",aNode.GetText(),(mNodeStackPos)*2);
#endif

  return NS_OK;
}

/**
 *  This method gets called by the parser when a <FRAMESET> 
 *  tag has been consumed.
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
NS_IMETHODIMP
nsHTMLNullSink::OpenFrameset(const nsIParserNode& aNode) {
  mNodeStack[mNodeStackPos++]=(eHTMLTags)aNode.GetNodeType();

#ifdef VERBOSE_DEBUG
  DebugDump("<",aNode.GetText(),(mNodeStackPos-1)*2);
#endif

  return NS_OK;
}

/**
 *  This method gets called by the parser when a </FRAMESET> 
 *  tag has been consumed.
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
NS_IMETHODIMP
nsHTMLNullSink::CloseFrameset(const nsIParserNode& aNode){
  NS_PRECONDITION(mNodeStackPos > 0, "node stack empty");

  mNodeStack[--mNodeStackPos]=eHTMLTag_unknown;

#ifdef VERBOSE_DEBUG
  DebugDump("</",aNode.GetText(),(mNodeStackPos)*2);
#endif

  return NS_OK;
}

/**
 *  This method gets called by the parser when any general
 *  type of container has been consumed and needs to be 
 *  opened. This includes things like <OL>, <Hn>, etc...
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
NS_IMETHODIMP
nsHTMLNullSink::OpenContainer(const nsIParserNode& aNode){  
  mNodeStack[mNodeStackPos++]=(eHTMLTags)aNode.GetNodeType();

#ifdef VERBOSE_DEBUG
  DebugDump("<",aNode.GetText(),(mNodeStackPos-1)*2);
#endif

  return NS_OK;
}

/**
 *  This method gets called by the parser when a close
 *  container tag has been consumed and needs to be closed.
 *  
 *  @updated gess 3/25/98
 *  @param  
 *  @return 
 */
NS_IMETHODIMP
nsHTMLNullSink::CloseContainer(const nsIParserNode& aNode){
  NS_PRECONDITION(mNodeStackPos > 0, "node stack empty");

  mNodeStack[--mNodeStackPos]=eHTMLTag_unknown;

#ifdef VERBOSE_DEBUG
  DebugDump("</",aNode.GetText(),(mNodeStackPos)*2);
#endif

  return NS_OK;
}

/**
 *  This gets called by the parser when you want to add
 *  a leaf node to the current container in the content
 *  model.
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
NS_IMETHODIMP
nsHTMLNullSink::AddLeaf(const nsIParserNode& aNode){

#ifdef VERBOSE_DEBUG
  DebugDump("<",aNode.GetText(),(mNodeStackPos)*2);
#endif

  return NS_OK;
}

/**
 *  This gets called by the parser when you want to add
 *  a PI node to the current container in the content
 *  model.
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
NS_IMETHODIMP
nsHTMLNullSink::AddProcessingInstruction(const nsIParserNode& aNode){

#ifdef VERBOSE_DEBUG
  DebugDump("<",aNode.GetText(),(mNodeStackPos)*2);
#endif

  return NS_OK;
}

/**
 *  This gets called by the parser when you want to add
 *  a comment node to the current container in the content
 *  model.
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
NS_IMETHODIMP
nsHTMLNullSink::AddComment(const nsIParserNode& aNode){

#ifdef VERBOSE_DEBUG
  DebugDump("<",aNode.GetText(),(mNodeStackPos)*2);
#endif

  return NS_OK;
}

 /**
  * This method gets called when the parser begins the process
  * of building the content model via the content sink.
  *
  * @update 5/7/98 gess
 */     
NS_IMETHODIMP
nsHTMLNullSink::WillBuildModel(void){
  return NS_OK;
}

 /**
  * This method gets called when the parser concludes the process
  * of building the content model via the content sink.
  *
  * @update 5/7/98 gess
  */     
NS_IMETHODIMP
nsHTMLNullSink::DidBuildModel(PRInt32 aQualityLevel){
  return NS_OK;
}

/**
 * This method gets called when the parser gets i/o blocked,
 * and wants to notify the sink that it may be a while before
 * more data is available.
 *
 * @update 5/7/98 gess
 */     
NS_IMETHODIMP
nsHTMLNullSink::WillInterrupt(void) {
  return NS_OK;
}

/**
 * This method gets called when the parser i/o gets unblocked,
 * and we're about to start dumping content again to the sink.
 *
 * @update 5/7/98 gess
 */     
NS_IMETHODIMP
nsHTMLNullSink::WillResume(void) {
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLNullSink::SetParser(nsIParser* aParser) 
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLNullSink::NotifyError(const nsParserError* aError)
{
  return NS_OK;
}

/**
 * 
 * @update	gpk03/14/99
 * @param 
 * @return
 */
nsresult nsHTMLNullSink::DoFragment(PRBool aFlag) 
{
  return NS_OK;
}


