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

#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"
#include "nsUCSupport.h"
#include "nsString.h"
#include "nsIStringEnumerator.h"
#include "nsTArray.h"

//----------------------------------------------------------------------------
// Global functions and data [declaration]

#define ARRAY_SIZE(_array)  (sizeof(_array) / sizeof(_array[0]))
#define SMALL_BUFFER_SIZE   512
#define MED_BUFFER_SIZE     1024
#define BIG_BUFFER_SIZE     2048

static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

//----------------------------------------------------------------------------
// Class nsTestLog [declaration]

/**
 * A Logging class for test programs.
 *
 * This simple test program will not trigger a component registration. So 
 * Mozilla has to be run once before running this, so that the necessary 
 * components will be registered. Also, please observe that the ContractID's are 
 * case sensitive now!
 *
 * @created         28/Mar/2000
 * @author  Catalin Rotaru [CATA]
 */
class nsTestLog
{
private:

  static const char * kTraceDelimiter;

  nsCAutoString mTrace;

public:

  void AddTrace(const char * aTrace);
  void DelTrace(const char * aTrace);
  void PrintError(const char * aCall, const int aError);
  void PrintError(const char * aCall, const char * aMessage);
};
  
//----------------------------------------------------------------------------
// Class nsTestUConv [declaration]

/**
 * The main class of the program.
 *
 * XXX Create a very general set of "bug and regression" test cases and the 
 * one in TestTempBug()
 * XXX Apply the new argument style (pointers) to the converters interfaces
 *
 * @created         28/Mar/2000
 * @author  Catalin Rotaru [CATA]
 */
class nsTestUConv
{
private:

  nsTestLog mLog;

  /**
   * Run the built-in set of self tests for encoders.
   */
  nsresult TestEncoders();

  /**
   * Run the built-in set of self tests for decoders.
   */
  nsresult TestDecoders();

  /**
   * Run the built-in set of self tests for the CharsetManager.
   */
  nsresult TestCharsetManager();

  /**
   * Display charset detectors and their attributes.
   */
  nsresult DisplayDetectors();

  /**
   * Display charsets and their attributes.
   */
  nsresult DisplayCharsets();

  /**
   * Run a temporary debug test. This method is ment as a placeholder when some
   * quick debugging is needed.
   */
  nsresult TestTempBug();

  nsresult Encode(PRUnichar ** aSrc, PRUnichar * aSrcEnd, char ** aDest, 
    char * aDestEnd, const nsAFlatCString& aCharset);

  /**
   * Bridge methods between the new argument style (poiters) and the old one 
   * (lengths). To be removed when the converter interfaces will switch to the
   * new style.
   *
   * This wraps an encoder Convert() call.
   */
  nsresult ConvertEncode(PRUnichar ** aSrc, PRUnichar * aSrcEnd, char ** aDest, 
    char * aDestEnd, nsIUnicodeEncoder * aEncoder);

  /**
   * This wraps an encoder Finish() call.
   */
  nsresult FinishEncode(char ** aDest, char * aDestEnd, 
    nsIUnicodeEncoder * aEncoder);

  void PrintSpaces(int aCount);

public:

  /**
   * Main method of the program.
   */
  nsresult Main(int aArgC, char ** aArgV);
};
  
//----------------------------------------------------------------------------
// Global functions and data [implementation]

int main(int argc, char ** argv)
{
  nsTestUConv testObj;
  nsresult res;
 
  res = testObj.Main(argc, argv);
  return (NS_FAILED(res));
}

//----------------------------------------------------------------------------
// Class nsTestLog [implementation]

const char * nsTestLog::kTraceDelimiter = ".";

void nsTestLog::AddTrace(const char * aTrace)
{
  mTrace.Append(aTrace);
  mTrace.Append(kTraceDelimiter);
}

void nsTestLog::DelTrace(const char * aTrace)
{
  mTrace.Truncate(mTrace.Length() - strlen(aTrace) - strlen(kTraceDelimiter));
}

void nsTestLog::PrintError(const char * aCall, const int aError)
{
  const char * trace = mTrace.get();
  printf("ERROR at %s%s code=0x%x.\n", trace, aCall, aError);
}

