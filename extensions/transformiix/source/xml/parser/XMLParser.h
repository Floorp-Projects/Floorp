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
 */

#ifndef MITRE_XMLPARSER_H
#define MITRE_XMLPARSER_H

#include "dom.h"

#ifdef TX_EXE
#include <iostream.h>

typedef struct  {
    Document* document;
    Node*  currentNode;
} ParserState;
#endif

/**
 * Implementation of an In-Memory DOM based XML parser.  The actual XML
 * parsing is provided by EXPAT.
**/
class XMLParser
{
  public:
    XMLParser();
   ~XMLParser();

    Document* getDocumentFromURI(const String& href, Document* aLoader, String& errMsg);
#ifdef TX_EXE
    Document* parse(istream& inputStream, const String& uri);
    const String& getErrorString();

  protected:
    String  errorString;
#endif
};

#endif
