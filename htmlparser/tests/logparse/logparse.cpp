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
#include "CNavDTD.h"
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
static NS_DEFINE_IID(kLoggingSinkCID, NS_LOGGING_SINK_IID);

// Interface IID's
static NS_DEFINE_IID(kIParserIID, NS_IPARSER_IID);
static NS_DEFINE_IID(kILoggingSinkIID, NS_ILOGGING_SINK_IID);

//----------------------------------------------------------------------

static void SetupRegistry()
{
  nsComponentManager::RegisterComponent(kParserCID, NULL, NULL, PARSER_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kLoggingSinkCID, NULL, NULL, PARSER_DLL,PR_FALSE,PR_FALSE);
}

//----------------------------------------------------------------------

static const char* kWorkingDir = "s:/mozilla/htmlparser/tests/logparse";

nsresult GenerateBaselineFile(const char* aSourceFilename,const char* aBaselineFilename) {
	nsresult result=NS_OK;

	if(aSourceFilename && aBaselineFilename) {

		fstream theInputStream(aSourceFilename,ios::in | ios::nocreate);

		// Create a parser
		nsIParser* parser;
		nsresult rv = nsComponentManager::CreateInstance(kParserCID,nsnull,kIParserIID,(void**)&parser);
		if (NS_OK != rv) {
			cout << "Unable to create a parser (" << rv << ")" <<endl;
			return -1;
		}

		// Create a sink
		nsILoggingSink* sink;
		rv = nsComponentManager::CreateInstance(kLoggingSinkCID,nsnull,kILoggingSinkIID,(void**)&sink);
		if (NS_OK != rv) {
			cout << "Unable to create a sink (" << rv << ")" <<endl;
			return -1;
		}

		{
			fstream theOutputStream(aBaselineFilename,ios::out);
			sink->SetOutputStream(theOutputStream);

			// Parse the document, having the sink write the data to fp
			nsIDTD* dtd = nsnull;
			NS_NewNavHTMLDTD(&dtd);
			parser->RegisterDTD(dtd);
			parser->SetContentSink(sink);
			result = parser->Parse(theInputStream);
			NS_RELEASE(parser); 
			NS_RELEASE(sink);
		}

	}
  return (NS_OK == result) ? 0 : -1;
}

//----------------------------------------------------------------------

PRBool CompareFiles(const char* aFilename1, const char* aFilename2) {
	PRBool result=PR_TRUE;

	fstream theFirstStream(aFilename1,ios::in | ios::nocreate);
	fstream theSecondStream(aFilename2,ios::in | ios::nocreate);
	
	PRBool done=PR_FALSE;		
	char   ch1,ch2;

	while(!done) {
		theFirstStream >> ch1;
		theSecondStream >> ch2;
		if(ch1!=ch2) {
			result=PR_FALSE;
			break;
		}
		done=PRBool((theFirstStream.ipfx(1)==0) || (theSecondStream.ipfx(1)==0));
	}
	return result;
}

//----------------------------------------------------------------------

void ComputeTempFilename(const char* anIndexFilename, char* aTempFilename) {
	if(anIndexFilename) {
		strcpy(aTempFilename,anIndexFilename);			
		char* pos=strrchr(aTempFilename,'\\');
		if(!pos)
			pos=strrchr(aTempFilename,'/');
		if(pos) {
			(*pos)=0;
			strcat(aTempFilename,"/temp.blx");
			return;
		}
	}
		//fall back to our last resort...
	strcpy(aTempFilename,"c:/windows/temp/temp.blx");
}

//----------------------------------------------------------------------

static const char* kAppName = "logparse ";
static const char* kOption1 = "Compare baseline file-set";
static const char* kOption2 = "Generate baseline ";
static const char* kResultMsg[2] = {" failed!"," ok."};

void ValidateBaselineFiles(const char* anIndexFilename) {

	fstream theIndexFile(anIndexFilename,ios::in | ios::nocreate);
	char		theFilename[500];
	char		theBaselineFilename[500];
	char		theTempFilename[500];
	PRBool	done=PR_FALSE;

	ComputeTempFilename(anIndexFilename,theTempFilename);

	while(!done) {
		theIndexFile >> theFilename;
		theIndexFile >> theBaselineFilename;
		if(theFilename[0] && theBaselineFilename[0]) {
			if(0==GenerateBaselineFile(theFilename,theTempFilename)) {
				PRBool matches=CompareFiles(theTempFilename,theBaselineFilename);
				cout << theFilename << kResultMsg[matches] << endl;
			}
		}
		theFilename[0]=0;
		theBaselineFilename[0]=0;
		done=PRBool(theIndexFile.ipfx(1)==0);
	}


		// Now it's time to compare our output to the baseline...
//	if(!CompareFiles(aBaselineFilename,aBaselineFilename)){
//		cout << "File: \"" << aSourceFilename << "\" does not match baseline." << endl;
//	}

}


//----------------------------------------------------------------------

int main(int argc, char** argv)
{
  if (argc < 2) {
		cout << "Usage: " << kAppName << " [options] [filename]" << endl; 
		cout << "       -c [filelist]   " << kOption1 << endl; 
		cout << "       -g [in] [out]   " << kOption2 << endl; 
    return -1;
  }

	int result=0;

	SetupRegistry();

	if(0==strcmp("-c",argv[1])) {

		if(argc>2) {
			cout << kOption1 << "..." << endl;

				//Open the master filelist, and read the filenames.
				//Each line contains a source filename and a baseline filename, separated by a space.
			ValidateBaselineFiles(argv[2]);
		}
		else {
			cout << kAppName << ": Filelist missing for -c option -- nothing to do." << endl;
		}

	}
	else if(0==strcmp("-g",argv[1])) {
		if(argc>3) {
			cout << kOption2 << argv[3] << " from " << argv[2] << "..." << endl;
			GenerateBaselineFile(argv[2],argv[3]);
		}
		else {
			cout << kAppName << ": Filename(s) missing for -g option -- nothing to do." << endl;
		}
	}
	else {
			cout << kAppName << ": Unknown options -- nothing to do." << endl;
	}
	return result;
}
