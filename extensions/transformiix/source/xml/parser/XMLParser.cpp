/*
 * (C) Copyright The MITRE Corporation 1999  All rights reserved.
 *
 * The program is provided "as is" without any warranty express or
 * implied, including the warranty of non-infringement and the implied
 * warranties of merchantibility and fitness for a particular purpose.
 * The Copyright owner will not be liable for any damages suffered by
 * you as a result of using the Program. In no event will the Copyright
 * owner be liable for any special, indirect or consequential damages or
 * lost profits even if the Copyright owner has been advised of the
 * possibility of their occurrence.
 */

#include "xmlparser.h"

/**
 *  Implementation of an In-Memory DOM based XML parser.  The actual XML
 *  parsing is provided by EXPAT.
 *
 * @author <a href="tomk@mitre.org">Tom Kneeland</a>
 * @author <a href="kvisco@mitre.org">Keith Visco</a>
 *
 * Modification History:
 * Who  When         What
 * TK   05/03/99     Created
 * KV   06/15/1999   Fixed bug in parse method which read from cin instead of
 *                   the istream parameter.
 * KV   06/15/1999   Changed #parse method to return a Document
 * KV   06/17/1999   made a bunch of changes
 *
**/

/**
 * Creates a new XMLParser
**/
XMLParser::XMLParser()
{
  errorState = MB_FALSE;
} //-- XMLParser


XMLParser::~XMLParser()
{
    //-- clean up
} //-- ~XMLParser

  /**
   *  Parses the given input stream and returns a DOM Document.
   *  A NULL pointer will be returned if errors occurred
  **/
Document* XMLParser::parse(istream& inputStream)
{
  const int bufferSize = 1000;

  char buf[bufferSize];
  int done;
  errorState = MB_FALSE;
  errorString.clear();
  if ( !inputStream ) {
    errorString.append("unable to parse xml, invalid or unopen stream");
    return NULL;
  }
  XML_Parser parser = XML_ParserCreate(NULL);
  ParserState ps;
  ps.document = new Document();
  ps.currentNode = ps.document;

  XML_SetUserData(parser, &ps);
  XML_SetElementHandler(parser, startElement, endElement);
  XML_SetCharacterDataHandler(parser, charData);
  XML_SetProcessingInstructionHandler(parser, piHandler);
  do
    {
      inputStream.read(buf, bufferSize);
      done = inputStream.eof();

      if (!XML_Parse(parser, buf, inputStream.gcount(), done))
        {
          errorString.append(XML_ErrorString(XML_GetErrorCode(parser)));
          errorString.append(" at line ");
          errorString.append(XML_GetCurrentLineNumber(parser));
          done = true;
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

const DOMString& XMLParser::getErrorString()
{
  return errorString;
}

void startElement(void *userData, const XML_Char *name, const XML_Char **atts)
{
  ParserState* ps = (ParserState*)userData;
  Element* newElement;
  Attr* newAttribute;
  DOM_CHAR* attName;
  DOM_CHAR* attValue;
  XML_Char** theAtts = (XML_Char**)atts;

  newElement = ps->document->createElement((DOM_CHAR*) name);

  while (*theAtts)
    {
      attName = (DOM_CHAR*)*theAtts++;
      attValue = (DOM_CHAR*)*theAtts++;
      newElement->setAttribute(attName, attValue);
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
    DOMString data;
    data.append((const DOM_CHAR*)s, len);
    ps->currentNode->appendChild(ps->document->createTextNode(data));
} //-- charData

/**
 * Handles ProcessingInstructions
**/
void piHandler(void *userData, const XML_Char *target, const XML_Char *data) {
    ParserState* ps = (ParserState*)userData;
    DOMString targetStr((const DOM_CHAR*) target);
    DOMString dataStr((const DOM_CHAR*) data);

    ps->currentNode->appendChild(
        ps->document->createProcessingInstruction(targetStr, dataStr));

} //-- piHandler