void nsTestLog::PrintError(const char * aCall, const char * aMessage)
{
  const char * trace = mTrace.get();
  printf("ERROR at %s%s reason: %s.\n", trace, aCall, aMessage);
}

//----------------------------------------------------------------------------
// Class nsTestUConv [implementation]

nsresult nsTestUConv::TestEncoders()
{
  const char * trace = "TestEncoders";
  mLog.AddTrace(trace);
  nsresult res = NS_OK;

  nsCOMPtr<nsICharsetConverterManager> ccMan = 
           do_GetService(kCharsetConverterManagerCID, &res);
  if (NS_FAILED(res)) return res;
  
  nsCOMPtr<nsIUTF8StringEnumerator> encoders;
  res = ccMan->GetEncoderList(getter_AddRefs(encoders));
  if (NS_FAILED(res)) return res;

  bool hasMore;
  encoders->HasMore(&hasMore);
  
  nsCAutoString charset;
  while (hasMore) {
    encoders->GetNext(charset);

    encoders->HasMore(&hasMore);
  }
  
  mLog.DelTrace(trace);
  return res;
}

nsresult nsTestUConv::TestDecoders()
{
  const char * trace = "TestDecoders";
  mLog.AddTrace(trace);
  nsresult res = NS_OK;

  // XXX write me

  mLog.DelTrace(trace);
  return res;
}

nsresult nsTestUConv::TestCharsetManager()
{
  const char * trace = "TestCharsetManager";
  mLog.AddTrace(trace);
  nsresult res = NS_OK;
  nsAutoString name;
  nsCOMPtr<nsIAtom> csAtom;

  nsCOMPtr<nsICharsetConverterManager> ccMan = 
           do_GetService(kCharsetConverterManagerCID, &res);
  if (NS_FAILED(res)) {
    mLog.PrintError("NS_WITH_SERVICE", res);
    return res;
  }

  mLog.DelTrace(trace);
  return res;
}

nsresult nsTestUConv::DisplayDetectors()
{
  const char * trace = "DisplayDetectors";
  mLog.AddTrace(trace);
  nsresult res = NS_OK;

  nsCOMPtr<nsICharsetConverterManager> ccMan = 
           do_GetService(kCharsetConverterManagerCID, &res);
  if (NS_FAILED(res)) {
    mLog.PrintError("NS_WITH_SERVICE", res);
    return res;
  }

  // charset detectors
  nsCOMPtr<nsIUTF8StringEnumerator> detectors;

  res = ccMan->GetCharsetDetectorList(getter_AddRefs(detectors));
  if (NS_FAILED(res)) {
    mLog.PrintError("GetCharsetDetectorList()", res);
    return res;
  }

  printf("***** Character Set Detectors *****\n");

  bool hasMore;
  detectors->HasMore(&hasMore);
  while (hasMore) {
    nsCAutoString detectorName;
    res = detectors->GetNext(detectorName);
    if (NS_FAILED(res)) {
      mLog.PrintError("GetNext()", res);
      return res;
    }

    printf("%s", detectorName.get());
    PrintSpaces(36 - detectorName.Length()); // align to hard coded column number

    nsAutoString title;
    res = ccMan->GetCharsetTitle(detectorName.get(), title);
    if (NS_FAILED(res)) title.SetLength(0);
    printf("\"%s\"\n", NS_LossyConvertUTF16toASCII(title).get());

    detectors->HasMore(&hasMore);
  }
  
  mLog.DelTrace(trace);
  return NS_OK;
}

