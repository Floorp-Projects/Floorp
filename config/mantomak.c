/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define DEFAULT_MANIFEST_EXT ".mn"
#define DEFAULT_MAKEFILE_EXT ".win"

typedef struct char_list_struct {
    char *m_pString;
    struct char_list_struct *m_pNext;
} char_list;

typedef struct macro_list_struct   {
    char *m_pMacro;
    char_list *m_pValue;
    struct macro_list_struct *m_pNext;
} macro_list;

void help(void);
char *input_filename(const char *);
char *output_filename(const char *, const char *);
int input_to_output(FILE *, FILE *);
int output_rules(FILE *);
int output_end(FILE *);
int buffer_to_output(char *, FILE *);
macro_list *extract_macros(char *);
char *find_macro(char *, char **);
void add_macro(char *, macro_list **);
int macro_length(char *);
int value_length(char *);
void add_values(char *, char_list **);
char *skip_white(char *);
int write_macros(macro_list *, FILE *);
int write_values(char_list *, FILE *, int);
void free_macro_list(macro_list *);
void free_char_list(char_list *);
void morph_macro(macro_list **, char *, char *, char *);
void slash_convert(macro_list *, char *);
int explicit_rules(macro_list *, char *, FILE *);
void create_classroot(macro_list **ppList );

int main(int argc, char *argv[])
{
	int iOS = 0;
	char *pInputFile = NULL;
	char *pOutputFile = NULL;

	/*      Figure out arguments.
	 *      [REQUIRED] First argument is input file.
	 *      [OPTIONAL] Second argument is output file.
	 */
	if(argc > 1)    {
		FILE *pInputStream = NULL;
		FILE *pOutputStream = NULL;

		/*      Form respective filenames.
		 */
		pInputFile = input_filename(argv[1]);
		pOutputFile = output_filename(pInputFile, argc > 2 ? argv[2] : NULL);

		if(pInputFile == NULL)  {
			fprintf(stderr, "MANTOMAK:  Unable to form input filename\n");
			iOS = 1;
		}
		else    {
			pInputStream = fopen(pInputFile, "rb");
			if(pInputStream == NULL)        {
				fprintf(stderr, "MANTOMAK:  Unable to open input file %s\n", pInputFile);
				iOS = 1;
			}
		}
		if(pOutputFile == NULL) {
			fprintf(stderr, "MANTOMAK:  Unable to form output filename\n");
			iOS = 1;
		}
		else if(pInputStream != NULL)   {
			pOutputStream = fopen(pOutputFile, "wt");
			if(pOutputStream == NULL)       {
				fprintf(stderr, "MANTOMAK:  Unable to open output file %s\n", pOutputFile);
				iOS = 1;
			}
		}

		/*      Only do the real processing if our error code is not
		 *              already set.
		 */
		if(iOS == 0)    {
			iOS = input_to_output(pInputStream, pOutputStream);
		}

		if(pInputStream != NULL)        {
			fclose(pInputStream);
			pInputStream = NULL;
		}
		if(pOutputStream != NULL)       {
			fclose(pOutputStream);
			pOutputStream = NULL;
		}
	}
	else    {
		help();
		iOS = 1;
	}

	if(pInputFile)  {
		free(pInputFile);
		pInputFile = NULL;
	}
	if(pOutputFile) {
		free(pOutputFile);
		pOutputFile = NULL;
	}

	return(iOS);
}

void help(void)
{
	fprintf(stderr, "USAGE:\tmantomak.exe InputFile [OutputFile]\n\n");
	fprintf(stderr, "InputFile:\tManifest file.  If without extension, \"%s\" assumed.\n", DEFAULT_MANIFEST_EXT);
	fprintf(stderr, "OutputFile:\tNMake file.  If not present, \"InputFile%s\" assumed.\n", DEFAULT_MAKEFILE_EXT);
}

char *input_filename(const char *pInputFile)
{
	char aResult[_MAX_PATH];
	char aDrive[_MAX_DRIVE];
	char aDir[_MAX_DIR];
	char aName[_MAX_FNAME];
	char aExt[_MAX_EXT];

	if(pInputFile == NULL)  {
		return(NULL);
	}

	_splitpath(pInputFile, aDrive, aDir, aName, aExt);

	if(aExt[0] == '\0')     {
		/*      No extension provided.
		 *      Use the default.
		 */
		strcpy(aExt, DEFAULT_MANIFEST_EXT);
	}

	aResult[0] = '\0';
	_makepath(aResult, aDrive, aDir, aName, aExt);

	if(aResult[0] == '\0')  {
		return(NULL);
	}
	else    {
		return(strdup(aResult));
	}
}

