/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is TransforMiiX XSLT processor.
 * 
 * The Initial Developer of the Original Code is The MITRE Corporation.
 * Portions created by MITRE are Copyright (C) 1999 The MITRE Corporation.
 *
 * Portions created by Keith Visco as a Non MITRE employee,
 * (C) 1999 Keith Visco. All Rights Reserved.
 * 
 * Contributor(s): 
 * Tom Kneeland, tomk@mitre.org
 *    -- original author.
 *
 * Keith Visco, kvisco@ziplink.net
 *    -- finished implementation. Too many changes to list here.
 *
 * Bob Miller, Oblix Inc., kbob@oblix.com
 *    -- fixed assignment to "true" to be MB_TRUE, in ::parse()
 *
 * Marina Mechtcheriakova, mmarina@mindspring.com
 *    -- UNICODE fix in method startElement, changed  casting of 
 *       char* to PRUnichar* to use the proper String constructor, 
 *       see method startElement
 *    -- Removed a number of castings of XML_Char to PRUnichar since they
 *       were not working on Windows properly
 *
 */

#include "XMLParser.h"
#include "txURIUtils.h"
#ifndef TX_EXE
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsISyncLoadDOMService.h"
#include "nsNetUtil.h"
#else
#include "xmlparse.h"
#endif

/**
 *  Implementation of an In-Memory DOM based XML parser.  The actual XML
 *  parsing is provided by EXPAT.
**/

/**
 * Creates a new XMLParser
**/
XMLParser::XMLParser()
{
} //-- XMLParser


XMLParser::~XMLParser()
{
    //-- clean up
} //-- ~XMLParser

Document* XMLParser::getDocumentFromURI(const String& href,
                                        Document* aLoader,
                                        String& errMsg)
{
#ifndef TX_EXE
    nsCOMPtr<nsIURI> documentURI;
    nsresult rv = NS_NewURI(getter_AddRefs(documentURI), href);
    NS_ENSURE_SUCCESS(rv, 0);

    nsCOMPtr<nsIDOMDocument> theDocument;
    nsCOMPtr<nsIDocument> loaderDocument = do_QueryInterface(aLoader->getNSObj());
    nsCOMPtr<nsILoadGroup> loadGroup;
    nsCOMPtr<nsIURI> loaderUri;
    loaderDocument->GetDocumentLoadGroup(getter_AddRefs(loadGroup));
    loaderDocument->GetDocumentURL(getter_AddRefs(loaderUri));
    NS_ENSURE_TRUE(loaderUri, 0);

    nsCOMPtr<nsIChannel> channel;
    rv = NS_NewChannel(getter_AddRefs(channel), documentURI, nsnull, loadGroup);
    NS_ENSURE_SUCCESS(rv, 0);

    nsCOMPtr<nsISyncLoadDOMService> loader =
      do_GetService("@mozilla.org/content/syncload-dom-service;1", &rv);
    NS_ENSURE_SUCCESS(rv, 0);
    rv = loader->LoadDocument(channel, loaderUri, getter_AddRefs(theDocument));
    if (NS_FAILED(rv) || !theDocument) {
        errMsg.Append(NS_LITERAL_STRING("Document load of "));
        errMsg.Append(href);
        errMsg.Append(NS_LITERAL_STRING(" failed."));
        return NULL;
    }

    return new Document(theDocument);
#else
    istream* xslInput = URIUtils::getInputStream(href, errMsg);

    Document* resultDoc = 0;
    if ( xslInput ) {
        resultDoc = parse(*xslInput, href);
        delete xslInput;
    }
    if (!resultDoc) {
        errMsg.Append(getErrorString());
    }
    return resultDoc;
#endif

}

#ifdef TX_EXE
/*-------------------------------------
 * Sax related methods for XML parsers
 * ------------------------------------*/
extern "C" {
void charData(void* userData, const XML_Char* s, int len);
int startElement(void *userData, const XML_Char* name, const XML_Char** atts);
int endElement(void *userData, const XML_Char* name);
int piHandler(void *userData, const XML_Char *target, const XML_Char *data);
void commentHandler(void *userData, const XML_Char *s);
}

