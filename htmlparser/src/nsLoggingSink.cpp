/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#include "nsILoggingSink.h"
#include "nsHTMLTags.h"

static NS_DEFINE_IID(kIContentSinkIID, NS_ICONTENT_SINK_IID);
static NS_DEFINE_IID(kIHTMLContentSinkIID, NS_IHTML_CONTENT_SINK_IID);
static NS_DEFINE_IID(kILoggingSinkIID, NS_ILOGGING_SINK_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

// list of tags that have skipped content
static char gSkippedContentTags[] = {
  eHTMLTag_style,
  eHTMLTag_script,
  eHTMLTag_server,
  eHTMLTag_textarea,
  eHTMLTag_title,
  0
};

class nsLoggingSink : public nsILoggingSink {
public:
  nsLoggingSink();
  ~nsLoggingSink();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIContentSink
  NS_IMETHOD WillBuildModel();
  NS_IMETHOD DidBuildModel(PRInt32 aQualityLevel);
  NS_IMETHOD WillInterrupt();
  NS_IMETHOD WillResume();
  NS_IMETHOD OpenContainer(const nsIParserNode& aNode);
  NS_IMETHOD CloseContainer(const nsIParserNode& aNode);
  NS_IMETHOD AddLeaf(const nsIParserNode& aNode);

  // nsIHTMLContentSink
  NS_IMETHOD PushMark();
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

  // nsILoggingSink
  NS_IMETHOD Init(FILE* fp);

  nsresult OpenNode(const char* aKind, const nsIParserNode& aNode);
  nsresult CloseNode(const char* aKind);
  nsresult LeafNode(const nsIParserNode& aNode);
  nsresult WriteAttributes(const nsIParserNode& aNode);
  nsresult QuoteText(const nsString& aValue, nsString& aResult);
  PRBool WillWriteAttributes(const nsIParserNode& aNode);

protected:
  FILE* mFile;
};

nsresult
NS_NewHTMLLoggingSink(nsIContentSink** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsLoggingSink* it = new nsLoggingSink();
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIContentSinkIID, (void**) aInstancePtrResult);
}

nsLoggingSink::nsLoggingSink()
{
  NS_INIT_REFCNT();
  mFile = nsnull;
}

nsLoggingSink::~nsLoggingSink()
{
  if (nsnull != mFile) {
    fclose(mFile);
    mFile = nsnull;
  }
}

NS_IMPL_ADDREF(nsLoggingSink)
NS_IMPL_RELEASE(nsLoggingSink)

nsresult
nsLoggingSink::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(nsnull != aInstancePtr, "null ptr");
  if (nsnull == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  if (aIID.Equals(kISupportsIID)) {
    nsISupports* tmp = this;
    *aInstancePtr = (void*) tmp;
  }
  else if (aIID.Equals(kIContentSinkIID)) {
    nsIContentSink* tmp = this;
    *aInstancePtr = (void*) tmp;
  }
  else if (aIID.Equals(kIHTMLContentSinkIID)) {
    nsIHTMLContentSink* tmp = this;
    *aInstancePtr = (void*) tmp;
  }
  else if (aIID.Equals(kILoggingSinkIID)) {
    nsILoggingSink* tmp = this;
    *aInstancePtr = (void*) tmp;
  }
  else {
    *aInstancePtr = nsnull;
    return NS_NOINTERFACE;
  }
  NS_ADDREF(this);
  return NS_OK;
}

nsresult
nsLoggingSink::Init(FILE* fp)
{
  if (nsnull == fp) {
    return NS_ERROR_NULL_POINTER;
  }
  mFile = fp;
  return NS_OK;
}

NS_IMETHODIMP
nsLoggingSink::WillBuildModel()
{
  fputs("<begin>\n", mFile);
  return NS_OK;
}

NS_IMETHODIMP
nsLoggingSink::DidBuildModel(PRInt32 aQualityLevel)
{
  fputs("</begin>\n", mFile);
  return NS_OK;
}

NS_IMETHODIMP
nsLoggingSink::WillInterrupt()
{
  return NS_OK;
}

