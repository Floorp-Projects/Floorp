/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *  
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *  
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): Akkana Peck.
 */

#include "nsTextConverter.h"

#include "nsParserCIID.h"
#include "nsIParser.h"
#include "CNavDTD.h"
#include "nsXIFDTD.h"
#include "nsHTMLContentSinkStream.h"
#include "nsHTMLToTXTSinkStream.h"

#include "nsCOMPtr.h"
#include "nsIStringStream.h"

//////////////////////////////////////////////////
// nsTextConverter
//////////////////////////////////////////////////

NS_IMPL_ISUPPORTS2(nsTextConverter, nsIStreamConverter, nsIStreamListener);

nsTextConverter::nsTextConverter()
{
    NS_INIT_ISUPPORTS();
}

// Convert from mime type aFromType), to _retval
// (nsIInputStream of mime type aToType).
NS_IMETHODIMP
nsTextConverter::Convert(nsIInputStream *aFromStream, 
                         const PRUnichar *aFromType, 
                         const PRUnichar *aToType, 
                         nsISupports *ctxt, 
                         nsIInputStream **_retval)
{
  nsCOMPtr<nsIParser> parser;

  static NS_DEFINE_IID(kCParserIID, NS_IPARSER_IID);
  static NS_DEFINE_IID(kCParserCID, NS_PARSER_IID);

  nsresult rv = nsComponentManager::CreateInstance(kCParserCID, 
                                                   nsnull, 
                                                   kCParserIID, 
                                                   getter_AddRefs(parser));

  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIHTMLContentSink> sink;
  nsString toType (aToType);
  if (toType == "text/html")
    rv = NS_New_HTML_ContentSinkStream(getter_AddRefs(sink), aStream,
                                       charset, mFlags);

  else if (toType == "text/plain")
    rv = NS_New_HTMLToTXT_SinkStream(getter_AddRefs(sink), aStream, charset,
                                     mWrapColumn, mFlags);
  else
    return NS_ERROR_ILLEGAL_VALUE;

  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIDTD> dtd;
  nsString fromType(aFromType);
  if (fromType == "text/xif")
    rv = NS_NewXIFDTD(getter_AddRefs(dtd));
  else if (fromType == "text/html")
    rv = NS_NewNavHTMLDTD(getter_AddRefs(dtd));
  else
    return NS_ERROR_ILLEGAL_VALUE;

  if (NS_FAILED(rv))
    return rv;

  parser->SetContentSink(sink);

  parser->RegisterDTD(dtd);

  parser->Parse(*aFromStream, PR_FALSE, 0);

#if 0
  char buf[1024];
  PRUint32 read;
  nsresult rv = aFromStream->Read(buf, 1024, &read);
  if (NS_FAILED(rv)) return rv;

  // Get the first character 
  nsString to(aToType);
  char *toMIME = to.ToNewCString();
  char toChar = *toMIME;

  for (PRUint32 i = 0; i < read; i++) 
    buf[i] = toChar;

  nsString convDataStr(buf);
  nsIInputStream *inputData = nsnull;
  nsISupports *inputDataSup = nsnull;

  rv = NS_NewStringInputStream(&inputDataSup, convDataStr);
  if (NS_FAILED(rv)) return rv;

  rv = inputDataSup->QueryInterface(NS_GET_IID(nsIInputStream), (void**)&inputData);
  NS_RELEASE(inputDataSup);
  *_retval = inputData;
  if (NS_FAILED(rv)) return rv;

  return NS_OK; 
#endif
}

/* This method initializes any internal state before the stream converter
 * begins asyncronous conversion */
NS_IMETHODIMP
nsTextConverter::AsyncConvertData(const PRUnichar *aFromType,
                                  const PRUnichar *aToType, 
                                  nsIStreamListener *aListener, 
                                  nsISupports *ctxt)
{
  NS_ASSERTION(aListener, "null listener");

  mListener = aListener;
  NS_ADDREF(mListener);

  // based on these types, setup internal state to handle the appropriate conversion.
  fromType = aFromType;
  toType = aToType;

  return NS_OK; 
};

// nsIStreamListener method
/* This method handles asyncronous conversion of data. */
NS_IMETHODIMP
nsTextConverter::OnDataAvailable(nsIChannel *channel,
                                 nsISupports *ctxt, 
                                 nsIInputStream *inStr, 
                                 PRUint32 sourceOffset, 
                                 PRUint32 count)
{
  nsresult rv;
  nsIInputStream *convertedStream = nsnull;
  // just make a syncronous call to the Convert() method.
  // Anything can happen here, I just happen to be using the sync call to 
  // do the actual conversion.
  rv = Convert(inStr, fromType.GetUnicode(), toType.GetUnicode(), ctxt, &convertedStream);
  if (NS_FAILED(rv)) return rv;

  PRUint32 len;
  convertedStream->GetLength(&len);
  return mListener->OnDataAvailable(channel, ctxt, convertedStream, sourceOffset, len);
};

// nsIStreamObserver methods
/* These methods just pass through directly to the mListener */
NS_IMETHODIMP
nsTextConverter::OnStartRequest(nsIChannel *channel, nsISupports *ctxt)
{
  return mListener->OnStartRequest(channel, ctxt);
};

NS_IMETHODIMP
nsTextConverter::OnStopRequest(nsIChannel *channel, nsISupports *ctxt, 
                               nsresult status, const PRUnichar *errorMsg)
{
  return mListener->OnStopRequest(channel, ctxt, status, errorMsg);
};


////////////////////////////////////////////////////////////////////////
// nsTextConverterFactory
////////////////////////////////////////////////////////////////////////
nsTextConverterFactory::nsTextConverterFactory(const nsCID &aClass, 
                                               const char* className,
                                               const char* contractID)
  : mClassID(aClass), mClassName(className), mContractID(contractID)
{
  NS_INIT_ISUPPORTS();
}

nsTextConverterFactory::~nsTextConverterFactory()
{
}

NS_IMPL_ISUPPORTS(nsTextConverterFactory, NS_GET_IID(nsIFactory));

NS_IMETHODIMP
nsTextConverterFactory::CreateInstance(nsISupports *aOuter,
                                       const nsIID &aIID,
                                       void **aResult)
{
  if (! aResult)
    return NS_ERROR_NULL_POINTER;

  if (aOuter)
    return NS_ERROR_NO_AGGREGATION;

  *aResult = nsnull;

  nsresult rv = NS_OK;

  nsISupports *inst = nsnull;
  if (mClassID.Equals(knsTextConverterCID)) {
    nsTextConverter *conv = new nsTextConverter();
    if (!conv) return NS_ERROR_OUT_OF_MEMORY;
    conv->QueryInterface(NS_GET_IID(nsISupports), (void**)&inst);
  }
  else {
    return NS_ERROR_NO_INTERFACE;
  }

  if (!inst)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(inst);
  *aResult = inst;
  NS_RELEASE(inst);
  return rv;
}

nsresult nsTextConverterFactory::LockFactory(PRBool aLock)
{
  // Not implemented in simplest case.
  return NS_OK;
}
////////////////////////////////////////////////////////////////////////
// nsTextConverterFactory END
////////////////////////////////////////////////////////////////////////
