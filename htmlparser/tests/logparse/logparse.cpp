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
#include "nsRepository.h"
#include "nsParserCIID.h"
#include "nsIParser.h"
#include "nsILoggingSink.h"
#include "CNavDTD.h"
#include <fstream.h>

#ifdef XP_PC
#define PARSER_DLL "raptorhtmlpars.dll"
#endif
#ifdef XP_MAC
#endif
#ifdef XP_UNIX
#define PARSER_DLL "libraptorhtmlpars.so"
#endif

// Class IID's
static NS_DEFINE_IID(kParserCID, NS_PARSER_IID);
static NS_DEFINE_IID(kLoggingSinkCID, NS_LOGGING_SINK_IID);

// Interface IID's
static NS_DEFINE_IID(kIParserIID, NS_IPARSER_IID);
static NS_DEFINE_IID(kILoggingSinkIID, NS_ILOGGING_SINK_IID);

static void SetupRegistry()
{
  NSRepository::RegisterFactory(kParserCID, PARSER_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kLoggingSinkCID, PARSER_DLL,PR_FALSE,PR_FALSE);
}

//----------------------------------------------------------------------

int main(int argc, char** argv)
{
  if (argc != 3) {
    fprintf(stderr, "Usage: logparse in out\n");
    return -1;
  }

  fstream *in = new fstream();
  in->open(argv[1], ios::in | ios::nocreate);

  FILE* fp = fopen(argv[2], "wb");
  if (nsnull == fp) {
    fprintf(stderr, "can't create '%s'\n", argv[2]);
    return -1;
  }

  SetupRegistry();

  // Create a parser
  nsIParser* parser;
  nsresult rv = NSRepository::CreateInstance(kParserCID,
                                             nsnull,
                                             kIParserIID,
                                             (void**)&parser);
  if (NS_OK != rv) {
    fprintf(stderr, "Unable to create a parser (%x)\n", rv);
    return -1;
  }

  // Create a sink
  nsILoggingSink* sink;
  rv = NSRepository::CreateInstance(kLoggingSinkCID,
                                    nsnull,
                                    kILoggingSinkIID,
                                    (void**)&sink);
  if (NS_OK != rv) {
    fprintf(stderr, "Unable to create a sink (%x)\n", rv);
    return -1;
  }
  sink->Init(fp);

  // Parse the document, having the sink write the data to fp
  nsIDTD* dtd = nsnull;
  NS_NewNavHTMLDTD(&dtd);
  parser->RegisterDTD(dtd);
  parser->SetContentSink(sink);
  PRInt32 status = parser->Parse(*in);
  NS_RELEASE(parser);
  NS_RELEASE(sink);

  return (NS_OK == status) ? 0 : -1;
}
