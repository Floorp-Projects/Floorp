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

struct sym_file {
	// file data structures.
	FSSpec mFileSpec;
	SInt16 mFileRef;
	UInt8* mPageBuffer;
	UInt32 mPageOffset;
	UInt32 mBufferOffset;
	
	DiskSymbolHeaderBlock mHeader;
	ResourceTableEntry mCodeEntry;
	UInt8** mNameTable;
	
	sym_file(const FSSpec* spec);
	~sym_file();
	
	void* operator new(size_t n) { return ::GC_malloc(n); }
	void operator delete(void* ptr) { return ::GC_free(ptr); }
	
	bool init();
	bool open();
	bool close();
	bool seek(UInt32 position);
	UInt32 read(void* buffer, UInt32 count);

	const UInt8* getName(UInt32 nameIndex);
};

sym_file::sym_file(const FSSpec* spec)
	:	mFileSpec(*spec), mFileRef(-1), mPageBuffer(NULL), mPageOffset(0), mBufferOffset(0),
		mNameTable(NULL)
{
}

sym_file::~sym_file()
{
	// this seems to die when we are being debugged.
	if (mFileRef != -1) {
		::FSClose(mFileRef);
		mFileRef = -1;
	}
	
	if (mPageBuffer != NULL) {
		GC_free(mPageBuffer);
		mPageBuffer = NULL;
	}
	
	if (mNameTable != NULL) {
		UInt8** nameTable = mNameTable;
		UInt16 pageCount = mHeader.dshb_nte.dti_page_count;
		while (pageCount-- > 0) {
			GC_free(*nameTable++);
		}
		GC_free(mNameTable);
		mNameTable = NULL;
	}
}

bool sym_file::init()
{
	// open the file.
	if (!open())
		return false;

	// read the header. should make sure it is a valid .xSYM file, etc.
	DiskSymbolHeaderBlock& header = mHeader;
	long count = sizeof(header);
	if (::FSRead(mFileRef, (long*) &count, &header) != noErr || count != sizeof(header))
		return false;

	// read the name table.
	// mNameTable = new UInt8*[header.dshb_nte.dti_page_count];
	mNameTable = (UInt8**) GC_malloc_ignore_off_page(header.dshb_nte.dti_page_count * sizeof(UInt8*));
	if (mNameTable == NULL)
		return false;
	
	// seek to first page of the name table.
	::SetFPos(mFileRef, fsFromStart, header.dshb_nte.dti_first_page * header.dshb_page_size);
	// read all of the pages into memory, for speed.
	for (int i = 0; i < header.dshb_nte.dti_page_count; i++) {
		// allocate each page atomically, so GC won't have to scan them.
		UInt8* page = mNameTable[i] = (UInt8*) GC_malloc_atomic(header.dshb_page_size);
		if (page == NULL)
			return false;
		count = header.dshb_page_size;
		if (::FSRead(mFileRef, &count, page) != noErr || count != header.dshb_page_size)
			return false;
	}

	// allocate the page buffer and initialize offsets.
	// mPageBuffer = (UInt8*) new UInt8[header.dshb_page_size];
	mPageBuffer = (UInt8*) GC_malloc_atomic(header.dshb_page_size);

	// read the RTE tables.
	seek(header.dshb_rte.dti_first_page * header.dshb_page_size + sizeof(ResourceTableEntry));
	for (int i = 1; i <= header.dshb_rte.dti_object_count; i++) {
		ResourceTableEntry resEntry;
		if (read(&resEntry, sizeof(resEntry)) == sizeof(resEntry)) {
			switch (resEntry.rte_ResType) {
			case 'CODE':
				mCodeEntry = resEntry;
				break;
			case 'DATA':
				break;
			}
		}
	}
	
	return true;
}

bool sym_file::open()
{
	if (mFileRef == -1) {
		// open the specified symbol file.
		if (::FSpOpenDF(&mFileSpec, fsRdPerm, &mFileRef) != noErr)
			return false;
	}
	return true;
}

