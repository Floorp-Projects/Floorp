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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

/**
 * MODULE NOTES:
 * 
 * If you've been paying attention to our many content sink classes, you may be
 * asking yourself, "why do we need yet another one?" The answer is that this 
 * implementation, unlike all the others, really sends its output a given stream
 * rather than to an actual content sink (as defined in our HTML document system).
 *
 * We use this class for a number of purposes: 
 *	 1) For actual document i/o using XIF (xml interchange format)
 *   2) For document conversions
 *   3) For debug purposes (to cause output to go to cout or a file)
 *
 * If no stream is declared in the constructor then all output goes to cout. 
 * The file is pretty printed according to the pretty printing interface. subclasses 
 * may choose to override this behavior or set runtime flags for desired results.
 */

#ifndef  NS_HTMLTOTEXTSINK_STREAM
#define  NS_HTMLTOTEXTSINK_STREAM

#include "nsIHTMLContentSink.h"

#include "nsHTMLTags.h"
#include "nsParserCIID.h"
#include "nsCOMPtr.h"

#define NS_IHTMLTOTEXTSINKSTREAM_IID  \
  {0xa39c6bff, 0x15f0, 0x11d2, \
  {0x80, 0x41, 0x0, 0x10, 0x4b, 0x98, 0x3f, 0xd4}}


class nsIUnicodeEncoder;
class nsIOutputStream;

class nsIHTMLToTXTSinkStream : public nsIHTMLContentSink {
  public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IHTMLTOTEXTSINKSTREAM_IID)
  NS_DEFINE_STATIC_CID_ACCESSOR(NS_HTMLTOTXTSINKSTREAM_CID)

  NS_IMETHOD Initialize(nsIOutputStream* aOutStream, 
                        nsString* aOutString,
                        PRUint32 aFlags) = 0;
  NS_IMETHOD SetCharsetOverride(const nsString* aCharset) = 0;
  NS_IMETHOD SetWrapColumn(PRUint32 aWrapCol) = 0;
};

class nsHTMLToTXTSinkStream : public nsIHTMLToTXTSinkStream
{
  public:

  /**
   * Standard constructor
   * @update	gpk02/03/99
   */
  nsHTMLToTXTSinkStream();

  /**
   * virtual destructor
   * @update	gpk02/03/99
   */
  virtual ~nsHTMLToTXTSinkStream();

  NS_IMETHOD Initialize(nsIOutputStream* aOutStream, 
                        nsString* aOutString,
                        PRUint32 aFlags);

  NS_IMETHOD SetCharsetOverride(const nsString* aCharset);


  // nsISupports
  NS_DECL_ISUPPORTS
 
  /*******************************************************************
   * The following methods are inherited from nsIContentSink.
   * Please see that file for details.
   *******************************************************************/
  NS_IMETHOD WillBuildModel(void);
  NS_IMETHOD DidBuildModel(PRInt32 aQualityLevel);
  NS_IMETHOD WillInterrupt(void);
  NS_IMETHOD WillResume(void);
  NS_IMETHOD SetParser(nsIParser* aParser);
  NS_IMETHOD OpenContainer(const nsIParserNode& aNode);
  NS_IMETHOD CloseContainer(const nsIParserNode& aNode);
  NS_IMETHOD AddLeaf(const nsIParserNode& aNode);
  NS_IMETHOD NotifyError(const nsParserError* aError);
  NS_IMETHOD AddComment(const nsIParserNode& aNode);
  NS_IMETHOD AddProcessingInstruction(const nsIParserNode& aNode);
  NS_IMETHOD AddDocTypeDecl(const nsIParserNode& aNode, PRInt32 aMode=0);
  NS_IMETHOD FlushPendingNotifications() { return NS_OK; }

  /*******************************************************************
   * The following methods are inherited from nsIHTMLContentSink.
   * Please see that file for details.
   *******************************************************************/
  NS_IMETHOD SetTitle(const nsString& aValue);
  NS_IMETHOD OpenHTML(const nsIParserNode& aNode);
  NS_IMETHOD CloseHTML(const nsIParserNode& aNode);
  NS_IMETHOD OpenHead(const nsIParserNode& aNode);
  NS_IMETHOD CloseHead(const nsIParserNode& aNode);
  NS_IMETHOD OpenBody(const nsIParserNode& aNode);
  NS_IMETHOD CloseBody(const nsIParserNode& aNode);
  NS_IMETHOD OpenForm(const nsIParserNode& aNode);
  NS_IMETHOD CloseForm(const nsIParserNode& aNode);
  NS_IMETHOD OpenMap(const nsIParserNode& aNode);
  NS_IMETHOD CloseMap(const nsIParserNode& aNode);
  NS_IMETHOD OpenFrameset(const nsIParserNode& aNode);
  NS_IMETHOD CloseFrameset(const nsIParserNode& aNode);
  NS_IMETHOD DoFragment(PRBool aFlag);
  NS_IMETHOD BeginContext(PRInt32 aPosition);
  NS_IMETHOD EndContext(PRInt32 aPosition);

