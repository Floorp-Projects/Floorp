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

#include "nsCalICalendarContentSink.h"
#include "nscalcoreicalCIID.h"
//#include "nsCalICalendarTokens.h"
//#include "nsCalICalendarDTD.h"
//#include "nsCalICalendarParserCIID.h"
//#include "nsICalICalendarParserObject.h"
//#include "nsICalendarShell.h"
//#include "nsCalParserCIID.h"
//#include "nsxpfcCIID.h"

//
//#include "vevent.h"
//#include "nscal.h"
//#include "vtodo.h"
//#include "vfrbsy.h"
//#include "vtimezne.h"
//#include "valarm.h"
//

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIContentSinkIID, NS_ICONTENT_SINK_IID);
static NS_DEFINE_IID(kICalICalendarContentSinkIID, NS_ICALICALENDAR_CONTENT_SINK_IID);
static NS_DEFINE_IID(kICalICalendarParserObjectIID, NS_ICALICALENDAR_PARSER_OBJECT_IID);

static NS_DEFINE_IID(kCCalICalendarVCalendarCID, NS_CALICALENDARVCALENDAR_CID);
static NS_DEFINE_IID(kCCalICalendarVEventCID,    NS_CALICALENDARVEVENT_CID);
static NS_DEFINE_IID(kCCalICalendarVTodoCID,     NS_CALICALENDARVTODO_CID);
static NS_DEFINE_IID(kCCalICalendarVJournalCID,  NS_CALICALENDARVJOURNAL_CID);
static NS_DEFINE_IID(kCCalICalendarVFreebusyCID, NS_CALICALENDARVFREEBUSY_CID);
static NS_DEFINE_IID(kCCalICalendarVTimeZoneCID, NS_CALICALENDARVTIMEZONE_CID);
static NS_DEFINE_IID(kCCalICalendarVAlarmCID,    NS_CALICALENDARVALARM_CID);

nsCalICalendarContentSink::nsCalICalendarContentSink() {
}

nsCalICalendarContentSink::~nsCalICalendarContentSink() {
}

NS_IMETHODIMP nsCalICalendarContentSink::Init() {
  return NS_OK;
}

nsresult nsCalICalendarContentSink::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (nsIContentSink*)(this);
  }
  else if (aIID.Equals(kIContentSinkIID)) {
    *aInstancePtr = (nsIContentSink*)(this);
  }
  else if (aIID.Equals(kICalICalendarContentSinkIID)) {
    *aInstancePtr = (nsICalICalendarContentSink*)(this);
  }
  else {
    *aInstancePtr=0;
    return NS_NOINTERFACE;
  }
  NS_ADDREF_THIS();
  return NS_OK;
}

NS_IMPL_ADDREF(nsCalICalendarContentSink);
NS_IMPL_RELEASE(nsCalICalendarContentSink);

nsresult nsCalICalendarContentSink::SetViewerContainer(nsIWebViewerContainer * aViewerContainer)
{
  mViewerContainer = aViewerContainer;
  return NS_OK;
}

NS_IMETHODIMP nsCalICalendarContentSink::OpenContainer(const nsIParserNode& aNode)
{
  nsICalICalendarParserObject * object = nsnull;
  eCalICalendarTags tag = (eCalICalendarTags) aNode.GetNodeType();
  nsresult res;
  nsCID aclass;

  nsString tex = aNode.GetText();
  res = CIDFromTag(tag, aclass);

  if (NS_OK != res)
    return res;

  res = nsRepository::CreateInstance(aclass, nsnull, kICalICalendarParserObjectIID,
                                     (void **) &object);

  if (NS_OK != res)
    return res;

  AddToHierarchy(*object, PR_TRUE);
  object->Init();
  ConsumeAttributes(aNode,*object);
  // todo: finish
  return NS_OK;
}

NS_IMETHODIMP nsCalICalendarContentSink::CloseContainer(const nsIParserNode& aNode)
{
  //nsISupports * container = (nsISupports *)mXPFCStack->Pop();
  
  // todo: finish
  
  //NS_IF_RELEASE(container);
  return NS_OK;
}

