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
#include "nsCalParserCIID.h"
#include "nsxpfcCIID.h"
#include "nsCalICalendarDTD.h"
#include "nsToken.h"
#include "nsScanner.h"
#include "nsParser.h" // ?
#include "nsParserNode.h" // ?
#include "nsIParserNode.h" // ?
#include "nsCalICalendarContentSink.h"
#include "nsCalICalendarTokens.h" // todo:
#include "nsCalICalendarParserCIID.h"
#include "nsParserCIID.h"
#include "nsCRT.h" // ?

//#include "nsCalICalendarStrings.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIDTDIID,      NS_IDTD_IID);
static NS_DEFINE_IID(kClassIID,     NS_ICALICALENDAR_DTD_IID);
static NS_DEFINE_IID(kBaseClassIID, NS_ICALICALENDAR_DTD_IID);

static NS_DEFINE_IID(kCalICalendarCParserNodeCID, NS_CALICALENDARPARSER_NODE_IID);
static NS_DEFINE_IID(kCParserNodeIID, NS_IPARSER_NODE_IID);

static const char* kNullToken = "Error: Null token given";
static const char* kCalICalendarTextHeader = "BEGIN:VCALENDAR";
static const char* kCalICalendarTextContentType = "text/calendar";
static nsAutoString gCalEmpty;

static eCalICalendarTags gCalNonContainers[]={
  eCalICalendarTag_action, eCalICalendarTag_attach, 
  eCalICalendarTag_attendee, eCalICalendarTag_calscale, 
  eCalICalendarTag_categories, eCalICalendarTag_class, 
  eCalICalendarTag_comment, eCalICalendarTag_completed, 
  eCalICalendarTag_contact, eCalICalendarTag_created, 
  eCalICalendarTag_description, 
  eCalICalendarTag_dtend, eCalICalendarTag_dtstart, 
  eCalICalendarTag_due, eCalICalendarTag_duration, 
  eCalICalendarTag_exdate, eCalICalendarTag_exrule, 
  eCalICalendarTag_freebusy, eCalICalendarTag_geo, 
  eCalICalendarTag_last_modified, eCalICalendarTag_location, 
  eCalICalendarTag_method, eCalICalendarTag_organizer, 
  eCalICalendarTag_percent_complete, eCalICalendarTag_priority, 
  eCalICalendarTag_prodid, eCalICalendarTag_rdate, 
  eCalICalendarTag_recurrence_id, eCalICalendarTag_related_to, 
  eCalICalendarTag_repeat, eCalICalendarTag_request_status, 
  eCalICalendarTag_resources, eCalICalendarTag_rrule, 
  eCalICalendarTag_sequence, 
  eCalICalendarTag_status, eCalICalendarTag_summary, 
  eCalICalendarTag_transp, eCalICalendarTag_trigger, 
  eCalICalendarTag_tzid, eCalICalendarTag_tzname, 
  eCalICalendarTag_tzoffsetfrom, eCalICalendarTag_tzoffsetto, 
  eCalICalendarTag_tzurl, eCalICalendarTag_uid, 
  eCalICalendarTag_url, 
  eCalICalendarTag_version, 
};

struct nsCalICalendarTagEntry {
  char mName[32];
  eCalICalendarTags fTagID;
};

nsCalICalendarTagEntry gCalICalendarTagTable[] =
{
  {"vcalendar", eCalICalendarTag_vcalendar},
  {"vevent",    eCalICalendarTag_vevent},
  {"vtodo",     eCalICalendarTag_vtodo},
  {"vjournal",  eCalICalendarTag_vjournal},
  {"vfreebusy", eCalICalendarTag_vfreebusy},
  {"valarm",    eCalICalendarTag_valarm},
  {"vtimezone", eCalICalendarTag_vtimezone}
};

eCalICalendarTags DetermineCalICalendarComponentType(const nsString& aString)
{
  PRInt32  result=-1;
  PRInt32  cnt=sizeof(gCalICalendarTagTable)/sizeof(nsCalICalendarTagEntry);
  PRInt32  low=0; 
  PRInt32  high=cnt-1;
  PRInt32  middle=0;

  while(low<=high){
    middle=(PRInt32)(low+high)/2;
    //result=aString.Compare(gCalICalendarTagTable[middle].mName, nsCRT::strlen(gXPFCXMLTagTable[middle].mName), PR_TRUE);
    result=aString.Compare(gCalICalendarTagTable[middle].mName, PR_TRUE);
    if (result==0)
      return gCalICalendarTagTable[middle].fTagID; 
    if (result<0)
      high=middle-1; 
    else low=middle+1; 
  }
  return eCalICalendarTag_userdefined;  
}

