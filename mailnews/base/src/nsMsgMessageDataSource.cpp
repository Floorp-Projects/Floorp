/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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

#include "msgCore.h"    // precompiled header...

#include "nsMsgMessageDataSource.h"
//#include "prlog.h"
#include "nsIRDFService.h"
//#include "nsIServiceManager.h"
#include "nsRDFCID.h"
#include "rdf.h"
//#include "nsIRDFResourceFactory.h"
//#include "nsIRDFObserver.h"
//#include "nsIRDFNode.h"
//#include "plstr.h"
//#include "prmem.h"
//#include "prio.h"
//#include "prprf.h"
//#include "nsString.h"
//#include "nsIMsgFolder.h"
//#include "nsISupportsArray.h"
//#include "nsFileSpec.h"
//#include "nsMsgFolderFlags.h"
#include "nsRDFCursorUtils.h"
#include "nsIMessage.h"
//#include "nsMsgFolder.h"
//#include "nsIMsgHeaderParser.h"
//#include "nsMsgBaseCID.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"

// this is used for notification of observers using nsVoidArray
typedef struct _nsMsgRDFNotification {
  nsIRDFResource *subject;
  nsIRDFResource *property;
  nsIRDFNode *object;
} nsMsgRDFNotification;
                                                


static NS_DEFINE_CID(kRDFServiceCID,              NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kMsgHeaderParserCID,			NS_MSGHEADERPARSER_CID); 

// we need this because of an egcs 1.0 (and possibly gcc) compiler bug
// that doesn't allow you to call ::nsISupports::GetIID() inside of a class
// that multiply inherits from nsISupports
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIRDFCursorIID, NS_IRDFCURSOR_IID);


nsIRDFResource* nsMsgMessageDataSource::kNC_Subject;
nsIRDFResource* nsMsgMessageDataSource::kNC_Sender;
nsIRDFResource* nsMsgMessageDataSource::kNC_Date;
nsIRDFResource* nsMsgMessageDataSource::kNC_Status;


#define NC_NAMESPACE_URI "http://home.netscape.com/NC-rdf#"

DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Subject);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Sender);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Date);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Status);


////////////////////////////////////////////////////////////////////////
// Utilities

static PRBool
peq(nsIRDFResource* r1, nsIRDFResource* r2)
{
  PRBool result;
  if (NS_SUCCEEDED(r1->EqualsResource(r2, &result)) && result) {
    return PR_TRUE;
  } else {
    return PR_FALSE;
  }
}

static PRBool
peqSort(nsIRDFResource* r1, nsIRDFResource* r2, PRBool *isSort)
{
	if(!isSort)
		return PR_FALSE;

	char *r1Str, *r2Str;
	nsString r1nsStr, r2nsStr, r1nsSortStr;

	r1->GetValue(&r1Str);
	r2->GetValue(&r2Str);

	r1nsStr = r1Str;
	r2nsStr = r2Str;
	r1nsSortStr = r1Str;

	delete[] r1Str;
	delete[] r2Str;

	//probably need to not assume this will always come directly after property.
	r1nsSortStr +="?sort=true";

	if(r1nsStr == r2nsStr)
	{
		*isSort = PR_FALSE;
		return PR_TRUE;
	}
	else if(r1nsSortStr == r2nsStr)
	{
		*isSort = PR_TRUE;
		return PR_TRUE;
	}
  else
	{
		//In case the resources are equal but the values are different.  I'm not sure if this
		//could happen but it is feasible given interface.
		*isSort = PR_FALSE;
		return(peq(r1, r2));
	}
}

void nsMsgMessageDataSource::createNode(nsString& str, nsIRDFNode **node) const
{
	nsIRDFLiteral * value;
	*node = nsnull;
	if(NS_SUCCEEDED(mRDFService->GetLiteral((const PRUnichar*)str, &value))) {
		*node = value;
	}
}

void nsMsgMessageDataSource::createNode(PRUint32 value, nsIRDFNode **node) const
{
	char *valueStr = PR_smprintf("%d", value);
	nsString str(valueStr);
	createNode(str, node);
	PR_smprintf_free(valueStr);
}

nsMsgMessageDataSource::nsMsgMessageDataSource():
  mURI(nsnull),
  mObservers(nsnull),
  mInitialized(PR_FALSE),
  mRDFService(nsnull),
  mHeaderParser(nsnull)
{
  NS_INIT_REFCNT();

  nsresult rv = nsServiceManager::GetService(kRDFServiceCID,
                                             nsIRDFService::GetIID(),
                                             (nsISupports**) &mRDFService); // XXX probably need shutdown listener here

	rv = nsComponentManager::CreateInstance(kMsgHeaderParserCID, 
                                          NULL, 
                                          nsIMsgHeaderParser::GetIID(), 
                                          (void **) &mHeaderParser);

  PR_ASSERT(NS_SUCCEEDED(rv));
}

