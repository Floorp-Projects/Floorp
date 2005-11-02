/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is TransforMiiX XSLT processor code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peterv@propagandism.org>
 *   Axel Hecht <axel@pike.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef TRANSFRMX_TXSTANDALONEXSLTPROCESSOR_H
#define TRANSFRMX_TXSTANDALONEXSLTPROCESSOR_H

#include "txStylesheet.h"
#include "txXSLTProcessor.h"
#include "ErrorObserver.h"

#ifndef __BORLANDC__
#include <iostream.h>
#include <fstream.h>
#endif

class txStreamXMLEventHandler;

/**
 * txStandaloneXSLTProcessor
 *
 * Use of the standalone TransforMiiX API:
 *
 * The XSLT Processor need initialisation and shutdown
 * by
 *  txStandaloneXSLTProcessor::Init();
 * and
 *  txStandaloneXSLTProcessor::Shutdown();
 * Be sure to always call these functions in pairs.
 *
 * The API to transform documents consists of entry points
 * to transform either one or two documents.
 * If you provide one document, the stylesheet location is
 * computed from the processing instruction. If that cannot
 * be found, an error is issued.
 *
 * The result is output to a stream.
 *
 * Documents can be provided either by path or by DOM Document.
 *
 * Stylesheet parameters
 *  XXX TODO 
 * 
 */

class txStandaloneXSLTProcessor : public txXSLTProcessor
{
public:
    /**
     * Methods that print the result to a stream
     */

    /**
     * Transform a XML document given by path.
     * The stylesheet is retrieved by a processing instruction,
     * or an error is returned.
     *
     * @param aXMLPath path to the source document
     * @param aOut     stream to which the result is feeded
     * @param aErr     error observer
     * @result NS_OK if transformation was successful
     */
    nsresult transform(nsACString& aXMLPath, ostream& aOut,
                       ErrorObserver& aErr);

    /**
     * Transform a XML document given by path with the given
     * stylesheet.
     *
     * @param aXMLPath path to the source document
     * @param aXSLPath path to the style document
     * @param aOut     stream to which the result is feeded
     * @param aErr     error observer
     * @result NS_OK if transformation was successful
     */
    nsresult transform(nsACString& aXMLPath, nsACString& aXSLPath,
                       ostream& aOut, ErrorObserver& aErr);

    /**
     * Transform a XML document.
     * The stylesheet is retrieved by a processing instruction,
     * or an error is returned.
     *
     * @param aXMLDoc source document
     * @param aOut    stream to which the result is feeded
     * @param aErr    error observer
     * @result NS_OK if transformation was successful
     */
    nsresult transform(txXPathNode& aXMLDoc, ostream& aOut, ErrorObserver& aErr);

    /**
     * Transform a XML document with the given stylesheet.
     *
     * @param aXMLDoc  source document
     * @param aXSLNode style node
     * @param aOut     stream to which the result is feeded
     * @param aErr     error observer
     * @result NS_OK if transformation was successful
     */
    nsresult transform(txXPathNode& aXMLDoc, txStylesheet* aXSLNode,
                       ostream& aOut, ErrorObserver& aErr);

protected:
    /**
     * Parses all XML Stylesheet PIs associated with the
     * given XML document. If any stylesheet PIs are found with
     * type="text/xsl" the href pseudo attribute value will be
     * added to the given href argument. If multiple text/xsl stylesheet PIs
     * are found, the one closest to the end of the document is used.
     */
    static void getHrefFromStylesheetPI(Document& xmlDocument, nsAString& href);

    /**
     * Parses the contents of data, returns the type and href pseudo attributes
     */
    static void parseStylesheetPI(const nsAFlatString& data,
                                  nsAString& type,
                                  nsAString& href);

    /**
     * Create a Document from a path.
     *
     * @param aPath path to the xml file
     * @param aErr  ErrorObserver
     * @result Document XML Document, or null on error
     */
    static txXPathNode* parsePath(const nsACString& aPath, ErrorObserver& aErr);
};

#endif
