/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Patrick C. Beard <beard@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Memory.h>

#include "SymFiles.h"
#include "sym_file.h"
#include "gc.h"

extern "C" {
FILE* FSp_fopen(ConstFSSpecPtr spec, const char * open_mode);
}

static int getFileReferenceTableEntry(FILE* symFile,
									   const DiskSymbolHeaderBlock& header,
									   const FileReference& fileRef,
									   FileReferenceTableEntry* frte)
{
	UInt32 fileRefsPerPage = (header.dshb_page_size / sizeof(FileReferenceTableEntry));
	UInt32 fileRefPage = (fileRef.fref_frte_index / fileRefsPerPage);
	UInt32 fileRefIndex = (fileRef.fref_frte_index % fileRefsPerPage);

	// seek to the specified frte.
	fseek(symFile, (header.dshb_frte.dti_first_page + fileRefPage) * header.dshb_page_size + fileRefIndex * sizeof(FileReferenceTableEntry), SEEK_SET);
	return fread(frte, sizeof(FileReferenceTableEntry), 1, symFile);
}

static int getContainedStatementTableEntry(FILE* symFile,
									   const DiskSymbolHeaderBlock& header,
									   UInt32 statementIndex,
									   ContainedStatementsTableEntry* statementEntry)
{
	UInt32 entriesPerPage = (header.dshb_page_size / sizeof(ContainedStatementsTableEntry));
	UInt32 entryPage = (statementIndex / entriesPerPage);
	UInt32 entryIndex = (statementIndex % entriesPerPage);
	fseek(symFile, (header.dshb_csnte.dti_first_page + entryPage) * header.dshb_page_size + entryIndex * sizeof(ContainedStatementsTableEntry), SEEK_SET);
	return fread(statementEntry, sizeof(ContainedStatementsTableEntry), 1, symFile);
}

inline UInt32 delta(UInt32 x, UInt32 y)
{
	if (x > y)
		return (x - y);
	else
		return (y - x);
}

inline bool isMeatyModule(const ModulesTableEntry& moduleEntry)
{
	return (moduleEntry.mte_kind == MODULE_KIND_PROCEDURE) || (moduleEntry.mte_kind == MODULE_KIND_FUNCTION);
}

static const UInt8* getName(const DiskSymbolHeaderBlock& header, const UInt8* nameTable[], UInt32 nameIndex)
{
	UInt32 nameOffset = 2 * nameIndex;
	const UInt8* page = nameTable[nameOffset / header.dshb_page_size];
	const UInt8* name = page + (nameOffset % header.dshb_page_size);
	return name;
}

static int getSource(UInt32 codeOffset, FILE* symFile,
					  const DiskSymbolHeaderBlock& header,
					  const ResourceTableEntry& codeEntry,
					  const UInt8* nameTable[], char fileName[256],  UInt32* fileOffset)
{
	// since module entries can't span pages, must compute which page module entry size.
	UInt32 modulesPerPage = (header.dshb_page_size / sizeof(ModulesTableEntry));

	// search for MTE nearest specified offset.
	// seek to first MTE.
	for (UInt16 i = codeEntry.rte_mte_first; i <= codeEntry.rte_mte_last; i++) {
		ModulesTableEntry moduleEntry;
		UInt32 modulePage = (i / modulesPerPage);
		UInt32 moduleIndex = (i % modulesPerPage);
		fseek(symFile, (header.dshb_mte.dti_first_page + modulePage) * header.dshb_page_size + moduleIndex * sizeof(ModulesTableEntry), SEEK_SET);
		if (fread(&moduleEntry, sizeof(moduleEntry), 1, symFile) == 1) {
			const UInt8* moduleName = getName(header, nameTable, moduleEntry.mte_nte_index);
			// printf("module name = %#s\n", moduleName);
			if (isMeatyModule(moduleEntry) && (codeOffset >= moduleEntry.mte_res_offset) && (codeOffset - moduleEntry.mte_res_offset) < moduleEntry.mte_size) {
				FileReferenceTableEntry frte;
				if (getFileReferenceTableEntry(symFile, header, moduleEntry.mte_imp_fref, &frte) == 1) {
					// get the name of the file.
					const UInt8* name = getName(header, nameTable, frte.frte_fn.nte_index);
					UInt32 length = name[0];
					BlockMoveData(name + 1, fileName, length);
					fileName[length] = '\0';
					// printf("file name = %s\n", fileName);
					// try to refine the location, using the contained statements table entries.
					UInt32 closestFileOffset = moduleEntry.mte_imp_fref.fref_offset;
					UInt32 closestCodeOffset = moduleEntry.mte_res_offset;
					UInt32 closestCodeDelta = 0xFFFFFFFF;
					UInt32 currentFileOffset, currentCodeOffset = moduleEntry.mte_res_offset, currentCodeDelta;
					for (UInt32 j = moduleEntry.mte_csnte_idx_1; j <= moduleEntry.mte_csnte_idx_2; j++) {
						ContainedStatementsTableEntry statementEntry;
						if (getContainedStatementTableEntry(symFile, header, j, &statementEntry) == 1) {
							switch (statementEntry.csnte_file.change) {
							case kSourceFileChange:
								currentFileOffset = statementEntry.csnte_file.fref.fref_offset;
								break;
							case kEndOfList:
								break;
							default:
								currentFileOffset += statementEntry.csnte.file_delta;
								currentCodeOffset = moduleEntry.mte_res_offset + statementEntry.csnte.mte_offset;
								currentCodeDelta = delta(currentCodeOffset, codeOffset);
								if (currentCodeDelta < closestCodeDelta) {
									closestFileOffset = currentFileOffset;
									closestCodeOffset = currentCodeOffset;
									closestCodeDelta = currentCodeDelta;
								}
								break;
							}
						}
					}
					*fileOffset = closestFileOffset;
					// printf("closest file offset = %d\n", closestFileOffset);
					// printf("closest code offset = %d\n", closestCodeOffset);
					return 1;
				}
			}
		}
	}
	return 0;
}