char *output_filename(const char *pInputFile, const char *pOutputFile)
{
	char aResult[_MAX_PATH];
	char aDrive[_MAX_DRIVE];
	char aDir[_MAX_DIR];
	char aName[_MAX_FNAME];
	char aExt[_MAX_EXT];

	if(pOutputFile != NULL) {
		return(strdup(pOutputFile));
	}

	/*      From here on out, we have to create our own filename,
	 *              implied from the input file name.
	 */

	if(pInputFile == NULL)  {
		return(NULL);
	}

	_splitpath(pInputFile, aDrive, aDir, aName, aExt);
	strcpy(aExt, DEFAULT_MAKEFILE_EXT);

	aResult[0] = '\0';
	_makepath(aResult, aDrive, aDir, aName, aExt);

	if(aResult[0] == '\0')  {
		return(NULL);
	}
	else    {
		return(strdup(aResult));
	}
}

int input_to_output(FILE *pInput, FILE *pOutput)
{
	char *pHog = NULL;
	long lSize = 0;
	int iRetval = 0;

	/*      Read the entire file into memory.
	 */
	fseek(pInput, 0, SEEK_END);
	lSize = ftell(pInput);
	fseek(pInput, 0, SEEK_SET);

	pHog = (char *)malloc(lSize + 1);
	if(pHog)        {
		*(pHog + lSize) = '\0';
		fread(pHog, lSize, 1, pInput);

		iRetval = buffer_to_output(pHog, pOutput);

		free(pHog);
		pHog = NULL;
	}
	else    {
		fprintf(stderr, "MANTOMAK:  Out of Memory....\n");
		iRetval = 1;
	}

	return(iRetval);
}

int output_rules(FILE *pOutput)
{
	int iRetval = 0;

	if(EOF ==
	fputs("\n"
	      "!if \"$(MANIFEST_LEVEL)\"==\"RULES\""
	      "\n",
	    pOutput))
    {
		fprintf(stderr, "MANTOMAK:  Error writing to file....\n");
		iRetval = 1;
	}
	return(iRetval);
}

int output_end(FILE *pOutput)
{
	int iRetval = 0;

	if(EOF ==
	fputs("\n"
	      "!endif"
	      "\n",
	    pOutput))
    {
		fprintf(stderr, "MANTOMAK:  Error writing to file....\n");
		iRetval = 1;
	}
	return(iRetval);
}


int buffer_to_output(char *pBuffer, FILE *pOutput)
{
    int iRetval = 0;
    macro_list *pMacros = NULL;

    /*  Tokenize the macros and their corresponding values.
     */
    pMacros = extract_macros(pBuffer);
    if(pMacros != NULL) {
	/*  Perform forward to backslash conversion on those macros known to be
	 *      path information only.
	 */
	slash_convert(pMacros, "JBOOTDIRS");
	slash_convert(pMacros, "JDIRS");
	slash_convert(pMacros, "DEPTH");
	slash_convert(pMacros, "NS_DEPTH");
	slash_convert(pMacros, "PACKAGE");
	slash_convert(pMacros, "JMC_GEN_DIR");
	slash_convert(pMacros, "DIST_PUBLIC");

	/*  Process some of the macros, and convert them
	 *      into different macros with different data.
	 */
	morph_macro(&pMacros, "JMC_GEN", "JMC_HEADERS", "$(JMC_GEN_DIR)\\%s.h");
	morph_macro(&pMacros, "JMC_GEN", "JMC_STUBS", "$(JMC_GEN_DIR)\\%s.c");
	morph_macro(&pMacros, "JMC_GEN", "JMC_OBJS", ".\\$(OBJDIR)\\%s.obj");
	morph_macro(&pMacros, "CSRCS", "C_OBJS", ".\\$(OBJDIR)\\%s.obj");
	morph_macro(&pMacros, "CPPSRCS", "CPP_OBJS", ".\\$(OBJDIR)\\%s.obj");
	morph_macro(&pMacros, "REQUIRES", "LINCS", "-I$(XPDIST)\\public\\%s");

	create_classroot( &pMacros );

	/*  Output the Macros and the corresponding values.
	 */
	iRetval = write_macros(pMacros, pOutput);

	/*  Output rule file inclusion
	 */
	if(iRetval == 0)    {
	    iRetval = output_rules(pOutput);
	}

	/*  Output explicit build rules/dependencies for JMC_GEN.
	 */
	if(iRetval == 0)    {
	    iRetval = explicit_rules(pMacros, "JMC_GEN", pOutput);
	}

	if(iRetval == 0)    {
	    iRetval = output_end(pOutput);
	}
	/*  Free off the macro list.
	 */
	free_macro_list(pMacros);
	pMacros = NULL;
    }

    return(iRetval);
}

