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
#include "nsIParser.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kIDTDIID,      NS_IDTD_IID);
static NS_DEFINE_IID(kClassIID,     NS_INAVHTML_DTD_IID); 


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

/**
 *  This method is defined in nsIParser. It is used to 
 *  cause the COM-like construction of an nsHTMLParser.
 *  
 *  @update  gess 4/8/98
 *  @param   nsIParser** ptr to newly instantiated parser
 *  @return  NS_xxx error result
 */
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


/**
 *  Default constructor
 *  
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
CNavDTD::CNavDTD() : nsIDTD() {
  mParser=0;
}

/**
 *  Default destructor
 *  
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
CNavDTD::~CNavDTD(){
}

/**
 * 
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return 
 */
void CNavDTD::SetParser(nsIParser* aParser) {
  mParser=aParser;
}


static char formElementTags[]= {  
    eHTMLTag_button,  eHTMLTag_fieldset,  eHTMLTag_input,
    eHTMLTag_isindex, eHTMLTag_label,     eHTMLTag_legend,
    eHTMLTag_select,  eHTMLTag_textarea,0};

static char gHeadingTags[]={
  eHTMLTag_h1,  eHTMLTag_h2,  eHTMLTag_h3,  
  eHTMLTag_h4,  eHTMLTag_h5,  eHTMLTag_h6, 
  0};

  
/**
 *  This method is called to determine whether or not a tag
 *  of one type can contain a tag of another type.
 *  
 *  @update  gess 4/8/98
 *  @param   aParent -- tag enum of parent container
 *  @param   aChild -- tag enum of child container
 *  @return  PR_TRUE if parent can contain child
 */
