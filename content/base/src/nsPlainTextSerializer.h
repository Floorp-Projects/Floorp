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

#ifndef nsPlainTextSerializer_h__
#define nsPlainTextSerializer_h__

#include "nsIContentSerializer.h"
#include "nsIHTMLContentSink.h"
#include "nsHTMLTags.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsILineBreaker.h"
#include "nsIParserService.h"
#include "nsIContent.h"
#include "nsIAtom.h"
#include "nsIHTMLToTextSink.h"
#include "nsIDocumentEncoder.h"


class nsPlainTextSerializer : public nsIContentSerializer,
                              public nsIHTMLContentSink,
                              public nsIHTMLToTextSink
{
public:
  nsPlainTextSerializer();
  virtual ~nsPlainTextSerializer();

  NS_DECL_ISUPPORTS

  // nsIContentSerializer
  NS_IMETHOD Init(PRUint32 flags, PRUint32 aWrapColumn,
                  nsIAtom* aCharSet);

  NS_IMETHOD AppendText(nsIDOMText* aText, PRInt32 aStartOffset,
                        PRInt32 aEndOffset, nsAWritableString& aStr);
  NS_IMETHOD AppendCDATASection(nsIDOMCDATASection* aCDATASection,
                                PRInt32 aStartOffset, PRInt32 aEndOffset,
                                nsAWritableString& aStr) { return NS_OK; }
  NS_IMETHOD AppendProcessingInstruction(nsIDOMProcessingInstruction* aPI,
                                         PRInt32 aStartOffset,
                                         PRInt32 aEndOffset,
                                         nsAWritableString& aStr)  { return NS_OK; }
  NS_IMETHOD AppendComment(nsIDOMComment* aComment, PRInt32 aStartOffset,
                           PRInt32 aEndOffset, nsAWritableString& aStr)  { return NS_OK; }
  NS_IMETHOD AppendDoctype(nsIDOMDocumentType *aDoctype,
                           nsAWritableString& aStr)  { return NS_OK; }
  NS_IMETHOD AppendElementStart(nsIDOMElement *aElement,
                                nsAWritableString& aStr); 
  NS_IMETHOD AppendElementEnd(nsIDOMElement *aElement,
                              nsAWritableString& aStr);
  NS_IMETHOD Flush(nsAWritableString& aStr);

  // nsIContentSink
  NS_IMETHOD WillBuildModel(void) { return NS_OK; }
  NS_IMETHOD DidBuildModel(PRInt32 aQualityLevel) { return NS_OK; }
  NS_IMETHOD WillInterrupt(void) { return NS_OK; }
  NS_IMETHOD WillResume(void) { return NS_OK; }
  NS_IMETHOD SetParser(nsIParser* aParser) { return NS_OK; }
  NS_IMETHOD OpenContainer(const nsIParserNode& aNode);
  NS_IMETHOD CloseContainer(const nsIParserNode& aNode);
  NS_IMETHOD AddLeaf(const nsIParserNode& aNode);
  NS_IMETHOD NotifyError(const nsParserError* aError) { return NS_OK; }
  NS_IMETHOD AddComment(const nsIParserNode& aNode) { return NS_OK; }
  NS_IMETHOD AddProcessingInstruction(const nsIParserNode& aNode) { return NS_OK; }
  NS_IMETHOD AddDocTypeDecl(const nsIParserNode& aNode, PRInt32 aMode=0) { return NS_OK; }
  NS_IMETHOD FlushPendingNotifications() { return NS_OK; }
  NS_IMETHOD SetDocumentCharset(nsAWritableString& aCharset) { return NS_OK; }

  // nsIHTMLContentSink
  NS_IMETHOD SetTitle(const nsString& aValue) { return NS_OK; }
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
  NS_IMETHOD BeginContext(PRInt32 aPosition) { return NS_OK; }
  NS_IMETHOD EndContext(PRInt32 aPosition) { return NS_OK; }
  NS_IMETHOD WillProcessTokens(void) { return NS_OK; }
  NS_IMETHOD DidProcessTokens(void) { return NS_OK; }
  NS_IMETHOD WillProcessAToken(void) { return NS_OK; }
  NS_IMETHOD DidProcessAToken(void) { return NS_OK; }

  // nsIHTMLToTextSink
  NS_IMETHOD Initialize(nsAWritableString* aOutString,
                        PRUint32 aFlags, PRUint32 aWrapCol);

protected:
  nsresult GetAttributeValue(nsIAtom* aName, nsString& aValueRet);
  void AddToLine(const PRUnichar* aStringToAdd, PRInt32 aLength);
  void EndLine(PRBool softlinebreak);
  void EnsureVerticalSpace(PRInt32 noOfRows);
  void FlushLine();
  void OutputQuotesAndIndent(PRBool stripTrailingSpaces=PR_FALSE);
  void Output(nsString& aString);
  void Write(const nsAReadableString& aString);
  PRBool IsBlockLevel(PRInt32 aId);
  PRBool IsContainer(PRInt32 aId);
  PRBool IsInPre();
  PRBool IsCurrentNodeConverted();
  nsresult GetIdForContent(nsIContent* aContent, PRInt32* aID);
  nsresult GetParserService(nsIParserService** aParserService);
  nsresult DoOpenContainer(PRInt32 aTag);
  nsresult DoCloseContainer(PRInt32 aTag);
  nsresult DoAddLeaf(PRInt32 aTag, const nsAReadableString& aText);

  // Inlined functions
  inline PRBool MayWrap()
  {
    return mWrapColumn &&
      ((mFlags & nsIDocumentEncoder::OutputFormatted) ||
       (mFlags & nsIDocumentEncoder::OutputWrap));
  }

  inline PRBool DoOutput()
  {
    return !mInHead;
  }
  
protected:
  nsString         mCurrentLine;

  PRBool           mInHead;
  PRInt32          mIndent;
  // mInIndentString keeps a header that has to be written in the indent.
  // That could be, for instance, the bullet in a bulleted list.
  nsString         mInIndentString;
  PRInt32          mCiteQuoteLevel;
  PRBool           mAtFirstColumn;
  PRInt32          mFlags;

  // The wrap column is how many standard sized chars (western languages)
  // should be allowed on a line. There could be less chars if the chars
  // are wider than latin chars of more if the chars are more narrow.
  PRUint32         mWrapColumn;

  // The width of the line as it will appear on the screen (approx.) 
  PRUint32         mCurrentLineWidth; 

  // Treat quoted text as though it's preformatted -- don't wrap it.
  // Having it on a pref is a temporary measure, See bug 69638.
  PRInt32          mSpanLevel;
  PRBool           mQuotesPreformatted;

  PRBool           mDoFragment;
  PRInt32          mEmptyLines; // Will be the number of empty lines before
                                // the current. 0 if we are starting a new
                                // line and -1 if we are in a line.
  PRBool           mInWhitespace;
  PRBool           mPreFormatted;
  PRBool           mStartedOutput; // we've produced at least a character

  nsString         mURL;
  PRBool           mStructs;           // Output structs (pref)
  PRInt32          mHeaderStrategy;    /* Header strategy (pref)
                                          0 = no indention
                                          1 = indention, increased with
                                              header level (default)
                                          2 = numbering and slight indention */
  PRInt32          mHeaderCounter[7];  /* For header-numbering:
                                          Number of previous headers of
                                          the same depth and in the same
                                          section.
                                          mHeaderCounter[1] for <h1> etc. */

  nsCOMPtr<nsIContent> mContent;
  nsIParserNode*       mParserNode;

  nsAWritableString*            mOutputString;

  // The tag stack: the stack of tags we're operating on, so we can nest:
  nsHTMLTag       *mTagStack;
  PRUint32         mTagStackIndex;

  // The stack for ordered lists:
  PRInt32         *mOLStack;
  PRUint32         mOLStackIndex;

  PRUint32         mULCount;

  nsString                     mLineBreak;
  nsCOMPtr<nsILineBreaker>     mLineBreaker;
  nsCOMPtr<nsIParserService>   mParserService;

  // Conveniance constant. It would be nice to have it as a const static
  // variable, but that causes issues with OpenBSD and module unloading.
  const nsString          kSpace;
};

extern nsresult NS_NewPlainTextSerializer(nsIContentSerializer** aSerializer);

#endif
