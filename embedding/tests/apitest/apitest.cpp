/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: Mozilla-sample-code 1.0
 *
 * Copyright (c) 2002 Netscape Communications Corporation and
 * other contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this Mozilla sample software and associated documentation files
 * (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Contributor(s):
 *   Adam Lock <adamlock@netscape.com>
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