PRInt32 CalICalendarDispatchTokenHandler(CToken* aToken,nsIDTD* aDTD) {
  PRInt32 result=0;
  eCalICalendarTokenTypes theType = (eCalICalendarTokenTypes)aToken->GetTokenType();
  nsCalICalendarDTD* theDTD=(nsCalICalendarDTD*)aDTD;

  nsString& name = aToken->GetStringValueXXX();
  eCalICalendarTags type = DetermineCalICalendarComponentType(name);

  if (type != eCalICalendarTag_userdefined)
    aToken->SetTypeID(type);

  if (aDTD) {
    switch(theType) {
      case eCalICalendarToken_begin:
        result=theDTD->HandleBeginToken(aToken); break;
      case eCalICalendarToken_end:
        result=theDTD->HandleEndToken(aToken); break;
      case eCalICalendarToken_attribute:
        result=theDTD->HandleAttributeToken(aToken); break;
      case eCalICalendarToken_propertyvalue:
        result=theDTD->HandlePropertyValueToken(aToken); break;
      default:
        result=0;
    }//switch
  }//if
  return result;
}


/* token deallocator */
class nsCalTokenDeallocator: public nsDequeFunctor {
public:
  virtual void* operator()(void* anObject) {
    CToken* aToken = (CToken*)anObject;
    delete aToken;
    return 0;
  }
};
static nsCalTokenDeallocator gCalTokenKiller;

/* token recycler, cuts down number of tokens that get created during the run of system */
class nsCalTokenRecycler: public nsITokenRecycler {
public:
  nsCalTokenRecycler();
  virtual ~nsCalTokenRecycler();
  virtual void RecycleToken(CToken* aToken);
  virtual CToken* CreateTokenOfType(eCalICalendarTokenTypes aType,eCalICalendarTags aTag,
                                    const nsString& aString);

protected:
  nsDeque* mTokenCache[eCalICalendarToken_last-1];
};

nsCalTokenRecycler::nsCalTokenRecycler() : nsITokenRecycler() {
  int i=0;
  for (i=0;i<eCalICalendarToken_last-1;i++) {
    mTokenCache[i]=new nsDeque(gCalTokenKiller);
    //mTotals[i] = 0;
  }
}

nsCalTokenRecycler::~nsCalTokenRecycler() {
  int i;
  for(i=0;i<eCalICalendarToken_last-1;i++){
    delete mTokenCache[i];
  }
}

void nsCalTokenRecycler::RecycleToken(CToken* aToken) {
  if(aToken) {
    PRInt32 theType=aToken->GetTokenType();
    mTokenCache[theType-1]->Push(aToken);
  }
}

CToken * nsCalTokenRecycler::CreateTokenOfType(eCalICalendarTokenTypes aType,
                                               eCalICalendarTags aTag,
                                               const nsString& aString)
{
  CToken* result=(CToken*)mTokenCache[aType-1]->Pop();

  if(result) {
    result->Reinitialize(aTag,aString);
  }
  else {
    switch(aType) {
      //todo: finish
      case eCalICalendarToken_begin:          result=new CCalICalendarBeginToken(aString, aTag); break;
      case eCalICalendarToken_end:            result=new CCalICalendarEndToken(aString, aTag); break;
      case eCalICalendarToken_propertyname:   result=new CCalICalendarIdentifierToken(aTag); break;
      case eCalICalendarToken_attribute:      result=new CCalICalendarAttributeToken(); break;
      case eCalICalendarToken_propertyvalue:  result=new CCalICalendarPropertyValueToken(); break;
      default:
        break;
    }
  }
  return result;
}

nsCalTokenRecycler gCalTokenRecycler;

struct nsCalICalendarComponentEntry {
  char mName[32];
  eCalICalendarTags fTagID;
};


