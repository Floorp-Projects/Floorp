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
 * Axel Hecht.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "txStandaloneStylesheetCompiler.h"
#include "TxLog.h"
#include "txStylesheetCompiler.h"
#include "txURIUtils.h"
#include "xmlparse.h"

/**
 *  Implementation of an In-Memory DOM based XML parser.  The actual XML
 *  parsing is provided by EXPAT.
 */
class txDriver : public txACompileObserver
{
  public:
    nsresult parse(istream& aInputStream, const nsAString& aUri);
    const nsAString& getErrorString();

    /**
     * Expat handlers
     */
    void StartElement(const XML_Char *aName, const XML_Char **aAtts);
    void EndElement(const XML_Char* aName);
    void CharacterData(const XML_Char* aChars, int aLength);
    int ExternalEntityRef(const XML_Char *aContext, const XML_Char *aBase,
                          const XML_Char *aSystemId,
                          const XML_Char *aPublicId);

    TX_DECL_ACOMPILEOBSERVER;

    nsRefPtr<txStylesheetCompiler> mCompiler;
  protected:
    void createErrorString();
    nsString  mErrorString;
    // keep track of the nsresult returned by the handlers, expat forgets them
    nsresult mRV;
    XML_Parser mExpatParser;
    nsAutoRefCnt mRefCnt;
};

nsresult
TX_CompileStylesheetPath(const txParsedURL& aURL, txStylesheet** aResult)
{
    *aResult = nsnull;
    nsAutoString errMsg, filePath;

    aURL.getFile(filePath);
    PR_LOG(txLog::xslt, PR_LOG_ALWAYS,
           ("TX_CompileStylesheetPath: %s\n",
            NS_LossyConvertUCS2toASCII(filePath).get()));
    istream* xslInput = URIUtils::getInputStream(filePath, errMsg);
    if (!xslInput) {
        return NS_ERROR_FAILURE;
    }
    nsRefPtr<txDriver> driver = new txDriver();
    if (!driver) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    nsAutoString spec = filePath;
    if (!aURL.mRef.IsEmpty()) {
        spec.Append(PRUnichar('#'));
        spec.Append(aURL.mRef);
    }
    driver->mCompiler =  new txStylesheetCompiler(spec, driver);
    if (!driver->mCompiler) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    nsresult rv = driver->parse(*xslInput, filePath);
    if (NS_FAILED(rv)) {
        return rv;
    };
    *aResult = driver->mCompiler->getStylesheet();
    NS_ADDREF(*aResult);
    return NS_OK;
}

/**
 * expat C stub handlers
 */

// shortcut macro for redirection into txDriver method calls
#define TX_DRIVER(_userData) NS_STATIC_CAST(txDriver*, _userData)

PR_STATIC_CALLBACK(void)
startElement(void *aUserData, const XML_Char *aName, const XML_Char **aAtts)
{
    if (!aUserData) {
        NS_WARNING("no userData in startElement handler");
        return;
    }
    TX_DRIVER(aUserData)->StartElement(aName, aAtts);
}

PR_STATIC_CALLBACK(void)
endElement(void *aUserData, const XML_Char* aName)
{
    if (!aUserData) {
        NS_WARNING("no userData in endElement handler");
        return;
    }
    TX_DRIVER(aUserData)->EndElement(aName);
}

PR_STATIC_CALLBACK(void)
charData(void* aUserData, const XML_Char* aChars, int aLength)
{
    if (!aUserData) {
        NS_WARNING("no userData in charData handler");
        return;
    }
    TX_DRIVER(aUserData)->CharacterData(aChars, aLength);
}

PR_STATIC_CALLBACK(int)
externalEntityRefHandler(XML_Parser aParser,
                         const XML_Char *aContext,
                         const XML_Char *aBase,
                         const XML_Char *aSystemId,
                         const XML_Char *aPublicId)
{
    // aParser is aUserData is the txDriver,
    // we set that in txDriver::parse
    NS_ENSURE_TRUE(aParser, XML_ERROR_NONE);
    return TX_DRIVER(aParser)->ExternalEntityRef(aContext, aBase,
                                                 aSystemId, aPublicId);
}


/**
 *  Parses the given input stream and returns a DOM Document.
 *  A NULL pointer will be returned if errors occurred
 */
nsresult
txDriver::parse(istream& aInputStream, const nsAString& aUri)
{
    mErrorString.Truncate();
    if (!aInputStream) {
        mErrorString.Append(NS_LITERAL_STRING("unable to parse xml: invalid or unopen stream encountered."));
        return NS_ERROR_FAILURE;
    }
    mExpatParser = XML_ParserCreate(nsnull);
    if (!mExpatParser) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    XML_SetUserData(mExpatParser, this);
    XML_SetElementHandler(mExpatParser, startElement, endElement);
    XML_SetCharacterDataHandler(mExpatParser, charData);
#ifdef XML_DTD
    XML_SetParamEntityParsing(mExpatParser, XML_PARAM_ENTITY_PARSING_ALWAYS);
#endif
    XML_SetExternalEntityRefHandler(mExpatParser, externalEntityRefHandler);
    XML_SetExternalEntityRefHandlerArg(mExpatParser, this);
    XML_SetBase(mExpatParser,
                (const XML_Char*)(PromiseFlatString(aUri).get()));

    const int bufferSize = 1024;
    char buf[bufferSize];
    PRBool done;
    int success;
    mRV = NS_OK;
    do {
        aInputStream.read(buf, bufferSize);
        done = aInputStream.eof();
        success = XML_Parse(mExpatParser, buf, aInputStream.gcount(), done);
        // mRV is set in onDoneCompiling in case of an error
        if (!success || NS_FAILED(mRV)) {
            createErrorString();
            done = MB_TRUE;
        }
    } while (!done);
    aInputStream.clear();

    // clean up
    XML_ParserFree(mExpatParser);
    mCompiler->doneLoading();
    if (!success) {
        return NS_ERROR_FAILURE;
    }
    return mRV;
}