nsresult nsTestUConv::DisplayCharsets()
{
  const char * trace = "DisplayCharsets";
  mLog.AddTrace(trace);
  nsresult res = NS_OK;

  nsCOMPtr<nsICharsetConverterManager> ccMan = 
           do_GetService(kCharsetConverterManagerCID, &res);
  if (NS_FAILED(res)) {
    mLog.PrintError("NS_WITH_SERVICE", res);
    return res;
  }

  nsCOMPtr<nsIUTF8StringEnumerator> decoders;
  nsCOMPtr<nsIUTF8StringEnumerator> encoders;

  res = ccMan->GetDecoderList(getter_AddRefs(decoders));
  if (NS_FAILED(res)) {
    mLog.PrintError("GetDecoderList()", res);
    return res;
  }

  res = ccMan->GetEncoderList(getter_AddRefs(encoders));
  if (NS_FAILED(res)) {
    mLog.PrintError("GetEncoderList()", res);
    return res;
  }


  printf("***** Character Sets *****\n");

  PRUint32 encCount = 0, decCount = 0;
  PRUint32 basicEncCount = 0, basicDecCount = 0;

  nsTArray<nsCString> allCharsets;
  
  nsCAutoString charset;
  bool hasMore;
  encoders->HasMore(&hasMore);
  while (hasMore) {
    res = encoders->GetNext(charset);
    if (NS_SUCCEEDED(res))
      allCharsets.AppendElement(charset);

    encoders->HasMore(&hasMore);
  }

  nsAutoString prop, str;
  PRUint32 count = allCharsets.Length();
  for (PRUint32 i = 0; i < count; i++) {

    const nsCString& charset = allCharsets[i];
    printf("%s", charset.get());
    PrintSpaces(24 - charset.Length());  // align to hard coded column number


    nsCOMPtr<nsIUnicodeDecoder> dec = NULL;
    res = ccMan->GetUnicodeDecoder(charset.get(), getter_AddRefs(dec));
    if (NS_FAILED(res)) printf (" "); 
    else {
      printf("D");
      decCount++;
    }
#ifdef NS_DEBUG
    // show the "basic" decoder classes
    if (dec) {
      nsCOMPtr<nsIBasicDecoder> isBasic = do_QueryInterface(dec);
      if (isBasic) {
        basicDecCount++;
        printf("b");
      }
      else printf(" ");
    }
    else printf(" ");
#endif

    nsCOMPtr<nsIUnicodeEncoder> enc = NULL;
    res = ccMan->GetUnicodeEncoder(charset.get(), getter_AddRefs(enc));
    if (NS_FAILED(res)) printf (" "); 
    else {
      printf("E");
      encCount++;
    }

#ifdef NS_DEBUG
    if (enc) {
      nsCOMPtr<nsIBasicEncoder> isBasic = do_QueryInterface(enc);
      if (isBasic) {
        basicEncCount++;
        printf("b");
      }
      else printf(" ");
    }
    else printf(" ");
#endif
    
    printf(" ");

    prop.AssignLiteral(".notForBrowser");
    res = ccMan->GetCharsetData(charset.get(), prop.get(), str);
    if ((dec != NULL) && (NS_FAILED(res))) printf ("B"); 
    else printf("X");

    prop.AssignLiteral(".notForComposer");
    res = ccMan->GetCharsetData(charset.get(), prop.get(), str);
    if ((enc != NULL) && (NS_FAILED(res))) printf ("C"); 
    else printf("X");

    prop.AssignLiteral(".notForMailView");
    res = ccMan->GetCharsetData(charset.get(), prop.get(), str);
    if ((dec != NULL) && (NS_FAILED(res))) printf ("V"); 
    else printf("X");

    prop.AssignLiteral(".notForMailEdit");
    res = ccMan->GetCharsetData(charset.get(), prop.get(), str);
    if ((enc != NULL) && (NS_FAILED(res))) printf ("E"); 
    else printf("X");

    printf("(%3d, %3d) ", encCount, decCount);
    res = ccMan->GetCharsetTitle(charset.get(), str);
    if (NS_FAILED(res)) str.SetLength(0);
    NS_LossyConvertUTF16toASCII buff2(str);
    printf(" \"%s\"\n", buff2.get());
  }

  printf("%u of %u decoders are basic (%d%%)\n",
         basicDecCount, decCount, (basicDecCount * 100) / decCount);

  printf("%u of %u encoders are basic (%d%%)\n",
         basicEncCount, encCount, (basicEncCount * 100) / encCount);
  mLog.DelTrace(trace);
  return NS_OK;
}