nsresult nsCalICalendarDTD::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aIID.Equals(kISupportsIID)) { //do IUnknown...
    *aInstancePtr = (nsIDTD*)(this);
  }
  else if (aIID.Equals(kIDTDIID)) { //do IParser base class...
    *aInstancePtr = (nsIDTD*)(this);
  }
  else if (aIID.Equals(kClassIID)) { //do this class...
    *aInstancePtr = (nsCalICalendarDTD*)(this);
  }
  else {
    *aInstancePtr=0;
    return NS_NOINTERFACE;
  }
  ((nsISupports*) *aInstancePtr)->AddRef();
  return NS_OK;
}

NS_CALICALENDARPARS nsresult NS_NewICAL_DTD(nsIDTD** aInstancePtrResult)
{
  nsCalICalendarDTD * it = new nsCalICalendarDTD();

  if (it == 0) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kClassIID, (void **) aInstancePtrResult);
}

NS_IMPL_ADDREF(nsCalICalendarDTD)
NS_IMPL_RELEASE(nsCalICalendarDTD)

nsCalICalendarDTD::nsCalICalendarDTD() : nsIDTD() , mTokenDeque(gCalTokenKiller)
{
  NS_INIT_REFCNT();
  mParser=0;
}

nsCalICalendarDTD::~nsCalICalendarDTD(){
}

PRBool nsCalICalendarDTD::CanParse(nsString& aContentType, PRInt32 aVersion)
{
  if (aContentType == kCalICalendarTextContentType)
    return PR_TRUE;

  return PR_FALSE;
}

eAutoDetectResult nsCalICalendarDTD::AutoDetectContentType(nsString& aBuffer,nsString& aType)
{
  if (aType == kCalICalendarTextContentType || aBuffer.Find(kCalICalendarTextHeader) != -1)
  {
    aType = kCalICalendarTextContentType;
    return eValidDetect;
  }
  return eUnknownDetect;
}

nsresult nsCalICalendarDTD::HandleToken(CToken* aToken)
{
  nsresult result=NS_OK;

  if(aToken) {

    CCalICalendarToken*     theToken= (CCalICalendarToken*)(aToken);
    eCalICalendarTokenTypes theType=eCalICalendarTokenTypes(theToken->GetTokenType());

    result=CalICalendarDispatchTokenHandler(theToken,this);
  }
  return result;
}

nsresult nsCalICalendarDTD::CreateNewInstance(nsIDTD** aInstancePtrResult)
{
  /*
  static NS_DEFINE_IID(kCCalCalICalendarDTD, NS_ICALICALENDAR_DTD_IID);

  nsresult result = nsRepository::CreateInstance(kCCalCalICalendarDTD,
                                                 nsnull,
                                                 kIDTDIID,
                                                 (void**) aInstancePtrResult);

  return (result);
  */
  return NS_NewICAL_DTD(aInstancePtrResult);
}

nsresult nsCalICalendarDTD::HandleAttributeToken(CToken* aToken) {
  NS_PRECONDITION(0 != aToken,kNullToken);
  NS_ERROR("attribute encountered -- this shouldn't happen!");

  return NS_OK;
}

nsresult nsCalICalendarDTD::HandlePropertyValueToken(CToken* aToken) {

  /*
    todo: handle "Content-type" stuff here
    */
  /*
    XXX: this is bad hack.  I really shouldn't be handling MIME.
    Someone else should parse all the MIME.
    This DTD should really know how to parse iCal.
    I should really NS_ERROR if this is called.
   */

  return NS_OK;
}

