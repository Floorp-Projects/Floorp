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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communiactions Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *      Boris Zbarsky <bzbarsky@mit.edu>  (original author)
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

#include "nscore.h"
#include "nsLoadSaveContentSink.h"

nsresult
NS_NewLoadSaveContentSink(nsILoadSaveContentSink** aResult,
                          nsIXMLContentSink* aBaseSink)
{
  NS_PRECONDITION(aResult, "Null out ptr?  Who do you think you are, flouting XPCOM contract?");
  NS_ENSURE_ARG_POINTER(aBaseSink);
  nsLoadSaveContentSink* it;
  NS_NEWXPCOM(it, nsLoadSaveContentSink);
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  nsresult rv = it->Init(aBaseSink);
  if (NS_FAILED(rv)) {
    delete it;
    return rv;
  }

  return CallQueryInterface(it, aResult);  
}

nsLoadSaveContentSink::nsLoadSaveContentSink()
{
}

nsLoadSaveContentSink::~nsLoadSaveContentSink()
{
}

nsresult
nsLoadSaveContentSink::Init(nsIXMLContentSink* aBaseSink)
{
  NS_PRECONDITION(aBaseSink, "aBaseSink needs to exist");
  mBaseSink = aBaseSink;
  mExpatSink = do_QueryInterface(aBaseSink);
  if (!mExpatSink) {
    return NS_ERROR_INVALID_ARG;
  }

  return NS_OK;
}

NS_IMPL_THREADSAFE_ADDREF(nsLoadSaveContentSink)
NS_IMPL_THREADSAFE_RELEASE(nsLoadSaveContentSink)

NS_INTERFACE_MAP_BEGIN(nsLoadSaveContentSink)
  NS_INTERFACE_MAP_ENTRY(nsIXMLContentSink)
  NS_INTERFACE_MAP_ENTRY(nsIExpatSink)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXMLContentSink)
NS_INTERFACE_MAP_END

// nsIContentSink
NS_IMETHODIMP
nsLoadSaveContentSink::WillBuildModel(void)
{
  return mBaseSink->WillBuildModel();
}

NS_IMETHODIMP
nsLoadSaveContentSink::DidBuildModel(void)
{
  return mBaseSink->DidBuildModel();
}

NS_IMETHODIMP
nsLoadSaveContentSink::WillInterrupt(void)
{
  return mBaseSink->WillInterrupt();
}

NS_IMETHODIMP
nsLoadSaveContentSink::WillResume(void)
{
  return mBaseSink->WillResume();
}

NS_IMETHODIMP
nsLoadSaveContentSink::SetParser(nsIParser* aParser)
{
  return mBaseSink->SetParser(aParser);
}

void
nsLoadSaveContentSink::FlushContent(PRBool aNotify)
{
  mBaseSink->FlushContent(aNotify);
}

NS_IMETHODIMP
nsLoadSaveContentSink::SetDocumentCharset(nsAString& aCharset)
{
  return mBaseSink->SetDocumentCharset(aCharset);
}

// nsIExpatSink

NS_IMETHODIMP
nsLoadSaveContentSink::HandleStartElement(const PRUnichar *aName, 
                                          const PRUnichar **aAtts, 
                                          PRUint32 aAttsCount, 
                                          PRInt32 aIndex, 
                                          PRUint32 aLineNumber)
{
  return mExpatSink->HandleStartElement(aName, aAtts, aAttsCount, aIndex,
                                        aLineNumber);
}

NS_IMETHODIMP
nsLoadSaveContentSink::HandleEndElement(const PRUnichar *aName)
{
  return mExpatSink->HandleEndElement(aName);
}

NS_IMETHODIMP
nsLoadSaveContentSink::HandleComment(const PRUnichar *aName)
{
  return mExpatSink->HandleComment(aName);
}

NS_IMETHODIMP
nsLoadSaveContentSink::HandleCDataSection(const PRUnichar *aData, 
                                          PRUint32 aLength)
{
  return mExpatSink->HandleCDataSection(aData, aLength);
}

NS_IMETHODIMP
nsLoadSaveContentSink::HandleDoctypeDecl(const nsAString & aSubset, 
                                         const nsAString & aName, 
                                         const nsAString & aSystemId, 
                                         const nsAString & aPublicId,
                                         nsISupports* aCatalogData)
{
  return mExpatSink->HandleDoctypeDecl(aSubset, aName, aSystemId, aPublicId,
                                       aCatalogData);
}

NS_IMETHODIMP
nsLoadSaveContentSink::HandleCharacterData(const PRUnichar *aData, 
                                           PRUint32 aLength)
{
  return mExpatSink->HandleCharacterData(aData, aLength);
}

NS_IMETHODIMP
nsLoadSaveContentSink::HandleProcessingInstruction(const PRUnichar *aTarget, 
                                                   const PRUnichar *aData)
{
  return mExpatSink->HandleProcessingInstruction(aTarget, aData);
}

NS_IMETHODIMP
nsLoadSaveContentSink::HandleXMLDeclaration(const PRUnichar* aData,
                                            PRUint32 aLength)
{
  return mExpatSink->HandleXMLDeclaration(aData, aLength);
}

NS_IMETHODIMP
nsLoadSaveContentSink::ReportError(const PRUnichar* aErrorText, 
                                   const PRUnichar* aSourceText)
{
  // XXX Do error reporting here.  I see no reason to call ReportError
  // on the "base" sink; all we need to do is drop the document on the
  // floor...
  return NS_OK;
}
