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

#include "nsCalXMLContentSink.h"
#include "nsHTMLTokens.h"
#include "nsCalXMLDTD.h"
#include "nsIXMLParserObject.h"
#include "nsCalUICIID.h"
#include "nsXPFCCanvas.h"
#include "nsCalTimebarUserHeading.h"
#include "nsCalTimebarContextController.h"
#include "nsCalTodoComponentCanvas.h"
#include "nsCalMonthContextController.h"
#include "nsCalMultiDayViewCanvas.h"
#include "nsCalMultiUserViewCanvas.h"
#include "nsCalCommandCanvas.h"
#include "nsXPFCHTMLCanvas.h"
#include "nsICalendarShell.h"
#include "nsCalParserCIID.h"
#include "nsxpfcCIID.h"
#include "nsCalendarContainer.h"
#include "nsXPFCToolkit.h"
#include "nsIServiceManager.h"
#include "nsxpfcCIID.h"
#include "nsIXPFCObserverManager.h"

static NS_DEFINE_IID(kISupportsIID,             NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kIContentSinkIID,          NS_ICONTENT_SINK_IID);
static NS_DEFINE_IID(kClassIID,                 NS_CALXMLCONTENTSINK_IID); 
static NS_DEFINE_IID(kIHTMLContentSinkIID,      NS_IHTML_CONTENT_SINK_IID);
static NS_DEFINE_IID(kIXPFCCanvasIID,           NS_IXPFC_CANVAS_IID);
static NS_DEFINE_IID(kCXPFCCanvasCID,           NS_XPFC_CANVAS_CID);
static NS_DEFINE_IID(kCXPFCHTMLCanvasCID,       NS_XPFC_HTML_CANVAS_CID);
static NS_DEFINE_IID(kIXMLParserObjectIID,      NS_IXML_PARSER_OBJECT_IID);
static NS_DEFINE_IID(kCCalTimebarContextControllerCID, NS_CAL_TIMEBAR_CONTEXT_CONTROLLER_CID);
static NS_DEFINE_IID(kCCalMonthContextControllerCID, NS_CAL_MONTH_CONTEXT_CONTROLLER_CID);
static NS_DEFINE_IID(kCCalTodoComponentCanvasCID, NS_CAL_TODOCOMPONENTCANVAS_CID);
static NS_DEFINE_IID(kCCalMultiDayViewCanvasCID, NS_CAL_MULTIDAYVIEWCANVAS_CID);
static NS_DEFINE_IID(kCCalMultiUserViewCanvasCID, NS_CAL_MULTIUSERVIEWCANVAS_CID);
static NS_DEFINE_IID(kCalMonthViewCanvasCID, NS_CAL_MONTHVIEWCANVAS_CID);
static NS_DEFINE_IID(kCCalCommandCanvasCID, NS_CAL_COMMANDCANVAS_CID);
static NS_DEFINE_IID(kCalTimebarUserHeadingCID,     NS_CAL_TIMEBARUSERHEADING_CID);
static NS_DEFINE_IID(kCalTimebarScaleCID,     NS_CAL_TIMEBARSCALE_CID);
static NS_DEFINE_IID(kCalTodoComponentCanvasCID,     NS_CAL_TODOCOMPONENTCANVAS_CID);
static NS_DEFINE_IID(kCalCommandCanvasCID,     NS_CAL_COMMANDCANVAS_CID);
static NS_DEFINE_IID(kCalMultiDayViewCanvasCID,     NS_CAL_MULTIDAYVIEWCANVAS_CID);
static NS_DEFINE_IID(kCalMultiUserViewCanvasCID,     NS_CAL_MULTIUSERVIEWCANVAS_CID);
static NS_DEFINE_IID(kCalTimebarCanvasCID, NS_CAL_TIMEBARCANVAS_CID);

static NS_DEFINE_IID(kCalContextcontrollerIID, NS_ICAL_CONTEXT_CONTROLLER_IID);
static NS_DEFINE_IID(kIXPFCXMLContentSinkIID,  NS_IXPFC_XML_CONTENT_SINK_IID); 
static NS_DEFINE_IID(kCXPFolderCanvas,         NS_XP_FOLDER_CANVAS_CID);
static NS_DEFINE_IID(kCXPItem,                 NS_XP_ITEM_CID);

