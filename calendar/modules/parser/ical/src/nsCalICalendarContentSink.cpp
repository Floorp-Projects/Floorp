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


#include "jdefines.h"
#include "nsCalICalendarContentSink.h"
#include "nscalcoreicalCIID.h"
#include "nsICalendar.h"
#include "nsICalProperty.h"
#include "nsICalICalendarContainer.h"
#include "nsICalTimeBasedEvent.h"
#include "nsICalVEvent.h"
#include "nsIXPFCICalContentSink.h"
#include "nsxpfcCIID.h"
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
static NS_DEFINE_IID(kICalICalendarContainerIID, NS_ICALICALENDARCONTAINER_IID);

static NS_DEFINE_IID(kCCalICalendarVCalendarCID, NS_CALICALENDARVCALENDAR_CID);
//static NS_DEFINE_IID(kCCalICalendarVCalendarCID, NS_ICALENDAR_CID);
static NS_DEFINE_IID(kCCalICalendarVEventCID,    NS_CALICALENDARVEVENT_CID);
static NS_DEFINE_IID(kCCalICalendarVTodoCID,     NS_CALICALENDARVTODO_CID);
static NS_DEFINE_IID(kCCalICalendarVJournalCID,  NS_CALICALENDARVJOURNAL_CID);
static NS_DEFINE_IID(kCCalICalendarVFreebusyCID, NS_CALICALENDARVFREEBUSY_CID);
static NS_DEFINE_IID(kCCalICalendarVTimeZoneCID, NS_CALICALENDARVTIMEZONE_CID);
static NS_DEFINE_IID(kCCalICalendarVAlarmCID,    NS_CALICALENDARVALARM_CID);
static NS_DEFINE_IID(kICalendarIID, NS_ICALENDAR_IID);
static NS_DEFINE_IID(kICalTimeBasedEventIID, NS_ICALTIMEBASEDEVENT_IID);
static NS_DEFINE_IID(kCCalVEventCID, NS_CALICALENDARVEVENT_CID);
static NS_DEFINE_IID(kICalPropertyIID, NS_ICALPROPERTY_IID);
static NS_DEFINE_IID(kCCalStringPropertyCID, NS_CALSTRINGPROPERTY_CID);
static NS_DEFINE_IID(kCCalDateTimePropertyCID, NS_CALDATETIMEPROPERTY_CID);
static NS_DEFINE_IID(kCCalIntegerPropertyCID, NS_CALINTEGERPROPERTY_CID);

static NS_DEFINE_IID(kICalXPFCICalContentSinkIID, NS_IXPFC_ICAL_CONTENT_SINK_IID); 
static NS_DEFINE_IID(kIXPFCContentSinkIID, NS_IXPFC_CONTENT_SINK_IID); 

//static NS_DEFINE_IID(kCXPFCObserverManagerCID, NS_XPFC_OBSERVERMANAGER_CID);
//static NS_DEFINE_IID(kIXPFCObserverManagerIID, NS_IXPFC_OBSERVERMANAGER_IID);

nsCalICalendarContentSink::nsCalICalendarContentSink() 
{
  NS_INIT_REFCNT();
  static NS_DEFINE_IID(kCStackCID, NS_STACK_CID);
  nsresult res = nsRepository::CreateInstance(kCStackCID, nsnull,
                                      kCStackCID, (void **) &mXPFCStack);
  if (NS_OK != res)
    return;

  mXPFCStack->Init();

  mCalendarList = nsnull;
#if 0
  static NS_DEFINE_IID(kCArrayCID, NS_ARRAY_CID);

#ifdef NS_WIN32
  #define XPFC_DLL "xpfc10.dll"
#else
  #define XPFC_DLL "libxpfc10.so"
#endif

  static NS_DEFINE_IID(kCArrayIteratorCID, NS_ARRAY_ITERATOR_CID);

  nsRepository::RegisterFactory(kCArrayCID, XPFC_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCArrayIteratorCID, XPFC_DLL, PR_FALSE, PR_FALSE);

  if (nsnull == mCalendarList)
  {
    res = nsRepository::CreateInstance(kCArrayCID, nsnull,
                                       kCArrayCID, (void **)&mCalendarList);
    if (NS_OK == res)
    {    
      mCalendarList->Init();
    }
  }
#endif

}

nsCalICalendarContentSink::~nsCalICalendarContentSink() {
  NS_IF_RELEASE(mCalendarList);
}