const nsAString&
txDriver::getErrorString()
{
    return mErrorString;
}

void
txDriver::StartElement(const XML_Char *aName, const XML_Char **aAtts)
{
    PRInt32 attcount = 0;
    const XML_Char** atts = aAtts;
    while (*atts) {
        ++atts;
        ++attcount;
    }
    PRInt32 idOffset = XML_GetIdAttributeIndex(mExpatParser);
    nsresult rv =
        mCompiler->startElement(NS_STATIC_CAST(const PRUnichar*, aName), 
                                NS_STATIC_CAST(const PRUnichar**, aAtts),
                                attcount/2, idOffset);
    if (NS_FAILED(rv)) {
        PR_LOG(txLog::xslt, PR_LOG_ALWAYS, 
               ("compile failed at %i with %x\n",
                XML_GetCurrentLineNumber(mExpatParser), rv));
        mCompiler->cancel(rv);
    }
}

void
txDriver::EndElement(const XML_Char* aName)
{
    nsresult rv = mCompiler->endElement();
    if (NS_FAILED(rv)) {
        mCompiler->cancel(rv);
    }
}

void
txDriver::CharacterData(const XML_Char* aChars, int aLength)
{
    const PRUnichar* pChars = NS_STATIC_CAST(const PRUnichar*, aChars);
    // ignore rv, as this expat handler returns void
    nsresult rv = mCompiler->characters(Substring(pChars, pChars + aLength));
    if (NS_FAILED(rv)) {
        mCompiler->cancel(rv);
    }
}

int
txDriver::ExternalEntityRef(const XML_Char *aContext, const XML_Char *aBase,
                            const XML_Char *aSystemId,
                            const XML_Char *aPublicId)
{
    if (aPublicId) {
        // not supported, this is "http://some.site.net/foo.dtd" stuff
        return XML_ERROR_EXTERNAL_ENTITY_HANDLING;
    }
    nsAutoString absUrl;
    URIUtils::resolveHref(nsDependentString((PRUnichar*)aSystemId),
                          nsDependentString((PRUnichar*)aBase), absUrl);
    istream* extInput = URIUtils::getInputStream(absUrl, mErrorString);
    if (!extInput) {
        return XML_ERROR_EXTERNAL_ENTITY_HANDLING;
    }
    XML_Parser parent = mExpatParser;
    mExpatParser = 
        XML_ExternalEntityParserCreate(mExpatParser, aContext, nsnull);
    if (!mExpatParser) {
        mExpatParser = parent;
        delete extInput;
        return XML_ERROR_EXTERNAL_ENTITY_HANDLING;
    }
    XML_SetBase(mExpatParser, absUrl.get());

    const int bufSize = 1024;
    char buffer[bufSize];
    int result;
    PRBool done;
    do {
        extInput->read(buffer, bufSize);
        done = extInput->eof();
        if (!(result =
              XML_Parse(mExpatParser, buffer,  extInput->gcount(), done))) {
            createErrorString();
            mErrorString.Append(PRUnichar('\n'));
            done = MB_TRUE;
        }
    } while (!done);

    delete extInput;
    XML_ParserFree(mExpatParser);

    mExpatParser = parent;

    return result;
}

void
txDriver::createErrorString()
{
    XML_Error errCode = XML_GetErrorCode(mExpatParser);
    mErrorString.AppendWithConversion(XML_ErrorString(errCode));
    mErrorString.Append(NS_LITERAL_STRING(" at line "));
    mErrorString.AppendInt(XML_GetCurrentLineNumber(mExpatParser));
    mErrorString.Append(NS_LITERAL_STRING(" in "));
    mErrorString.Append((const PRUnichar*)XML_GetBase(mExpatParser));
}

/**
 * txACompileObserver implementation
 */

nsrefcnt
txDriver::AddRef()
{
    return ++mRefCnt;
}

nsrefcnt
txDriver::Release()
{
    if (--mRefCnt == 0) {
        mRefCnt = 1; //stabilize
        delete this;
        return 0;
    }
    return mRefCnt;
}

void
txDriver::onDoneCompiling(txStylesheetCompiler* aCompiler, nsresult aResult,
                          const PRUnichar *aErrorText, const PRUnichar *aParam)
{
    // store the nsresult as expat forgets about it
    mRV = aResult;
}

nsresult
txDriver::loadURI(const nsAString& aUri, txStylesheetCompiler* aCompiler)
{
    nsAutoString errMsg;
    istream* xslInput = URIUtils::getInputStream(aUri, errMsg);
    if (!xslInput) {
        return NS_ERROR_FAILURE;
    }
    nsRefPtr<txDriver> driver = new txDriver();
    if (!driver) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    driver->mCompiler = aCompiler;
    return driver->parse(*xslInput, aUri);
}
