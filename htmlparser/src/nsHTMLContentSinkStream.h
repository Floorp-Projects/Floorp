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
 * @update  gpk 7/12/98
 * 
 * This file declares the concrete HTMLContentSink class.
 * This class is used during the parsing process as the
 * primary interface between the parser and the content
 * model.
 * This content sink writes to a stream. If no stream
   is declared in the constructor then all output goes
   to cout.
   The file is pretty printed according to the pretty
   printing interface. subclasses may choose to override
   this behavior or set runtime flags for desired
   resutls.
 */

#ifndef  NS_HTMLCONTENTSINK_STREAM
#define  NS_HTMLCONTENTSINK_STREAM

#include "nsIParserNode.h"
#include "nsIHTMLContentSink.h"
#include "nshtmlpars.h"
#include "nsHTMLTokens.h"


#define NS_HTMLCONTENTSINK_STREAM_IID  \
  {0xa39c6bff, 0x15f0, 0x11d2, \
  {0x80, 0x41, 0x0, 0x10, 0x4b, 0x98, 0x3f, 0xd4}}



class ostream;

class nsHTMLContentSinkStream : public nsIHTMLContentSink {
  public:

  NS_DECL_ISUPPORTS

  /**
   * 
   * @update	gess7/7/98
   * @param 
   * @return
   */
  nsHTMLContentSinkStream(); 

  /**
   * 
   * @update	gess7/7/98
   * @param 
   * @return
   */
  nsHTMLContentSinkStream(ostream& aStream); 

  /**
   * 
   * @update	gess7/7/98
   * @param 
   * @return
   */
  virtual ~nsHTMLContentSinkStream();


  /**
   * 
   * @update	gess7/7/98
   * @param 
   * @return
   */
  void SetOutputStream(ostream& aStream);

   /**
    * This method gets called by the parser when it encounters
    * a title tag and wants to set the document title in the sink.
    *
    * @update 4/1/98 gess
    * @param  nsString reference to new title value
    * @return PR_TRUE if successful. 
    */     
  virtual PRBool SetTitle(const nsString& aValue);

   /**
    * This method is used to open the outer HTML container.
    *
    * @update 4/1/98 gess
    * @param  nsIParserNode reference to parser node interface
    * @return PR_TRUE if successful. 
    */     
    virtual PRInt32 OpenHTML(const nsIParserNode& aNode);

   /**
    * This method is used to close the outer HTML container.
    *
    * @update 4/1/98 gess
    * @param  nsIParserNode reference to parser node interface
    * @return PR_TRUE if successful. 
    */     
    virtual PRInt32 CloseHTML(const nsIParserNode& aNode);

   /**
    * This method is used to open the only HEAD container.
    *
    * @update 4/1/98 gess
    * @param  nsIParserNode reference to parser node interface
    * @return PR_TRUE if successful. 
    */     
    virtual PRInt32 OpenHead(const nsIParserNode& aNode);

   /**
    * This method is used to close the only HEAD container.
    *
    * @update 4/1/98 gess
    * @param  nsIParserNode reference to parser node interface
    * @return PR_TRUE if successful. 
    */     
    virtual PRInt32 CloseHead(const nsIParserNode& aNode);
  
   /**
    * This method is used to open the main BODY container.
    *
    * @update 4/1/98 gess
    * @param  nsIParserNode reference to parser node interface
    * @return PR_TRUE if successful. 
    */     
    virtual PRInt32 OpenBody(const nsIParserNode& aNode);

   /**
    * This method is used to close the main BODY container.
    *
    * @update 4/1/98 gess
    * @param  nsIParserNode reference to parser node interface
    * @return PR_TRUE if successful. 
    */     
    virtual PRInt32 CloseBody(const nsIParserNode& aNode);

   /**
    * This method is used to open a new FORM container.
    *
    * @update 4/1/98 gess
    * @param  nsIParserNode reference to parser node interface
    * @return PR_TRUE if successful. 
    */     
    virtual PRInt32 OpenForm(const nsIParserNode& aNode);

   /**
    * This method is used to close the outer FORM container.
    *
    * @update 4/1/98 gess
    * @param  nsIParserNode reference to parser node interface
    * @return PR_TRUE if successful. 
    */     
    virtual PRInt32 CloseForm(const nsIParserNode& aNode);
        
   /**
    * This method is used to open the FRAMESET container.
    *
    * @update 4/1/98 gess
    * @param  nsIParserNode reference to parser node interface
    * @return PR_TRUE if successful. 
    */     
    virtual PRInt32 OpenFrameset(const nsIParserNode& aNode);

   /**
    * This method is used to close the FRAMESET container.
    *
    * @update 4/1/98 gess
    * @param  nsIParserNode reference to parser node interface
    * @return PR_TRUE if successful. 
    */     
    virtual PRInt32 CloseFrameset(const nsIParserNode& aNode);
        