nsresult nsCalICalendarDTD::HandleBeginToken(CToken* aToken)
{
  CCalICalendarBeginToken * st  = (CCalICalendarBeginToken*)aToken;
  eCalICalendarTags tokenTagType = (eCalICalendarTags) st->GetTypeID();
  nsCalICalendarCParserNode * attrNode = nsnull;

  //Begin by gathering up attributes...
  static NS_DEFINE_IID(kCalICalendarCParserNodeCID, NS_CALICALENDARPARSER_NODE_IID);
  static NS_DEFINE_IID(kCParserNodeIID, NS_IPARSER_NODE_IID);

  nsresult result = nsRepository::CreateInstance(kCalICalendarCParserNodeCID, nsnull, kCParserNodeIID,(void**) &attrNode);

  if (NS_OK != result)
    return result;

  attrNode->Init((CCalICalendarToken*)aToken,mLineNumber);

  PRInt16     attrCount=aToken->GetAttributeCount();
  result=(0==attrCount) ? NS_OK : CollectAttributes(*attrNode,attrCount);

  // now set skipped content to property value token
  /* todo: finish
  if (tokenTagType == eCalICalendarTag_begin) {
    tokenTagType = ComponentTypeFromObject(*attrNode);
    st->SetTypeID(tokenTagType);
  }
  */
  if(NS_OK==result) {

    switch(tokenTagType) {
      case eCalICalendarTag_vcalendar:
      case eCalICalendarTag_vevent:
      case eCalICalendarTag_vtodo:
      case eCalICalendarTag_vjournal:
      case eCalICalendarTag_vfreebusy:
      case eCalICalendarTag_valarm:
      case eCalICalendarTag_vtimezone:          
      {
        // todo: xxx: close any already open containers;
        if (eCalICalendarTag_begin == aToken->GetTypeID())
          mSink->OpenContainer(*attrNode);
      }
      break;

   		case eCalICalendarTag_action:
  		case eCalICalendarTag_attach:
      case eCalICalendarTag_attendee:
      case eCalICalendarTag_calscale:
      case eCalICalendarTag_categories:
      case eCalICalendarTag_class:
      case eCalICalendarTag_comment:
      case eCalICalendarTag_completed:
      case eCalICalendarTag_contact:
      case eCalICalendarTag_created:
      case eCalICalendarTag_description:
      case eCalICalendarTag_dtend:
      case eCalICalendarTag_dtstart:
      case eCalICalendarTag_dtstamp:
      case eCalICalendarTag_due:
      case eCalICalendarTag_duration:
      case eCalICalendarTag_exdate:
      case eCalICalendarTag_exrule:
      case eCalICalendarTag_freebusy:
      case eCalICalendarTag_geo:
      case eCalICalendarTag_last_modified:
      case eCalICalendarTag_location:
      case eCalICalendarTag_method:
      case eCalICalendarTag_organizer:
      case eCalICalendarTag_percent_complete:
      case eCalICalendarTag_priority:
      case eCalICalendarTag_prodid:
      case eCalICalendarTag_rdate:
      case eCalICalendarTag_rrule:
      case eCalICalendarTag_recurrence_id:
      case eCalICalendarTag_related_to:
      case eCalICalendarTag_repeat:
      case eCalICalendarTag_request_status:
      case eCalICalendarTag_resources:
      case eCalICalendarTag_sequence:
      case eCalICalendarTag_status:
      case eCalICalendarTag_summary:
      case eCalICalendarTag_transp:
      case eCalICalendarTag_trigger:
      case eCalICalendarTag_tzid:
      case eCalICalendarTag_tzname:
      case eCalICalendarTag_tzoffsetfrom:
      case eCalICalendarTag_tzoffsetto:
      case eCalICalendarTag_tzurl:
      case eCalICalendarTag_uid:
      case eCalICalendarTag_url:
      case eCalICalendarTag_version:
      {
        // todo: finish
        /*
        if (((nsCalICalendarContentSink *)mSink)->IsContainer(*attrNode) == PR_TRUE)
            st->SetTypeID(eCalICalendarTag_property);
            */
        result = CollectPropertyValue(*attrNode,attrCount);
        mSink->AddLeaf(*attrNode);   
      }
      break;

      default:
        break;
    }
  }

  NS_RELEASE(attrNode);

  if(eCalICalendarTag_newline==tokenTagType)
    mLineNumber++;

  return result;
}

PRInt32 nsCalICalendarDTD::CollectPropertyValue(nsCalICalendarCParserNode& aNode,PRInt32 aCount) {
  
  CToken* theToken=mParser->PeekToken();
  if(theToken) {
    eCalICalendarTokenTypes theType=eCalICalendarTokenTypes(theToken->GetTokenType());
    if(eCalICalendarToken_propertyvalue==theType){
        mParser->PopToken(); //pop it for real...
        aNode.SetSkippedContent((CCalICalendarToken*)theToken);
    }
    else return kInterrupted;
  }
  return kNoError;
}

PRInt32 nsCalICalendarDTD::CollectAttributes(nsCalICalendarCParserNode& aNode,PRInt32 aCount) {
  int attr=0;
  for(attr=0;attr<aCount;attr++){
    CToken* theToken=mParser->PeekToken();
    if(theToken) {
      eCalICalendarTokenTypes theType=eCalICalendarTokenTypes(theToken->GetTokenType());
      if(eCalICalendarToken_attribute==theType){
        mParser->PopToken(); //pop it for real...
        aNode.AddAttribute((CCalICalendarToken*)theToken);
      }
    }
    else return kInterrupted;
  }
  return kNoError;
}