PRBool CNavDTD::CanContainFormElement(PRInt32 aParent,PRInt32 aChild) const {
  PRBool result=(mParser) ? mParser->HasOpenContainer(eHTMLTag_form) : PR_FALSE;
  return result;
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
PRBool CNavDTD::CanContain(PRInt32 aParent,PRInt32 aChild) const {

  PRBool result=PR_FALSE;

    //tagset1 has 65 members...
  static char  gTagSet1[]={ 
    eHTMLTag_a,         eHTMLTag_acronym,   eHTMLTag_address,   eHTMLTag_applet,
    eHTMLTag_bold,      eHTMLTag_basefont,  eHTMLTag_bdo,       eHTMLTag_big,
    eHTMLTag_blockquote,eHTMLTag_br,        eHTMLTag_button,    eHTMLTag_center,
    eHTMLTag_cite,      eHTMLTag_code,      eHTMLTag_dfn,       eHTMLTag_dir,
    eHTMLTag_div,       eHTMLTag_dl,        eHTMLTag_em,        eHTMLTag_fieldset,
    eHTMLTag_font,      eHTMLTag_form,      eHTMLTag_h1,        eHTMLTag_h2,
    eHTMLTag_h3,        eHTMLTag_h4,        eHTMLTag_h5,        eHTMLTag_h6,
    eHTMLTag_hr,        eHTMLTag_italic,    eHTMLTag_iframe,    eHTMLTag_img,
    eHTMLTag_input,     eHTMLTag_isindex,   
    
    eHTMLTag_kbd,       eHTMLTag_label,
    eHTMLTag_map,       eHTMLTag_menu,      eHTMLTag_newline,   eHTMLTag_nobr,
    eHTMLTag_noframes,  eHTMLTag_noscript,
    eHTMLTag_object,    eHTMLTag_ol,        eHTMLTag_paragraph, eHTMLTag_pre,
    eHTMLTag_quotation, eHTMLTag_strike,    eHTMLTag_samp,      eHTMLTag_script,
    eHTMLTag_select,    eHTMLTag_small,     eHTMLTag_span,      eHTMLTag_strong,
    eHTMLTag_sub,       eHTMLTag_sup,       eHTMLTag_table,     eHTMLTag_text,
    
    eHTMLTag_textarea,  eHTMLTag_tt,        eHTMLTag_u,         eHTMLTag_ul,        
    eHTMLTag_userdefined,   eHTMLTag_var,   
    eHTMLTag_whitespace,  //JUST ADDED!
    eHTMLTag_spacer,
    0};

    //tagset2 has 44 members...
  static char  gTagSet2[]={ 
    eHTMLTag_a,         eHTMLTag_acronym,   eHTMLTag_applet,    eHTMLTag_bold,
    eHTMLTag_basefont,  eHTMLTag_bdo,       eHTMLTag_big,       eHTMLTag_br,
    eHTMLTag_button,    eHTMLTag_cite,      eHTMLTag_code,      eHTMLTag_dfn,
    eHTMLTag_em,        eHTMLTag_font,      eHTMLTag_hr,        eHTMLTag_italic,    
    eHTMLTag_iframe,    eHTMLTag_img,       eHTMLTag_input,     eHTMLTag_kbd,       

    eHTMLTag_label,     eHTMLTag_map,       eHTMLTag_newline,   eHTMLTag_nobr,
    eHTMLTag_object,    eHTMLTag_paragraph, 
    eHTMLTag_quotation, eHTMLTag_strike,    eHTMLTag_samp,      eHTMLTag_script,    
    eHTMLTag_select,    eHTMLTag_small,     eHTMLTag_span,      eHTMLTag_strong,    
    eHTMLTag_sub,       eHTMLTag_sup,       eHTMLTag_text,      eHTMLTag_textarea,  
    eHTMLTag_tt,        eHTMLTag_u,         eHTMLTag_userdefined, eHTMLTag_var,       
    eHTMLTag_whitespace,//JUST ADDED!
    eHTMLTag_spacer,
    0};

    //tagset3 has 57 members...
  static char  gTagSet3[]={ 
    eHTMLTag_a,         eHTMLTag_acronym,   eHTMLTag_applet,    eHTMLTag_bold,
    eHTMLTag_bdo,       eHTMLTag_big,       eHTMLTag_br,        eHTMLTag_blockquote,
    eHTMLTag_body,      eHTMLTag_caption,   eHTMLTag_center,    eHTMLTag_cite,
    eHTMLTag_code,      eHTMLTag_dd,        eHTMLTag_del,       eHTMLTag_dfn,        
    eHTMLTag_div,       eHTMLTag_dt,        eHTMLTag_em,        eHTMLTag_fieldset,    
    eHTMLTag_font,      eHTMLTag_form,      eHTMLTag_h1,        eHTMLTag_h2,
    eHTMLTag_h3,        eHTMLTag_h4,        eHTMLTag_h5,        eHTMLTag_h6,
    eHTMLTag_italic,    eHTMLTag_iframe,    eHTMLTag_ins,       eHTMLTag_kbd,       

    eHTMLTag_label,     eHTMLTag_legend,    
    eHTMLTag_listitem,  eHTMLTag_newline,   //JUST ADDED!
        
    eHTMLTag_noframes,
    eHTMLTag_noscript,  eHTMLTag_object,    eHTMLTag_paragraph, eHTMLTag_pre,
    eHTMLTag_quotation, eHTMLTag_strike,    eHTMLTag_samp,      eHTMLTag_small,
    eHTMLTag_span,      eHTMLTag_strong,    eHTMLTag_sub,       eHTMLTag_sup,   
    eHTMLTag_td,        eHTMLTag_text,
    
    eHTMLTag_th,        eHTMLTag_tt,        eHTMLTag_u,         eHTMLTag_userdefined,
    eHTMLTag_var,       eHTMLTag_whitespace,  //JUST ADDED!
    eHTMLTag_spacer,
    0};

    //This hack code is here because we don't yet know what to do
    //with userdefined tags...  XXX Hack
  if(eHTMLTag_userdefined==aChild)  // XXX Hack: For now...
    result=PR_TRUE;

    //handle form elements (this is very much a WIP!!!)
  if(0!=strchr(formElementTags,aChild)){
    return CanContainFormElement(aParent,aChild);
  }
  
  switch(aParent) {
    case eHTMLTag_a:
      result=PRBool(0!=strchr(gTagSet2,aChild)); break;

    case eHTMLTag_acronym:
      result=PRBool(0!=strchr(gTagSet1,aChild)); break;

    case eHTMLTag_address:
      result=PRBool(0!=strchr(gTagSet2,aChild)); break;

    case eHTMLTag_applet:
      {
        static char okTags[]={  
          eHTMLTag_a,       eHTMLTag_acronym,   eHTMLTag_applet,    eHTMLTag_bold,
          eHTMLTag_basefont,eHTMLTag_bdo,       eHTMLTag_big,       eHTMLTag_br,
          eHTMLTag_button,  eHTMLTag_cite,      eHTMLTag_code,      eHTMLTag_dfn,
          eHTMLTag_em,      eHTMLTag_font,      eHTMLTag_italic,    eHTMLTag_iframe,
          eHTMLTag_img,     eHTMLTag_input,     eHTMLTag_kbd,       eHTMLTag_label,
          eHTMLTag_map,     eHTMLTag_object,    eHTMLTag_param,     eHTMLTag_quotation,
          eHTMLTag_samp,    eHTMLTag_script,    eHTMLTag_select,    eHTMLTag_small,
          eHTMLTag_span,    eHTMLTag_strike,    eHTMLTag_strong,    eHTMLTag_sub,
          eHTMLTag_sup,     eHTMLTag_textarea,  eHTMLTag_tt,        eHTMLTag_u,
          eHTMLTag_var,0};
        result=PRBool(0!=strchr(okTags,aChild));
      }


    case eHTMLTag_area:
    case eHTMLTag_base:
    case eHTMLTag_basefont:
    case eHTMLTag_br:
      break;  //singletons can't contain other tags

    case eHTMLTag_bdo:
    case eHTMLTag_big:
    case eHTMLTag_blink:
    case eHTMLTag_bold:
      result=PRBool(0!=strchr(gTagSet2,aChild)); break;

    case eHTMLTag_blockquote:
    case eHTMLTag_body:
      if(eHTMLTag_userdefined==aChild)
        result=PR_TRUE;
      else result=PRBool(0!=strchr(gTagSet1,aChild)); 
      break;

    case eHTMLTag_button:
      result=PRBool(0!=strchr(gTagSet3,aChild)); break;

    case eHTMLTag_caption:
      result=PRBool(0!=strchr(gTagSet2,aChild)); break;

    case eHTMLTag_center:
      result=PRBool(0!=strchr(gTagSet1,aChild)); break;

    case eHTMLTag_cite:   case eHTMLTag_code:
      result=PRBool(0!=strchr(gTagSet2,aChild)); break;

    case eHTMLTag_col:
    case eHTMLTag_colgroup:
      break;    //singletons can't contain anything...

    case eHTMLTag_dd:
      result=PRBool(0!=strchr(gTagSet1,aChild)); break;

    case eHTMLTag_del:  case eHTMLTag_dfn:
      result=PRBool(0!=strchr(gTagSet2,aChild)); break;

    case eHTMLTag_div:
      result=PRBool(0!=strchr(gTagSet1,aChild)); break;

    case eHTMLTag_dl:
      {
        char okTags[]={eHTMLTag_dd,eHTMLTag_dt,0};
        result=PRBool(0!=strchr(okTags,aChild));
      }
      break;

    case eHTMLTag_dt:
      break;    //singletons can't contain anything...

    case eHTMLTag_em:
      result=PRBool(0!=strchr(gTagSet2,aChild)); break;

    case eHTMLTag_fieldset:
      if(eHTMLTag_legend==aChild)
        result=PR_TRUE;
      else result=PRBool(0!=strchr(gTagSet1,aChild)); 
      break;

    case eHTMLTag_font:
      result=PRBool(0!=strchr(gTagSet2,aChild)); break;

    case eHTMLTag_form:
      result=PRBool(0!=strchr(gTagSet1,aChild)); break;

    case eHTMLTag_frame:
      break;  //singletons can't contain other tags

    case eHTMLTag_frameset:
      {
        static char okTags[]={eHTMLTag_frame,eHTMLTag_frameset,eHTMLTag_noframes,0};
        result=PRBool(0!=strchr(okTags,aChild));
      }

    case eHTMLTag_h1: case eHTMLTag_h2:
    case eHTMLTag_h3: case eHTMLTag_h4:
    case eHTMLTag_h5: case eHTMLTag_h6:
      {
        if(0!=strchr(gHeadingTags,aChild))
          result=PR_FALSE;
        else result=PRBool(0!=strchr(gTagSet1,aChild)); 
      }
      break;

    case eHTMLTag_head:
      {
        static char  okTags[]={
          eHTMLTag_base,  eHTMLTag_isindex, eHTMLTag_link,  eHTMLTag_meta,
          eHTMLTag_script,eHTMLTag_style,   eHTMLTag_title, 0};
        result=PRBool(0!=strchr(okTags,aChild));
      }
      break;

    case eHTMLTag_hr:      
      break;    //singletons can't contain anything...

    case eHTMLTag_html:
      {
        static char  okTags[]={eHTMLTag_body,eHTMLTag_frameset,eHTMLTag_head,0};
        result=PRBool(0!=strchr(okTags,aChild));
      }
      break;

    case eHTMLTag_iframe:
      result=PRBool(0!=strchr(gTagSet1,aChild)); break;

    case eHTMLTag_img:
    case eHTMLTag_input:
    case eHTMLTag_isindex:
    case eHTMLTag_spacer:
    case eHTMLTag_wbr:
      break;    //singletons can't contain anything...

    case eHTMLTag_italic:
    case eHTMLTag_ins:
    case eHTMLTag_kbd:
    case eHTMLTag_label:
    case eHTMLTag_legend:
      result=PRBool(0!=strchr(gTagSet2,aChild)); break;

    case eHTMLTag_layer:
      break;

    case eHTMLTag_link:
      break;    //singletons can't contain anything...

    case eHTMLTag_listitem:
      if (eHTMLTag_listitem == aChild) {
        return PR_FALSE;
      }
      result=PRBool(!strchr(gHeadingTags,aChild)); break;

    case eHTMLTag_listing:
      result = PR_TRUE; break;

    case eHTMLTag_map:
      result=PRBool(eHTMLTag_area==aChild); break;

    case eHTMLTag_marquee:
      result=PRBool(0!=strchr(gTagSet2,aChild)); break;

    case eHTMLTag_math:
      break; //nothing but plain text...

    case eHTMLTag_meta:
      break;  //singletons can't contain other tags

    case eHTMLTag_menu:
    case eHTMLTag_dir:
    case eHTMLTag_ol:
    case eHTMLTag_ul:
      // XXX kipp was here
      result=PRBool(!strchr(gHeadingTags,aChild)); break;

    case eHTMLTag_nobr:
      result=PR_TRUE; break;

    case eHTMLTag_noframes:
      if(eHTMLTag_body==aChild)
        result=PR_TRUE;
      else result=PRBool(0!=strchr(gTagSet1,aChild)); 
      break;

    case eHTMLTag_noscript:
      result=PRBool(0!=strchr(gTagSet1,aChild)); break;

    case eHTMLTag_note:

    case eHTMLTag_object:
      result=PRBool(0!=strchr(gTagSet2,aChild)); break;

    case eHTMLTag_option:
      //for now, allow an option to contain anything but another option...
      result=PRBool(eHTMLTag_option!=aChild); break;

    case eHTMLTag_paragraph:
      if(eHTMLTag_paragraph==aChild)
        result=PR_FALSE;
      else result=PRBool(0!=strchr(gTagSet2,aChild)); 
      break;

    case eHTMLTag_param:
      break;  //singletons can't contain other tags

    case eHTMLTag_plaintext:
      break;

    case eHTMLTag_pre:
    case eHTMLTag_quotation:
      result=PRBool(0!=strchr(gTagSet2,aChild)); break;

    case eHTMLTag_strike:
      result=PRBool(0!=strchr(gTagSet2,aChild)); break;

    case eHTMLTag_script:
      break; //unadorned script text...

    case eHTMLTag_select:
      result=PR_TRUE; break; //for now, allow select to contain anything...

    case eHTMLTag_small:
      result=PRBool(0!=strchr(gTagSet2,aChild)); break;

    case eHTMLTag_span:
      result=PRBool(0!=strchr(gTagSet1,aChild)); break;

    case eHTMLTag_style:
      break;  //singletons can't contain other tags

    case eHTMLTag_strong:
    case eHTMLTag_samp:
    case eHTMLTag_sub:
    case eHTMLTag_sup:
      result=PRBool(0!=strchr(gTagSet1,aChild)); break;

    case eHTMLTag_table:
      {
        static char  okTags[]={ 
          eHTMLTag_caption, eHTMLTag_col, eHTMLTag_colgroup,eHTMLTag_tbody,   
          eHTMLTag_tfoot,  /* eHTMLTag_tr,*/  eHTMLTag_thead,   0};
        result=PRBool(0!=strchr(okTags,aChild));
      }
      break;

    case eHTMLTag_tbody:
    case eHTMLTag_tfoot:
    case eHTMLTag_thead:
      result=PRBool(eHTMLTag_tr==aChild); break;

    case eHTMLTag_th:
      result=PRBool(0!=strchr(gTagSet1,aChild)); 
      break;

    case eHTMLTag_td:
      {
        static char  extraTags[]={eHTMLTag_newline,0};
        result=PRBool(0!=strchr(extraTags,aChild));
        if(PR_FALSE==result)
          result=PRBool(0!=strchr(gTagSet1,aChild)); 
      }
      break;

    case eHTMLTag_textarea:
    case eHTMLTag_title:
      break; //nothing but plain text...

    case eHTMLTag_tr:
      {
        static char  okTags[]={eHTMLTag_td,eHTMLTag_th,0};
        result=PRBool(0!=strchr(okTags,aChild));
      }
      break;

    case eHTMLTag_tt:
    case eHTMLTag_u:
      result=PRBool(0!=strchr(gTagSet2,aChild)); break;

    case eHTMLTag_var:
      result=PRBool(0!=strchr(gTagSet2,aChild)); break;

    case eHTMLTag_userdefined:
      result=PR_TRUE; break; //XXX for now...

    case eHTMLTag_xmp: 
    default:
      break;
  } //switch
  return result;
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
PRBool CNavDTD::CanContainIndirect(PRInt32 aParent,PRInt32 aChild) const {
  PRBool result=PR_FALSE;

  switch(aParent) {

    case eHTMLTag_html:
      {
        static char  okTags[]={
          eHTMLTag_head,    eHTMLTag_body,
          eHTMLTag_header,  eHTMLTag_footer,0
        };
        result=PRBool(0!=strchr(okTags,aChild));
      }

    case eHTMLTag_body:
      result=PR_TRUE; break;

    case eHTMLTag_table:
      {
        static char  okTags[]={
          eHTMLTag_caption, eHTMLTag_colgroup,
          eHTMLTag_tbody,   eHTMLTag_tfoot,
          eHTMLTag_thead,   eHTMLTag_tr,
          eHTMLTag_td,      eHTMLTag_th,
          eHTMLTag_col,     0};
        result=PRBool(0!=strchr(okTags,aChild));
      }
      break;

      result=PR_TRUE; break;

    default:
      break;
  }
  return result;
}

/**
 *  This method gets called to determine whether a given 
 *  tag can contain newlines. Most do not.
 *  
 *  @update  gess 3/25/98
 *  @param   aTag -- tag to test for containership
 *  @return  PR_TRUE if given tag can contain other tags
 */
PRBool CNavDTD::CanOmit(PRInt32 aParent,PRInt32 aChild) const {
  PRBool result=PR_FALSE;

  //begin with some simple (and obvious) cases...
  switch((eHTMLTags)aChild) {

    case eHTMLTag_userdefined:
    case eHTMLTag_comment:
      result=PR_TRUE; 
      break;

    case eHTMLTag_button:       case eHTMLTag_fieldset:
    case eHTMLTag_input:        case eHTMLTag_isindex:
    case eHTMLTag_label:        case eHTMLTag_legend:
    case eHTMLTag_select:       case eHTMLTag_textarea:
      if(PR_FALSE==mParser->HasOpenContainer(eHTMLTag_form))
        result=PR_TRUE; 
      break;

    case eHTMLTag_newline:    
    case eHTMLTag_whitespace:

      switch((eHTMLTags)aParent) {
        case eHTMLTag_html:     case eHTMLTag_head:   
        case eHTMLTag_title:    case eHTMLTag_map:    
        case eHTMLTag_tr:       case eHTMLTag_table:  
        case eHTMLTag_thead:    case eHTMLTag_tfoot:  
        case eHTMLTag_tbody:    case eHTMLTag_col:    
        case eHTMLTag_colgroup: case eHTMLTag_unknown:
          result=PR_TRUE;
        default:
          break;
      } //switch
      break;

    case eHTMLTag_entity:
      switch((eHTMLTags)aParent) {
        case eHTMLTag_tr:       case eHTMLTag_table:  
        case eHTMLTag_thead:    case eHTMLTag_tfoot:  
        case eHTMLTag_tbody:    
          result=PR_TRUE;
        default:
          break;
      } //switch
      break;

    default:
      if(eHTMLTag_unknown==aParent)
        result=PR_FALSE;
      break;
  } //switch
  return result;
}

/**
 *  This method gets called to determine whether a given
 *  ENDtag can be omitted. Admittedly,this is a gross simplification.
 *  
 *  @update  gess 3/25/98
 *  @param   aTag -- tag to test for containership
 *  @return  PR_TRUE if given tag can contain other tags
 */
PRBool CNavDTD::CanOmitEndTag(PRInt32 aParent,PRInt32 aChild) const {
  PRBool result=PR_FALSE;

  //begin with some simple (and obvious) cases...
  switch((eHTMLTags)aChild) {

    case eHTMLTag_userdefined:
    case eHTMLTag_comment:
      result=PR_TRUE; 
      break;

    case eHTMLTag_h1:           case eHTMLTag_h2:
    case eHTMLTag_h3:           case eHTMLTag_h4:
    case eHTMLTag_h5:           case eHTMLTag_h6:
      {
        if(0!=strchr(gHeadingTags,aParent))
          result=PR_FALSE;
        //Actually, we probably need to walk the stack here...
        else result=PR_TRUE; 
      }
      break;

    case eHTMLTag_select:       case eHTMLTag_option:
    case eHTMLTag_button:       case eHTMLTag_fieldset:
    case eHTMLTag_input:        case eHTMLTag_isindex:
    case eHTMLTag_label:        case eHTMLTag_legend:
    case eHTMLTag_textarea:
      if(PR_FALSE==mParser->HasOpenContainer(aChild))
        result=PR_TRUE; 
      break;

    case eHTMLTag_newline:    
    case eHTMLTag_whitespace:

      switch((eHTMLTags)aParent) {
        case eHTMLTag_html:     case eHTMLTag_head:   
        case eHTMLTag_title:    case eHTMLTag_map:    
        case eHTMLTag_tr:       case eHTMLTag_table:  
        case eHTMLTag_thead:    case eHTMLTag_tfoot:  
        case eHTMLTag_tbody:    case eHTMLTag_col:    
        case eHTMLTag_colgroup: case eHTMLTag_unknown:
          result=PR_TRUE;
        default:
          break;
      } //switch
      break;

    default:
      result=(!mParser->HasOpenContainer(aChild));
      break;
  } //switch
  return result;
}

/**
 *  This method gets called to determine whether a given 
 *  tag is itself a container
 *  
 *  @update  gess 4/8/98
 *  @param   aTag -- tag to test for containership
 *  @return  PR_TRUE if given tag can contain other tags
 */
PRBool CNavDTD::IsContainer(PRInt32 aTag) const {
  PRBool result=PR_FALSE;

  switch(aTag){
    case eHTMLTag_area:       case eHTMLTag_base:
    case eHTMLTag_basefont:   case eHTMLTag_br:
    case eHTMLTag_col:        case eHTMLTag_colgroup:
    case eHTMLTag_frame:
    case eHTMLTag_hr:         case eHTMLTag_img:
    case eHTMLTag_input:      case eHTMLTag_isindex:
    case eHTMLTag_link:
    case eHTMLTag_math:       case eHTMLTag_meta:
//    case eHTMLTag_option:     
    case eHTMLTag_param:
    case eHTMLTag_style:      case eHTMLTag_spacer:
    case eHTMLTag_wbr:
    case eHTMLTag_form:
    case eHTMLTag_newline:
    case eHTMLTag_whitespace:
    case eHTMLTag_text: 
      result=PR_FALSE;
      break;

    default:
      result=PR_TRUE;
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
PRInt32 CNavDTD::GetDefaultParentTagFor(PRInt32 aTag) const{
  eHTMLTags result=eHTMLTag_unknown;
  switch(aTag) {

    case eHTMLTag_text:
      result=eHTMLTag_paragraph; break;

    case eHTMLTag_html:
      result=eHTMLTag_unknown; break;

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
      result=eHTMLTag_table; break;

    case eHTMLTag_tr:
      result=eHTMLTag_tbody; break;

    case eHTMLTag_td:
    case eHTMLTag_th:
      result=eHTMLTag_tr; break;

    case eHTMLTag_col:
      result=eHTMLTag_colgroup; break;    

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


/**
 * This method gets called at various times by the parser
 * whenever we want to verify a valid context stack. This
 * method also gives us a hook to add debugging metrics.
 *
 * @update  gess4/6/98
 * @param   aStack[] array of ints (tokens)
 * @param   aCount number of elements in given array
 * @return  TRUE if stack is valid, else FALSE
 */
PRBool CNavDTD::VerifyContextVector(PRInt32* aVector,PRInt32 aCount) const {
  PRBool result=PR_TRUE;

  if(aCount>0) {

  }
  return result;
}


/**
 * This method tries to design a context vector (without actually
 * changing our parser state) from the parent down to the
 * child. 
 *
 * @update  gess4/6/98
 * @param   aVector is the string where we store our output vector
 *          in bottom-up order.
 * @param   aParent -- tag type of parent
 * @param   aChild -- tag type of child
 * @return  TRUE if propagation closes; false otherwise
 */
PRBool CNavDTD::ForwardPropagate(nsString& aVector,PRInt32 aParentTag,PRInt32 aChildTag) const {
  PRBool result=PR_FALSE;

  switch(aParentTag) {
    case eHTMLTag_table:
      {
        static char  tableTags[]={eHTMLTag_tr,eHTMLTag_td,0};
        if(strchr(tableTags,aChildTag)) {
          //if you're here, we know we can correctly backward propagate.
          result=BackwardPropagate(aVector,aParentTag,aChildTag);
        }
      }
      break;

    case eHTMLTag_tr:
      if(PR_TRUE==CanContain(eHTMLTag_td,aChildTag)) {
        aVector.Append((PRUnichar)eHTMLTag_td);
        result=BackwardPropagate(aVector,aParentTag,eHTMLTag_td);
//        result=PR_TRUE;
      }

      break;

    case eHTMLTag_th:
      break;

    default:
      break;
  }//switch
  return result;
}


/**
 * This method tries to design a context map (without actually
 * changing our parser state) from the child up to the parent.
 *
 * @update  gess4/6/98
 * @param   aVector is the string where we store our output vector
 *          in bottom-up order.
 * @param   aParent -- tag type of parent
 * @param   aChild -- tag type of child
 * @return  TRUE if propagation closes; false otherwise
 */
PRBool CNavDTD::BackwardPropagate(nsString& aVector,PRInt32 aParentTag,PRInt32 aChildTag) const {

  PRBool    result=PR_FALSE;
  eHTMLTags theParentTag=(eHTMLTags)aChildTag;

//  aVector.Truncate();

    //create the necessary stack of parent tags...
    //continue your search until you run out of known parents,
    //or you find the specific parent you were given (aParentTag).
//  aVector.Append((PRUnichar)aChildTag);
  do {
    theParentTag=(eHTMLTags)GetDefaultParentTagFor(theParentTag);
    if(theParentTag!=eHTMLTag_unknown) {
      aVector.Append((PRUnichar)theParentTag);
    }
  } while((theParentTag!=eHTMLTag_unknown) && (theParentTag!=aParentTag));
  
  return PRBool(aParentTag==theParentTag);
}



