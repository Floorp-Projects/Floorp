/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsExpatDriver.h"
#include "nsIParser.h"
#include "nsCOMPtr.h"
#include "nsParserCIID.h"
#include "CParserContext.h"
#include "nsIExpatSink.h"
#include "nsIContentSink.h"
#include "nsParserMsgUtils.h"
#include "nsSpecialSystemDirectory.h"
#include "nsIURL.h"
#include "nsIUnicharInputStream.h"
#include "nsNetUtil.h"
#include "prprf.h"
#include "prmem.h"
#include "nsTextFormatter.h"

static const PRUint32 kNotInDoctype = PRUint32(-1);
static const char* kDTDDirectory = "res/dtd/";

/***************************** EXPAT CALL BACKS *******************************/

 // The callback handlers that get called from the expat parser 
PR_STATIC_CALLBACK(int)
Driver_HandleStartElement(void *aUserData, 
                          const XML_Char *aName, 
                          const XML_Char **aAtts) 
{
  NS_ASSERTION(aUserData, "expat driver should exist");
  if (aUserData) {
    return NS_STATIC_CAST(nsExpatDriver*,aUserData)->HandleStartElement((const PRUnichar*)aName,
                                                                        (const PRUnichar**)aAtts);
  }
  return XML_ERROR_NONE;
}

PR_STATIC_CALLBACK(int)
Driver_HandleEndElement(void *aUserData, 
                        const XML_Char *aName) 
{
  NS_ASSERTION(aUserData, "expat driver should exist");
  if (aUserData) {
    return NS_STATIC_CAST(nsExpatDriver*,aUserData)->HandleEndElement((const PRUnichar*)aName);
  }
  return XML_ERROR_NONE;
}

PR_STATIC_CALLBACK(void)
Driver_HandleCharacterData(void *aUserData,
                           const XML_Char *aData, 
                           int aLength)
{
  NS_ASSERTION(aUserData, "expat driver should exist");
  if (aUserData) {
    NS_STATIC_CAST(nsExpatDriver*,aUserData)->HandleCharacterData((PRUnichar*)aData,
                                                                  PRUint32(aLength));
  }
}

PR_STATIC_CALLBACK(void)
Driver_HandleComment(void *aUserData,
                     const XML_Char *aName) 
{
  NS_ASSERTION(aUserData, "expat driver should exist");
  if(aUserData) {
    NS_STATIC_CAST(nsExpatDriver*,aUserData)->HandleComment((const PRUnichar*)aName);
  }
}

PR_STATIC_CALLBACK(int)
Driver_HandleProcessingInstruction(void *aUserData, 
                                   const XML_Char *aTarget, 
                                   const XML_Char *aData)
{
  NS_ASSERTION(aUserData, "expat driver should exist");
  if (aUserData) {
    return NS_STATIC_CAST(nsExpatDriver*,aUserData)->HandleProcessingInstruction((const PRUnichar*)aTarget,
                                                                                 (const PRUnichar*)aData);
  }
  return XML_ERROR_NONE;
}

PR_STATIC_CALLBACK(void)
Driver_HandleDefault(void *aUserData, 
                     const XML_Char *aData, 
                     int aLength) 
{
  NS_ASSERTION(aUserData, "expat driver should exist");
  if (aUserData) {
    NS_STATIC_CAST(nsExpatDriver*,aUserData)->HandleDefault((const PRUnichar*)aData,
                                                            PRUint32(aLength));
  }
}

PR_STATIC_CALLBACK(void)
Driver_HandleStartCdataSection(void *aUserData)
{
  NS_ASSERTION(aUserData, "expat driver should exist");
  if (aUserData) {
    NS_STATIC_CAST(nsExpatDriver*,aUserData)->HandleStartCdataSection();
  }
}

PR_STATIC_CALLBACK(void)
Driver_HandleEndCdataSection(void *aUserData)
{
  NS_ASSERTION(aUserData, "expat driver should exist");
  if (aUserData) {
    NS_STATIC_CAST(nsExpatDriver*,aUserData)->HandleEndCdataSection();
  }
}

PR_STATIC_CALLBACK(void)
Driver_HandleStartDoctypeDecl(void *aUserData, 
                                   const XML_Char *aDoctypeName)
{
  NS_ASSERTION(aUserData, "expat driver should exist");
  if (aUserData) {
    NS_STATIC_CAST(nsExpatDriver*,aUserData)->HandleStartDoctypeDecl();
  }
}