nsresult nsCalICalendarDTD::HandleEndToken(CToken* aToken)
{
  nsresult    result=NS_OK;
  CCalICalendarEndToken* et = (CCalICalendarEndToken *)(aToken);
  eCalICalendarTags tokenTagType=(eCalICalendarTags)et->GetTypeID();
  nsCParserNode * attrNode = nsnull;

  static NS_DEFINE_IID(kCalICalendarCParserNodeCID, NS_CALICALENDARPARSER_NODE_IID);
  static NS_DEFINE_IID(kCParserNodeIID, NS_IPARSER_NODE_IID);

  result = nsRepository::CreateInstance(kCalICalendarCParserNodeCID, nsnull, kCParserNodeIID, (void**)&attrNode);

  if (NS_OK != result)
    return result;

  attrNode->Init((CCalICalendarToken *)aToken, mLineNumber);

  /* todo: finish
  if (tokenTagType == eCalICalendarTag_object) {
    tokenTagType = ComponentTypeFromObject(*attrNode);
    et->SetTypeID(tokenTagType);
  }
  */
  switch(tokenTagType) {

    case eCalICalendarTag_vcalendar:
    case eCalICalendarTag_vevent:
    case eCalICalendarTag_vtodo:
    case eCalICalendarTag_vjournal:
    case eCalICalendarTag_vfreebusy:
    case eCalICalendarTag_valarm:
    case eCalICalendarTag_vtimezone:
      {
        mSink->CloseContainer(*attrNode);
      }
    break;

    case eCalICalendarTag_attach:
    case eCalICalendarTag_attendee:
    case eCalICalendarTag_calscale:
    case eCalICalendarTag_categories:
    case eCalICalendarTag_class:
    case eCalICalendarTag_comment:
    case eCalICalendarTag_completed:
    case eCalICalendarTag_contact:
    case eCalICalendarTag_created:
    case eCalICalendarTag_description:
    case eCalICalendarTag_dtend:
    case eCalICalendarTag_dtstart:
    case eCalICalendarTag_due:
    case eCalICalendarTag_duration:
    case eCalICalendarTag_exdate:
    case eCalICalendarTag_exrule:
    case eCalICalendarTag_freebusy:
    case eCalICalendarTag_geo:
    case eCalICalendarTag_last_modified:
    case eCalICalendarTag_location:
    case eCalICalendarTag_method:
    case eCalICalendarTag_organizer:
    case eCalICalendarTag_percent_complete:
    case eCalICalendarTag_priority:
    case eCalICalendarTag_prodid:
    case eCalICalendarTag_rdate:
    case eCalICalendarTag_rrule:
    case eCalICalendarTag_recurrence_id:
    case eCalICalendarTag_related_to:
    case eCalICalendarTag_repeat:
    case eCalICalendarTag_request_status:
    case eCalICalendarTag_resources:
    case eCalICalendarTag_sequence:
    case eCalICalendarTag_status:
    case eCalICalendarTag_summary:
    case eCalICalendarTag_transp:
    case eCalICalendarTag_trigger:
    case eCalICalendarTag_tzid:
    case eCalICalendarTag_tzname:
    case eCalICalendarTag_tzoffsetfrom:
    case eCalICalendarTag_tzoffsetto:
    case eCalICalendarTag_tzurl:
    case eCalICalendarTag_uid:
    case eCalICalendarTag_url:
    case eCalICalendarTag_version:
      break;
    default:
      break;
  }

  NS_RELEASE(attrNode);

  return result;
}

/*
eCalICalendarTags nsCalICalendarDTD::ComponentTypeFromObject(const nsIParserNode& aNode)
{
  PRInt32 i = 0;

  for (i = 0; i < aNode.GetAttributeCount(); i++) {

    nsString key = aNode.GetKeyAt(i);
    key.StripChars("\"");
    if (key.EqualsIgnoreCase(XPFC_STRING_CLASS)) {
      nsString value = aNode.GetValueAt(i);

      value.StripChars("\"");

      if (value.EqualsIgnoreCase("attach"))
        return (eCalICalendarTag_attach);
      // todo: finish
    }
  }
  return (eCalICalendarTag_unknown);
}
*/