static NS_DEFINE_IID(kCXPFCObserverManagerCID, NS_XPFC_OBSERVERMANAGER_CID);
static NS_DEFINE_IID(kIXPFCObserverManagerIID, NS_IXPFC_OBSERVERMANAGER_IID);


class ControlListEntry {
public:
  nsIXMLParserObject * object;
  nsString control;

  ControlListEntry(nsIXMLParserObject * aObject, 
            nsString  aControl) { 
    object = aObject;
    control = aControl;
  }
  ~ControlListEntry() {
  }
};


nsCalXMLContentSink::nsCalXMLContentSink() : nsIHTMLContentSink()
{
  mCalendarContainer = nsnull;
  mControlList = nsnull;

  NS_INIT_REFCNT();

  static NS_DEFINE_IID(kCVectorCID, NS_ARRAY_CID);

  nsresult res = nsRepository::CreateInstance(kCVectorCID, 
                                              nsnull, 
                                              kCVectorCID, 
                                              (void **)&mTimeContextList);

  if (NS_OK != res)
    return ;

  mTimeContextList->Init();


  res = nsRepository::CreateInstance(kCVectorCID, 
                                     nsnull, 
                                     kCVectorCID, 
                                     (void **)&mOrphanCanvasList);

  if (NS_OK != res)
    return ;

  mOrphanCanvasList->Init();

  static NS_DEFINE_IID(kCStackCID, NS_STACK_CID);

  res = nsRepository::CreateInstance(kCStackCID, 
                                     nsnull, 
                                     kCStackCID, 
                                     (void **)&mCanvasStack);

  if (NS_OK != res)
    return ;

  mCanvasStack->Init();

  if (mControlList == nsnull) {

    static NS_DEFINE_IID(kCVectorIteratorCID, NS_ARRAY_ITERATOR_CID);
    static NS_DEFINE_IID(kCVectorCID, NS_ARRAY_CID);

    nsresult res = nsRepository::CreateInstance(kCVectorCID, 
                                       nsnull, 
                                       kCVectorCID, 
                                       (void **)&mControlList);

    if (NS_OK != res)
      return ;

    mControlList->Init();
  }


}

nsCalXMLContentSink::~nsCalXMLContentSink() 
{

  if (mOrphanCanvasList != nsnull) 
  {

    nsIIterator * iterator;

    mOrphanCanvasList->CreateIterator(&iterator);
    iterator->Init();

    nsIXPFCCanvas * item;

    while(!(iterator->IsDone()))
    {
      item = (nsIXPFCCanvas *) iterator->CurrentItem();
      NS_RELEASE(item);
      iterator->Next();
    }
    NS_RELEASE(iterator);

    mOrphanCanvasList->RemoveAll();
    NS_RELEASE(mOrphanCanvasList);
  }


  NS_RELEASE(mTimeContextList);
  NS_RELEASE(mCanvasStack);

  if (mControlList != nsnull) 
  {

    nsIIterator * iterator;

    mControlList->CreateIterator(&iterator);
    iterator->Init();

    ControlListEntry * item;

	while(!(iterator->IsDone()))
	{
		item = (ControlListEntry *) iterator->CurrentItem();
		delete item;
		iterator->Next();
	}
	NS_RELEASE(iterator);

    mControlList->RemoveAll();
    NS_RELEASE(mControlList);
  }

}

NS_IMPL_ADDREF(nsCalXMLContentSink)
NS_IMPL_RELEASE(nsCalXMLContentSink)


nsresult nsCalXMLContentSink::QueryInterface(const nsIID& aIID, void** aInstancePtr)  
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      

  if(aIID.Equals(kISupportsIID))    {  //do IUnknown...
    *aInstancePtr = (nsIContentSink*)(this);                                        
  }
  else if(aIID.Equals(kIContentSinkIID)) {  //do nsIContentSink base class...
    *aInstancePtr = (nsIContentSink*)(this);                                        
  }
  else if(aIID.Equals(kIHTMLContentSinkIID)) {
    *aInstancePtr = (nsIHTMLContentSink*)(this);
  }
  else if(aIID.Equals(kClassIID)) {  //do this class...
    *aInstancePtr = (nsCalXMLContentSink*)(this);                                        
  }                 
  else if(aIID.Equals(kIXPFCXMLContentSinkIID)) {  //do this class...
    *aInstancePtr = (nsIXPFCXMLContentSink*)(this);                                        
  }                 
  else {
    *aInstancePtr=0;
    return NS_NOINTERFACE;
  }
  ((nsISupports*) *aInstancePtr)->AddRef();
  return NS_OK;                                                        
}