PR_STATIC_CALLBACK(void)
Driver_HandleEndDoctypeDecl(void *aUserData) 
{
  NS_ASSERTION(aUserData, "expat driver should exist");
  if (aUserData) {
    NS_STATIC_CAST(nsExpatDriver*,aUserData)->HandleEndDoctypeDecl();
  }
}


PR_STATIC_CALLBACK(int)
Driver_HandleExternalEntityRef(XML_Parser parser,
                               const XML_Char *openEntityNames,
                               const XML_Char *base,
                               const XML_Char *systemId,
                               const XML_Char *publicId)
{
  int result = PR_TRUE;
 
  // Load the external entity into a buffer
  nsCOMPtr<nsIInputStream> in;
  nsAutoString absURL;

  nsresult rv = nsExpatDriver::OpenInputStream(publicId, systemId, base, getter_AddRefs(in), absURL);

  if (NS_FAILED(rv)) {
    return result;
  }

  nsCOMPtr<nsIUnicharInputStream> uniIn;

  NS_NewUTF8ConverterStream(getter_AddRefs(uniIn), in, 1024);
  
  if (uniIn) {
    XML_Parser entParser = 
      XML_ExternalEntityParserCreate(parser, 0, (const XML_Char*) NS_LITERAL_STRING("UTF-16").get());

    if (entParser) {
      PRUint32 readCount = 0;
      PRUnichar tmpBuff[1024] = {0};
      PRUnichar *uniBuf = tmpBuff;
      
      XML_SetBase(entParser, (const XML_Char*) absURL.get());

      while (NS_SUCCEEDED(uniIn->Read(uniBuf, 0, 1024, &readCount)) && result) {
        if (readCount) {
          // Pass the buffer to expat for parsing
          result = XML_Parse(entParser, (char *)uniBuf,  readCount * sizeof(PRUnichar), 0);
        }
        else {
          // done reading
          result = XML_Parse(entParser, nsnull, 0, 1);
          break;
        }
      }
      XML_ParserFree(entParser);
    }
  }
  
  return result;
}

/***************************** END CALL BACKS *********************************/

NS_IMPL_ISUPPORTS2(nsExpatDriver,
                   nsITokenizer,
                   nsIDTD);

nsresult 
NS_NewExpatDriver(nsIDTD** aResult) { 
  nsExpatDriver* driver = nsnull;
  NS_NEWXPCOM(driver, nsExpatDriver);
  NS_ENSURE_TRUE(driver,NS_ERROR_OUT_OF_MEMORY);

  return driver->QueryInterface(NS_GET_IID(nsIDTD), (void**)aResult);
}

nsExpatDriver::nsExpatDriver()
  :mExpatParser(0), 
   mSink(0), 
   mBuffer(0),
   mInCData(0), 
   mBytesParsed(0),
   mBytePosition(0),
   mInternalState(NS_OK),
   mDoctypePos(-1)
{
  NS_INIT_REFCNT();
}

nsExpatDriver::~nsExpatDriver() 
{
  NS_IF_RELEASE(mSink);
  if (mExpatParser) {
    XML_ParserFree(mExpatParser);
    mExpatParser = nsnull;
  }
}

nsresult 
nsExpatDriver::HandleStartElement(const PRUnichar *aValue, 
                                  const PRUnichar **aAtts)
{ 
  NS_ASSERTION(mSink, "content sink not found!");

  if (mSink){
    mSink->HandleStartElement(aValue, aAtts, 
                              XML_GetSpecifiedAttributeCount(mExpatParser) / 2, 
                              XML_GetIdAttributeIndex(mExpatParser), 
                              XML_GetCurrentLineNumber(mExpatParser));
  }
  return NS_OK;
}

nsresult 
nsExpatDriver::HandleEndElement(const PRUnichar *aValue)
{
  NS_ASSERTION(mSink, "content sink not found!");

  if (mSink){
    nsresult result = mSink->HandleEndElement(aValue);
    if (result == NS_ERROR_HTMLPARSER_BLOCK) {
      mInternalState = NS_ERROR_HTMLPARSER_BLOCK;
      XML_BlockParser(mExpatParser);
    }
  }
  
  return NS_OK;
}

