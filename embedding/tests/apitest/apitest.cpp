/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *  Adam Lock <adamlock@netscape.com>
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

#include <stdio.h>
#include <crtdbg.h>

#include "nsEmbedAPI.h"

int main(int argc, char *argv[])
{
    _CrtSetReportMode( _CRT_WARN, _CRTDBG_MODE_FILE );
    _CrtSetReportFile( _CRT_WARN, _CRTDBG_FILE_STDOUT );
   

    _CrtMemState sBefore, sMiddle, sAfter;

    // Store a memory checkpoint in the s1 memory-state structure
    _CrtMemCheckpoint(&sBefore);

   	char *pszBinDirPath = nsnull;
	if (argc > 1)
	{
		pszBinDirPath = argv[1];
	}
	printf("apitest running...\n");
	
	nsCOMPtr<nsILocalFile> binDir;
	NS_NewLocalFile(pszBinDirPath, PR_TRUE, getter_AddRefs(binDir));
	
	nsresult rv = NS_InitEmbedding(binDir, nsnull);
	if (NS_FAILED(rv))
	{
		printf("NS_InitEmbedding FAILED (rv = 0x%08x)\n", rv);
		// DROP THROUGH - NS_TermEmbedding should cope
	}
    else
    {
        printf("NS_InitEmbedding SUCCEEDED (rv = 0x%08x)\n", rv);
    }

	// TODO put lot's of interesting diagnostic stuff here such as time taken
	//      to initialise
    // Memory used up etc.
    _CrtMemCheckpoint(&sMiddle);


	rv = NS_TermEmbedding();
	if (NS_FAILED(rv))
	{
		printf("NS_TermEmbedding FAILED (rv = 0x%08x)\n", rv);
		// DROP THROUGH
	}
    else
    {
        printf("NS_TermEmbedding SUCCEEDED (rv = 0x%08x)\n", rv);
    }

	// TODO more interesting stuff. Time to shutdown

    // Dump the leaked memory
    printf("FINAL LEAKAGE:\n");
    _CrtMemCheckpoint(&sAfter);
     _CrtMemState sDifference;
    if ( _CrtMemDifference(&sDifference, &sBefore, &sAfter))
    {
        _CrtMemDumpStatistics(&sDifference);
    }

    _CrtDumpMemoryLeaks( );

    printf("apitest complete\n");

	return 0;
}