NS_IMETHODIMP nsCalXMLContentSink::Init()
{
  return NS_OK;
}

NS_IMETHODIMP nsCalXMLContentSink::SetViewerContainer(nsIWebViewerContainer * aViewerContainer)
{
  mCalendarContainer = (nsCalendarContainer*)aViewerContainer;

  return NS_OK;
}
  

/*
 * this method gets invoked whenever a container type tag
 * is encountered.
 */

NS_IMETHODIMP nsCalXMLContentSink::OpenContainer(const nsIParserNode& aNode)
{  
  nsIXMLParserObject * object = nsnull;
  eCalXMLTags tag = (eCalXMLTags) aNode.GetNodeType();
  nsresult res;
  nsCID aclass;

  nsString text = aNode.GetText();

  res = CIDFromTag(tag, aclass);

  if (NS_OK != res)
    return res ;

  res = nsRepository::CreateInstance(aclass, 
                                     nsnull, 
                                     kIXMLParserObjectIID, 
                                     (void **)&object);

  if (NS_OK != res)
    return res ;

  AddToHierarchy(*object, PR_TRUE);

  object->Init();        

  /*
   * ConsumeAttributes
   */

  ConsumeAttributes(aNode,*object);

  /*
   * If this was a root panel, add it to the widget
   */

  if (tag == eCalXMLTag_rootpanel)
  {
    nsIXPFCCanvas * child = (nsIXPFCCanvas *) mCanvasStack->Top();

    /*
     * this should never occur...
     */

    if (child == nsnull)
      return NS_ERROR_UNEXPECTED;

    nsIXPFCCanvas * root ;

    gXPFCToolkit->GetRootCanvas(&root);

    root->AddChildCanvas(child);

    NS_RELEASE(root);
  }

  NS_RELEASE(object);

  return NS_OK;
}


/*
 * Consume the Attributes for this object
 */

NS_IMETHODIMP nsCalXMLContentSink::ConsumeAttributes(const nsIParserNode& aNode, nsIXMLParserObject& aObject)
{  
  PRInt32 i = 0;
  nsString key,value;
  nsString scontrol;
  PRBool control = PR_FALSE;

  for (i = 0; i < aNode.GetAttributeCount(); i++) {

   key   = aNode.GetKeyAt(i);
   value = aNode.GetValueAt(i);

   key.StripChars("\"");
   value.StripChars("\"");

   aObject.SetParameter(key,value);

   if (key.EqualsIgnoreCase(CAL_STRING_CONTROL))
   {
     control = PR_TRUE;
     scontrol = value;
   }

  }

  /*
   * If this object has something it wants to control, store away the string and source
   * object.  Later on, we'll walk the content tree Registering appropriate subjects
   * and observers.
   */

  if (control == PR_TRUE)
    mControlList->Append(new ControlListEntry(&aObject, scontrol));

  return NS_OK;
}


NS_IMETHODIMP nsCalXMLContentSink::CloseContainer(const nsIParserNode& aNode)
{
  nsIXPFCCanvas * canvas = (nsIXPFCCanvas *)mCanvasStack->Pop();  
  NS_IF_RELEASE(canvas);
  return NS_OK;
}

NS_IMETHODIMP nsCalXMLContentSink::AddToHierarchy(nsIXMLParserObject& aObject, PRBool aPush)
{  
  /*
   * Add as a child of current top node
   */

  nsIXPFCCanvas * canvas  = nsnull;

  nsresult res = aObject.QueryInterface(kIXPFCCanvasIID,(void**)&canvas);

  if (NS_OK != res)
    return res;

  nsIXPFCCanvas * parent = (nsIXPFCCanvas *) mCanvasStack->Top();

  /*
   * If we have no canvas on the stack, that means 
   * we do not yet know where this panel belongs, so
   * add it to the free list.  
   */

  if (parent == nsnull) {

    mOrphanCanvasList->Append(canvas);
    NS_ADDREF(canvas);

  } else {
   
    parent->AddChildCanvas(canvas);

  }

  if (aPush == PR_TRUE)
  {
    mCanvasStack->Push(canvas);
    NS_ADDREF(canvas);
  }

  NS_RELEASE(canvas);

  return res;
}

