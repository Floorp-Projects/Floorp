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

#include "CNavDTD.h"
#include "nsHTMLTokens.h"
#include "nsCRT.h"
#include "nsParserTypes.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kIDTDIID,      NS_IDTD_IID);
static NS_DEFINE_IID(kHTMLDTDIID,   NS_IHTML_DTD_IID);
static NS_DEFINE_IID(kClassIID,     NS_INAVHTML_DTD_IID); 


/**-------------------------------------------------------
 *  This method gets called as part of our COM-like interfaces.
 *  Its purpose is to create an interface to parser object
 *  of some type.
 *  
 *  @update   gess 4/8/98
 *  @param    nsIID  id of object to discover
 *  @param    aInstancePtr ptr to newly discovered interface
 *  @return   NS_xxx result code
 *------------------------------------------------------*/
nsresult CNavDTD::QueryInterface(const nsIID& aIID, void** aInstancePtr)  
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
    *aInstancePtr = (CNavDTD*)(this);                                        
  }                 
  else {
    *aInstancePtr=0;
    return NS_NOINTERFACE;
  }
  ((nsISupports*) *aInstancePtr)->AddRef();
  return NS_OK;                                                        
}

/**-------------------------------------------------------
 *  This method is defined in nsIParser. It is used to 
 *  cause the COM-like construction of an nsHTMLParser.
 *  
 *  @update  gess 4/8/98
 *  @param   nsIParser** ptr to newly instantiated parser
 *  @return  NS_xxx error result
 *------------------------------------------------------*/
NS_HTMLPARS nsresult NS_NewNavHTMLDTD(nsIDTD** aInstancePtrResult)
{
  CNavDTD* it = new CNavDTD();

  if (it == 0) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kClassIID, (void **) aInstancePtrResult);
}


NS_IMPL_ADDREF(CNavDTD)
NS_IMPL_RELEASE(CNavDTD)


/**-------------------------------------------------------
 *  Default constructor
 *  
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
CNavDTD::CNavDTD() : nsHTMLDTD() {
}

/**-------------------------------------------------------
 *  Default destructor
 *  
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
CNavDTD::~CNavDTD(){
}

/** ------------------------------------------------------
 *  This method is called to determine whether or not a tag
 *  of one type can contain a tag of another type.
 *  
 *  @update  gess 4/8/98
 *  @param   aParent -- tag enum of parent container
 *  @param   aChild -- tag enum of child container
 *  @return  PR_TRUE if parent can contain child
 */ //----------------------------------------------------
PRBool CNavDTD::CanContain(PRInt32 aParent,PRInt32 aChild) const {
  PRBool result=nsHTMLDTD::CanContain(aParent,aChild);
  return result;
}


/** ------------------------------------------------------
 *  This method is called to determine whether or not a tag
 *  of one type can contain a tag of another type.
 *  
 *  @update  gess 4/8/98
 *  @param   aParent -- tag enum of parent container
 *  @param   aChild -- tag enum of child container
 *  @return  PR_TRUE if parent can contain child
 */ //----------------------------------------------------
PRBool CNavDTD::CanContainIndirect(PRInt32 aParent,PRInt32 aChild) const {
  PRBool result=nsHTMLDTD::CanContainIndirect(aParent,aChild);
  return result;
}

/** -------------------------------------------------------
 *  This method gets called to determine whether a given 
 *  tag can contain newlines. Most do not.
 *  
 *  @update  gess 3/25/98
 *  @param   aTag -- tag to test for containership
 *  @return  PR_TRUE if given tag can contain other tags
 */ //----------------------------------------------------