nsresult 
nsExpatDriver::HandleCharacterData(const PRUnichar *aValue, 
                                   const PRUint32 aLength)
{
  NS_ASSERTION(mSink, "content sink not found!");

  if (mInCData) {
    mCDataText.Append(aValue,aLength);
  } 
  else if (mSink){
    mInternalState = mSink->HandleCharacterData(aValue, aLength);
  }
  
  return NS_OK;
}

nsresult 
nsExpatDriver::HandleComment(const PRUnichar *aValue)
{
  NS_ASSERTION(mSink, "content sink not found!");

  if (mSink && mDoctypePos == kNotInDoctype){
    mInternalState = mSink->HandleComment(aValue);
  }
  
  return NS_OK;
}

nsresult 
nsExpatDriver::HandleProcessingInstruction(const PRUnichar *aTarget, 
                                           const PRUnichar *aData)
{
  NS_ASSERTION(mSink, "content sink not found!");

  if (mSink){
    nsresult result = mSink->HandleProcessingInstruction(aTarget, aData);
    if (result == NS_ERROR_HTMLPARSER_BLOCK) {
      mInternalState = NS_ERROR_HTMLPARSER_BLOCK;
      XML_BlockParser(mExpatParser);
    }
  }

  return NS_OK;
}

nsresult 
nsExpatDriver::HandleDefault(const PRUnichar *aValue, 
                             const PRUint32 aLength) 
{
  NS_ASSERTION(mSink, "content sink not found!");

  if (mSink && mDoctypePos == kNotInDoctype) {
    static const PRUnichar newline[] = {'\n','\0'};
    for (PRUint32 i = 0; i < aLength && NS_SUCCEEDED(mInternalState); i++) {
      if (aValue[i] == '\n' || aValue[i] == '\r') {
        mInternalState = mSink->HandleCharacterData(newline, 1);
      }
    }
  }
  
  return NS_OK;
}

nsresult 
nsExpatDriver::HandleStartCdataSection()
{
  mInCData = PR_TRUE;
  return NS_OK;
}

nsresult 
nsExpatDriver::HandleEndCdataSection()
{
  NS_ASSERTION(mSink, "content sink not found!");

  mInCData = PR_FALSE;
  if (mSink) {
    mInternalState = mSink->HandleCDataSection(mCDataText.get(),mCDataText.Length()); 
  }
  mCDataText.Truncate();
  
  return NS_OK;
}

nsresult 
nsExpatDriver::HandleStartDoctypeDecl()
{
  mDoctypePos = XML_GetCurrentByteIndex(mExpatParser);
  return NS_OK;
}

nsresult 
nsExpatDriver::HandleEndDoctypeDecl() 
{
  NS_ASSERTION(mSink, "content sink not found!");

  const PRUnichar* doctypeStart = mBuffer + ( mDoctypePos - mBytesParsed ) / 2;
  const PRUnichar* doctypeEnd   = mBuffer + ( XML_GetCurrentByteIndex(mExpatParser) - mBytesParsed ) / 2;
 
  if(mSink) {
    mInternalState = mSink->HandleDoctypeDecl(doctypeStart, doctypeEnd - doctypeStart);
  }
  
  mDoctypePos = kNotInDoctype;

  return NS_OK;
}

// Initially added for bug 113400 to switch from the remote "XHTML 1.0 plus
// MathML 2.0" DTD to the the lightweight customized version that Mozilla uses.
// Since Mozilla is not validating, no need to fetch a *huge* file at each click.
// XXX The cleanest solution here would be to fix Bug 98413: Implement XML Catalogs 
struct nsCatalogEntry {
  const char* mPublicID;
  const char* mLocalDTD;
};

// The order of this table is guestimated to be in the optimum order
static const nsCatalogEntry kCatalogTable[] = {
 {"-//W3C//DTD XHTML 1.0 Transitional//EN",    "xhtml11.dtd" },
 {"-//W3C//DTD XHTML 1.1//EN",                 "xhtml11.dtd" },
 {"-//W3C//DTD XHTML 1.0 Strict//EN",          "xhtml11.dtd" },
 {"-//W3C//DTD XHTML 1.0 Frameset//EN",        "xhtml11.dtd" },
 {"-//W3C//DTD XHTML Basic 1.0//EN",           "xhtml11.dtd" },
 {"-//W3C//DTD XHTML 1.1 plus MathML 2.0//EN", "mathml.dtd"  },
 {"-//W3C//DTD SVG 20001102//EN",              "svg.dtd"     },
 {nsnull, nsnull}
};