int explicit_rules(macro_list *pList, char *pMacro, FILE *pOutput)
{
    int iRetval = 0;
    macro_list *pEntry = NULL;

    if(pList == NULL || pMacro == NULL || pOutput == NULL) {
	return(0);
    }

    /*  Find macro of said name.
     *  Case insensitive.
     */
    pEntry = pList;
    while(pEntry)    {
	if(stricmp(pEntry->m_pMacro, pMacro) == 0)  {
	    break;
	}

	pEntry = pEntry->m_pNext;
    }

    if(pEntry)  {
	/*  Decide style of rule depending on macro name.
	 */
	if(stricmp(pEntry->m_pMacro, "JMC_GEN") == 0)    {
	    char_list *pNames = NULL;
	    char *pModuleName = NULL;
	    char *pClassName = NULL;

	    pNames = pEntry->m_pValue;
	    while(pNames)   {
		pModuleName = pNames->m_pString;
		pClassName = pModuleName + 1;

		fprintf(pOutput, "$(JMC_GEN_DIR)\\%s.h", pModuleName);
		fprintf(pOutput, ": ");
		fprintf(pOutput, "$(JMCSRCDIR)\\%s.class", pClassName);
		fprintf(pOutput, "\n    ");
		fprintf(pOutput, "$(JMC) -d $(JMC_GEN_DIR) -interface $(JMC_GEN_FLAGS) $(?F:.class=)");
		fprintf(pOutput, "\n");

		fprintf(pOutput, "$(JMC_GEN_DIR)\\%s.c", pModuleName);
		fprintf(pOutput, ": ");
		fprintf(pOutput, "$(JMCSRCDIR)\\%s.class", pClassName);
		fprintf(pOutput, "\n    ");
		fprintf(pOutput, "$(JMC) -d $(JMC_GEN_DIR) -module $(JMC_GEN_FLAGS) $(?F:.class=)");
		fprintf(pOutput, "\n");

		pNames = pNames->m_pNext;
	    }
	}
	else    {
	    /*  Don't know how to format macro.
	     */
	    iRetval = 69;
	}
    }

    return(iRetval);
}

void slash_convert(macro_list *pList, char *pMacro)
{
    macro_list *pEntry = NULL;

    if(pList == NULL || pMacro == NULL) {
	return;
    }

    /*  Find macro of said name.
     *  Case insensitive.
     */
    pEntry = pList;
    while(pEntry)    {
	if(stricmp(pEntry->m_pMacro, pMacro) == 0)  {
	    break;
	}

	pEntry = pEntry->m_pNext;
    }

    if(pEntry)  {
	char *pConvert = NULL;
	char_list *pValue = pEntry->m_pValue;

	while(pValue)   {
	    pConvert = pValue->m_pString;
	    while(pConvert && *pConvert)    {
		if(*pConvert == '/')    {
		    *pConvert = '\\';
		}
		pConvert++;
	    }
	    pValue = pValue->m_pNext;
	}
    }
}

