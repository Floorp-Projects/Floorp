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

#include <iostream.h>
#include "baseutils.h"
#include "xmlparse.h"
#include "DOM.h"

typedef struct  {
    Document* document;
    Node*  currentNode;
} ParserState;

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
 * KV   06/15/1999   Changed #parse method to return document
 * KV   06/17/1999   Made many changes
 *
**/
class XMLParser
{
  /*-----------------6/18/99 12:43PM------------------
   * Sax related methods for XML parsers
   * --------------------------------------------------*/
  friend void charData(void* userData, const XML_Char* s, int len);
  friend void startElement(void *userData, const XML_Char* name,
                           const XML_Char** atts);
  friend void endElement(void *userData, const XML_Char* name);

  friend void piHandler(void *userData, const XML_Char *target, const XML_Char *data);

  public:
    XMLParser();
   ~XMLParser();

    Document* parse(istream& inputStream);
    const DOMString& getErrorString();

  protected:

    Document*  theDocument;
    Element*   currentElement;
    MBool      errorState;
    DOMString  errorString;
};

/*-----------------6/18/99 12:43PM------------------
 * Sax related methods for XML parsers
 * --------------------------------------------------*/
void charData(void* userData, const XML_Char* s, int len);
void startElement(void *userData, const XML_Char* name, const XML_Char** atts);
void endElement(void *userData, const XML_Char* name);
void piHandler(void *userData, const XML_Char *target, const XML_Char *data);