static void
RemapDTD(const XML_Char* aPublicID, nsAWritableCString& aLocalDTD)
{
  nsCAutoString publicID;
  publicID.AssignWithConversion((const PRUnichar*)aPublicID);

  // linear search for now since the number of entries is going to
  // be negligible, and the fix for bug 98413 would get rid of this
  // code anyway
  aLocalDTD.Truncate();
  const nsCatalogEntry* data = kCatalogTable;
  while (data->mPublicID) {
    if (publicID.Equals(data->mPublicID)) {
      aLocalDTD = data->mLocalDTD;
      return;
    }
    ++data;
  }
}

// aDTD is an in/out parameter.  Returns true if the aDTD is a chrome url or if the
// filename contained within the url exists in the special DTD directory.  
// For the latter case, aDTD is set to the file: url that points to the DTD 
// file found in the local DTD directory AND the old URI is relased.
static PRBool
IsLoadableDTD(const XML_Char* aFPIStr, nsCOMPtr<nsIURI>* aDTD)
{
  PRBool isLoadable = PR_FALSE;
  nsresult res = NS_OK;

  if (!aDTD || !*aDTD) {
    NS_ASSERTION(0, "Null parameter.");
    return PR_FALSE;
  }

  // Return true if the url is a chrome url
  res = (*aDTD)->SchemeIs("chrome", &isLoadable);

  // If the url is not a chrome url, check to see if a DTD file of the same name
  // exists in the special DTD directory
  if (!isLoadable) {
    // try to see if we can map the public ID to a known local DTD
    nsXPIDLCString fileName;
    if (aFPIStr) {
      RemapDTD(aFPIStr, fileName);
    }
    if (fileName.IsEmpty()) {
      // try to see if the user has installed the DTD file -- we extract the
      // filename.ext of the DTD here. Hence, for any DTD for which we have
      // no predefined mapping, users just have to copy the DTD file to our
      // special DTD directory and it will be picked
      nsCOMPtr<nsIURL> dtdURL;
      dtdURL = do_QueryInterface(*aDTD, &res);
      if (NS_FAILED(res)) {
        return PR_FALSE;
      }
      res = dtdURL->GetFileName(getter_Copies(fileName));
      if (NS_FAILED(res) || fileName.IsEmpty()) {
        return PR_FALSE;
      }
    }
    nsSpecialSystemDirectory dtdPath(nsSpecialSystemDirectory::OS_CurrentProcessDirectory);
    dtdPath += PromiseFlatCString(nsDependentCString(kDTDDirectory) + fileName).get();
    if (dtdPath.Exists()) {
      // The DTD was found in the local DTD directory.
      // Set aDTD to a file: url pointing to the local DTD
      nsFileURL dtdFile(dtdPath);
      nsCOMPtr<nsIURI> dtdURI;
      NS_NewURI(getter_AddRefs(dtdURI), dtdFile.GetURLString());
      if (dtdURI) {
        *aDTD = dtdURI;
        isLoadable = PR_TRUE;
      }
    }
  }

  return isLoadable;
}

nsresult
nsExpatDriver::OpenInputStream(const XML_Char* aFPIStr,
                               const XML_Char* aURLStr, 
                               const XML_Char* aBaseURL, 
                               nsIInputStream** in, 
                               nsAString& aAbsURL) 
{
  nsresult rv;
  nsCOMPtr<nsIURI> baseURI;  
  rv = NS_NewURI(getter_AddRefs(baseURI), NS_ConvertUCS2toUTF8((const PRUnichar*)aBaseURL).get());
  if (NS_SUCCEEDED(rv) && baseURI) {
    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), NS_ConvertUCS2toUTF8((const PRUnichar*)aURLStr).get(), baseURI);
    if (NS_SUCCEEDED(rv) && uri) {
      if (IsLoadableDTD(aFPIStr, address_of(uri))) {
        rv = NS_OpenURI(in, uri);
        nsXPIDLCString absURL;
        uri->GetSpec(getter_Copies(absURL));
        CopyASCIItoUCS2(absURL, aAbsURL);
      } 
      else {
        rv = NS_ERROR_NOT_IMPLEMENTED;
      }
    }    
  }
  return rv;
}