void morph_macro(macro_list **ppList, char *pMacro, char *pMorph, char *pPrintf)
{
    macro_list *pEntry = NULL;

    if(ppList == NULL || pMacro == NULL || pMorph == NULL || pPrintf == NULL) {
	return;
    }

    /*  Find macro of said name.
     *  Case insensitive.
     */
    pEntry = *ppList;
    while(pEntry)    {
	if(stricmp(pEntry->m_pMacro, pMacro) == 0)  {
	    break;
	}

	pEntry = pEntry->m_pNext;
    }

    if(pEntry)  {
	char_list *pFilename = NULL;
	char aPath[_MAX_PATH];
	char aDrive[_MAX_DRIVE];
	char aDir[_MAX_DIR];
	char aFName[_MAX_FNAME];
	char aExt[_MAX_EXT];
	char *pBuffer = NULL;

	/*  Start with buffer size needed.
	 *  We expand this as we go along if needed.
	 */
	pBuffer = (char *)malloc(strlen(pMorph) + 2);
	strcpy(pBuffer, pMorph);
	strcat(pBuffer, "=");

	/*  Go through each value, converting over to new macro.
	 */
	pFilename = pEntry->m_pValue;
	while(pFilename) {
	    _splitpath(pFilename->m_pString, aDrive, aDir, aFName, aExt);

	    /*  Expand buffer by required amount.
	     */
	    sprintf(aPath, pPrintf, aFName);
	    strcat(aPath, " ");
	    pBuffer = (char *)realloc(pBuffer, _msize(pBuffer) + strlen(aPath));
	    strcat(pBuffer, aPath);

	    pFilename = pFilename->m_pNext;
	}

	/*  Add the macro.
	 */
	add_macro(pBuffer, ppList);

	free(pBuffer);
	pBuffer = NULL;
    }
}

void create_classroot(macro_list **ppList )
{
    char cwd[512];
    int i, i2;
    macro_list *pEntry = NULL;
    macro_list *pE;

    /*  Find macro of said name.
     *  Case insensitive.
     */
    pEntry = *ppList;
    while(pEntry)    {
	if(stricmp(pEntry->m_pMacro, "PACKAGE") == 0)  {
	    break;
	}

	pEntry = pEntry->m_pNext;
    }

    if(pEntry == 0 || pEntry->m_pValue == 0 || pEntry->m_pValue->m_pString == 0)  {
	return;
    }

    _getcwd( cwd, 512 );

    i = strlen( pEntry->m_pValue->m_pString );
    i2 = strlen( cwd );

    cwd[i2-i-1] = 0;

    pE = NULL;
    pE = (macro_list *)calloc(sizeof(macro_list),1);
    pE->m_pMacro = strdup("CLASSROOT");
    pE->m_pValue = (char_list *)calloc(sizeof(char_list),1);
    pE->m_pValue->m_pString = strdup(cwd);

    while(*ppList)  {
	ppList = &((*ppList)->m_pNext);
    }
    *ppList = pE;
}


int write_macros(macro_list *pList, FILE *pOutput)
{
    int iRetval = 0;
    int iLineLength = 0;

    if(pList == NULL || pOutput == NULL)    {
	return(0);
    }

	if(EOF ==
	fputs("\n"
	      "!if \"$(MANIFEST_LEVEL)\"==\"MACROS\""
	      "\n",
	    pOutput))
    {
		fprintf(stderr, "MANTOMAK:  Error writing to file....\n");
		return(1);
	}

    while(pList)    {
        int bIgnoreForWin16 = 0;

        /* The following macros should not be emitted for Win16 */
        if (0 == strcmp(pList->m_pMacro, "LINCS")) {
            bIgnoreForWin16 = 1;
        }


        if (bIgnoreForWin16) {
            if(0 > fprintf(pOutput, "!if \"$(MOZ_BITS)\" != \"16\"\n"))  {
                fprintf(stderr, "MANTOMAK:  Error writing to file....\n");
                iRetval = 1;
                break;
            }
        }

	if(0 > fprintf(pOutput, "%s=", pList->m_pMacro))  {
		fprintf(stderr, "MANTOMAK:  Error writing to file....\n");
	    iRetval = 1;
	    break;
	}
	iLineLength += strlen(pList->m_pMacro) + 1;

	iRetval = write_values(pList->m_pValue, pOutput, iLineLength);
	if(iRetval) {
	    break;
	}

	if(EOF == fputc('\n', pOutput))    {
		fprintf(stderr, "MANTOMAK:  Error writing to file....\n");
	    iRetval = 1;
	    break;
	}
	iLineLength = 0;

	pList = pList->m_pNext;

        if (bIgnoreForWin16) {
            if(0 > fprintf(pOutput, "!endif\n"))  {
                fprintf(stderr, "MANTOMAK:  Error writing to file....\n");
                iRetval = 1;
                break;
            }
            bIgnoreForWin16 = 0;
        }
    }

	if(EOF ==
	fputs("\n"
	      "!endif"
	      "\n",
	    pOutput))
    {
		fprintf(stderr, "MANTOMAK:  Error writing to file....\n");
		return(1);
	}
    return(iRetval);
}

