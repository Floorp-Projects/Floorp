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

#include "nscalexport.h"
#include "nsCalXMLDTD.h"
#include "nsParser.h"
#include "nsParserNode.h"
#include "nsCalXMLContentSink.h"
#include "nsHTMLTokens.h"
#include "nsParserCIID.h"
#include "nsCRT.h"
#include "nsCalParserCIID.h"
#include "nsxpfcstrings.h"


static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kIDTDIID,      NS_IDTD_IID);
static NS_DEFINE_IID(kClassIID,     NS_ICALXML_DTD_IID); 
static NS_DEFINE_IID(kBaseClassIID, NS_INAVHTML_DTD_IID); 

static NS_DEFINE_IID(kCParserNodeCID, NS_PARSER_NODE_IID); 
static NS_DEFINE_IID(kCParserNodeIID, NS_IPARSER_NODE_IID); 

static const char* kCalXMLTextContentType = "text/julian";
static const char* kCalXMLDocHeader= "<!DOCTYPE julian";

struct nsCalXMLTagEntry {
  char      mName[32];
  eCalXMLTags  fTagID;
};

nsCalXMLTagEntry gCalXMLTagTable[] =
{
  {"!!UNKNOWN",             eCalXMLTag_unknown},
  {"!DOCTYPE",              eCalXMLTag_doctype},
  {"?XML",                  eCalXMLTag_xml},

  {"attr",                  eCalXMLTag_attr},
  {"calendar",              eCalXMLTag_calendar},
  {"commandcanvas",         eCalXMLTag_commandcanvas},
  {"comment",               eCalXMLTag_comment},
  {"control",               eCalXMLTag_control},
  {"ctx",                   eCalXMLTag_ctx},
  {"foldercanvas",          eCalXMLTag_foldercanvas},
  {"htmlcanvas",            eCalXMLTag_htmlcanvas},
  {"leaf",                  eCalXMLTag_leaf},
  {"mcc",                   eCalXMLTag_mcc},
  {"multidayviewcanvas",    eCalXMLTag_multidayviewcanvas},
  {"object",                eCalXMLTag_object},
  {"panel",                 eCalXMLTag_panel},
  {"rootpanel",             eCalXMLTag_rootpanel},
  {"set",                   eCalXMLTag_set},
  {"tcc",                   eCalXMLTag_tcc},
  {"timebarscale",          eCalXMLTag_timebarscale},
  {"timebaruserheading",    eCalXMLTag_timebaruserheading},
  {"todocanvas",            eCalXMLTag_todocanvas},
  {"xpitem",                eCalXMLTag_xpitem},
};


eCalXMLTags DetermineCalXMLTagType(const nsString& aString)
{
  PRInt32  result=-1;
  PRInt32  cnt=sizeof(gCalXMLTagTable)/sizeof(nsCalXMLTagEntry);
  PRInt32  low=0; 
  PRInt32  high=cnt-1;
  PRInt32  middle=0;

  while(low<=high){
    middle=(PRInt32)(low+high)/2;
    result=aString.Compare(gCalXMLTagTable[middle].mName, PR_TRUE); 
//    result=aString.Compare(gCalXMLTagTable[middle].mName, nsCRT::strlen(gCalXMLTagTable[middle].mName), PR_TRUE);
    if (result==0)
      return gCalXMLTagTable[middle].fTagID; 
    if (result<0)
      high=middle-1; 
    else low=middle+1; 
  }
  return eCalXMLTag_userdefined;
}


PRInt32 CalXMLDispatchTokenHandler(CToken* aToken,nsIDTD* aDTD) {
  
  PRInt32         result=0;

  eHTMLTokenTypes theType= (eHTMLTokenTypes)aToken->GetTokenType();  
  nsCalXMLDTD*    theDTD=(nsCalXMLDTD*)aDTD;

  nsString& name = aToken->GetStringValueXXX();
  eCalXMLTags type = DetermineCalXMLTagType(name);
  
  if (type != eCalXMLTag_userdefined)
    aToken->SetTypeID(type);
  
  if(aDTD) {
    switch(theType) {
      case eToken_start:
        result=theDTD->HandleStartToken(aToken); break;
      case eToken_end:
        result=theDTD->HandleEndToken(aToken); break;
      case eToken_comment:
        result=theDTD->HandleCommentToken(aToken); break;
      case eToken_entity:
        result=theDTD->HandleEntityToken(aToken); break;
      case eToken_attribute:
        result=theDTD->HandleAttributeToken(aToken); break;
      default:
        result=0;
    }//switch
  }//if
  return result;
}