static nsresult 
CreateErrorText(const PRUnichar* aDescription,
                const PRUnichar* aSourceURL,
                const PRInt32 aLineNumber,
                const PRInt32 aColNumber,
                nsString& aErrorString)
{
  aErrorString.Truncate();

  nsAutoString msg;
  nsresult rv = nsParserMsgUtils::GetLocalizedStringByName(XMLPARSER_PROPERTIES,"XMLParsingError",msg);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // XML Parsing Error: %1$S\nLocation: %2$S\nLine Number %3$d, Column %4$d:
  PRUnichar *message = nsTextFormatter::smprintf(msg.get(),aDescription,aSourceURL,aLineNumber,aColNumber);
  if (!message) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aErrorString.Assign(message);
  nsTextFormatter::smprintf_free(message);

  return NS_OK;
}

static nsresult 
CreateSourceText(const PRInt32 aColNumber, 
                 const PRUnichar* aSourceLine, 
                 nsString& aSourceString)
{  
  PRInt32 errorPosition = aColNumber;

  aSourceString.Append(aSourceLine);
  aSourceString.Append(PRUnichar('\n'));
  for (PRInt32 i = 0; i < errorPosition - 1; i++) {
    aSourceString.Append(PRUnichar('-'));
  }
  aSourceString.Append(PRUnichar('^'));  

  return NS_OK;
}

nsresult 
nsExpatDriver::HandleError(const char *aBuffer,
                           PRUint32 aLength,
                           PRBool aIsFinal) 
{

  PRInt32 code = XML_GetErrorCode(mExpatParser);
  NS_WARN_IF_FALSE(code >= 1, "unexpected XML error code");
  
  // Map Expat error code to an error string
  // XXX Deal with error returns.
  nsAutoString description;
  nsParserMsgUtils::GetLocalizedStringByID(XMLPARSER_PROPERTIES, code, description);

  if (code == XML_ERROR_TAG_MISMATCH) {
    description.Append(NS_LITERAL_STRING(". Expected: "));
    description.Append(NS_LITERAL_STRING("</"));
    description.Append((const PRUnichar*)XML_GetMismatchedTag(mExpatParser));
    description.Append(NS_LITERAL_STRING(">."));
  }
  
  nsAutoString sourceLine;
  if (!aIsFinal) {
    GetLine(aBuffer, aLength, (XML_GetCurrentByteIndex(mExpatParser) - mBytesParsed), sourceLine);
  }
  else {
    sourceLine.Append(mLastLine);
  }

  // Adjust the column number so that it is one based rather than zero based.
  PRInt32 colNumber = XML_GetCurrentColumnNumber(mExpatParser) + 1; 

  nsAutoString errorText;
  CreateErrorText(description.get(), 
                  (PRUnichar*)XML_GetBase(mExpatParser), 
                  XML_GetCurrentLineNumber(mExpatParser), 
                  colNumber, errorText);

  nsAutoString sourceText;
  CreateSourceText(colNumber, sourceLine.get(), sourceText);

  NS_ASSERTION(mSink,"no sink?");
  if (mSink) {
    mSink->ReportError(errorText.get(), sourceText.get());
  }

  return NS_ERROR_HTMLPARSER_STOPPARSING;
}

nsresult 
nsExpatDriver::ParseBuffer(const char* aBuffer, 
                           PRUint32 aLength, 
                           PRBool aIsFinal)
{
  nsresult result = NS_OK;
  NS_ASSERTION((aBuffer && aLength) || (aBuffer == nsnull && aLength == 0), "?");
  
  if (mExpatParser && mInternalState == NS_OK) {
    if (!XML_Parse(mExpatParser, aBuffer, aLength, aIsFinal)) {
      if (mInternalState == NS_ERROR_HTMLPARSER_BLOCK ||
          mInternalState == NS_ERROR_HTMLPARSER_STOPPARSING) {
        mBytePosition = (XML_GetCurrentByteIndex(mExpatParser) - mBytesParsed);
        mBytesParsed += mBytePosition;
      }
      else {
        HandleError(aBuffer,aLength,aIsFinal);
        mInternalState = NS_ERROR_HTMLPARSER_STOPPARSING;
      }
      return mInternalState;
    }
    else if (aBuffer && aLength) {
      // Cache the last line in the buffer
      GetLine(aBuffer, aLength, aLength - sizeof(PRUnichar), mLastLine);
    }
	  mBytesParsed += aLength; 
    mBytePosition = 0;
  }
  
  return result;
}