bool sym_file::close()
{
	if (mFileRef != -1) {
		::FSClose(mFileRef);
		mFileRef = -1;
	}
	return true;
}

bool sym_file::seek(UInt32 position)
{
	if (position != (mPageOffset + mBufferOffset)) {
		// if position is off current page, the resync the buffer.
		UInt32 pageSize = mHeader.dshb_page_size;
		if (position < mPageOffset || position >= (mPageOffset + pageSize)) {
			// make sure the file is open.
			if (!open())
				return false;
			// read the nearest page to this offset.
			mPageOffset = (position - (position % pageSize));
			if (::SetFPos(mFileRef, fsFromStart, mPageOffset) != noErr)
				return false;
			long count = pageSize;
			if (::FSRead(mFileRef, &count, mPageBuffer) != noErr)
				return false;
		}
		mBufferOffset = (position - mPageOffset);
	}
	return true;
}

UInt32 sym_file::read(void* buffer, UInt32 count)
{
	// we don't handle reads that aren't on the current page.
	if ((mBufferOffset + count) < mHeader.dshb_page_size) {
		::BlockMoveData(mPageBuffer + mBufferOffset, buffer, count);
		mBufferOffset += count;
		return count;
	}
	return 0;
}

const UInt8* sym_file::getName(UInt32 nameIndex)
{
	UInt32 nameOffset = 2 * nameIndex;
	const UInt8* page = mNameTable[nameOffset / mHeader.dshb_page_size];
	const UInt8* name = page + (nameOffset % mHeader.dshb_page_size);
	return name;
}

static bool getFileReferenceTableEntry(sym_file* symbols,
									  const FileReference& fileRef,
									  FileReferenceTableEntry* frte)
{
	const DiskSymbolHeaderBlock& header = symbols->mHeader;
	UInt32 fileRefsPerPage = (header.dshb_page_size / sizeof(FileReferenceTableEntry));
	UInt32 fileRefPage = (fileRef.fref_frte_index / fileRefsPerPage);
	UInt32 fileRefIndex = (fileRef.fref_frte_index % fileRefsPerPage);

	// seek to the specified frte.
	symbols->seek((header.dshb_frte.dti_first_page + fileRefPage) * header.dshb_page_size + fileRefIndex * sizeof(FileReferenceTableEntry));
	return (symbols->read(frte, sizeof(FileReferenceTableEntry)) == sizeof(FileReferenceTableEntry));
}

static bool getContainedStatementTableEntry(sym_file* symbols,
									       UInt32 statementIndex,
									       ContainedStatementsTableEntry* statementEntry)
{
	const DiskSymbolHeaderBlock& header = symbols->mHeader;
	UInt32 entriesPerPage = (header.dshb_page_size / sizeof(ContainedStatementsTableEntry));
	UInt32 entryPage = (statementIndex / entriesPerPage);
	UInt32 entryIndex = (statementIndex % entriesPerPage);
	
	// seek to the specified statement entry.
	symbols->seek((header.dshb_csnte.dti_first_page + entryPage) * header.dshb_page_size + entryIndex * sizeof(ContainedStatementsTableEntry));
	return (symbols->read(statementEntry, sizeof(ContainedStatementsTableEntry)) == sizeof(ContainedStatementsTableEntry));
}

inline bool isMeatyModule(const ModulesTableEntry& moduleEntry)
{
	return (moduleEntry.mte_kind == kModuleKindProcedure) || (moduleEntry.mte_kind == kModuleKindFunction);
}

inline UInt32 delta(UInt32 x, UInt32 y)
{
	if (x > y)
		return (x - y);
	else
		return (y - x);
}

