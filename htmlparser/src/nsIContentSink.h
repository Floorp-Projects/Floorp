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
 * @update  gess 4/1/98
 * 
 * This file declares the concrete IContentSink interface.
 * This pure virtual interface is used as the "glue" that
 * connects the parsing process to the content model 
 * construction process.
 *
 */

#ifndef  ICONTENTSINK
#define  ICONTENTSINK

#include "nsIParserNode.h"
#include "nsISupports.h"

#define NS_ICONTENTSINK_IID      \
  {0x4c2f7e34, 0xc286,  0x11d1,  \
  {0x80, 0x22, 0x00,    0x60, 0x08, 0x14, 0x98, 0x89}}


class nsIContentSink : public nsISupports {
  public:

   /**
    * This method is used to open a generic container in the sink.
    *
    * @update 4/1/98 gess
    * @param  nsIParserNode reference to parser node interface
    * @return PR_TRUE if successful. 
    */     
  virtual PRBool        OpenContainer(const nsIParserNode& aNode) = 0;

   /**
    *  This method gets called by the parser when a close
    *  container tag has been consumed and needs to be closed.
    *
    * @update 4/1/98 gess
    * @param  nsIParserNode reference to parser node interface
    * @return PR_TRUE if successful. 
    */     
  virtual PRBool        CloseContainer(const nsIParserNode& aNode) = 0;

   /**
    * This method gets called by the parser when a the
    * topmost container in the content sink needs to be closed.
    *
    * @update 4/1/98 gess
    * @return PR_TRUE if successful. 
    */     
  virtual PRBool        CloseTopmostContainer() = 0;

   /**
    * This gets called by the parser when you want to add
    * a leaf node to the current container in the content
    * model.
    *
    * @update 4/1/98 gess
    * @param  nsIParserNode reference to parser node interface
    * @return PR_TRUE if successful. 
    */     
  virtual PRBool        AddLeaf(const nsIParserNode& aNode) = 0;

   /**
    * This method gets called when the parser begins the process
    * of building the content model via the content sink.
    *
    * @update 5/7/98 gess
   */     
  virtual void WillBuildModel(void)=0;

   /**
    * This method gets called when the parser concludes the process
    * of building the content model via the content sink.
    *
    * @update 5/7/98 gess
    */     
  virtual void DidBuildModel(void)=0;

  /**
    * This method gets called when the parser gets i/o blocked,
    * and wants to notify the sink that it may be a while before
    * more data is available.
    *
    * @update 5/7/98 gess
   */     

  virtual void WillInterrupt(void)=0;

   /**
    * This method gets called when the parser i/o gets unblocked,
    * and we're about to start dumping content again to the sink.
    *
    * @update 5/7/98 gess
    */     
  virtual void WillResume(void)=0;

};

extern nsresult NS_NewHTMLContentSink(nsIContentSink** aInstancePtrResult);


#endif