nsresult nsCalXMLDTD::QueryInterface(const nsIID& aIID, void** aInstancePtr)  
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
    *aInstancePtr = (nsCalXMLDTD*)(this);                                        
  }                 
  else {
    *aInstancePtr=0;
    return NS_NOINTERFACE;
  }
  ((nsISupports*) *aInstancePtr)->AddRef();
  return NS_OK;                                                        
}


NS_IMPL_ADDREF(nsCalXMLDTD)
NS_IMPL_RELEASE(nsCalXMLDTD)

nsCalXMLDTD::nsCalXMLDTD() : CNavDTD() 
{
  NS_INIT_REFCNT();
}

nsCalXMLDTD::~nsCalXMLDTD()
{
}

PRBool nsCalXMLDTD::CanParse(nsString& aContentType, PRInt32 aVersion)
{
  if (aContentType == kCalXMLTextContentType)
    return PR_TRUE;

  return PR_FALSE;
}

eAutoDetectResult nsCalXMLDTD::AutoDetectContentType(nsString& aBuffer,nsString& aType)
{
  if ((aType == kCalXMLTextContentType) || (aBuffer.Find(kCalXMLDocHeader) != -1))
  {
    aType = kCalXMLTextContentType;
    return eValidDetect;
  }

  return eUnknownDetect;
}


nsresult nsCalXMLDTD::HandleToken(CToken* aToken)
{
  nsresult result=NS_OK;

  if(aToken) {

    CHTMLToken*     theToken= (CHTMLToken*)(aToken);
    eHTMLTokenTypes theType=eHTMLTokenTypes(theToken->GetTokenType());

    result=CalXMLDispatchTokenHandler(theToken,this);

  }
  return result;

  //return CNavDTD::HandleToken(aToken);
}



nsresult nsCalXMLDTD::CreateNewInstance(nsIDTD** aInstancePtrResult)
{
  static NS_DEFINE_IID(kCCalXMLDTD, NS_ICALXML_DTD_IID);

  nsresult result = nsRepository::CreateInstance(kCCalXMLDTD, 
                                                 nsnull, 
                                                 kIDTDIID,
                                                 (void**) aInstancePtrResult);

  return (result);
}


nsresult nsCalXMLDTD::HandleStartToken(CToken* aToken) 
{
  CStartToken * st          = (CStartToken*)aToken;
  eCalXMLTags tokenTagType  = (eCalXMLTags) st->GetTypeID();
  nsCParserNode * attrNode = nsnull;

  //Begin by gathering up attributes...
  static NS_DEFINE_IID(kCParserNodeCID, NS_PARSER_NODE_IID); 
  static NS_DEFINE_IID(kCParserNodeIID, NS_IPARSER_NODE_IID); 

  nsresult result = nsRepository::CreateInstance(kCParserNodeCID, nsnull, kCParserNodeIID,(void**) &attrNode);

  if (NS_OK != result)
    return result;

  attrNode->Init((CHTMLToken*)aToken,mLineNumber);

  PRInt16       attrCount=aToken->GetAttributeCount();
  result=(0==attrCount) ? NS_OK : CollectAttributes(*attrNode,attrCount);

  if (tokenTagType == eCalXMLTag_object) {
    tokenTagType = TagTypeFromObject(*attrNode);
    st->SetTypeID(tokenTagType);
  }

  if(NS_OK==result) {
  
      switch(tokenTagType) {

        case eCalXMLTag_calendar:
           break;

        /*
         * the Panel Tag represents the core container object for layout
         */

        case eCalXMLTag_object:
        case eCalXMLTag_rootpanel:
        case eCalXMLTag_panel:
        case eCalXMLTag_foldercanvas:
        {
          mSink->OpenContainer(*attrNode);
        }
        break;

        case eCalXMLTag_mcc:
        case eCalXMLTag_tcc:
        case eCalXMLTag_ctx:
        case eCalXMLTag_commandcanvas:
        case eCalXMLTag_multidayviewcanvas:
        case eCalXMLTag_timebarscale:
        case eCalXMLTag_timebaruserheading:
        case eCalXMLTag_todocanvas:
        case eCalXMLTag_xpitem:
        case eCalXMLTag_htmlcanvas:
        {
          mSink->AddLeaf(*attrNode);
        }
        break;

        case eCalXMLTag_control:
        {
          mSink->AddLeaf(*attrNode);
        }
        break;

        case eCalXMLTag_set:
           break;

        default:
          break;
      }
  } 

  NS_RELEASE(attrNode);

  if(eHTMLTag_newline==tokenTagType)
    mLineNumber++;

  return result;
}