/**
 *  Parses the given input stream and returns a DOM Document.
 *  A NULL pointer will be returned if errors occurred
**/
Document* XMLParser::parse(istream& inputStream, const String& uri)
{
  const int bufferSize = 1000;

  char buf[bufferSize];
  int done;
  errorString.Truncate();
  if ( !inputStream ) {
    errorString.Append(NS_LITERAL_STRING("unable to parse xml: invalid or unopen stream encountered."));
    return NULL;
  }
  XML_Parser parser = XML_ParserCreate(NULL);
  ParserState ps;
  ps.document = new Document();
  ps.document->documentBaseURI = uri;
  ps.currentNode = ps.document;

  XML_SetUserData(parser, &ps);
  XML_SetElementHandler(parser, startElement, endElement);
  XML_SetCharacterDataHandler(parser, charData);
  XML_SetProcessingInstructionHandler(parser, piHandler);
  XML_SetCommentHandler(parser,commentHandler);
  do
    {
      inputStream.read(buf, bufferSize);
      done = inputStream.eof();

      if (!XML_Parse(parser, buf, inputStream.gcount(), done))
        {
          errorString.getNSString().AppendWithConversion(XML_ErrorString(XML_GetErrorCode(parser)));
          errorString.Append(NS_LITERAL_STRING(" at line "));
          errorString.getNSString().AppendInt(XML_GetCurrentLineNumber(parser));
          done = MB_TRUE;
          delete ps.document;
          ps.document = NULL;
        }
    } while (!done);
    inputStream.clear();

  //if (currentElement)
    //theDocument->appendChild(currentElement);

  // clean up
  XML_ParserFree(parser);

  return ps.document;
}

const String& XMLParser::getErrorString()
{
  return errorString;
}


int startElement(void *userData, const XML_Char *name, const XML_Char **atts)
{
  ParserState* ps = (ParserState*)userData;
  Element* newElement;
  XML_Char** theAtts = (XML_Char**)atts;

  String nodeName((PRUnichar *)name);
  newElement = ps->document->createElement(nodeName);

  while (*theAtts)
    {
      String attName((PRUnichar *)*theAtts++);
      String attValue((PRUnichar *)*theAtts++);
      newElement->setAttribute(attName, attValue);
    }

    ps->currentNode->appendChild(newElement);
    ps->currentNode = newElement;

  return XML_ERROR_NONE;
} //-- startElement

int endElement(void *userData, const XML_Char* name)
{
    ParserState* ps = (ParserState*)userData;
    if (ps->currentNode->getParentNode())
        ps->currentNode = ps->currentNode->getParentNode();
    return XML_ERROR_NONE;
} //-- endElement

void charData(void* userData, const XML_Char* s, int len)
{
    ParserState* ps = (ParserState*)userData;
    String data((PRUnichar*)s, len);
    Node* prevSib = ps->currentNode->getLastChild();
    if (prevSib && prevSib->getNodeType()==Node::TEXT_NODE){
      ((CharacterData*)prevSib)->appendData(data);
    } else {
      ps->currentNode->appendChild(ps->document->createTextNode(data));
    };
} //-- charData

void commentHandler(void* userData, const XML_Char* s)
{
    ParserState* ps = (ParserState*)userData;
    String data((PRUnichar*)s);
    ps->currentNode->appendChild(ps->document->createComment(data));
} //-- commentHandler

/**
 * Handles ProcessingInstructions
**/
int piHandler(void *userData, const XML_Char *target, const XML_Char *data) {
    ParserState* ps = (ParserState*)userData;
    String targetStr((PRUnichar *)target);
    String dataStr((PRUnichar *)data);

    ps->currentNode->appendChild(
        ps->document->createProcessingInstruction(targetStr, dataStr));

    return XML_ERROR_NONE;
} //-- piHandler

#endif