  /*******************************************************************
   * The following methods are specific to this class.
   *******************************************************************/
  NS_IMETHOD SetWrapColumn(PRUint32 aWrapCol)   { mWrapColumn = aWrapCol; return NS_OK; };

protected:
  void EnsureBufferSize(PRInt32 aNewSize);

  nsresult InitEncoder(const nsString& aCharset);

  void AddToLine(const PRUnichar * aStringToAdd, PRInt32 aLength);
  void EndLine(PRBool softlinebreak);
  void EnsureVerticalSpace(PRInt32 noOfRows);
  void FlushLine();
  void WriteQuotesAndIndent();
  void WriteSimple(nsString& aString);
  void Write(const nsString& aString);
  void EncodeToBuffer(nsString& aString);
  NS_IMETHOD GetValueOfAttribute(const nsIParserNode& aNode,
                                 char* aMatchKey,
                                 nsString& aValueRet);
  PRBool DoOutput();

protected:
  nsIOutputStream* mStream;
  nsString*        mString;
  nsString         mCurrentLine;

  PRInt32          mIndent;
  // mInIndentString keeps a header that has to be written in the indent.
  // That could be, for instance, the bullet in a bulleted list.
  nsString         mInIndentString;
  PRInt32          mCiteQuoteLevel;
  PRInt32          mColPos;
  PRInt32          mFlags;
  PRUint32         mWrapColumn;
  PRBool           mDoFragment;

  // For format=flowed
  PRInt32          mEmptyLines; // Will be the number of empty lines before
                                // the current. 0 if we are starting a new
                                // line and -1 if we are in a line.
  PRBool           mInWhitespace;
  PRBool           mPreFormatted;
  PRBool           mCacheLine;   // If the line should be cached before output. This makes it possible to do smarter wrapping.

  nsString         mURL;

  // The tag stack: the stack of tags we're operating on, so we can nest:
  nsHTMLTag       *mTagStack;
  PRUint32         mTagStackIndex;

  // The stack for ordered lists:
  PRInt32         *mOLStack;
  PRUint32         mOLStackIndex;

  char*            mBuffer;
  PRInt32          mBufferLength;  // The length of the data in the buffer
  PRInt32          mBufferSize;    // The actual size of the buffer, regardless of the data

  nsIUnicodeEncoder*  mUnicodeEncoder;
  nsString            mCharsetOverride;
};

inline nsresult
NS_New_HTMLToTXT_SinkStream(nsIHTMLContentSink** aInstancePtrResult, 
                            nsIOutputStream* aOutStream,
                            const nsString* aCharsetOverride=nsnull,
                            PRUint32 aWrapColumn=0, PRUint32 aFlags=0)
{
  nsCOMPtr<nsIHTMLToTXTSinkStream> it;
  nsresult rv;

  rv = nsComponentManager::CreateInstance(nsIHTMLToTXTSinkStream::GetCID(),
                                          nsnull,
                                          NS_GET_IID(nsIHTMLToTXTSinkStream),
                                          getter_AddRefs(it));
  if (NS_SUCCEEDED(rv)) {
    rv = it->Initialize(aOutStream, nsnull, aFlags);

    if (NS_SUCCEEDED(rv)) {
      it->SetWrapColumn(aWrapColumn);
      if (aCharsetOverride != nsnull) {
        it->SetCharsetOverride(aCharsetOverride);
      }
      rv = it->QueryInterface(NS_GET_IID(nsIHTMLContentSink),
                              (void**)aInstancePtrResult);
    }
  }
  
  return rv;
}

inline nsresult
NS_New_HTMLToTXT_SinkStream(nsIHTMLContentSink** aInstancePtrResult, 
                            nsString* aOutString,
                            PRUint32 aWrapColumn=0, PRUint32 aFlags=0)
{
  nsCOMPtr<nsIHTMLToTXTSinkStream> it;
  nsresult rv;

  rv = nsComponentManager::CreateInstance(nsIHTMLToTXTSinkStream::GetCID(),
                                          nsnull,
                                          NS_GET_IID(nsIHTMLToTXTSinkStream),
                                          getter_AddRefs(it));
  if (NS_SUCCEEDED(rv)) {
    rv = it->Initialize(nsnull, aOutString, aFlags);

    if (NS_SUCCEEDED(rv)) {
      it->SetWrapColumn(aWrapColumn);
      nsAutoString ucs2; ucs2.AssignWithConversion("ucs2");
      it->SetCharsetOverride(&ucs2);
      rv = it->QueryInterface(NS_GET_IID(nsIHTMLContentSink),
                              (void**)aInstancePtrResult);
    }
  }
  
  return rv;
}


#endif

