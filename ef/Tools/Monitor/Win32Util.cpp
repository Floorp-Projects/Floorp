/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
// Win32Util.cpp
//
// Scott M. Silver

#include "Win32Util.h"
#include <Windows.h>

void
showLastError()
{
	// if we fail put of one of those sweet Win32 dialogs
	char* lpMsgBuf;

	::FormatMessage( 
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		(LPTSTR) &lpMsgBuf,
		0,
		NULL);		
	::MessageBox( NULL, lpMsgBuf, "GetLastError", MB_OK|MB_ICONINFORMATION );
}


/* retrieve name of module from module's open file handle */
void retrieveModuleName (
    char      *lpszModule,
    HANDLE    hFile)
{
    HANDLE		     hMapFile;
    LPVOID		     lpFile;
    char		     *lpszName;
    int 		     nSections;
    ULONG		     VAExportDir;
    int 		     i=0;
    int 		     ImageHdrOffset;
    PIMAGE_SECTION_HEADER    psh;
    PIMAGE_FILE_HEADER	     pfh;
    PIMAGE_OPTIONAL_HEADER   poh;
    PIMAGE_EXPORT_DIRECTORY  ped;


    /* memory map handle to DLL for easy access */
    hMapFile = CreateFileMapping (hFile,
				  (LPSECURITY_ATTRIBUTES)NULL,
				  PAGE_READONLY,
				  0,
				  0,
				  NULL);

    /* map view of entire file */
    lpFile = MapViewOfFile (hMapFile, FILE_MAP_READ, 0, 0, 0);

    /* if DOS based file */
    if (*((USHORT *)lpFile) == IMAGE_DOS_SIGNATURE)
	{
	/* file image header offset exists after DOS header and nt signature */
	ImageHdrOffset = (int)((ULONG *)lpFile)[15] + sizeof (ULONG);
	if (*((ULONG *)((char *)lpFile + ImageHdrOffset - sizeof (ULONG))) !=
	    IMAGE_NT_SIGNATURE)
	    {
	    strcpy (lpszModule, "Error, no IMAGE_NT_SIGNATURE");
	    goto EXIT;
	    }
	}

    pfh = (PIMAGE_FILE_HEADER)((char *)lpFile + ImageHdrOffset);

    /* if optional header exists and exports directory exists proceed */
    if (pfh->SizeOfOptionalHeader)
	{
	/* locate virtual address for Export Image Directory in OptionalHeader */
	poh = (PIMAGE_OPTIONAL_HEADER)((char *)pfh + sizeof (IMAGE_FILE_HEADER));
	VAExportDir = poh->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;

	/* locate section where export virtual address is located */
	psh = (PIMAGE_SECTION_HEADER)((char *)poh + pfh->SizeOfOptionalHeader);
	nSections = pfh->NumberOfSections;
	while (i++<nSections)
	    {
	    if (psh->VirtualAddress <= VAExportDir &&
		psh->VirtualAddress + psh->SizeOfRawData > VAExportDir)
		break;
	    psh++;
	    }

	/* locate export image directory */
	if (i < nSections)
	    ped = (PIMAGE_EXPORT_DIRECTORY)((char *)lpFile +
		(VAExportDir - psh->VirtualAddress) + psh->PointerToRawData);
	else
	    {
	    strcpy (lpszModule, "IMAGE_EXPORT_DIRECTORY not found");
	    goto EXIT;
	    }

	/* read name from export directory */
	lpszName = (char *)lpFile + ped->Name + (psh->PointerToRawData - psh->VirtualAddress);
	strcpy (lpszModule, lpszName);
	}

    else
	strcpy (lpszModule, "Error, no IMAGE_OPTIONAL_HEADER");

EXIT:
    /* clean up before exiting */
    UnmapViewOfFile (lpFile);
    CloseHandle (hMapFile);
}


DWORD
threadSuspendCount(HANDLE inThreadH)
{
	DWORD suspendCount;

	suspendCount = ::SuspendThread(inThreadH);
	::ResumeThread(inThreadH);

	return (suspendCount);
}
