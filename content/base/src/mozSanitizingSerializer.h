/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org HTML Sanitizer code.
 *
 * The Initial Developer of the Original Code is
 * Ben Bucksch <mozilla@bucksch.org>.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Netscape
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef mozSanitizingSerializer_h__
#define mozSanitizingSerializer_h__

#include "mozISanitizingSerializer.h"
#include "nsIContentSerializer.h"
#include "nsIHTMLContentSink.h"
#include "nsHTMLTags.h"
#include "nsCOMPtr.h"
#include "nsIParserService.h"
#include "nsIContent.h"
#include "nsIAtom.h"
#include "nsIDocumentEncoder.h"
#include "nsString.h"


class mozSanitizingHTMLSerializer : public nsIContentSerializer,
                                    public nsIHTMLContentSink,
                                    public mozISanitizingHTMLSerializer
{
public:
  mozSanitizingHTMLSerializer();
  virtual ~mozSanitizingHTMLSerializer();
  static PRBool PR_CALLBACK ReleaseProperties(nsHashKey* key, void* data,
                                              void* closure);

  NS_DECL_ISUPPORTS

  // nsIContentSerializer
  NS_IMETHOD Init(PRUint32 flags, PRUint32 dummy, const char* aCharSet, 
                  PRBool aIsCopying);

  NS_IMETHOD AppendText(nsIDOMText* aText, PRInt32 aStartOffset,
                        PRInt32 aEndOffset, nsAString& aStr);
  NS_IMETHOD AppendCDATASection(nsIDOMCDATASection* aCDATASection,
                                PRInt32 aStartOffset, PRInt32 aEndOffset,
                                nsAString& aStr)
                      { return NS_OK; }
  NS_IMETHOD AppendProcessingInstruction(nsIDOMProcessingInstruction* aPI,
                                         PRInt32 aStartOffset,
                                         PRInt32 aEndOffset,
                                         nsAString& aStr)
                      { return NS_OK; }
  NS_IMETHOD AppendComment(nsIDOMComment* aComment, PRInt32 aStartOffset,
                           PRInt32 aEndOffset, nsAString& aStr)
                      { return NS_OK; }
  NS_IMETHOD AppendDoctype(nsIDOMDocumentType *aDoctype, nsAString& aStr)
                      { return NS_OK; }
  NS_IMETHOD AppendElementStart(nsIDOMElement *aElement, PRBool aHasChildren,
                                nsAString& aStr); 
  NS_IMETHOD AppendElementEnd(nsIDOMElement *aElement, nsAString& aStr);
  NS_IMETHOD Flush(nsAString& aStr);

  NS_IMETHOD AppendDocumentStart(nsIDOMDocument *aDocument,
                                 nsAString& aStr);

  // nsIContentSink
  NS_IMETHOD WillBuildModel(void) { return NS_OK; }
  NS_IMETHOD DidBuildModel(void) { return NS_OK; }
  NS_IMETHOD WillInterrupt(void) { return NS_OK; }
  NS_IMETHOD WillResume(void) { return NS_OK; }
  NS_IMETHOD SetParser(nsIParser* aParser) { return NS_OK; }
  NS_IMETHOD OpenContainer(const nsIParserNode& aNode);
  NS_IMETHOD CloseContainer(const nsHTMLTag aTag);
  NS_IMETHOD AddHeadContent(const nsIParserNode& aNode);
  NS_IMETHOD AddLeaf(const nsIParserNode& aNode);
  NS_IMETHOD AddComment(const nsIParserNode& aNode) { return NS_OK; }
  NS_IMETHOD AddProcessingInstruction(const nsIParserNode& aNode)
                                                    { return NS_OK; }
  NS_IMETHOD AddDocTypeDecl(const nsIParserNode& aNode);
  virtual void FlushContent(PRBool aNotify) { }
  NS_IMETHOD SetDocumentCharset(nsACString& aCharset);
  virtual nsISupports *GetTarget() { return nsnull; }

  // nsIHTMLContentSink
  NS_IMETHOD OpenHTML(const nsIParserNode& aNode);
  NS_IMETHOD CloseHTML();
  NS_IMETHOD OpenHead(const nsIParserNode& aNode);
  NS_IMETHOD CloseHead();
  NS_IMETHOD SetTitle(const nsString& aValue);
  NS_IMETHOD OpenBody(const nsIParserNode& aNode);
  NS_IMETHOD CloseBody();
  NS_IMETHOD OpenForm(const nsIParserNode& aNode);
  NS_IMETHOD CloseForm();
  NS_IMETHOD OpenMap(const nsIParserNode& aNode);
  NS_IMETHOD CloseMap();
  NS_IMETHOD OpenFrameset(const nsIParserNode& aNode);
  NS_IMETHOD CloseFrameset();
  NS_IMETHOD IsEnabled(PRInt32 aTag, PRBool* aReturn);
  NS_IMETHOD NotifyTagObservers(nsIParserNode* aNode) { return NS_OK; }
  NS_IMETHOD_(PRBool) IsFormOnStack() { return PR_FALSE; }
  NS_IMETHOD BeginContext(PRInt32 aPosition) { return NS_OK; }
  NS_IMETHOD EndContext(PRInt32 aPosition) { return NS_OK; }
  NS_IMETHOD WillProcessTokens(void) { return NS_OK; }
  NS_IMETHOD DidProcessTokens(void) { return NS_OK; }
  NS_IMETHOD WillProcessAToken(void) { return NS_OK; }
  NS_IMETHOD DidProcessAToken(void) { return NS_OK; }

  // nsISanitizingHTMLSerializer
  NS_IMETHOD Initialize(nsAString* aOutString,
                        PRUint32 aFlags, const nsAString& allowedTags);

protected:
  nsresult ParsePrefs(const nsAString& aPref);
  nsresult ParseTagPref(const nsCAutoString& tagpref);
  PRBool IsAllowedTag(nsHTMLTag aTag);
  PRBool IsAllowedAttribute(nsHTMLTag aTag, const nsAString& anAttributeName);
  nsresult SanitizeAttrValue(nsHTMLTag aTag, const nsAString& attr_name,
                             nsString& value /*inout*/);
  nsresult SanitizeTextNode(nsString& value /*inout*/);
  PRBool IsContainer(PRInt32 aId);
  static PRInt32 GetIdForContent(nsIContent* aContent);
  nsresult GetParserService(nsIParserService** aParserService);
  nsresult DoOpenContainer(PRInt32 aTag);
  nsresult DoCloseContainer(PRInt32 aTag);
  nsresult DoAddLeaf(PRInt32 aTag, const nsAString& aText);
  void Write(const nsAString& aString);

protected:
  PRInt32                      mFlags;
  nsHashtable                  mAllowedTags;

  nsCOMPtr<nsIContent>         mContent;
  nsAString*                   mOutputString;
  nsIParserNode*               mParserNode;
  nsCOMPtr<nsIParserService>   mParserService;
};

nsresult
NS_NewSanitizingHTMLSerializer(nsIContentSerializer** aSerializer);

#endif
