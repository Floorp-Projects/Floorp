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
 * The Original Code is Mozilla Communicator client code.
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
#include "nsIComponentManager.h"
#include "nsParserCIID.h"
#include "nsIParser.h"
#include "nsILoggingSink.h"
#include "nsIInputStream.h"
#include "prprf.h"
#include <fstream.h>


#ifdef XP_PC
#define PARSER_DLL "gkparser.dll"
#endif
#ifdef XP_MAC
#endif
#if defined(XP_UNIX) || defined(XP_BEOS)
#define PARSER_DLL "libhtmlpars"MOZ_DLL_SUFFIX
#endif

// Class IID's
static NS_DEFINE_CID(kParserCID, NS_PARSER_CID);
static NS_DEFINE_IID(kLoggingSinkCID, NS_LOGGING_SINK_CID);

// Interface IID's
static NS_DEFINE_IID(kIParserIID, NS_IPARSER_IID);
static NS_DEFINE_IID(kILoggingSinkIID, NS_ILOGGING_SINK_IID);

static NS_DEFINE_CID(kNavDTDCID, NS_CNAVDTD_CID);

//----------------------------------------------------------------------

static void SetupRegistry()
{
  nsComponentManager::RegisterComponentLib(kParserCID, NULL, NULL, PARSER_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kLoggingSinkCID, NULL, NULL, PARSER_DLL,PR_FALSE,PR_FALSE);
}

//----------------------------------------------------------------------

nsresult ParseData(char* anInputStream,char* anOutputStream) {
	nsresult result=NS_OK;

  if(anInputStream && anOutputStream) {
    PRFileDesc* in=PR_Open(anInputStream, PR_RDONLY, 777);
          
    if (!in) {
      return NS_ERROR_FAILURE;
    }

    nsString   stream;
    char       buffer[1024];
    PRBool     done=PR_FALSE;
    PRInt32    length=0;

    while(!done) {
      length = PR_Read(in, buffer, sizeof(buffer));
      if (length>0) {
        stream.AppendWithConversion(buffer,length); 
      }
      else {
        done=PR_TRUE;
      }
    }

    PR_Close(in);

    // Create a parser
    nsCOMPtr<nsIParser> parser;
    result = nsComponentManager::CreateInstance(kParserCID,nsnull,kIParserIID,getter_AddRefs(parser));
    if (NS_SUCCEEDED(result)) {
      // Create a sink
      nsCOMPtr<nsILoggingSink> sink;
      result = nsComponentManager::CreateInstance(kLoggingSinkCID,nsnull,kILoggingSinkIID,getter_AddRefs(sink));
      if (NS_SUCCEEDED(result)) {
        // Create a dtd
        nsCOMPtr<nsIDTD> dtd;
        result=nsComponentManager::CreateInstance(kNavDTDCID,nsnull,NS_GET_IID(nsIDTD),getter_AddRefs(dtd));
        if(NS_SUCCEEDED(result)) {
          // Parse the document, having the sink write the data to fp
          PRFileDesc* out;
          out = PR_Open(anOutputStream, PR_CREATE_FILE|PR_WRONLY, 777);
          
          //fstream out(anOutputStream,ios::out);

          if (!out) {
            return NS_ERROR_FAILURE;
          }
			    
          sink->SetOutputStream(out);
          parser->RegisterDTD(dtd);
	        parser->SetContentSink(sink);
	        result = parser->Parse(stream, 0, NS_LITERAL_STRING("text/html"), PR_FALSE, PR_TRUE);

          PR_Close(out);
        }
        else {
          cout << "Unable to create a dtd (" << result << ")" <<endl;
        }
      }
      else {
        cout << "Unable to create a sink (" << result << ")" <<endl;
      }
    }
    else {
     cout << "Unable to create a parser (" << result << ")" <<endl;
    }
  }
  return result;
}


//---------------------------------------------------------------------

int main(int argc, char** argv)
{
  if (argc < 3) {
		cout << "Usage: " << "" << "<inputfile> <outputfile>" << endl; 
    return -1;
  }

  int result=0;

  //SetupRegistry();

  ParseData(argv[1],argv[2]);

  return result;
}
