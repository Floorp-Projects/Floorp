/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
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

/*
 * A serializer and content sink that removes potentially insecure or
 * otherwise dangerous or offending HTML (eg for display of HTML
 * e-mail attachments or something).
 */

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
#include "nsString.h"
#include "nsIParser.h"
#include "nsHashtable.h"

class mozSanitizingHTMLSerializer : public nsIContentSerializer,
                                    public nsIHTMLContentSink,
                                    public mozISanitizingHTMLSerializer
{
public:
  mozSanitizingHTMLSerializer();
  virtual ~mozSanitizingHTMLSerializer();
  static bool ReleaseProperties(nsHashKey* key, void* data, void* closure);

  NS_DECL_ISUPPORTS

  // nsIContentSerializer
  NS_IMETHOD Init(PRUint32 flags, PRUint32 dummy, const char* aCharSet, 
                  bool aIsCopying, bool aIsWholeDocument);

  NS_IMETHOD AppendText(nsIContent* aText, PRInt32 aStartOffset,
                        PRInt32 aEndOffset, nsAString& aStr);
  NS_IMETHOD AppendCDATASection(nsIContent* aCDATASection,
                                PRInt32 aStartOffset, PRInt32 aEndOffset,
                                nsAString& aStr)
                      { return NS_OK; }
  NS_IMETHOD AppendProcessingInstruction(nsIContent* aPI,
                                         PRInt32 aStartOffset,
                                         PRInt32 aEndOffset,
                                         nsAString& aStr)
                      { return NS_OK; }
  NS_IMETHOD AppendComment(nsIContent* aComment, PRInt32 aStartOffset,
                           PRInt32 aEndOffset, nsAString& aStr)
                      { return NS_OK; }
  NS_IMETHOD AppendDoctype(nsIContent *aDoctype, nsAString& aStr)
                      { return NS_OK; }
  NS_IMETHOD AppendElementStart(mozilla::dom::Element* aElement,
                                mozilla::dom::Element* aOriginalElement,
                                nsAString& aStr);
  NS_IMETHOD AppendElementEnd(mozilla::dom::Element* aElement,
                              nsAString& aStr);
  NS_IMETHOD Flush(nsAString& aStr);

  NS_IMETHOD AppendDocumentStart(nsIDocument *aDocument,
                                 nsAString& aStr);

  // nsIContentSink
  NS_IMETHOD WillParse(void) { return NS_OK; }
  NS_IMETHOD WillInterrupt(void) { return NS_OK; }
  NS_IMETHOD WillResume(void) { return NS_OK; }
  NS_IMETHOD SetParser(nsIParser* aParser) { return NS_OK; }
  NS_IMETHOD OpenContainer(const nsIParserNode& aNode);
  NS_IMETHOD CloseContainer(const nsHTMLTag aTag);
  NS_IMETHOD AddLeaf(const nsIParserNode& aNode);
  NS_IMETHOD AddComment(const nsIParserNode& aNode) { return NS_OK; }
  NS_IMETHOD AddProcessingInstruction(const nsIParserNode& aNode)
                                                    { return NS_OK; }
  NS_IMETHOD AddDocTypeDecl(const nsIParserNode& aNode);
  virtual void FlushPendingNotifications(mozFlushType aType) { }
  NS_IMETHOD SetDocumentCharset(nsACString& aCharset);
  virtual nsISupports *GetTarget() { return nsnull; }

  // nsIHTMLContentSink
  NS_IMETHOD OpenHead();
  NS_IMETHOD IsEnabled(PRInt32 aTag, bool* aReturn);
  NS_IMETHOD NotifyTagObservers(nsIParserNode* aNode) { return NS_OK; }
  NS_IMETHOD_(bool) IsFormOnStack() { return false; }
  NS_IMETHOD BeginContext(PRInt32 aPosition) { return NS_OK; }
  NS_IMETHOD EndContext(PRInt32 aPosition) { return NS_OK; }
  NS_IMETHOD DidProcessTokens(void) { return NS_OK; }
  NS_IMETHOD WillProcessAToken(void) { return NS_OK; }
  NS_IMETHOD DidProcessAToken(void) { return NS_OK; }

  // nsISanitizingHTMLSerializer
  NS_IMETHOD Initialize(nsAString* aOutString,
                        PRUint32 aFlags, const nsAString& allowedTags);

protected:
  nsresult ParsePrefs(const nsAString& aPref);
  nsresult ParseTagPref(const nsCAutoString& tagpref);
  bool IsAllowedTag(nsHTMLTag aTag);
  bool IsAllowedAttribute(nsHTMLTag aTag, const nsAString& anAttributeName);
  nsresult SanitizeAttrValue(nsHTMLTag aTag, const nsAString& attr_name,
                             nsString& value /*inout*/);
  nsresult SanitizeTextNode(nsString& value /*inout*/);
  bool IsContainer(PRInt32 aId);
  static PRInt32 GetIdForContent(nsIContent* aContent);
  nsresult GetParserService(nsIParserService** aParserService);
  nsresult DoOpenContainer(PRInt32 aTag);
  nsresult DoCloseContainer(PRInt32 aTag);
  nsresult DoAddLeaf(PRInt32 aTag, const nsAString& aText);
  void Write(const nsAString& aString);

protected:
  PRInt32                      mFlags;
  PRUint32                     mSkipLevel;
  nsHashtable                  mAllowedTags;

  nsRefPtr<mozilla::dom::Element> mElement;
  nsAString*                   mOutputString;
  nsIParserNode*               mParserNode;
  nsCOMPtr<nsIParserService>   mParserService;
};

nsresult
NS_NewSanitizingHTMLSerializer(nsIContentSerializer** aSerializer);

#endif