PRBool CNavDTD::CanOmit(PRInt32 aParent,PRInt32 aChild) const {
  PRBool result=PR_FALSE;

  switch((eHTMLTags)aParent) {
    case eHTMLTag_tr:
    case eHTMLTag_table:
    case eHTMLTag_thead:
    case eHTMLTag_tfoot:
    case eHTMLTag_tbody:
    case eHTMLTag_col:
    case eHTMLTag_colgroup:
      if((aChild==eHTMLTag_newline) ||
         (aChild==eHTMLTag_whitespace))
        result=PR_TRUE;
      break;

    default:
      result=PR_FALSE;
      break;
  }
  return result;
}

/** -------------------------------------------------------
 *  This method gets called to determine whether a given 
 *  tag is itself a container
 *  
 *  @update  gess 4/8/98
 *  @param   aTag -- tag to test for containership
 *  @return  PR_TRUE if given tag can contain other tags
 */ //----------------------------------------------------
PRBool CNavDTD::IsContainer(PRInt32 aTag) const {
  PRBool result=nsHTMLDTD::IsContainer(aTag);
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
PRInt32 CNavDTD::GetDefaultParentTagFor(PRInt32 aTag) const{
  eHTMLTags result=eHTMLTag_unknown;
  switch(aTag) {
    case eHTMLTag_html:
      result=(eHTMLTags)kNotFound; break;

    case eHTMLTag_body:
    case eHTMLTag_head:
    case eHTMLTag_header:
    case eHTMLTag_footer:
    case eHTMLTag_frameset:
      result=eHTMLTag_html; break;

      //These tags are head specific...
    case eHTMLTag_style:
    case eHTMLTag_meta:
    case eHTMLTag_title:
    case eHTMLTag_base:
    case eHTMLTag_link:
      result=eHTMLTag_head; break;

      //These tags are table specific...
    case eHTMLTag_caption:
    case eHTMLTag_colgroup:
    case eHTMLTag_tbody:
    case eHTMLTag_tfoot:
    case eHTMLTag_thead:
    case eHTMLTag_tr:
      result=eHTMLTag_table; break;

    case eHTMLTag_td:
    case eHTMLTag_th:
      result=eHTMLTag_tr; break;

    case eHTMLTag_col:
      result=eHTMLTag_colgroup; break;    

      //These have to do with listings...
    case eHTMLTag_listitem:
      result=eHTMLTag_ul; break;    

    case eHTMLTag_dd:
    case eHTMLTag_dt:
      result=eHTMLTag_dl; break;    

    case eHTMLTag_option:
      result=eHTMLTag_select; break;    

      //These have to do with image maps...
    case eHTMLTag_area:
      result=eHTMLTag_map; break;    

      //These have to do with applets...
    case eHTMLTag_param:
      result=eHTMLTag_applet; break;    

      //These have to do with frames...
    case eHTMLTag_frame:
      result=eHTMLTag_frameset; break;    

    default:
      result=eHTMLTag_body; //XXX Hack! Just for now.
      break;
  }
  return result;
}


/** ------------------------------------------------------
 * This method gets called at various times by the parser
 * whenever we want to verify a valid context stack. This
 * method also gives us a hook to add debugging metrics.
 *
 * @update	gess4/6/98
 * @param   aStack[] array of ints (tokens)
 * @param   aCount number of elements in given array
 * @return  TRUE if stack is valid, else FALSE
 */ //-----------------------------------------------------
PRBool CNavDTD::VerifyContextStack(eHTMLTags aStack[],PRInt32 aCount) const {
  PRBool result=PR_TRUE;

  if(aCount>0) {

  }
  return result;
}


/** -------------------------------------------------------
 * This method tries to design a context map (without actually
 * changing our parser state) from the parent down to the
 * child. 
 *
 * @update	gess4/6/98
 * @param   aParent -- tag type of parent
 * @param   aChild -- tag type of child
 * @return  Non zero count of intermediate nodes; 
 *          0 if unable to comply
 */ //----------------------------------------------------
PRInt32 CNavDTD::CreateContextMapBetween(PRInt32 aParent,PRInt32 aChild) const {
  PRInt32 result=0;
  return result;
}