/*
 * Find the CID from the XML Tag Object
 */

NS_IMETHODIMP nsCalXMLContentSink::CIDFromTag(eCalXMLTags tag, nsCID &aClass)
{
  switch(tag)
  {
    case eCalXMLTag_tcc:
      aClass = kCCalTimebarContextControllerCID;
      break;

    case eCalXMLTag_mcc:
      aClass = kCCalMonthContextControllerCID;
      break;

    case eCalXMLTag_timebaruserheading:
      aClass = kCalTimebarUserHeadingCID;
      break;

    case eCalXMLTag_timebarscale:
      aClass = kCalTimebarScaleCID;
      break;

    case eCalXMLTag_todocanvas:
      aClass = kCalTodoComponentCanvasCID;
      break;

    case eCalXMLTag_commandcanvas:
      aClass = kCalCommandCanvasCID;
      break;

    case eCalXMLTag_multidayviewcanvas:
      aClass = kCalMultiDayViewCanvasCID;
      break;

    case eCalXMLTag_multiuserviewcanvas:
      aClass = kCalMultiUserViewCanvasCID;
      break;

    case eCalXMLTag_monthviewcanvas:
      aClass = kCalMonthViewCanvasCID;
      break;

    case eCalXMLTag_panel:
    case eCalXMLTag_rootpanel:
      aClass = kCXPFCCanvasCID;
      break;

    case eCalXMLTag_htmlcanvas:
      aClass = kCXPFCHTMLCanvasCID;
      break;

    case eCalXMLTag_foldercanvas:
      aClass = kCXPFolderCanvas;
      break;

    case eCalXMLTag_xpitem:
      aClass = kCXPItem;
      break;
      
    default:
      return (NS_ERROR_UNEXPECTED);
      break;
  }


  return NS_OK;
}

/*
 * Add a Leaf XML Object
 */


NS_IMETHODIMP nsCalXMLContentSink::AddLeaf(const nsIParserNode& aNode)
{

  static NS_DEFINE_IID(kCCalTimeContextCID,  NS_CAL_TIME_CONTEXT_CID);
  static NS_DEFINE_IID(kCCalTimeContextIID,  NS_ICAL_TIME_CONTEXT_IID);

  nsIXMLParserObject * object = nsnull;
  eCalXMLTags tag = (eCalXMLTags) aNode.GetNodeType();
  nsresult res;
  nsCID aclass;

  nsString text = aNode.GetText();

  res = CIDFromTag(tag, aclass);

  if (NS_OK != res)
  {
    /*
     * Check to see if this is a control node
     */

    if (tag == eCalXMLTag_control)
      AddControl(aNode);
    else if (tag == eCalXMLTag_ctx)
      AddCtx(aNode);

    return res ;
  }

  res = nsRepository::CreateInstance(aclass, 
                                     nsnull, 
                                     kIXMLParserObjectIID, 
                                     (void **)&object);

  if (NS_OK != res)
    return res ;

  AddToHierarchy(*object, PR_FALSE);

  object->Init();        

  ConsumeAttributes(aNode,*object);

  NS_RELEASE(object);

  return NS_OK;
}

NS_IMETHODIMP nsCalXMLContentSink::AddCtx(const nsIParserNode& aNode)
{

  static NS_DEFINE_IID(kCCalTimeContextCID,  NS_CAL_TIME_CONTEXT_CID);
  static NS_DEFINE_IID(kCCalTimeContextIID,  NS_ICAL_TIME_CONTEXT_IID);

  nsICalTimeContext * context;

  nsresult res = nsRepository::CreateInstance(kCCalTimeContextCID, 
                                     nsnull, 
                                     kCCalTimeContextIID, 
                                     (void **)&context);

  if (NS_OK != res)
    return res ;

  context->Init();

  /*
   * Apply this context all the way down.  We should probably wait until
   * parsing is completed before doing this OR slip the context into the 
   * base canvas class, which may be easier.
   */
  nsIXPFCCanvas * root ;

  gXPFCToolkit->GetRootCanvas(&root);

  ApplyContext(root, context);

  NS_RELEASE(root);
  NS_RELEASE(context);

  return NS_OK;
}