void 
nsExpatDriver::GetLine(const char* aSourceBuffer, 
                       PRUint32 aLength, 
                       PRUint32 aOffset, 
                       nsString& aLine)
{
  /* Figure out the line inside aSourceBuffer that contains character specified by aOffset.
     Copy it into aLine. */
  NS_ASSERTION(aOffset >= 0 && aOffset < aLength, "?");
  /* Assert that the byteIndex and the length of the buffer is even */
  NS_ASSERTION(aOffset % 2 == 0 && aLength % 2 == 0, "?");  
  PRUnichar* start = (PRUnichar* ) &aSourceBuffer[aOffset];  /* Will try to find the start of the line */
  PRUnichar* end = (PRUnichar* ) &aSourceBuffer[aOffset];    /* Will try to find the end of the line */
  PRUint32 startIndex = aOffset / sizeof(PRUnichar);          /* Track the position of the 'start' pointer into the buffer */
  PRUint32 endIndex = aOffset / sizeof(PRUnichar);          /* Track the position of the 'end' pointer into the buffer */
  PRUint32 numCharsInBuffer = aLength / sizeof(PRUnichar);
  PRBool reachedStart;
  PRBool reachedEnd;
  

  /* Use start to find the first new line before the error position and
     end to find the first new line after the error position */
  reachedStart = (startIndex <= 0 || '\n' == *start || '\r' == *start);
  reachedEnd = (endIndex >= numCharsInBuffer || '\n' == *end || '\r' == *end);
  while (!reachedStart || !reachedEnd) {
    if (!reachedStart) {
      --start;
      --startIndex;
      reachedStart = (startIndex <= 0 || '\n' == *start || '\r' == *start);
    }
    if (!reachedEnd) {
      ++end;
      ++endIndex;
      reachedEnd = (endIndex >= numCharsInBuffer || '\n' == *end || '\r' == *end);
    }
  }

  aLine.Truncate(0);
  if (startIndex == endIndex) {
    // Special case if the error is on a line where the only character is a newline.
    // Do nothing
  }
  else {
    NS_ASSERTION(endIndex - startIndex >= sizeof(PRUnichar), "?");
    /* At this point, there are two cases.  Either the error is on the first line or
       on subsequent lines.  If the error is on the first line, startIndex will decrement
       all the way to zero.  If not, startIndex will decrement to the position of the
       newline character on the previous line.  So, in the first case, the start position
       of the error line = startIndex (== 0).  In the second case, the start position of the
       error line = startIndex + 1.  In both cases, the end position of the error line will be 
       (endIndex - 1).  */
    PRUint32 startPosn = (startIndex <= 0) ? startIndex : startIndex + 1;
        
    /* At this point, the substring starting at startPosn and ending at (endIndex - 1),
       is the line on which the error occurred. Copy that substring into the error structure. */
    const PRUnichar* unicodeBuffer = (const PRUnichar*) aSourceBuffer;
    aLine.Append(&unicodeBuffer[startPosn], endIndex - startPosn);
  }
}

     
NS_IMETHODIMP
nsExpatDriver::CreateNewInstance(nsIDTD** aInstancePtrResult)
{ 
  return NS_NewExpatDriver(aInstancePtrResult);
}