int write_values(char_list *pList, FILE *pOutput, int iLineLength)
{
    int iRetval = 0;
    
    if(pList == NULL || pOutput == NULL)    {
	return(0);
    }

    while(pList)    {
	if(iLineLength == 0)    {
	    if(EOF == fputs("    ", pOutput))  {
		    fprintf(stderr, "MANTOMAK:  Error writing to file....\n");
		iRetval = 1;
		break;
	    }
	    iLineLength += 4;

	    if(0 > fprintf(pOutput, "%s ", pList->m_pString))   {
		    fprintf(stderr, "MANTOMAK:  Error writing to file....\n");
		iRetval = 1;
		break;
	    }
	    iLineLength += strlen(pList->m_pString) + 1;
	}
	else if(iLineLength + strlen(pList->m_pString) > 72)    {
	    if(EOF == fputs("\\\n", pOutput)) {
		    fprintf(stderr, "MANTOMAK:  Error writing to file....\n");
		iRetval = 1;
		break;
	    }
	    iLineLength = 0;
	    continue;
	}
	else    {
	    if(0 > fprintf(pOutput, "%s ", pList->m_pString))   {
		    fprintf(stderr, "MANTOMAK:  Error writing to file....\n");
		iRetval = 1;
		break;
	    }
	    iLineLength += strlen(pList->m_pString) + 1;
	}

	pList = pList->m_pNext;
    }

    return(iRetval);
}

macro_list *extract_macros(char *pBuffer)
{
    macro_list *pRetval = NULL;
    char *pTraverse = NULL;
    char *pMacro = NULL;

    pTraverse = pBuffer;
    while(pTraverse)    {
	pMacro = NULL;
	pTraverse = find_macro(pTraverse, &pMacro);
	if(pMacro)  {
	    add_macro(pMacro, &pRetval);
	}
    }

    return(pRetval);
}

void add_macro(char *pString, macro_list **ppList)
{
    macro_list *pEntry = NULL;
    int iLength = 0;

    if(pString == NULL || *pString == '\0' || ppList == NULL) {
	return;
    }

    /*  Allocate a new list entry for the macro.
     */
    pEntry = (macro_list *)malloc(sizeof(macro_list));
    memset(pEntry, 0, sizeof(macro_list));

    /*  Very first part of the string is the macro name.
     *  How long is it?
     */
    iLength = macro_length(pString);
    pEntry->m_pMacro = (char *)malloc(iLength + 1);
    memset(pEntry->m_pMacro, 0, iLength + 1);
    strncpy(pEntry->m_pMacro, pString, iLength);

    /*  Skip to the values.
     *  These are always on the right side of an '='
     */
    pString = strchr(pString, '=');
    if(pString) {
	pString++;
    }
    add_values(pString, &(pEntry->m_pValue));

    /*  Add the macro to the end of the macro list.
     */
    while(*ppList)  {
	ppList = &((*ppList)->m_pNext);
    }
    *ppList = pEntry;
}

void add_values(char *pString, char_list **ppList)
{
    char_list **ppTraverse = NULL;
    char_list *pEntry = NULL;
    int iLength = 0;
    int iBackslash = 0;

    if(pString == NULL || *pString == '\0' || ppList == NULL)   {
	return;
    }

    while(pString)  {
	/*  Find start of value.
	 */
	iBackslash = 0;
	while(*pString) {
	    if(*pString == '\\')    {
		iBackslash++;
	    }
	    else if(*pString == '\n')   {
		if(iBackslash == 0)  {
		    /*  End of values.
		     *  Setting to NULL gets out of all loops.
		     */
		    pString = NULL;
		    break;
		}
		iBackslash = 0;
	    }
	    else if(!isspace(*pString))  {
		/*  Backslashes part of string.
		 *  This screws up if a backslash is in the middle of the string.
		 */
		pString -= iBackslash;
		break;
	    }

	    pString++;
	}
	if(pString == NULL || *pString == '\0') {
	    break;
	}

	/*  Do not honor anything beginning with a #
	 */
	if(*pString == '#') {
	    /*  End of line.
	     */
	    while(*pString && *pString != '\n') {
		pString++;
	    }
	    continue;
	}

	/*  Very first part of the string is value name.
	 *  How long is it?
	 */
	iLength = value_length(pString);

	/*  Do not honor $(NULL)
	 */
	if(_strnicmp(pString, "$(NULL)", 7) == 0)    {
	    pString += iLength;
	    continue;
	}

	/*  Allocate a new list entry for the next value.
	 */
	pEntry = (char_list *)malloc(sizeof(char_list));
	memset(pEntry, 0, sizeof(char_list));

	pEntry->m_pString = (char *)malloc(iLength + 1);
	memset(pEntry->m_pString, 0, iLength + 1);
	strncpy(pEntry->m_pString, pString, iLength);

	/*  Add new value entry to the end of the list.
	 */
	ppTraverse = ppList;
	while(*ppTraverse)  {
	    ppTraverse = &((*ppTraverse)->m_pNext);
	}
	*ppTraverse = pEntry;

	/*  Go on to next value.
	 */
	pString += iLength;
    }
}