NS_IMETHODIMP
nsLoggingSink::WillResume()
{
  return NS_OK;
}

NS_IMETHODIMP
nsLoggingSink::OpenContainer(const nsIParserNode& aNode)
{
  return OpenNode("container", aNode);
}

NS_IMETHODIMP
nsLoggingSink::CloseContainer(const nsIParserNode& aNode)
{
  return CloseNode("container");
}

NS_IMETHODIMP
nsLoggingSink::AddLeaf(const nsIParserNode& aNode)
{
  return LeafNode(aNode);
}

NS_IMETHODIMP
nsLoggingSink::PushMark()
{
  return NS_OK;
}

NS_IMETHODIMP
nsLoggingSink::SetTitle(const nsString& aValue)
{
  nsAutoString tmp;
  QuoteText(aValue, tmp);
  fputs("<title value=\"", mFile);
  fputs(tmp, mFile);
  fputs("\"/>", mFile);
  return NS_OK;
}

NS_IMETHODIMP
nsLoggingSink::OpenHTML(const nsIParserNode& aNode)
{
  return OpenNode("html", aNode);
}

NS_IMETHODIMP
nsLoggingSink::CloseHTML(const nsIParserNode& aNode)
{
  return CloseNode("html");
}

NS_IMETHODIMP
nsLoggingSink::OpenHead(const nsIParserNode& aNode)
{
  return OpenNode("head", aNode);
}

NS_IMETHODIMP
nsLoggingSink::CloseHead(const nsIParserNode& aNode)
{
  return CloseNode("head");
}

NS_IMETHODIMP
nsLoggingSink::OpenBody(const nsIParserNode& aNode)
{
  return OpenNode("body", aNode);
}

NS_IMETHODIMP
nsLoggingSink::CloseBody(const nsIParserNode& aNode)
{
  return CloseNode("body");
}

NS_IMETHODIMP
nsLoggingSink::OpenForm(const nsIParserNode& aNode)
{
  return OpenNode("form", aNode);
}

NS_IMETHODIMP
nsLoggingSink::CloseForm(const nsIParserNode& aNode)
{
  return CloseNode("form");
}

NS_IMETHODIMP
nsLoggingSink::OpenMap(const nsIParserNode& aNode)
{
  return OpenNode("map", aNode);
}

NS_IMETHODIMP
nsLoggingSink::CloseMap(const nsIParserNode& aNode)
{
  return CloseNode("map");
}

NS_IMETHODIMP
nsLoggingSink::OpenFrameset(const nsIParserNode& aNode)
{
  return OpenNode("frameset", aNode);
}

NS_IMETHODIMP
nsLoggingSink::CloseFrameset(const nsIParserNode& aNode)
{
  return CloseNode("frameset");
}

nsresult
nsLoggingSink::OpenNode(const char* aKind, const nsIParserNode& aNode)
{
  fprintf(mFile, "<open kind=\"%s\" ", aKind);

  nsHTMLTag nodeType = nsHTMLTag(aNode.GetNodeType());
  if ((nodeType >= eHTMLTag_unknown) &&
      (nodeType <= nsHTMLTag(NS_HTML_TAG_MAX))) {
    const char* tag = NS_EnumToTag(nodeType);
    fprintf(mFile, "tag=\"%s\"", tag);
  }
  else {
    const nsString& text = aNode.GetText();
    fputs("tag=\"", mFile);
    fputs(text, mFile);
    fputs(" \"", mFile);
  }

  if (WillWriteAttributes(aNode)) {
    fputs(">\n", mFile);
    WriteAttributes(aNode);
    fputs("</open>\n", mFile);
  }
  else {
    fputs("/>\n", mFile);
  }

  return NS_OK;
}

nsresult
nsLoggingSink::CloseNode(const char* aKind)
{
  fprintf(mFile, "<close kind=\"%s\"/>\n", aKind);
  return NS_OK;
}