/**
 * Returns the complete name table from the symbol file.
 */
static UInt8** getNameTable(FILE* symFile, const DiskSymbolHeaderBlock& header)
{
	UInt8** nameTable = new UInt8*[header.dshb_nte.dti_page_count];
	long count = 0;
	if (nameTable != NULL) {
		// seek to first page of the name table.
		fseek(symFile, header.dshb_nte.dti_first_page * header.dshb_page_size, SEEK_SET);
		// read all of the pages into memory, for speed.
		for (int i = 0; i < header.dshb_nte.dti_page_count; i++) {
			// allocate each page atomically, so GC won't have to scan them.
			UInt8* page = nameTable[i] = (UInt8*) GC_malloc_atomic(header.dshb_page_size);
			if (page != NULL)
				fread(page, header.dshb_page_size, 1, symFile);
		}
	}
	return nameTable;
}

struct sym_file {
	FILE* mFile;
	DiskSymbolHeaderBlock mHeader;
	ResourceTableEntry mCodeEntry;
	UInt8** mNameTable;
	
	sym_file() : mFile(NULL), mNameTable(NULL) {}
	~sym_file();
};

sym_file::~sym_file()
{
	// this seems to die when we are being debugged.
	if (mFile != NULL) {
		::fclose(mFile);
		mFile = NULL;
	}
	
	if (mNameTable != NULL) {
		UInt8** nameTable = mNameTable;
		UInt16 pageCount = mHeader.dshb_nte.dti_page_count;
		while (pageCount-- > 0) {
			GC_free(*nameTable++);
		}
		delete[] mNameTable;
		mNameTable = NULL;
	}
}

sym_file* open_sym_file(const FSSpec* symSpec)
{
	sym_file* symbols = new sym_file;
	if (symbols != NULL) {	
		symbols->mFile = FSp_fopen(symSpec, "rb");
		if (symbols->mFile == NULL)
			goto err_exit;
		
		// read the header. should make sure it is a valid .xSYM file, etc.
		DiskSymbolHeaderBlock& header = symbols->mHeader;
		if (fread(&header, sizeof(header), 1, symbols->mFile) != 1)
			goto err_exit;

		// read the RTE tables.
		fseek(symbols->mFile, header.dshb_rte.dti_first_page * header.dshb_page_size + sizeof(ResourceTableEntry), SEEK_SET);
		for (int i = 1; i <= header.dshb_rte.dti_object_count; i++) {
			ResourceTableEntry resEntry;
			if (fread(&resEntry, sizeof(resEntry), 1, symbols->mFile) == 1) {
				switch (resEntry.rte_ResType) {
				case 'CODE':
					symbols->mCodeEntry = resEntry;
					break;
				case 'DATA':
					break;
				}
			}
		}
		
		symbols->mNameTable = getNameTable(symbols->mFile, symbols->mHeader);
	}
	return symbols;

err_exit:
	delete symbols;
	return NULL;
}

void close_sym_file(sym_file* symbols)
{
	delete symbols;
}

int get_source(sym_file* symbols, UInt32 codeOffset, char fileName[256], UInt32* fileOffset)
{
	return getSource(codeOffset, symbols->mFile, symbols->mHeader, symbols->mCodeEntry, symbols->mNameTable, fileName, fileOffset);
}

#if 0

int main()
{
	printf ("Opening my own .xSYM file.\n");

	FILE* symFile = fopen("SymFiles.xSYM", "rb");
	if (symFile != NULL) {
		DiskSymbolHeaderBlock header;
		if (fread (&header, sizeof(header), 1, symFile) == 1) {
			// read the RTE tables.
			ResourceTableEntry codeEntry, dataEntry;
			fseek(symFile, header.dshb_rte.dti_first_page * header.dshb_page_size + sizeof(ResourceTableEntry), SEEK_SET);
			for (int i = 1; i <= header.dshb_rte.dti_object_count; i++) {
				ResourceTableEntry resEntry;
				if (fread(&resEntry, sizeof(resEntry), 1, symFile) == 1) {
					switch (resEntry.rte_ResType) {
					case 'CODE':
						codeEntry = resEntry;
						break;
					case 'DATA':
						dataEntry = resEntry;
						break;
					}
				}
			}
			printf("data entry size = %d\n", dataEntry.rte_res_size);
			printf("code entry size = %d\n", codeEntry.rte_res_size);
			printf("actual code size = %d\n", __code_end__ - __code_start__);
			// load the name table.
			UInt8** nameTable = getNameTable(symFile, header);
			// get source location of a routine.
			char fileName[256];
			UInt32 fileOffset;
			TV* tv = (TV*)&getSource;
			getSource(UInt32(tv->addr - __code_start__) + 536, symFile, header, codeEntry, nameTable, fileName, &fileOffset);
		}
		fclose(symFile);
	}
	
	return 0;
}

#endif