int get_source(sym_file* symbols, UInt32 codeOffset, char symbolName[256], char fileName[256], UInt32* fileOffset)
{
	const DiskSymbolHeaderBlock& header = symbols->mHeader;
	const ResourceTableEntry& codeEntry = symbols->mCodeEntry;

	// since module entries can't span pages, must compute which page module entry size.
	UInt32 modulesPerPage = (header.dshb_page_size / sizeof(ModulesTableEntry));

	// search for MTE nearest specified offset.
	// seek to first MTE.
	for (UInt16 i = codeEntry.rte_mte_first; i <= codeEntry.rte_mte_last; i++) {
		ModulesTableEntry moduleEntry;
		UInt32 modulePage = (i / modulesPerPage);
		UInt32 moduleIndex = (i % modulesPerPage);
		symbols->seek((header.dshb_mte.dti_first_page + modulePage) * header.dshb_page_size + moduleIndex * sizeof(ModulesTableEntry));
		if (symbols->read(&moduleEntry, sizeof(moduleEntry)) == sizeof(moduleEntry)) {
			if (isMeatyModule(moduleEntry) && (codeOffset >= moduleEntry.mte_res_offset) && (codeOffset - moduleEntry.mte_res_offset) < moduleEntry.mte_size) {
				FileReferenceTableEntry frte;
				if (getFileReferenceTableEntry(symbols, moduleEntry.mte_imp_fref, &frte)) {
					UInt32 length;
					// get the name of the symbol.
					const UInt8* moduleName = symbols->getName(moduleEntry.mte_nte_index);
					// printf("module name = %#s\n", moduleName);
					// trim off the leading "."
					length = moduleName[0] - 1;
					BlockMoveData(moduleName + 2, symbolName, length);
					symbolName[length] = '\0';
					// get the name of the file.
					const UInt8* name = symbols->getName(frte.frte_fn.nte_index);
					length = name[0];
					BlockMoveData(name + 1, fileName, length);
					fileName[length] = '\0';
					// printf("file name = %s\n", fileName);
					// try to refine the location, using the contained statements table entries.
					UInt32 closestFileOffset = moduleEntry.mte_imp_fref.fref_offset;
					UInt32 closestCodeOffset = moduleEntry.mte_res_offset;
					UInt32 closestCodeDelta = 0xFFFFFFFF;
					UInt32 currentFileOffset, currentCodeOffset = moduleEntry.mte_res_offset, currentCodeDelta;
					for (UInt32 j = moduleEntry.mte_csnte_idx_1; j <= moduleEntry.mte_csnte_idx_2; j++) {
						// only consider offsets less than the actual code offset, so we'll be sure
						// to match the nearest line before the code offset. this could probably be
						// a termination condition as well.
						if (currentCodeOffset > codeOffset)
							break;
						ContainedStatementsTableEntry statementEntry;
						if (getContainedStatementTableEntry(symbols, j, &statementEntry)) {
							switch (statementEntry.csnte_file.change) {
							case kSourceFileChange:
								currentFileOffset = statementEntry.csnte_file.fref.fref_offset;
								break;
							case kEndOfList:
								break;
							default:
								currentFileOffset += statementEntry.csnte.file_delta;
								currentCodeOffset = moduleEntry.mte_res_offset + statementEntry.csnte.mte_offset;
								if (currentCodeOffset <= codeOffset) {
									currentCodeDelta = delta(currentCodeOffset, codeOffset);
									if (currentCodeDelta < closestCodeDelta) {
										closestFileOffset = currentFileOffset;
										closestCodeOffset = currentCodeOffset;
										closestCodeDelta = currentCodeDelta;
									}
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

sym_file* open_sym_file(const FSSpec* symSpec)
{
	sym_file* symbols = new sym_file(symSpec);
	if (symbols != NULL) {
		if (!symbols->init()) {
			delete symbols;
			return NULL;
		}
		// don't leave the file open, if possible.
		symbols->close();
	}
	return symbols;
}

void close_sym_file(sym_file* symbols)
{
	if (symbols != NULL) {
		delete symbols;
	}
}
