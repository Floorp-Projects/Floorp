/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/**
 * MODULE NOTES:
 * @update  gpk 7/12/98
 * @update  gess 7/20/98
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
 * The output is formatted, or not, according to the flags passed in.
 * Tags with the attribute "_moz_dirty" will be prettyprinted
 * regardless of the flags (and this attribute will not be output).
 */

#ifndef  NS_TXTCONTENTSINK_STREAM
#define  NS_TXTCONTENTSINK_STREAM

#include "nsIHTMLContentSink.h"
#include "nsParserCIID.h"
#include "nsCOMPtr.h"
#include "nsHTMLTags.h"
#include "nsISaveAsCharset.h"
#include "nsIEntityConverter.h"
#include "nsIURI.h"
#include "nsAWritableString.h"

#define NS_IHTMLCONTENTSINKSTREAM_IID  \
  {0xa39c6bff, 0x15f0, 0x11d2, \
  {0x80, 0x41, 0x0, 0x10, 0x4b, 0x98, 0x3f, 0xd4}}

#if !defined(XP_MAC) && !defined(__KCC)
class ostream;
#endif

class nsIParserNode;
class nsIOutputStream;
class nsIDTD;

class nsIHTMLContentSinkStream : public nsIHTMLContentSink {
  public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IHTMLCONTENTSINKSTREAM_IID)
  NS_DEFINE_STATIC_CID_ACCESSOR(NS_HTMLCONTENTSINKSTREAM_CID)

  NS_IMETHOD Initialize(nsIOutputStream* aOutStream,
                        nsAWritableString* aOutString,
                        const nsAReadableString* aCharsetOverride,
                        PRUint32 aFlags) = 0;
};

class nsHTMLContentSinkStream : public nsIHTMLContentSinkStream 
{
  public:

  /**
   * Constructor with associated stream. If you use this, it means that you want
   * this class to emits its output to the stream you provide.
   * @update	gpk 04/30/99
   * @param		aOutStream -- stream where you want output sent
   * @param		aOutStream -- ref to string where you want output sent
   */
  nsHTMLContentSinkStream();

  /**
   * virtual destructor
   * @update	gess7/7/98
   */
  virtual ~nsHTMLContentSinkStream();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIHTMLContentSinkStream
  NS_IMETHOD Initialize(nsIOutputStream* aOutStream,
                        nsAWritableString* aOutString,
                        const nsAReadableString* aCharsetOverride,
                        PRUint32 aFlags);

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
  NS_IMETHOD GetPref(PRInt32 aTag,PRBool& aPref) { return NS_OK; }
  NS_IMETHOD DoFragment(PRBool aFlag);
  NS_IMETHOD BeginContext(PRInt32 aPosition);
  NS_IMETHOD EndContext(PRInt32 aPosition);

public:
  void SetLowerCaseTags(PRBool aDoLowerCase) { mLowerCaseTags = aDoLowerCase; }

protected:

    PRBool IsBlockLevel(eHTMLTags aTag);
    PRInt32 BreakBeforeOpen(eHTMLTags aTag);
    PRInt32 BreakAfterOpen(eHTMLTags aTag);
    PRInt32 BreakBeforeClose(eHTMLTags aTag);
    PRInt32 BreakAfterClose(eHTMLTags aTag);
    PRBool IndentChildren(eHTMLTags aTag);

    void WriteAttributes(const nsIParserNode& aNode);
    void AddStartTag(const nsIParserNode& aNode);
    void AddEndTag(const nsIParserNode& aNode);
    void AddIndent();
    void EnsureBufferSize(PRInt32 aNewSize);

    NS_IMETHOD InitEncoders();
    
    PRInt32 Write(const nsAReadableString& aString);   // returns # chars written
    void Write(const char* aCharBuffer);
    void Write(char aChar);

    // Handle wrapping and conditional wrapping:
    PRBool HasLongLines(const nsAReadableString& text);
    void WriteWrapped(const nsAReadableString& text);

    // Is this node "dirty", needing reformatting?
    PRBool IsDirty(const nsIParserNode& aNode);

protected:
    nsIOutputStream* mStream;
    nsAWritableString* mString;

    nsIDTD*   mDTD;

    int       mTabLevel;
    char*     mBuffer;
    PRInt32   mBufferSize;
    PRInt32   mBufferLength;

    PRInt32   mIndent;
    PRBool    mLowerCaseTags;
    PRInt32   mHTMLStackPos;
    eHTMLTags mHTMLTagStack[1024];  // warning: hard-coded nesting level
    PRBool    mDirtyStack[1024];    // warning: hard-coded nesting level
    PRInt32   mColPos;
    PRBool    mInBody;

    PRUint32  mFlags;
    nsCOMPtr<nsIURI> mURI;

    PRBool    mDoFormat;
    PRBool    mBodyOnly;
    PRBool    mHasOpenHtmlTag;
    PRInt32   mPreLevel;

    PRInt32   mMaxColumn;

    nsString  mLineBreak;

    nsCOMPtr<nsISaveAsCharset> mCharsetEncoder;
    nsCOMPtr<nsIEntityConverter> mEntityConverter;
    nsString mCharsetOverride;
};


inline nsresult
NS_New_HTML_ContentSinkStream(nsIHTMLContentSink** aInstancePtrResult, 
                              nsIOutputStream* aOutStream,
                              const nsAReadableString* aCharsetOverride,
                              PRUint32 aFlags)
{
  nsCOMPtr<nsIHTMLContentSinkStream> it;
  nsresult rv;

  rv = nsComponentManager::CreateInstance(nsIHTMLContentSinkStream::GetCID(),
                                          nsnull,
                                          NS_GET_IID(nsIHTMLContentSinkStream),
                                          getter_AddRefs(it));
  if (NS_SUCCEEDED(rv)) {
    rv = it->Initialize(aOutStream,
                        nsnull,
                        aCharsetOverride,
                        aFlags);

    if (NS_SUCCEEDED(rv)) {
      rv = it->QueryInterface(NS_GET_IID(nsIHTMLContentSink),
                              (void**)aInstancePtrResult);
    }
  }
  
  return rv;
}

inline nsresult
NS_New_HTML_ContentSinkStream(nsIHTMLContentSink** aInstancePtrResult, 
                              nsAWritableString* aOutString, PRUint32 aFlags)
{
  nsCOMPtr<nsIHTMLContentSinkStream> it;
  nsresult rv;

  rv = nsComponentManager::CreateInstance(nsIHTMLContentSinkStream::GetCID(),
                                          nsnull,
                                          NS_GET_IID(nsIHTMLContentSinkStream),
                                          getter_AddRefs(it));
  if (NS_SUCCEEDED(rv)) {
    rv = it->Initialize(nsnull,
                        aOutString,
                        nsnull,
                        aFlags);

    if (NS_SUCCEEDED(rv)) {
      rv = it->QueryInterface(NS_GET_IID(nsIHTMLContentSink),
                              (void**)aInstancePtrResult);
    }
  }
  
  return rv;
}

#endif