NS_IMETHODIMP nsCalXMLContentSink::ApplyContext(nsIXPFCCanvas * aCanvas, nsICalTimeContext * aContext)
{

  nsCalTimebarCanvas * time_canvas = nsnull;
  nsIIterator * iterator = nsnull;
  nsIXPFCCanvas * canvas = nsnull;

  nsresult res = aCanvas->QueryInterface(kCalTimebarCanvasCID,(void**)&time_canvas);

  if (res == NS_OK)
  {
    time_canvas->SetTimeContext(aContext);

    /*
     * XXX: Should this be for all Multi Views?
     *
     * If this is a MultiView Canvas, stop here
     */

    static NS_DEFINE_IID(kCalMultiDayViewCanvasCID, NS_CAL_MULTIDAYVIEWCANVAS_CID);
    nsCalMultiDayViewCanvas * multi;
    nsresult res = time_canvas->QueryInterface(kCalMultiDayViewCanvasCID,(void**)&multi);

    if (res == NS_OK)
    {
      NS_RELEASE(multi);
      NS_RELEASE(time_canvas);
      return NS_OK;
    }

    NS_RELEASE(time_canvas);

  }

  res = aCanvas->CreateIterator(&iterator);

  if (NS_OK != res)
    return NS_OK;

  iterator->Init();

  while(!(iterator->IsDone()))
  {
    canvas = (nsIXPFCCanvas *) iterator->CurrentItem();

    ApplyContext(canvas, aContext);

    iterator->Next();
  }

  NS_RELEASE(iterator);

  return NS_OK;
}

/*
 * When we get a control, it usually means we point to a canvas in
 * the free list and need to make that canvas a child of the canvas
 * currently on top of the stack
 */

NS_IMETHODIMP nsCalXMLContentSink::AddControl(const nsIParserNode& aNode)
{
  nsIXPFCCanvas * child = nsnull;
  nsIXMLParserObject * object = nsnull;
  nsIXPFCCanvas * parent = (nsIXPFCCanvas *) mCanvasStack->Top();

  /*
   * this should never occur...
   */

  if (parent == nsnull)
    return NS_ERROR_UNEXPECTED;

  /*
   * Find the rule for this control. It contains the name of the canvas to reparent
   */

  PRInt32 i = 0;
  nsString key,value;

  for (i = 0; i < aNode.GetAttributeCount(); i++) 
  {

    key   = aNode.GetKeyAt(i);
    value = aNode.GetValueAt(i);

    key.StripChars("\"");
    value.StripChars("\"");

    /*
     * Search the free list for a canvas of this name.  If it is there, just parent it.
     * If it is not there, it means we are re-parenting and we should search the hierarchy
     * to do that
     */

    if (key.EqualsIgnoreCase(CAL_STRING_RULE))
    {

      child = CanvasFromName(value);

      if (child == nsnull) 
      {
        nsIXPFCCanvas * root ;

        gXPFCToolkit->GetRootCanvas(&root);
  
        child = root->CanvasFromName(value);

        NS_RELEASE(root);
      }

      break;
    }

  }

  if (child == nsnull)
    return NS_ERROR_UNEXPECTED;

  /*
   * We now have a parent and child.  Let's Reparent and remove from free list
   * if it is there
   */

  child->Reparent(parent);

  /*
   * Pass certain attributes to the parent canvas (layout, weightminor, weighmajor, etc...)
   */
  nsresult res = child->QueryInterface(kIXMLParserObjectIID,(void**)&object);

  if (NS_OK == res)
  {
    for (i = 0; i < aNode.GetAttributeCount(); i++) 
    {

      key   = aNode.GetKeyAt(i);
      value = aNode.GetValueAt(i);

      key.StripChars("\"");
      value.StripChars("\"");

      if (key.EqualsIgnoreCase(CAL_STRING_LAYOUT) || key.EqualsIgnoreCase(CAL_STRING_WEIGHTMAJOR) || key.EqualsIgnoreCase(CAL_STRING_WEIGHTMINOR))
        object->SetParameter(key,value);
    }

    NS_RELEASE(object);
  }

  if (NS_OK == (mOrphanCanvasList->Remove(child)))
    NS_RELEASE(child);

  return res;
}

NS_IMETHODIMP nsCalXMLContentSink::PushMark()
{
  return NS_OK;
}

