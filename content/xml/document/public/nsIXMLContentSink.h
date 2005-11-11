/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
#ifndef nsIXMLContentSink_h___
#define nsIXMLContentSink_h___

#include "nsIContentSink.h"
#include "nsIParserNode.h"
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
