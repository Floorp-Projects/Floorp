/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef NS_CALICALENDARCONTENTSINK
#define NS_CALICALENDARCONTENTSINK

#include "nsIParserNode.h"
#include "nsIContentSink.h"
#include "nsString.h"
#include "nsxpfcCIID.h"
#include "nsIArray.h"
#include "nsIIterator.h"
#include "nsIStack.h"
#include "nsCalICalendarDTD.h"
#include "nsICalICalendarParserObject.h"
#include "nsICalICalendarContentSink.h"
#include "nsIXPFCICalContentSink.h"
#include "nsIXPFCContentSink.h"
//#include "nsCalICalendarParserCIID.h"

class nsCalICalendarContentSink : public nsICalICalendarContentSink,
                                  public nsIXPFCICalContentSink,
                                  public nsIXPFCContentSink,
                                  public nsIContentSink
{
public:

  NS_DECL_ISUPPORTS

  nsCalICalendarContentSink();
  virtual ~nsCalICalendarContentSink();

  //nsICalICalendarContentSink
  NS_IMETHOD Init();
  /*NS_IMETHOD SetViewerContainer(nsIWebViewerContainer * aViewerContainer);*/

  //nsIContentSink
  NS_IMETHOD OpenContainer(const nsIParserNode& aNode);
  NS_IMETHOD CloseContainer(const nsIParserNode& aNode);
  NS_IMETHOD AddLeaf(const nsIParserNode& aNode);
  NS_IMETHOD WillBuildModel(void);
  NS_IMETHOD DidBuildModel(PRInt32 aQualityLevel);
  NS_IMETHOD WillInterrupt(void);
  NS_IMETHOD WillResume(void);

private:
  NS_IMETHOD CIDFromTag(eCalICalendarTags tag, nsCID &aClass);
  NS_IMETHOD ConsumeAttributes(const nsIParserNode& aNode, nsICalICalendarParserObject& aObject);
  NS_IMETHOD AddToHierarchy(nsICalICalendarParserObject& aObject, PRBool aPush);

public:
  NS_IMETHOD_(PRBool) IsContainer(const nsIParserNode& aNode);

  NS_IMETHOD SetViewerContainer(nsIWebViewerContainer * aViewerContainer);
  NS_IMETHOD SetContentSinkContainer(nsISupports * aContentSinkContainer);
  NS_IMETHOD SetCalendarList(nsIArray * aCalendarList);


private:
    nsIWebViewerContainer * mViewerContainer ;
    nsIStack *  mXPFCStack;
    nsIArray * mOrphanMenuList;
    nsIArray * mContainerList ;
    PRUint32    mState;

    nsIArray * mCalendarList;
};


#endif