NS_IMETHODIMP nsCalXMLContentSink::OpenHTML(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP nsCalXMLContentSink::CloseHTML(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP nsCalXMLContentSink::OpenHead(const nsIParserNode& aNode) 
{
  return NS_OK;
}

NS_IMETHODIMP nsCalXMLContentSink::CloseHead(const nsIParserNode& aNode) 
{
  return NS_OK;
}

NS_IMETHODIMP nsCalXMLContentSink::SetTitle(const nsString& aValue)
{
  return NS_OK;
}

NS_IMETHODIMP nsCalXMLContentSink::OpenBody(const nsIParserNode& aNode) 
{
  return NS_OK;
}

NS_IMETHODIMP nsCalXMLContentSink::CloseBody(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP nsCalXMLContentSink::OpenForm(const nsIParserNode& aNode) 
{
  return NS_OK;
}

NS_IMETHODIMP nsCalXMLContentSink::CloseForm(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP nsCalXMLContentSink::OpenMap(const nsIParserNode& aNode) 
{
  return NS_OK;
}

NS_IMETHODIMP nsCalXMLContentSink::CloseMap(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP nsCalXMLContentSink::OpenFrameset(const nsIParserNode& aNode) 
{
  return NS_OK;
}

NS_IMETHODIMP nsCalXMLContentSink::CloseFrameset(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP nsCalXMLContentSink::WillBuildModel(void)
{
  return NS_OK;
}

NS_IMETHODIMP nsCalXMLContentSink::DidBuildModel(PRInt32 aQualityLevel)
{
  static NS_DEFINE_IID(kXPFCSubjectIID,  NS_IXPFC_SUBJECT_IID);
  static NS_DEFINE_IID(kXPFCObserverIID, NS_IXPFC_OBSERVER_IID);

  nsIXPFCCanvas * root ;
  nsIXPFCCanvas * canvas ;
  nsIXPFCCanvas * canvas2 ;
  nsresult res;

  gXPFCToolkit->GetRootCanvas(&root);

  
  /*
   * first, register all appropriate controls that have been stored up
   */

  nsIIterator * iterator;

  mControlList->CreateIterator(&iterator);

  iterator->Init();

  ControlListEntry * item ;

  nsICalContextController * context_controller;
  nsIXPFCSubject * subject;
  nsIXPFCSubject * subject2;
  nsCalTimebarCanvas * timebar;
  nsIXPFCObserver * observer;
  nsIXPFCObserver * observer2;
  nsICalTimeContext * context;


  nsIXPFCObserverManager* om;
  nsServiceManager::GetService(kCXPFCObserverManagerCID, kIXPFCObserverManagerIID, (nsISupports**)&om);

  while(!(iterator->IsDone()))
  {
    item = (ControlListEntry *) iterator->CurrentItem();

    /*
     * Ok, Here is how it works:
     *
     * 1) If this is a type of controller, we will register the object part as a subject
     *    and the string will reference a canvas which has a timecontext which will be our
     *    observer.  Then, we register the timecontext as a observer and the controller as an 
     *    subject.
     *
     * 2) If this is not a controller but is a canvas, we register them directly (context to controller)
     */

    res = item->object->QueryInterface(kCalContextcontrollerIID, (void**)&context_controller);

    /*
     * Is this a controller
     */

    if (NS_OK == res)
    {
      res = context_controller->QueryInterface(kXPFCSubjectIID, (void **)&subject);

      if (res == NS_OK)
      {
        canvas = root->CanvasFromName(item->control);

        if (canvas != nsnull)
        {

          res = canvas->QueryInterface(kCalTimebarCanvasCID, (void **)&timebar);

          if (res == NS_OK)
          {

            context = timebar->GetTimeContext();

            if (context != nsnull)
            {

              /*
               * first register controller with context
               */

              res = context->QueryInterface(kXPFCObserverIID, (void **)&observer);

              if (res == NS_OK)
              {
                om->Register(subject, observer);
              
                NS_RELEASE(observer);
              }

              /*
               * Now register context and target canvas
               *
               * What we want to do is register the canvas with the
               * Model, not the TimeContext.  Then, register the
               * Model with the TimeContext.
               *
               * Need to figure how this will get affected on Change 
               * commands
               */

              nsIModel * model = timebar->GetModel();
              res = context->QueryInterface(kXPFCSubjectIID, (void **)&subject2);

              // Register the Model as Observer of Context
              if ((nsnull != model) && (res == NS_OK))
              {
                res = model->QueryInterface(kXPFCObserverIID, (void **)&observer2);
                if (res == NS_OK)
                {
                  om->Register(subject2, observer2);
                  NS_RELEASE(observer2);
                }
                NS_RELEASE(subject2);

                // Register the Canvas as Observer of Model
                res = model->QueryInterface(kXPFCSubjectIID, (void **)&subject2);
                if (res == NS_OK)
                {
                  res = timebar->QueryInterface(kXPFCObserverIID, (void **)&observer2);
                  if (res == NS_OK)
                  {
                    om->Register(subject2, observer2);
                    NS_RELEASE(observer2);
                  }
                  NS_RELEASE(subject2);
                }

              }
            }

            NS_RELEASE(timebar);
          }

        }

        NS_RELEASE(subject);
      }

      NS_RELEASE(context_controller);

    } else {

    /*
     * Is this a canvas instead.
     *
     * What we really want to do here is register the
     * context with the Model and the Canvas with
     * the model.
     */


     res = item->object->QueryInterface(kIXPFCCanvasIID, (void**)&canvas);

     if (res == NS_OK)
     {

       res = canvas->QueryInterface(kCalTimebarCanvasCID, (void **)&timebar);

       if (res == NS_OK)
       {

          context = timebar->GetTimeContext();

          if (context != nsnull)
          {

            res = context->QueryInterface(kXPFCSubjectIID, (void **)&subject);

            if (res == NS_OK)
            {
              canvas2 = root->CanvasFromName(item->control);

              if (canvas2 != nsnull)
              {

                res = canvas2->QueryInterface(kXPFCObserverIID, (void **)&observer);

                if (res == NS_OK)
                {
                  nsIModel * model = canvas2->GetModel();

                  if (nsnull != model)
                  {
                    nsIXPFCSubject *  msubject;
                    nsIXPFCObserver * mobserver;
                    res = model->QueryInterface(kXPFCObserverIID, (void **)&mobserver);
                    if (NS_OK == res)
                    {
                      om->Register(subject, mobserver);
                      NS_RELEASE(mobserver);
                    }
                    res = model->QueryInterface(kXPFCSubjectIID, (void **)&msubject);
                    if (NS_OK == res)
                    {
                      om->Register(msubject, observer);
                      NS_RELEASE(msubject);
                    }
                  }
                  NS_RELEASE(observer);
                }

              }

              NS_RELEASE(subject);
            }

          }

          NS_RELEASE(timebar);

       }


       NS_RELEASE(canvas);

     }
         
    }

    iterator->Next();
  }

  NS_IF_RELEASE(iterator);
  /*
   * Layout the objects
   */

  root->Layout();

  if (root->GetView())
  {
    nsRect bounds;
    root->GetView()->GetBounds(bounds);
    gXPFCToolkit->GetViewManager()->UpdateView(root->GetView(), bounds, NS_VMREFRESH_AUTO_DOUBLE_BUFFER | NS_VMREFRESH_NO_SYNC);
  }


  NS_RELEASE(root);

  nsServiceManager::ReleaseService(kCXPFCObserverManagerCID, om);

  // XXX: Should we clean up everything here?
  return NS_OK;
}

NS_IMETHODIMP nsCalXMLContentSink::WillInterrupt(void) 
{
  return NS_OK;
}

NS_IMETHODIMP nsCalXMLContentSink::WillResume(void) 
{
  return NS_OK;
}


nsIXPFCCanvas * nsCalXMLContentSink::CanvasFromName(nsString& aName)
{
  nsresult res ;
  nsIIterator * iterator ;
  nsIXPFCCanvas * canvas;
  nsString child;

  // Iterate through the children
  res = mOrphanCanvasList->CreateIterator(&iterator);

  if (NS_OK != res)
    return nsnull;

  iterator->Init();

  while(!(iterator->IsDone()))
  {
    canvas = (nsIXPFCCanvas *) iterator->CurrentItem();

    child = canvas->GetNameID();

    if (child == aName) {
      NS_RELEASE(iterator);
      return canvas;
    }

    iterator->Next();
  }

  NS_RELEASE(iterator);

  return nsnull;
}
nsresult nsCalXMLContentSink::SetRootCanvas(nsIXPFCCanvas * aCanvas)
{
  mCanvasStack->Push(aCanvas);
  NS_ADDREF(aCanvas);
  return NS_OK;
}