nsresult nsCalXMLDTD::HandleEndToken(CToken* aToken) 
{

  nsresult    result=NS_OK;
  CEndToken*  et = (CEndToken*)(aToken);
  eCalXMLTags   tokenTagType=(eCalXMLTags)et->GetTypeID();
  nsCParserNode * attrNode = nsnull;

  static NS_DEFINE_IID(kCParserNodeCID, NS_PARSER_NODE_IID); 
  static NS_DEFINE_IID(kCParserNodeIID, NS_IPARSER_NODE_IID); 

  result = nsRepository::CreateInstance(kCParserNodeCID, nsnull, kCParserNodeIID, (void**)&attrNode);

  if (NS_OK != result)
    return result;

  attrNode->Init((CHTMLToken*)aToken,mLineNumber);

  if (tokenTagType == eCalXMLTag_object) {
    tokenTagType = TagTypeFromObject(*attrNode);
    et->SetTypeID(tokenTagType);
  }

  switch(tokenTagType) {

    case eCalXMLTag_calendar:

    case eCalXMLTag_object:
    case eCalXMLTag_panel:
    case eCalXMLTag_rootpanel:
    case eCalXMLTag_foldercanvas:
    {
      mSink->CloseContainer(*attrNode);
    }
    break;

    case eCalXMLTag_tcc:
    case eCalXMLTag_mcc:
    case eCalXMLTag_ctx:
    case eCalXMLTag_commandcanvas:
    case eCalXMLTag_multidayviewcanvas:
    case eCalXMLTag_timebarscale:
    case eCalXMLTag_timebaruserheading:
    case eCalXMLTag_todocanvas:
    case eCalXMLTag_htmlcanvas:
    case eCalXMLTag_control:
    case eCalXMLTag_xpitem:
       break;

    case eCalXMLTag_set:
       break;


    default:
      break;
  }

  NS_RELEASE(attrNode);

  return result;
}

eCalXMLTags nsCalXMLDTD::TagTypeFromObject(const nsIParserNode& aNode) 
{
  PRInt32 i = 0;
  
  for (i = 0; i < aNode.GetAttributeCount(); i++) {
   
   nsString key = aNode.GetKeyAt(i);

   key.StripChars("\"");

   if (key.EqualsIgnoreCase(CAL_STRING_CLASS)) {

      nsString value = aNode.GetValueAt(i);
    
      value.StripChars("\"");

      if (value.EqualsIgnoreCase(CAL_STRING_TIMEBAR_USER_HEADING))
        return eCalXMLTag_timebaruserheading;
      if (value.EqualsIgnoreCase(XPFC_STRING_HTML_CANVAS))
        return eCalXMLTag_htmlcanvas;
      if (value.EqualsIgnoreCase(XPFC_STRING_FOLDER_CANVAS))
        return eCalXMLTag_foldercanvas;
      if (value.EqualsIgnoreCase(XPFC_STRING_XPITEM))
        return eCalXMLTag_xpitem;
   }
   
  }

  return (eCalXMLTag_unknown);

}