NS_IMETHODIMP
nsExpatDriver::ConsumeToken(nsScanner& aScanner,
                            PRBool& aFlushTokens)
{

  // Ask the scanner to send us all the data it has
  // scanned and pass that data to expat.
  
  mInternalState = NS_OK; // Resume in case we're blocked.
  XML_UnblockParser(mExpatParser);

  nsReadingIterator<PRUnichar> start, end;
  aScanner.CurrentPosition(start);
  aScanner.EndReading(end);
  
  while (start != end) {
    PRUint32 fragLength = PRUint32(start.size_forward());
    mBuffer = start.get();
    
    mInternalState = ParseBuffer((const char *)mBuffer, 
                                 fragLength * sizeof(PRUnichar), 
                                 aFlushTokens);
    
    if (NS_FAILED(mInternalState)) {
      if (mInternalState == NS_ERROR_HTMLPARSER_BLOCK) {
        // mBytePosition / 2 => character position. Since one char = two bytes.
        aScanner.SetPosition(start.advance(mBytePosition / 2), PR_TRUE);
        aScanner.Mark();
      }
      return mInternalState;
    }

    start.advance(fragLength);
  }

  aScanner.SetPosition(end, PR_TRUE);
  
  if(NS_SUCCEEDED(mInternalState)) {
    return aScanner.Eof();
  }

  return NS_OK;
}

NS_IMETHODIMP_(eAutoDetectResult) 
nsExpatDriver::CanParse(CParserContext& aParserContext, 
                        const nsString& aBuffer, 
                        PRInt32 aVersion)
{
  eAutoDetectResult result = eUnknownDetect;

  if (eViewSource != aParserContext.mParserCommand) {
    if (aParserContext.mMimeType.EqualsWithConversion(kXMLTextContentType)         ||
        aParserContext.mMimeType.EqualsWithConversion(kXMLApplicationContentType)  ||
        aParserContext.mMimeType.EqualsWithConversion(kXHTMLApplicationContentType)||
        aParserContext.mMimeType.EqualsWithConversion(kRDFTextContentType)         ||
        aParserContext.mMimeType.EqualsWithConversion(kXULTextContentType)) {
      result=ePrimaryDetect;
    }
    else {
      if (0 == aParserContext.mMimeType.Length() &&
          kNotFound != aBuffer.Find("<?xml ")) {
        aParserContext.SetMimeType(NS_LITERAL_STRING(kXMLTextContentType));
        result=eValidDetect;
      }
    }
  }

  return result;
}

NS_IMETHODIMP 
nsExpatDriver::WillBuildModel(const CParserContext& aParserContext, 
                              nsIContentSink* aSink)
{

  NS_ENSURE_ARG_POINTER(aSink);

  aSink->QueryInterface(NS_GET_IID(nsIExpatSink),(void**)&(mSink));
  NS_ENSURE_TRUE(mSink,NS_ERROR_FAILURE);

  mExpatParser = XML_ParserCreate((const XML_Char*) NS_LITERAL_STRING("UTF-16").get());
  NS_ENSURE_TRUE(mExpatParser, NS_ERROR_FAILURE);

#ifdef XML_DTD
    XML_SetParamEntityParsing(mExpatParser, XML_PARAM_ENTITY_PARSING_ALWAYS);
#endif
    
  XML_SetBase(mExpatParser, (const XML_Char*) (aParserContext.mScanner->GetFilename()).get());

  // Set up the callbacks
  XML_SetElementHandler(mExpatParser, Driver_HandleStartElement, Driver_HandleEndElement);    
  XML_SetCharacterDataHandler(mExpatParser, Driver_HandleCharacterData);
  XML_SetProcessingInstructionHandler(mExpatParser, Driver_HandleProcessingInstruction);
  XML_SetDefaultHandlerExpand(mExpatParser, Driver_HandleDefault);
  XML_SetExternalEntityRefHandler(mExpatParser, Driver_HandleExternalEntityRef);
  XML_SetCommentHandler(mExpatParser, Driver_HandleComment);
  XML_SetCdataSectionHandler(mExpatParser, Driver_HandleStartCdataSection,
                             Driver_HandleEndCdataSection);

  XML_SetParamEntityParsing(mExpatParser, XML_PARAM_ENTITY_PARSING_UNLESS_STANDALONE);
  XML_SetDoctypeDeclHandler(mExpatParser, Driver_HandleStartDoctypeDecl, Driver_HandleEndDoctypeDecl);

  // Set up the user data.
  XML_SetUserData(mExpatParser, this);

  return aSink->WillBuildModel();
}

NS_IMETHODIMP 
nsExpatDriver::BuildModel(nsIParser* aParser,
                          nsITokenizer* aTokenizer,
                          nsITokenObserver* anObserver,
                          nsIContentSink* aSink) 
{
  return mInternalState;
}