NS_IMETHODIMP nsCalICalendarContentSink::AddLeaf(const nsIParserNode& aNode)
{
  nsICalICalendarParserObject * object = nsnull;
  eCalICalendarTags tag = (eCalICalendarTags) aNode.GetNodeType();
  nsresult res;
  nsCID aclass;

  nsString text = aNode.GetText();
  res = CIDFromTag(tag, aclass);
  res = nsRepository::CreateInstance(aclass,
                                      nsnull,
                                      kICalICalendarParserObjectIID,
                                      (void **)&object);
  if (NS_OK != res)
    return res;

  AddToHierarchy(*object, PR_FALSE);

  object->Init();

  ConsumeAttributes(aNode, *object);

  //ConsumePropertyValues(aNode, *object);

  return NS_OK;
}

NS_IMETHODIMP nsCalICalendarContentSink::WillBuildModel(void)
{
  return NS_OK;
}

NS_IMETHODIMP nsCalICalendarContentSink::DidBuildModel(PRInt32 aQualityLevel)
{
  return NS_OK;
}

NS_IMETHODIMP nsCalICalendarContentSink::WillInterrupt(void)
{
  return NS_OK;
}

NS_IMETHODIMP nsCalICalendarContentSink::WillResume(void)
{
  return NS_OK;
}


/*
 * Find the CID from the XML Tag Object
 */

NS_IMETHODIMP nsCalICalendarContentSink::CIDFromTag(eCalICalendarTags tag, nsCID &aClass)
{
  switch(tag)
  {
    case eCalICalendarTag_vcalendar:
      aClass = kCCalICalendarVCalendarCID;
      break;

    case eCalICalendarTag_vevent:
      aClass = kCCalICalendarVEventCID;
      break;

    case eCalICalendarTag_vtodo:
      aClass = kCCalICalendarVTodoCID;
      break;

    case eCalICalendarTag_vjournal:
      aClass = kCCalICalendarVJournalCID;
      break;

    case eCalICalendarTag_vfreebusy:
      aClass = kCCalICalendarVFreebusyCID;
      break;

    case eCalICalendarTag_vtimezone:
      aClass = kCCalICalendarVTimeZoneCID;
      break;

    case eCalICalendarTag_valarm:
      aClass = kCCalICalendarVAlarmCID;
      break;
      
    default:
      return (NS_ERROR_UNEXPECTED);
      break;
  }


  return NS_OK;
}

NS_IMETHODIMP nsCalICalendarContentSink::ConsumeAttributes(const nsIParserNode& aNode,
                                                           nsICalICalendarParserObject& aObject)
{
  PRInt32 i = 0;
  nsString key,value;
  nsString scontainer;
  //PRBool container;

  for (i = 0; i < aNode.GetAttributeCount(); i++) {
    key = aNode.GetKeyAt(i);
    value = aNode.GetValueAt(i);

    key.StripChars("\"");
    value.StripChars("\"");

    aObject.SetParameter(key,value);
  }

  eCalICalendarTags tag = (eCalICalendarTags) aNode.GetNodeType();

  // todo: finish

  return NS_OK;
}

/*
NS_IMETHODIMP nsCalICalendarContentSink::ConsumePropertyValues(const nsIParserNode& aNode,
                                                              nsICalICalendarParserObject& aObject)
{
  PRInt32 i = 0;
  nsString key,value;
  nsString scontainer;
  //PRBool container;

  for (i = 0; i < aNode.GetAttributeCount(); i++) {
    key = aNode.GetKeyAt(i);
    value = aNode.GetValueAt(i);

    key.StripChars("\"");
    value.StripChars("\"");

    aObject.SetParameter(key,value);
  }

  eCalICalendarTags tag = (eCalICalendarTags) aNode.GetNodeType();

  // todo: finish

  return NS_OK;
}
*/

NS_IMETHODIMP nsCalICalendarContentSink::AddToHierarchy(nsICalICalendarParserObject& aObject,
                                                        PRBool aPush)
{
  return NS_OK;
}

PRBool nsCalICalendarContentSink::IsContainer(const nsIParserNode& aNode)
{
  PRInt32 i = 0;
  nsString key;
  PRBool container = PR_FALSE;

  for (i = 0; i < aNode.GetAttributeCount(); i++) {

    key = aNode.GetKeyAt(i);
    key.StripChars("\"");

    // todo: finish
  }

  return container;
}
