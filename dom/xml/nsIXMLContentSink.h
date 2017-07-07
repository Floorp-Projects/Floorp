/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsIXMLContentSink_h___
#define nsIXMLContentSink_h___

#include "nsIContentSink.h"
#include "nsISupports.h"

class nsIDocument;
class nsIURI;
class nsIChannel;

#define NS_IXMLCONTENT_SINK_IID \
 { 0x63fedea0, 0x9b0f, 0x4d64, \
 { 0x9b, 0xa5, 0x37, 0xc6, 0x99, 0x73, 0x29, 0x35 } }

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

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IXMLCONTENT_SINK_IID)

};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIXMLContentSink, NS_IXMLCONTENT_SINK_IID)

nsresult
NS_NewXMLContentSink(nsIXMLContentSink** aInstancePtrResult, nsIDocument* aDoc,
                     nsIURI* aURL, nsISupports* aContainer,
                     nsIChannel *aChannel);

#endif // nsIXMLContentSink_h___