char *find_macro(char *pBuffer, char **ppMacro)
{
    char *pRetval = NULL;
    int iBackslash = 0;

    if(pBuffer == NULL || ppMacro == NULL) {
	return(NULL);
    }

    /*  Skip any whitespace in the buffer.
     *  If comments need to be skipped also, this is the place.
     */
    while(1)    {
	while(*pBuffer && isspace(*pBuffer))    {
	    pBuffer++;
	}
	if(*pBuffer == '#') {
	    /*  Go to the end of the line, it's a comment.
	     */
	    while(*pBuffer && *pBuffer != '\n') {
		pBuffer++;
	    }

	    continue;
	}
	break;
    }

    if(*pBuffer)    {
	/*  Should be at the start of a macro.
	 */
	*ppMacro = pBuffer;
    }

    /*  Find the end of the macro for the return value.
     *  This is the end of a line which does not contain a backslash at the end.
     */
    while(*pBuffer) {
	if(*pBuffer == '\\')    {
	    iBackslash++;
	}
	else if(*pBuffer == '\n')   {
	    if(iBackslash == 0)  {
		pRetval = pBuffer + 1;
		break;
	    }
	    iBackslash = 0;
	}
	else if(!isspace(*pBuffer))  {
	    iBackslash = 0;
	}

	pBuffer++;
    }

    return(pRetval);
}

int macro_length(char *pMacro)
{
    int iRetval = 0;

    if(pMacro == NULL)  {
	return(0);
    }

    /*  Length is no big deal.
     *  Problem is finding the end:
     *      whitespace
     *      '='
     */
    while(*pMacro)  {
	if(*pMacro == '=')  {
	    break;
	}
	else if(isspace(*pMacro))   {
	    break;
	}
	
	pMacro++;
	iRetval++;
    }

    return(iRetval);
}

int value_length(char *pValue)
{
    int iRetval = 0;

    if(pValue == NULL)  {
	return(0);
    }

    /*  Length is no big deal.
     *  Problem is finding the end:
     *      whitespace
     *      '\\'whitespace
     */
    while(*pValue)  {
	if(*pValue == '\\')  {
	    char *pFindNewline = pValue + 1;
	    /*  If whitespace to end of line, break here.
	     */
	    while(isspace(*pFindNewline))   {
		if(*pFindNewline == '\n')   {
		    break;
		}
		pFindNewline++;
	    }
	    if(*pFindNewline == '\n')   {
		break;
	    }
	}
	else if(isspace(*pValue))   {
	    break;
	}
	
	pValue++;
	iRetval++;
    }

    return(iRetval);
}

char *skip_white(char *pString)
{
    if(pString == NULL) {
	return(NULL);
    }

    while(*pString && isspace(*pString)) {
	pString++;
    }

    return(pString);
}

void free_macro_list(macro_list *pList)
{
    macro_list *pFree = NULL;

    if(pList == NULL)   {
	return;
    }

    while(pList)    {
	pFree = pList;
	pList = pList->m_pNext;

	pFree->m_pNext = NULL;

	free_char_list(pFree->m_pValue);
	pFree->m_pValue = NULL;

	free(pFree->m_pMacro);
	pFree->m_pMacro = NULL;

	free(pFree);
	pFree = NULL;
    }
}

void free_char_list(char_list *pList)
{
    char_list *pFree = NULL;

    if(pList == NULL)   {
	return;
    }

    while(pList)    {
	pFree = pList;
	pList = pList->m_pNext;

	pFree->m_pNext = NULL;

	free(pFree->m_pString);
	pFree->m_pString = NULL;

	free(pFree);
	pFree = NULL;
    }
}