nsresult nsTestUConv::TestTempBug()
{
  const char * trace = "TestTempBug";
  mLog.AddTrace(trace);
  nsresult res = NS_OK;

  NS_NAMED_LITERAL_CSTRING(charset, "ISO-2022-JP");
  PRUnichar src[] = {0x0043, 0x004e, 0x0045, 0x0054, 0x0020, 0x004A, 0x0061, 
    0x0070, 0x0061, 0x006E, 0x0020, 0x7DE8, 0x96C6, 0x5C40};
  PRUnichar * srcEnd = src + ARRAY_SIZE(src);
  char dest[BIG_BUFFER_SIZE];
  char * destEnd = dest + BIG_BUFFER_SIZE;

  PRUnichar * p = src;
  char * q = dest;
  res = Encode(&p, srcEnd, &q, destEnd, charset);

  mLog.DelTrace(trace);
  return res;
}

nsresult nsTestUConv::Encode(PRUnichar ** aSrc, PRUnichar * aSrcEnd, 
                             char ** aDest, char * aDestEnd, 
                             const nsAFlatCString& aCharset)
{
  const char * trace = "Encode";
  mLog.AddTrace(trace);
  nsresult res = NS_OK;

  nsCOMPtr<nsICharsetConverterManager> ccMan = 
           do_GetService(kCharsetConverterManagerCID, &res);
  if (NS_FAILED(res)) {
    mLog.PrintError("NS_WITH_SERVICE", res);
    return res;
  }

  nsCOMPtr<nsIUnicodeEncoder> enc;
  res = ccMan->GetUnicodeEncoder(aCharset.get(), getter_AddRefs(enc));
  if (NS_FAILED(res)) {
    mLog.PrintError("GetUnicodeEncoder()", res);
    return res;
  }

  res = ConvertEncode(aSrc, aSrcEnd, aDest, aDestEnd, enc);
  if (NS_FAILED(res)) {
    mLog.PrintError("Convert()", res);
    return res;
  }

  res = FinishEncode(aDest, aDestEnd, enc);
  if (NS_FAILED(res)) {
    mLog.PrintError("Finish()", res);
    return res;
  }

  mLog.DelTrace(trace);
  return res;
}

nsresult nsTestUConv::ConvertEncode(PRUnichar ** aSrc, PRUnichar * aSrcEnd, 
                                    char ** aDest, char * aDestEnd, 
                                    nsIUnicodeEncoder * aEncoder)
{
  PRUnichar * src = (*aSrc);
  char * dest = (*aDest);
  PRInt32 srcLen = aSrcEnd - src;
  PRInt32 destLen = aDestEnd - dest;

  nsresult res = aEncoder->Convert(src, &srcLen, dest, &destLen);

  (*aSrc) = src + srcLen;
  (*aDest) = dest + destLen;
  return res;
}

nsresult nsTestUConv::FinishEncode(char ** aDest, char * aDestEnd, 
                                   nsIUnicodeEncoder * aEncoder)
{
  char * dest = (*aDest);
  PRInt32 destLen = aDestEnd - dest;

  nsresult res = aEncoder->Finish(dest, &destLen);

  (*aDest) = dest + destLen;
  return res;
}

void nsTestUConv::PrintSpaces(int aCount)
{
  for (int i = 0; i < aCount; i++) printf(" ");
}

nsresult nsTestUConv::Main(int aArgC, char ** aArgV)
{
  const char * trace = "Main";
  mLog.AddTrace(trace);
  nsresult res = NS_OK;

  if (aArgC < 2) {
    // no arguments were passed to the program, so we just run the self tests
    res = TestCharsetManager();
    if (NS_SUCCEEDED(res)) res = TestEncoders();
    if (NS_SUCCEEDED(res)) res = TestDecoders();
  } else if (!strcmp(aArgV[1], "-tempbug")) {
    // we are testing a temporary bug
    res = TestTempBug();
  } else if (!strcmp(aArgV[1], "-display")) {
    // display all the available data
    res = DisplayDetectors();
    if (NS_SUCCEEDED(res)) res = DisplayCharsets();
  }

  mLog.DelTrace(trace);
  return res;
}