NS_IMETHODIMP nsCalICalendarDTD::WillBuildModel(nsString& aFilename,PRInt32 aLevel) {
  nsresult result=NS_OK;
/*
  mFilename=aFilename;

  if((aNotifySink) && (mSink)) {
    mLineNumber=1;
    result = mSink->WillBuildModel();
  }
  */
  return result;
}

NS_IMETHODIMP nsCalICalendarDTD::DidBuildModel(PRInt32 anErrorCode,PRInt32 aLevel){
  nsresult result=NS_OK;
  return result;
}

void nsCalICalendarDTD::SetParser(nsIParser* aParser) {
  mParser=(nsParser*)aParser;
  if (mParser)
    mParseMode = aParser->GetParseMode();
}

nsIContentSink* nsCalICalendarDTD::SetContentSink(nsIContentSink* aSink) {
  nsIContentSink* old=mSink;
  mSink= aSink;
  return old;
}

// todo: consume token

nsresult nsCalICalendarDTD::WillResumeParse(void){
  nsresult result = NS_OK;
  if(mSink) {
    result = mSink->WillResume();
  }
  return result;
}

nsresult nsCalICalendarDTD::WillInterruptParse(void){
  nsresult result = NS_OK;
  if(mSink) {
    result = mSink->WillResume();
  }
  return result;
}

PRBool nsCalICalendarDTD::Verify(nsString& aURLRef){
  PRBool result=PR_TRUE;
  return result;
}

PRBool nsCalICalendarDTD::IsContainer(PRInt32 aTag) const{
  PRBool result=PR_FALSE;
  return result;
}

PRBool nsCalICalendarDTD::CanContain(PRInt32 aParent,PRInt32 aChild) const{
  PRBool result=PR_FALSE;
  return result;
}

nsITokenRecycler* nsCalICalendarDTD::GetTokenRecycler(void){
  return 0;
}

nsresult nsCalICalendarDTD::ConsumeToken(CToken*& aToken){
  aToken = 0;
  if(mTokenDeque.GetSize()>0)
  {
    aToken=(CToken*)mTokenDeque.Pop();
    return NS_OK;
  }
  CScanner* theScanner=mParser->GetScanner();

  PRUnichar aChar;
  nsresult result=theScanner->GetChar(aChar);
  switch(result) {
    case kEOF:
      break;
      
    case kInterrupted:
      theScanner->RewindToMark();
      break;

    case NS_OK:
    default:
      switch(aChar) {
        case kNotFound:
          break;
          
        case kCR: case kLF:
          result=ConsumeNewline(aChar,*theScanner,aToken);
          break;
        default:
          if (!nsString::IsSpace(aChar)) {
            result=ConsumePropertyLine(aChar,*theScanner,aToken);
            break;
          }
          result=ConsumeWhitespace(aChar,*theScanner,aToken);
          break;
      } //switch
  }
  //if(NS_OK==result)
  //  result=theScanner->Eof();
  return result;
}

nsresult 
nsCalICalendarDTD::ConsumePropertyValue(PRUnichar aChar, CScanner& aScanner,
                                        CToken*&aToken)
{
  PRInt32 theDequeSize=mTokenDeque.GetSize();
  nsresult result=NS_OK;

  CCalICalendarPropertyValueToken* theToken= (CCalICalendarPropertyValueToken*)
    gCalTokenRecycler.CreateTokenOfType(eCalICalendarToken_propertyvalue,
                                        eCalICalendarTag_unknown, gCalEmpty);
  
  //aToken=gCalTokenRecycler.CreateTokenOfType(eCalICalendarToken_propertyvalue,
  //                                           eCalICalendarTag_unknown, gCalEmpty);
  if(theToken) {
    result=theToken->Consume(aChar,aScanner);// parse property value (need to handle multiline)
    //todo: also handle base64 and quoted-printable
    if (NS_OK!=result) {
     delete theToken;
     aToken=0;
    }
    else
    {
      mTokenDeque.Push(theToken);
    }
  }
  return result;
}