   /**
    * This method is used to a general container. 
    * This includes: OL,UL,DIR,SPAN,TABLE,H[1..6],etc.
    *
    * @update 4/1/98 gess
    * @param  nsIParserNode reference to parser node interface
    * @return PR_TRUE if successful. 
    */     
    virtual PRInt32 OpenContainer(const nsIParserNode& aNode);
    
   /**
    * This method is used to close a generic container.
    *
    * @update 4/1/98 gess
    * @param  nsIParserNode reference to parser node interface
    * @return PR_TRUE if successful. 
    */     
    virtual PRInt32 CloseContainer(const nsIParserNode& aNode);

   /**
    * This method is used to add a leaf to the currently 
    * open container.
    *
    * @update 4/1/98 gess
    * @param  nsIParserNode reference to parser node interface
    * @return PR_TRUE if successful. 
    */     
    virtual PRInt32 AddLeaf(const nsIParserNode& aNode);

   /**
    * This method gets called when the parser begins the process
    * of building the content model via the content sink.
    *
    * @update 5/7/98 gess
    */     
    virtual void WillBuildModel(void);

   /**
    * This method gets called when the parser concludes the process
    * of building the content model via the content sink.
    *
    * @param  aQualityLevel describes how well formed the doc was.
    *         0=GOOD; 1=FAIR; 2=POOR;
    * @update 5/7/98 gess
    */     
    virtual void DidBuildModel(PRInt32 aQualityLevel);

   /**
    * This method gets called when the parser gets i/o blocked,
    * and wants to notify the sink that it may be a while before
    * more data is available.
    *
    * @update 5/7/98 gess
    */     
    virtual void WillInterrupt(void);

   /**
    * This method gets called when the parser i/o gets unblocked,
    * and we're about to start dumping content again to the sink.
    *
    * @update 5/7/98 gess
    */     
    virtual void WillResume(void);


    /*
     * PrettyPrinting Methods
     *
    */

public:
  void SetLowerCaseTags(PRBool aDoLowerCase) { mLowerCaseTags = aDoLowerCase; }
    

protected:

    PRInt32 AddLeaf(const nsIParserNode& aNode, ostream& aStream);
    void WriteAttributes(const nsIParserNode& aNode,ostream& aStream);
    void AddStartTag(const nsIParserNode& aNode, ostream& aStream);
    void AddEndTag(const nsIParserNode& aNode, ostream& aStream);

    PRBool IsInline(eHTMLTags aTag) const;
    PRBool IsBlockLevel(eHTMLTags aTag) const;

    void AddIndent(ostream& aStream);


    /**
      * Desired line break state before the open tag.
      */
    PRInt32 BreakBeforeOpen(eHTMLTags aTag) const;

    /**
      * Desired line break state after the open tag.
      */
    PRInt32 BreakAfterOpen(eHTMLTags aTag) const;

    /**
      * Desired line break state before the close tag.
      */
    PRInt32 BreakBeforeClose(eHTMLTags aTag) const;

    /**
      * Desired line break state after the close tag.
      */
    PRInt32 BreakAfterClose(eHTMLTags aTag) const;

    /**
      * Indent/outdent when the open/close tags are encountered.
      * This implies that breakAfterOpen() and breakBeforeClose()
      * are true no matter what those methods return.
      */
    PRBool IndentChildren(eHTMLTags aTag) const;

    /**
      * All tags after this tag and before the closing tag will be output with no
      * formatting.
      */
    PRBool PreformattedChildren(eHTMLTags aTag) const;

    /**
      * Eat the close tag.  Pretty much just for <P*>.
      */
    PRBool EatOpen(eHTMLTags aTag) const;

    /**
      * Eat the close tag.  Pretty much just for </P>.
      */
    PRBool EatClose(eHTMLTags aTag) const;

    /**
      * Are we allowed to insert new white space before the open tag.
      *
      * Returning false does not prevent inserting WS
      * before the tag if WS insertion is allowed for another reason,
      * e.g. there is already WS there or we are after a tag that
      * has PermitWSAfter*().
      */
    PRBool PermitWSBeforeOpen(eHTMLTags aTag) const;

    /** @see PermitWSBeforeOpen */
    PRBool PermitWSAfterOpen(eHTMLTags aTag) const;

    /** @see PermitWSBeforeOpen */
    PRBool PermitWSBeforeClose(eHTMLTags aTag) const;

    /** @see PermitWSBeforeOpen */
    PRBool PermitWSAfterClose(eHTMLTags aTag) const;

    /** Certain structures allow us to ignore the whitespace from the source 
      * document */
    PRBool IgnoreWS(eHTMLTags aTag) const;


protected:
    ostream*  mOutput;
    int       mTabLevel;

    PRInt32   mIndent;
    PRBool    mLowerCaseTags;
    PRInt32   mHTMLStackPos;
    eHTMLTags mHTMLTagStack[1024];  // warning: hard-coded nesting level
    PRInt32   mColPos;


};

extern NS_HTMLPARS nsresult NS_New_HTML_ContentSinkStream(nsIHTMLContentSink** aInstancePtrResult);


#endif