nsresult
nsLoggingSink::WriteAttributes(const nsIParserNode& aNode)
{
  nsAutoString tmp, tmp2;
  PRInt32 ac = aNode.GetAttributeCount();
  for (PRInt32 i = 0; i < ac; i++) {
    const nsString& k = aNode.GetKeyAt(i);
    const nsString& v = aNode.GetValueAt(i);
    fputs(" <attr key=\"", mFile);
    fputs(k, mFile);
    fputs("\" value=\"", mFile);

    tmp.Truncate();
    tmp.Append(v);
    PRUnichar first = tmp.First();
    if ((first == '"') || (first == '\'')) {
      if (tmp.Last() == first) {
        tmp.Cut(0, 1);
        PRInt32 pos = tmp.Length() - 1;
        if (pos >= 0) {
          tmp.Cut(pos, 1);
        }
      } else {
        // Mismatched quotes - leave them in
      }
    }
    QuoteText(tmp, tmp2);
    fputs(tmp2, mFile);
    fputs("\"/>\n", mFile);
  }

  if (0 != strchr(gSkippedContentTags, aNode.GetNodeType())) {
    const nsString& content = aNode.GetSkippedContent();
    if (content.Length() > 0) {
      nsAutoString tmp;
      fputs(" <content value=\"", mFile);
      QuoteText(content, tmp);
      fputs(tmp, mFile);
      fputs("\"/>\n", mFile);
    }
  }

  return NS_OK;
}

PRBool
nsLoggingSink::WillWriteAttributes(const nsIParserNode& aNode)
{
  PRInt32 ac = aNode.GetAttributeCount();
  if (0 != ac) {
    return PR_TRUE;
  }
  if (0 != strchr(gSkippedContentTags, aNode.GetNodeType())) {
    const nsString& content = aNode.GetSkippedContent();
    if (content.Length() > 0) {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

nsresult
nsLoggingSink::LeafNode(const nsIParserNode& aNode)
{
  nsHTMLTag nodeType = nsHTMLTag(aNode.GetNodeType());
  if ((nodeType >= eHTMLTag_unknown) &&
      (nodeType <= nsHTMLTag(NS_HTML_TAG_MAX))) {
    const char* tag = NS_EnumToTag(nodeType);
    fprintf(mFile, "<leaf tag=\"%s\"", tag);
    if (WillWriteAttributes(aNode)) {
      fputs(">\n", mFile);
      WriteAttributes(aNode);
      fputs("</leaf>\n", mFile);
    }
    else {
      fputs("/>\n", mFile);
    }
  }
  else {
    PRInt32 pos;
    nsAutoString tmp;
    switch (nodeType) {
    case eHTMLTag_whitespace:
    case eHTMLTag_text:
      QuoteText(aNode.GetText(), tmp);
      fputs("<text value=\"", mFile);
      fputs(tmp, mFile);
      fputs("\"/>\n", mFile);
      break;

    case eHTMLTag_newline:
      fputs("<newline/>\n", mFile);
      break;

    case eHTMLTag_entity:
      tmp.Append(aNode.GetText());
      tmp.Cut(0, 1);
      pos = tmp.Length() - 1;
      if (pos >= 0) {
        tmp.Cut(pos, 1);
      }
      fputs("<entity value=\"", mFile);
      fputs(tmp, mFile);
      fputs("\"/>\n", mFile);
      break;

    default:
      NS_NOTREACHED("unsupported leaf node type");
    }
  }
  return NS_OK;
}

nsresult
nsLoggingSink::QuoteText(const nsString& aValue, nsString& aResult)
{
  aResult.Truncate();
  const PRUnichar* cp = aValue.GetUnicode();
  const PRUnichar* end = cp + aValue.Length();
  while (cp < end) {
    PRUnichar ch = *cp++;
    if (ch == '"') {
      aResult.Append("&quot;");
    }
    else if (ch == '&') {
      aResult.Append("&amp;");
    }
    else if ((ch < 32) || (ch >= 127)) {
      aResult.Append("&#");
      aResult.Append(PRInt32(ch), 10);
      aResult.Append(';');
    }
    else {
      aResult.Append(ch);
    }
  }
  return NS_OK;
}
