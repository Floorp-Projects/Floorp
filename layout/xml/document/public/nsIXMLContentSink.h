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
#ifndef nsIXMLContentSink_h___
#define nsIXMLContentSink_h___

#include "nsIContentSink.h"
#include "nsIParserNode.h"
#include "nsISupports.h"

class nsIDocument;
class nsIURI;
class nsIWebShell;

#define NS_IXMLCONTENT_SINK_IID \
 { 0xa6cf90c9, 0x15b3, 0x11d2, \
 { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } }

/**
 * This interface represents a content sink for generic XML files.
 * The goal of this sink is to deal with XML documents that do not
 * have pre-built semantics, though it may also be implemented for
 * cases in which semantics are hard-wired.
 *
 * The expectation is that the parser has already performed
 * well-formedness and validity checking.
 *
 * XXX The expectation is that entity expansion will be done by the sink
 * itself. This would require, however, that the sink has the ability
 * to query the parser for entity replacement text.
 *
 * XXX This interface does not contain a mechanism for the sink to
 * get specific schema/DTD information from the parser. This information
 * may be necessary for entity expansion. It is also necessary for 
 * building the DOM portions that relate to the schema.
 *
 * XXX This interface does not deal with the presence of an external
 * subset. It seems possible that this could be dealt with completely
 * at the parser level.
 */

class nsIXMLContentSink : public nsIContentSink {
public:

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IXMLCONTENT_SINK_IID)

  /**
   * This method is called by the parser when it encounters
   * the XML declaration for a document.
   *
   * @param  nsIParserNode reference to parser node interface
   */
  NS_IMETHOD AddXMLDecl(const nsIParserNode& aNode)=0;

  /**
   * This method is called by the parser when it encounters
   * character data - either regular CDATA or a marked CDATA
   * section.
   *
   * XXX Could be removed in favor of nsIContentSink::AddLeaf
   *
   * @param  nsIParserNode reference to parser node interface
   */
  NS_IMETHOD AddCharacterData(const nsIParserNode& aNode)=0;

  /**
   * This method is called by the parser when it encounters
   * an unparsed entity (i.e. NDATA)
   *
   * @param  nsIParserNode reference to parser node interface
   */
  NS_IMETHOD AddUnparsedEntity(const nsIParserNode& aNode)=0;

  /**
   * This method is called by the parser when it encounters
   * a notation.
   *
   * @param  nsIParserNode reference to parser node interface
   */
  NS_IMETHOD AddNotation(const nsIParserNode& aNode)=0;

  /**
   * This method is called by the parser when it encounters
   * an entity reference. Note that the expectation is that
   * the content sink itself will expand the entity reference
   * in the content model.
   *
   * @param  nsIParserNode reference to parser node interface
   */
  NS_IMETHOD AddEntityReference(const nsIParserNode& aNode)=0;


};

extern nsresult NS_NewXMLContentSink(nsIXMLContentSink** aInstancePtrResult,
                                     nsIDocument* aDoc,
                                     nsIURI* aURL,
                                     nsIWebShell* aWebShell);

#endif // nsIXMLContentSink_h___