nsMsgMessageDataSource::~nsMsgMessageDataSource (void)
{
  mRDFService->UnregisterDataSource(this);

  PL_strfree(mURI);
  if (mObservers) {
      for (PRInt32 i = mObservers->Count() - 1; i >= 0; --i) {
          nsIRDFObserver* obs = (nsIRDFObserver*) mObservers->ElementAt(i);
          NS_RELEASE(obs);
      }
      delete mObservers;
  }
  nsrefcnt refcnt;

  NS_RELEASE2(kNC_Subject, refcnt);
  NS_RELEASE2(kNC_Sender, refcnt);
  NS_RELEASE2(kNC_Date, refcnt);
  NS_RELEASE2(kNC_Status, refcnt);

  nsServiceManager::ReleaseService(kRDFServiceCID, mRDFService); // XXX probably need shutdown listener here
  NS_IF_RELEASE(mHeaderParser);
  mRDFService = nsnull;
}


NS_IMPL_ADDREF(nsMsgMessageDataSource)
NS_IMPL_RELEASE(nsMsgMessageDataSource)

NS_IMETHODIMP
nsMsgMessageDataSource::QueryInterface(REFNSIID iid, void** result)
{
  if (! result)
    return NS_ERROR_NULL_POINTER;

  *result = nsnull;
  if (iid.Equals(nsIRDFDataSource::GetIID()) ||
    iid.Equals(nsIRDFDataSource::GetIID()) ||
      iid.Equals(kISupportsIID))
  {
    *result = NS_STATIC_CAST(nsIRDFDataSource*, this);
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

 // nsIRDFDataSource methods
NS_IMETHODIMP nsMsgMessageDataSource::Init(const char* uri)
{
  if (mInitialized)
      return NS_ERROR_ALREADY_INITIALIZED;

  if ((mURI = PL_strdup(uri)) == nsnull)
      return NS_ERROR_OUT_OF_MEMORY;

  mRDFService->RegisterDataSource(this, PR_FALSE);

  if (! kNC_Subject) {
    
	mRDFService->GetResource(kURINC_Subject, &kNC_Subject);
	mRDFService->GetResource(kURINC_Sender, &kNC_Sender);
    mRDFService->GetResource(kURINC_Date, &kNC_Date);
    mRDFService->GetResource(kURINC_Status, &kNC_Status);
    
  }
  mInitialized = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP nsMsgMessageDataSource::GetURI(char* *uri)
{
  if ((*uri = nsXPIDLCString::Copy(mURI)) == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;
  else
    return NS_OK;
}

NS_IMETHODIMP nsMsgMessageDataSource::GetSource(nsIRDFResource* property,
                                               nsIRDFNode* target,
                                               PRBool tv,
                                               nsIRDFResource** source /* out */)
{
  PR_ASSERT(0);
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgMessageDataSource::GetTarget(nsIRDFResource* source,
                                               nsIRDFResource* property,
                                               PRBool tv,
                                               nsIRDFNode** target)
{
	nsresult rv = NS_RDF_NO_VALUE;

	// we only have positive assertions in the mail data source.
	if (! tv)
		return NS_RDF_NO_VALUE;

	nsIMessage *message;
	rv = source->QueryInterface(nsIMessage::GetIID(),
								(void **)&message);
	if (NS_SUCCEEDED(rv)) {
		rv = createMessageNode(message, property,target);
		NS_RELEASE(message);
	}
	else
		return NS_RDF_NO_VALUE;
  
  return rv;
}

//sender is the string we need to parse.  senderuserName is the parsed user name we get back.
nsresult nsMsgMessageDataSource::GetSenderName(nsAutoString& sender, nsAutoString *senderUserName)
{
	//XXXOnce we get the csid, use Intl version
	nsresult rv = NS_OK;
	if(mHeaderParser)
	{
		char *name;
		char *senderStr = sender.ToNewCString();
		if(NS_SUCCEEDED(rv = mHeaderParser->ExtractHeaderAddressName (nsnull, senderStr, &name)))
		{
			*senderUserName = name;
		}
		if(name)
			PL_strfree(name);
		if(senderStr)
			delete[] senderStr;
	}
	return rv;
}

NS_IMETHODIMP nsMsgMessageDataSource::GetSources(nsIRDFResource* property,
                                                nsIRDFNode* target,
                                                PRBool tv,
                                                nsIRDFAssertionCursor** sources)
{
  PR_ASSERT(0);
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgMessageDataSource::GetTargets(nsIRDFResource* source,
                                                nsIRDFResource* property,    
                                                PRBool tv,
                                                nsIRDFAssertionCursor** targets)
{
	nsresult rv = NS_RDF_NO_VALUE;

	nsIMessage* message;
	if (NS_SUCCEEDED(source->QueryInterface(nsIMessage::GetIID(), (void**)&message))) {
		if(peq(kNC_Subject, property) || peq(kNC_Date, property) ||
			peq(kNC_Status, property))
		{
			nsRDFSingletonAssertionCursor* cursor =
				new nsRDFSingletonAssertionCursor(this, source, property, PR_FALSE);
			if (cursor == nsnull)
				return NS_ERROR_OUT_OF_MEMORY;
			NS_ADDREF(cursor);
			*targets = cursor;
			rv = NS_OK;
		}
		NS_IF_RELEASE(message);
	}
	else {
	  //create empty cursor
	  nsISupportsArray *assertions;
	  NS_NewISupportsArray(&assertions);
	  nsRDFArrayAssertionCursor* cursor = 
		  new nsRDFArrayAssertionCursor(this,
							  source, property, 
							  assertions);
	  if(cursor == nsnull)
		  return NS_ERROR_OUT_OF_MEMORY;
	  NS_ADDREF(cursor);
	  *targets = cursor;
	  rv = NS_OK;
	}
	return rv;
}

NS_IMETHODIMP nsMsgMessageDataSource::Assert(nsIRDFResource* source,
                      nsIRDFResource* property, 
                      nsIRDFNode* target,
                      PRBool tv)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgMessageDataSource::Unassert(nsIRDFResource* source,
                        nsIRDFResource* property,
                        nsIRDFNode* target)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP nsMsgMessageDataSource::HasAssertion(nsIRDFResource* source,
                            nsIRDFResource* property,
                            nsIRDFNode* target,
                            PRBool tv,
                            PRBool* hasAssertion)
{
  *hasAssertion = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsMsgMessageDataSource::AddObserver(nsIRDFObserver* n)
{
  if (! mObservers) {
    if ((mObservers = new nsVoidArray()) == nsnull)
      return NS_ERROR_OUT_OF_MEMORY;
  }
  mObservers->AppendElement(n);
  return NS_OK;
}

NS_IMETHODIMP nsMsgMessageDataSource::RemoveObserver(nsIRDFObserver* n)
{
  if (! mObservers)
    return NS_OK;
  mObservers->RemoveElement(n);
  return NS_OK;
}


NS_IMETHODIMP nsMsgMessageDataSource::ArcLabelsIn(nsIRDFNode* node,
                                                 nsIRDFArcsInCursor** labels)
{
  PR_ASSERT(0);
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgMessageDataSource::ArcLabelsOut(nsIRDFResource* source,
                                                  nsIRDFArcsOutCursor** labels)
{
  nsISupportsArray *arcs=nsnull;
  nsresult rv = NS_RDF_NO_VALUE;
  
  //  if (arcs == nsnull)
  //    return NS_ERROR_OUT_OF_MEMORY;

  nsIMessage* message;
  if (NS_SUCCEEDED(source->QueryInterface(nsIMessage::GetIID(),
                                               (void**)&message))) {
    fflush(stdout);
    rv = getMessageArcLabelsOut(message, &arcs);
    NS_RELEASE(message);
  } else {
    // how to return an empty cursor?
    // for now return a 0-length nsISupportsArray
    NS_NewISupportsArray(&arcs);
  }

  nsRDFArrayArcsOutCursor* cursor =
    new nsRDFArrayArcsOutCursor(this, source, arcs);
  NS_RELEASE(arcs);
  
  if (cursor == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(cursor);
  *labels = cursor;
  
  return NS_OK;
}

nsresult
nsMsgMessageDataSource::getMessageArcLabelsOut(nsIMessage *folder,
                                              nsISupportsArray **aArcs)
{
  nsISupportsArray *arcs;
  NS_NewISupportsArray(&arcs);
  arcs->AppendElement(kNC_Subject);
  arcs->AppendElement(kNC_Sender);
  arcs->AppendElement(kNC_Date);
	arcs->AppendElement(kNC_Status);
  *aArcs = arcs;
  return NS_OK;
}


NS_IMETHODIMP
nsMsgMessageDataSource::GetAllResources(nsIRDFResourceCursor** aCursor)
{
  NS_NOTYETIMPLEMENTED("sorry!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgMessageDataSource::Flush()
{
  PR_ASSERT(0);
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMsgMessageDataSource::GetAllCommands(nsIRDFResource* source,
                                      nsIEnumerator/*<nsIRDFResource>*/** commands)
{
  nsresult rv;

  nsIMessage* message;
  nsISupportsArray* cmds = nsnull;

  if (NS_SUCCEEDED(source->QueryInterface(nsIMessage::GetIID(), (void**)&message))) {
    NS_RELEASE(message);       // release now that we know it's a message
    rv = NS_NewISupportsArray(&cmds);
    if (NS_FAILED(rv)) return rv;
  }

  if (cmds != nsnull)
    return cmds->Enumerate(commands);
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMsgMessageDataSource::IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                        nsIRDFResource*   aCommand,
                                        nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                                        PRBool* aResult)
{
  nsIMessage* message;

  PRUint32 cnt = aSources->Count();
  for (PRUint32 i = 0; i < cnt; i++) {
    nsCOMPtr<nsISupports> source = dont_QueryInterface((*aSources)[i]);
	if (NS_SUCCEEDED(source->QueryInterface(nsIMessage::GetIID(), (void**)&message))) {
      NS_RELEASE(message);       // release now that we know it's a message

      // we don't care about the arguments -- message commands are always enabled
        *aResult = PR_FALSE;
        return NS_OK;
    }
  }
  *aResult = PR_TRUE;
  return NS_OK; // succeeded for all sources
}

NS_IMETHODIMP
nsMsgMessageDataSource::DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                 nsIRDFResource*   aCommand,
                                 nsISupportsArray/*<nsIRDFResource>*/* aArguments)
{
  nsresult rv = NS_OK;

  // XXX need to handle batching of command applied to all sources

  PRUint32 cnt = aSources->Count();
  for (PRUint32 i = 0; i < cnt; i++) {
    nsISupports* source = (*aSources)[i];
    nsIMessage* message;
	if (NS_SUCCEEDED(source->QueryInterface(nsIMessage::GetIID(), (void**)&message))) {


      NS_RELEASE(message);
    }
  }
  return rv;
}

nsresult
nsMsgMessageDataSource::createMessageNode(nsIMessage *message,
                                         nsIRDFResource *property,
                                         nsIRDFNode **target)
{
		PRBool sort;
    if (peqSort(kNC_Subject, property, &sort))
      return createMessageNameNode(message, sort, target);
    else if (peqSort(kNC_Sender, property, &sort))
      return createMessageSenderNode(message, sort, target);
    else if (peq(kNC_Date, property))
      return createMessageDateNode(message, target);
		else if (peq(kNC_Status, property))
      return createMessageStatusNode(message, target);
    else
      return NS_RDF_NO_VALUE;
}


nsresult
nsMsgMessageDataSource::createMessageNameNode(nsIMessage *message,
                                             PRBool sort,
                                             nsIRDFNode **target)
{
  nsresult rv = NS_OK;
  nsAutoString subject;
  if(sort)
    {
      message->GetSubjectCollationKey(subject);
    }
  else
    {
      rv = message->GetMime2EncodedSubject(subject);
      PRUint32 flags;
      message->GetFlags(&flags);
      if(flags & MSG_FLAG_HAS_RE)
				{
					nsAutoString reStr="Re: ";
					reStr +=subject;
					subject = reStr;
				}
    }
  createNode(subject, target);
  return rv;
}


nsresult
nsMsgMessageDataSource::createMessageSenderNode(nsIMessage *message,
                                               PRBool sort,
                                               nsIRDFNode **target)
{
  nsresult rv = NS_OK;
  nsAutoString sender, senderUserName;
  if(sort)
    {
      message->GetAuthorCollationKey(sender);
      createNode(sender, target);
    }
  else
    {
      rv = message->GetMime2EncodedAuthor(sender);
      if(NS_SUCCEEDED(rv = GetSenderName(sender, &senderUserName)))
        createNode(senderUserName, target);
    }
  return rv;
}

nsresult
nsMsgMessageDataSource::createMessageDateNode(nsIMessage *message,
                                             nsIRDFNode **target)
{
  nsAutoString date;
  nsresult rv = message->GetProperty("date", date);
  PRInt32 error;
  time_t time = date.ToInteger(&error, 16);
  struct tm* tmTime = localtime(&time);
  char dateBuf[100];
  strftime(dateBuf, 100, "%m/%d/%Y %I:%M %p", tmTime);
  date = dateBuf;
  createNode(date, target);
  return rv;
}

nsresult
nsMsgMessageDataSource::createMessageStatusNode(nsIMessage *message,
                                               nsIRDFNode **target)
{
  PRUint32 flags;
  message->GetFlags(&flags);
  nsAutoString flagStr = "";
  if(flags & MSG_FLAG_REPLIED)
    flagStr = "replied";
  else if(flags & MSG_FLAG_FORWARDED)
    flagStr = "forwarded";
  else if(flags & MSG_FLAG_NEW)
    flagStr = "new";
  else if(flags & MSG_FLAG_READ)
    flagStr = "read";
  createNode(flagStr, target);
  return NS_OK;
}
  
