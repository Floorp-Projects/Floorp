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
 * Tom Kneeland
 *    -- original author.
 * Keith Visco 
 *    -- finished implementation
 *
 * $Id: XMLParser.h,v 1.6 2000/08/27 05:58:58 kvisco%ziplink.net Exp $
 */

#include <iostream.h>
#ifndef XML_UNICODE
#define XML_UNICODE
#endif
#ifndef MOZ_XSL
#include "xmlparse.h"
#include "URIUtils.h"
#endif
#include "dom.h"

typedef struct  {
    Document* document;
    Node*  currentNode;
} ParserState;

/**
 * Implementation of an In-Memory DOM based XML parser.  The actual XML
 * parsing is provided by EXPAT.
 * @author <a href="tomk@mitre.org">Tom Kneeland</a>
 * @author <a href="kvisco@ziplink.net">Keith Visco</a>
 * @version $Revision: 1.6 $ $Date: 2000/08/27 05:58:58 $
**/
class XMLParser
{
#ifndef MOZ_XSL
  /*-----------------6/18/99 12:43PM------------------
   * Sax related methods for XML parsers
   * --------------------------------------------------*/
  friend void charData(void* userData, const XML_Char* s, int len);
  friend void startElement(void *userData, const XML_Char* name,
                           const XML_Char** atts);
  friend void endElement(void *userData, const XML_Char* name);

  friend void piHandler(void *userData, const XML_Char *target, const XML_Char *data);
#endif

  public:
    XMLParser();
   ~XMLParser();

    Document* getDocumentFromURI(String& href, String& documentBase, String& errMsg);
#ifndef MOZ_XSL
    Document* parse(istream& inputStream);
    const String& getErrorString();

  protected:

    Document*  theDocument;
    Element*   currentElement;
    MBool      errorState;
    String  errorString;
#endif
};

#ifndef MOZ_XSL
/*-----------------6/18/99 12:43PM------------------
 * Sax related methods for XML parsers
 * --------------------------------------------------*/
void charData(void* userData, const XML_Char* s, int len);
void startElement(void *userData, const XML_Char* name, const XML_Char** atts);
void endElement(void *userData, const XML_Char* name);
void piHandler(void *userData, const XML_Char *target, const XML_Char *data);
#endif