NS_IMETHODIMP nsCalICalendarContentSink::Init() 
{
  nsresult res;

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
  else if (aIID.Equals(kIXPFCContentSinkIID)) {
    *aInstancePtr = (nsIXPFCContentSink*)(this);
  }
  else if (aIID.Equals(kICalICalendarContentSinkIID)) {
    *aInstancePtr = (nsICalICalendarContentSink*)(this);
  }
  else if (aIID.Equals(kICalXPFCICalContentSinkIID)) {
    *aInstancePtr = (nsIXPFCICalContentSink*)(this);
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

/*
nsresult nsCalICalendarContentSink::SetViewerContainer(nsIWebViewerContainer * aViewerContainer)
{
  mViewerContainer = aViewerContainer;
  return NS_OK;
}
*/

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

  res = nsRepository::CreateInstance(aclass, nsnull,
                                     kICalICalendarParserObjectIID,
                                     (void **)&object);
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
  
  nsISupports * container = (nsISupports *)mXPFCStack->Pop();
  
  nsISupports * parent = (nsISupports *) mXPFCStack->Top();
  nsresult res = NS_OK;

  nsICalendar * cal = nsnull;
  nsICalVEvent * event = nsnull;

  if (parent != nsnull && container != nsnull)
  {
    // todo: use ICalComponent instead
    res = container->QueryInterface(kCCalVEventCID, (void **) &event);
    if (NS_OK == res)
    {
      res = parent->QueryInterface(kICalendarIID, (void **) & cal);
      if (NS_OK == res)
      {
        if (event->IsValid())        
          cal->AddEvent(event);
      }
      //NS_RELEASE(cal);
    }
    NS_RELEASE(event);
  }
  else if (parent == nsnull && container != nsnull)
  {
    // todo: finish
    res = container->QueryInterface(kICalendarIID, (void **) &cal);
    if (NS_OK == res)
    {
      if (nsnull != mCalendarList)
      {
          mCalendarList->Append(cal);
          NS_ADDREF(cal);
      }
    }
  }

  NS_IF_RELEASE(container);
  return NS_OK;
}

NS_IMETHODIMP nsCalICalendarContentSink::AddLeaf(const nsIParserNode& aNode)
{
  nsICalICalendarParserObject * object = nsnull;
  nsICalICalendarContainer * container = nsnull;
  eCalICalendarTags tag = (eCalICalendarTags) aNode.GetNodeType();
  nsresult res;
  nsCID aclass;

  nsString text = aNode.GetText();
  res = CIDFromTag(tag, aclass);
  res = nsRepository::CreateInstance(aclass,
                                      nsnull,
                                      kICalICalendarParserObjectIID,
                                      (void **)&object);
  if (NS_OK == res)
  {
    nsICalICalendarContainer * parent = (nsICalICalendarContainer *) mXPFCStack->Top();
 
    if (nsnull != parent)
    {
      res = parent->QueryInterface(kICalICalendarContainerIID, (void **) &container);
      nsICalProperty * sp = nsnull;   
      
      if (res == NS_OK)
      {
        // it's a container
        res = object->QueryInterface(kICalPropertyIID, (void **) &sp);
        if (res == NS_OK)
          container->StoreProperty(tag, sp, 0);
        //NS_RELEASE(sp);
      }
    }
  }

#if 0
  AddToHierarchy(*object, PR_FALSE);
    
  object->Init();
    
  ConsumeAttributes(aNode, *object);
#endif
  //ConsumePropertyValues(aNode, *object);

  return NS_OK;
}

NS_IMETHODIMP nsCalICalendarContentSink::WillBuildModel(void)
{
  return NS_OK;
}

NS_IMETHODIMP nsCalICalendarContentSink::DidBuildModel(PRInt32 aQualityLevel)
{
#if 0
  static NS_DEFINE_IID(kXPFCSubjectIID, NS_IXPFC_SUBJECT_IID);
  static NS_DEFINE_IID(kXPFCObserverIID, NS_IXPFC_OBSERVER_IID);

  nsIIterator * iterator;
  mCalendarList->CreateIterator(&iterator);
  
  iterator->Init();

  nsCalendar * item;

  nsICalendar * cal;
  nsICalendar * cal2;

  nsIXPFCSubject * subject;
  nsIXPFCSubject * subject2;
  nsIXPFCObserver * observer;
  nsIXPFCObserver * observer2;
  
  nsIXPFCObserverManager * om;
  nsServiceManager::GetService(kCXPFCObserverManagerCID, kIXPFCObserverManagerIID, (nsISupports**)&om);

  while (!(iterator->IsDone()))
  {
    item = (nsCalendar *) iterator->CurrentItem();

    
    
  }
#endif
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
      
    case eCalICalendarTag_dtstart:
    case eCalICalendarTag_dtstamp:
    case eCalICalendarTag_dtend:
    case eCalICalendarTag_created:
    case eCalICalendarTag_last_modified:
      aClass = kCCalDateTimePropertyCID;
      break;

    case eCalICalendarTag_summary:
    case eCalICalendarTag_calscale:
    case eCalICalendarTag_method:
    case eCalICalendarTag_version:
    case eCalICalendarTag_status:
    case eCalICalendarTag_description:
    case eCalICalendarTag_transp:
    case eCalICalendarTag_location:
      aClass = kCCalStringPropertyCID;
      break;
  
    case eCalICalendarTag_sequence:
    case eCalICalendarTag_priority:
      aClass = kCCalIntegerPropertyCID;
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
  nsICalICalendarContainer * parent = (nsICalICalendarContainer *) mXPFCStack->Top();
  nsICalICalendarContainer * containerToPush = nsnull;
    
  nsresult res = aObject.QueryInterface(kICalICalendarContainerIID, (void **) &containerToPush);
  if (res == NS_OK)
  {
    if (aPush == PR_TRUE)
      mXPFCStack->Push(containerToPush);
    return NS_OK;
  }
  
  return NS_OK;
  /*
  nsICalICalendarContainer * parent = (nsICalICalendarContainer *) mXPFCStack->Top();

  nsICalICalendarContainer * containerToPush = nsnull;
    
  nsresult res = aObject.QueryInterface(kICalICalendarContainerIID, (void **) &containerToPush);
    
  if (NS_OK != res)
    return res;

  if (parent == nsnull)
  {
    //if (aPush == PR_TRUE)
    //  mXPFCStack->Push(containerToPush);
  }
  else
  {
    nsICalICalendarContainer * container = nsnull;

    res = parent->QueryInterface(kICalendarIID, (void **) &container);
    
    if (res == NS_OK)
    {
      // it's a calendar
      res = aObject.QueryInterface(kIParserNodeIID, (void **) &containerToPush);
      if (res == NS_OK)
      {
        
      }
      eCalICalendarTags tag = (eCalICalendarTags) aObject.GetNodeType();
      res = container->QueryInterface(kICalendar
      switch (tag)
      {
        case eCalICalendarTags_vevent:
          parent->AddVEvent(container);
        case eCalICalendarTags_version:
          parennt->SetVersion(container);
      }
      res = aObject.QueryInterface(kICalICalendarVEventIID, (void **)&container);
      if (res == NS_OK)
        parent->AddVEvent(container);
      
    }
    
    //if (aPush == PR_TRUE)
    //  mXPFCStack->Push(containerToPush);
  }

  if (mState == CALICALENDAR_PARSING_VCALENDAR)

  {
    nsICalendar * container = nsnull;
    nsICalendar * parent = nsnull;
    
    nsresult res = aObject.QueryInterface(kCCalendarIID, (void **) &container);
    
    parent = (nsICalendar *) mXPFCStack->Top();

    if (parent == nsnull)
    {
      if (aPush == PR_TRUE)
        mXPFCStack->Push(container);
    }
    else
    {

    }
  }
  return NS_OK;
  */
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

nsresult nsCalICalendarContentSink::SetContentSinkContainer(nsISupports * 
                                                            aContentSinkContainer)
{
  nsresult res;
  nsIArray * array;
  static NS_DEFINE_IID(kCArrayCID, NS_ARRAY_CID);
  res = aContentSinkContainer->QueryInterface(kCArrayCID, (void **)&array);
  if (NS_OK == res)
  {
    res = SetCalendarList(array);
    NS_RELEASE(array);
  }
  return res;
}

nsresult nsCalICalendarContentSink::SetViewerContainer(nsIWebViewerContainer * aViewerContainer)
{
  mViewerContainer = aViewerContainer;
  return NS_OK;
}

nsresult nsCalICalendarContentSink::SetCalendarList(nsIArray * aCalendarList)
{
  mCalendarList = aCalendarList;
  NS_ADDREF(aCalendarList);
  return NS_OK;
}