nsresult
nsCalICalendarDTD::ConsumePropertyLine(PRUnichar aChar,CScanner& aScanner,CToken*& aToken) 
{
  nsresult result = NS_OK;
  result = ConsumePropertyNameAndAttributes(aChar, aScanner, aToken);
  if ((0 != aToken) && (NS_OK == result))
  {
    if (eCalICalendarTag_begin != aToken->GetTypeID() && eCalICalendarTag_end != aToken->GetTypeID())
    {
      result = ConsumePropertyValue(aChar, aScanner, aToken);
      if(NS_OK!=result) {
        delete aToken;
        aToken=0;
      }
    }//if (eCalICalendarTag_begin ...)
  }
  return result;
}

nsresult
nsCalICalendarDTD::ConsumePropertyNameAndAttributes(PRUnichar aChar, CScanner& aScanner,
                                                    CToken*& aToken)
{
  PRInt32 theDequeSize=mTokenDeque.GetSize();
  nsresult result=NS_OK;

  aToken=gCalTokenRecycler.CreateTokenOfType(eCalICalendarToken_begin,
                                             eCalICalendarTag_unknown, gCalEmpty);

  if(aToken) {
    // eats BEGIN, but not BEGIN: or BEGIN;
    result=aToken->Consume(aChar,aScanner);  //tell new token to finish consuming text...

    if (eCalICalendarTag_begin != aToken->GetTypeID() && eCalICalendarTag_end != aToken->GetTypeID())
    {
    
      if (NS_OK==result) {
        if (((CCalICalendarBeginToken*)aToken)->IsAttributed()) {
          result=ConsumeAttributes(aChar,aScanner,(CCalICalendarBeginToken*)aToken);
        }
        if (NS_OK!=result) {
          while(mTokenDeque.GetSize()>theDequeSize) {
            delete mTokenDeque.PopBack();
          }
        }
      } //if
    } //if
    if (eCalICalendarTag_end == aToken->GetTypeID())
    {
      nsString endval = aToken->GetStringValueXXX();
      delete aToken;
      aToken = 0;
      // 
      aToken=gCalTokenRecycler.CreateTokenOfType(eCalICalendarToken_end,
                                                 eCalICalendarTag_end, endval);
    }
  }// if (eCalICalendar_begin ...)
  return result;
}

nsresult
nsCalICalendarDTD::ConsumeWhitespace(PRUnichar aChar,
                                     CScanner& aScanner,
                                     CToken*& aToken) {
  aToken=gCalTokenRecycler.CreateTokenOfType(eCalICalendarToken_whitespace,
                                             eCalICalendarTag_whitespace,
                                             gCalEmpty);
  nsresult result=NS_OK;
  if (aToken) {
    result=aToken->Consume(aChar, aScanner);
  }
  return kNoError;
}

nsresult
nsCalICalendarDTD::ConsumeNewline(PRUnichar aChar,
                                  CScanner& aScanner,
                                  CToken*& aToken) {
  aToken=gCalTokenRecycler.CreateTokenOfType(eCalICalendarToken_newline,
                                             eCalICalendarTag_newline,
                                             gCalEmpty);
  nsresult result=NS_OK;
  if (aToken) {
    result=aToken->Consume(aChar, aScanner);
  }
  return kNoError;
}

nsresult
nsCalICalendarDTD::ConsumeAttributes(PRUnichar aChar,
                                     CScanner& aScanner,
                                     CCalICalendarBeginToken* aToken) {
  PRBool done=PR_FALSE;
  nsresult result=NS_OK;
  PRInt16 theAttrCount=0;

  while((!done) && (result==NS_OK)) {
    CCalICalendarAttributeToken* theToken= (CCalICalendarAttributeToken*)
      gCalTokenRecycler.CreateTokenOfType(eCalICalendarToken_attribute,
                                          eCalICalendarTag_unknown,gCalEmpty);
    if(theToken){
      // first eat leading semicolon
      result=aScanner.Peek(aChar);
      if(aChar=kSemiColon)
        aScanner.GetChar(aChar);

      result=theToken->Consume(aChar,aScanner); //tell new token to finish consuming text...

      if (NS_OK==result){
        theAttrCount++;
        mTokenDeque.Push(theToken);
      }//if
      else delete theToken; //we can't keep it..
    }//if
    if (NS_OK==result){
      result=aScanner.Peek(aChar);
      if(aChar==kColon) {
        aScanner.GetChar(aChar);
        done=PR_TRUE;
      }//if
    }//if
  }//while
  aToken->SetAttributeCount(theAttrCount);
  return result;
}








