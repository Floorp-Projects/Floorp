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
 *       char* to DOM_CHAR* to use the proper String constructor, 
 *       see method startElement
 *    -- Removed a number of castings of XML_Char to DOM_CHAR since they
 *       were not working on Windows properly
 *
 */

#include "XMLParser.h"
#include "URIUtils.h"
#ifndef TX_EXE
#include "nsSyncLoader.h"
#include "nsNetUtil.h"
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
#ifdef TX_EXE
  errorState = MB_FALSE;
#endif
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
    nsresult rv = NS_NewURI(getter_AddRefs(documentURI), href.getConstNSString());
    NS_ENSURE_SUCCESS(rv, NULL);

    nsCOMPtr<nsISyncLoader> loader = do_CreateInstance(TRANSFORMIIX_SYNCLOADER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, NULL);

    nsCOMPtr<nsIDOMDocument> theDocument;
    nsCOMPtr<nsIDocument> loaderDocument = do_QueryInterface(aLoader->getNSObj());
    rv = loader->LoadDocument(documentURI, loaderDocument, getter_AddRefs(theDocument));
    if (NS_FAILED(rv) || !theDocument) {
        errMsg.append("Document load of ");
        errMsg.append(href);
        errMsg.append(" failed.");
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
        errMsg.append(getErrorString());
    }
    return resultDoc;
#endif

}

#ifdef TX_EXE
/**
 *  Parses the given input stream and returns a DOM Document.
 *  A NULL pointer will be returned if errors occurred
**/
Document* XMLParser::parse(istream& inputStream, const String& uri)
{
  const int bufferSize = 1000;

  char buf[bufferSize];
  int done;
  errorState = MB_FALSE;
  errorString.clear();
  if ( !inputStream ) {
    errorString.append("unable to parse xml: invalid or unopen stream encountered.");
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
          errorString.append(XML_ErrorString(XML_GetErrorCode(parser)));
          errorString.append(" at line ");
          errorString.append(XML_GetCurrentLineNumber(parser));
          done = MB_TRUE;
          errorState = MB_TRUE;
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


void startElement(void *userData, const XML_Char *name, const XML_Char **atts)
{
  ParserState* ps = (ParserState*)userData;
  Element* newElement;
  XML_Char* attName;
  XML_Char* attValue;
  XML_Char** theAtts = (XML_Char**)atts;

  String nodeName((UNICODE_CHAR *)name);
  newElement = ps->document->createElement(nodeName);

  while (*theAtts)
    {
      attName  = *theAtts++;
      attValue = *theAtts++;
      newElement->setAttribute((UNICODE_CHAR *)attName, (UNICODE_CHAR *)attValue);
    }

    ps->currentNode->appendChild(newElement);
    ps->currentNode = newElement;

} //-- startElement

void endElement(void *userData, const XML_Char* name)
{
    ParserState* ps = (ParserState*)userData;
    if (ps->currentNode->getParentNode())
        ps->currentNode = ps->currentNode->getParentNode();
} //-- endElement

void charData(void* userData, const XML_Char* s, int len)
{
    ParserState* ps = (ParserState*)userData;
    String data;
    data.append((UNICODE_CHAR*)s, len);
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
    String data((UNICODE_CHAR*)s);
    ps->currentNode->appendChild(ps->document->createComment(data));
} //-- commentHandler

/**
 * Handles ProcessingInstructions
**/
void piHandler(void *userData, const XML_Char *target, const XML_Char *data) {
    ParserState* ps = (ParserState*)userData;
    String targetStr((UNICODE_CHAR *)target);
    String dataStr((UNICODE_CHAR *)data);

    ps->currentNode->appendChild(
        ps->document->createProcessingInstruction(targetStr, dataStr));

} //-- piHandler

#endif