NS_IMETHODIMP 
nsExpatDriver::DidBuildModel(nsresult anErrorCode,
                             PRBool aNotifySink,
                             nsIParser* aParser,
                             nsIContentSink* aSink)
{
  // Check for mSink is intentional. This would make sure
  // that DidBuildModel() is called only once on the sink.
  nsresult result = (mSink)? aSink->DidBuildModel(0) : NS_OK;
  NS_IF_RELEASE(mSink);
  return result;
}

NS_IMETHODIMP nsExpatDriver::GetTokenizer(nsITokenizer*& aTokenizer)
{
  aTokenizer = this;
  return NS_OK;
}

NS_IMETHODIMP                     
nsExpatDriver::WillTokenize(PRBool aIsFinalChunk,
                            nsTokenAllocator* aTokenAllocator)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsExpatDriver::WillResumeParse(nsIContentSink* aSink)
{
  return (aSink)? aSink->WillResume():NS_OK;
}

NS_IMETHODIMP 
nsExpatDriver::WillInterruptParse(nsIContentSink* aSink)
{
  return (aSink)? aSink->WillInterrupt():NS_OK;
}

NS_IMETHODIMP
nsExpatDriver::DidTokenize(PRBool aIsFinalChunk)
{
  return ParseBuffer(nsnull, 0, aIsFinalChunk);
} 

NS_IMETHODIMP_(const nsIID&)  
nsExpatDriver::GetMostDerivedIID(void) const
{
  return NS_GET_IID(nsIDTD);
}

NS_IMETHODIMP_(void)
nsExpatDriver::Terminate()
{
  XML_BlockParser(mExpatParser); // XXX - not sure what happens to the unparsed data.
  mInternalState = NS_ERROR_HTMLPARSER_STOPPARSING;
}

/*************************** Unused methods ***************************************/ 

NS_IMETHODIMP_(CToken*)
nsExpatDriver::PushTokenFront(CToken* aToken)
{
  return 0;
}

NS_IMETHODIMP_(CToken*)
nsExpatDriver::PushToken(CToken* aToken)
{
  return 0;
}
	
NS_IMETHODIMP_(CToken*)
nsExpatDriver::PopToken(void)
{
  return 0;
}
	
NS_IMETHODIMP_(CToken*)           
nsExpatDriver::PeekToken(void)
{
  return 0;
}

NS_IMETHODIMP_(CToken*)           
nsExpatDriver::GetTokenAt(PRInt32 anIndex)
{
  return 0;
}

NS_IMETHODIMP_(PRInt32)
nsExpatDriver::GetCount(void)
{
  return 0;
}

NS_IMETHODIMP_(nsTokenAllocator*)
nsExpatDriver::GetTokenAllocator(void)
{
  return 0;
}
  
NS_IMETHODIMP_(void)              
nsExpatDriver::PrependTokens(nsDeque& aDeque)
{

}

NS_IMETHODIMP 
nsExpatDriver::HandleToken(CToken* aToken,nsIParser* aParser)
{
  return NS_OK;
}

NS_IMETHODIMP_(PRBool)  
nsExpatDriver::IsBlockElement(PRInt32 aTagID,PRInt32 aParentID) const 
{
  return PR_FALSE;
}

NS_IMETHODIMP_(PRBool)  
nsExpatDriver::IsInlineElement(PRInt32 aTagID,PRInt32 aParentID) const 
{
  return PR_FALSE;
}

NS_IMETHODIMP_(PRBool)
nsExpatDriver::IsContainer(PRInt32 aTag) const
{
  return PR_TRUE;
}

NS_IMETHODIMP_(PRBool)
nsExpatDriver::CanContain(PRInt32 aParent,PRInt32 aChild) const
{
  return PR_TRUE;
}

NS_IMETHODIMP 
nsExpatDriver::StringTagToIntTag(const nsAReadableString &aTag, PRInt32* aIntTag) const
{
  return NS_OK;
}

NS_IMETHODIMP_(const PRUnichar *)
nsExpatDriver::IntTagToStringTag(PRInt32 aIntTag) const
{
  return 0;
}

NS_IMETHODIMP 
nsExpatDriver::ConvertEntityToUnicode(const nsAReadableString& aEntity, PRInt32* aUnicode) const
{
  return NS_OK;
}
/******************************************************************************/
