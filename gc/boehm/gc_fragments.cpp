/*
	gc_fragments.cpp
 */

#include "gc_fragments.h"
#include "sym_file.h"
#include "gc.h"
#include "MetroNubUtils.h"

#include <string.h>

struct CodeLocation {
	CodeLocation* mNext;
	char* mCodeAddr;
	UInt32 mFileOffset;
	char mSymbolName[256];
	char mFileName[256];
	
	CodeLocation() : mNext(NULL), mCodeAddr(NULL), mFileOffset(0)
	{
		mFileName[0] = '\0';
	}
};

static const kCodeLocationCacheSize = 256;

struct CodeLocationCache {
	CodeLocation mEntries[kCodeLocationCacheSize];
	CodeLocation* mLocations;
	CodeLocation** mLastLink;

	CodeLocationCache();

	CodeLocation* findLocation(char* codeAddr);
	void saveLocation(char* codeAddr, char symbolName[256], char fileName[256], UInt32 fileOffset);
};

CodeLocationCache::CodeLocationCache()
	:	mLocations(NULL), mLastLink(&mLocations)
{
	// link all of the locations together in a list.
	int offset = kCodeLocationCacheSize;
	while (offset > 0) {
		CodeLocation* location = &mEntries[--offset];
		location->mNext = mLocations;
		mLocations = location;
	}
}

CodeLocation* CodeLocationCache::findLocation(char* codeAddr)
{
	CodeLocation** link = &mLocations;
	CodeLocation* location = *link;
	while (location != NULL && location->mCodeAddr != NULL) {
		if (location->mCodeAddr == codeAddr) {
			// move it to the head of the list.
			if (location != mLocations) {
				*link = location->mNext;
				location->mNext = mLocations;
				mLocations = location;
			}
			return location;
		}
		link = &location->mNext;
		location = *link;
		// maintain pointer to the last element in list for fast insertion.
		if (location != NULL)
			mLastLink = link;
	}
	return NULL;
}

void CodeLocationCache::saveLocation(char* codeAddr, char symbolName[256], char fileName[256], UInt32 fileOffset)
{
	CodeLocation** link = mLastLink;
	CodeLocation* location = *link;
	mLastLink = &mLocations;
	
	// move it to the head of the list.
	if (location != mLocations) {
		*link = location->mNext;
		location->mNext = mLocations;
		mLocations = location;
	}
	
	// save the specified location.
	location->mCodeAddr = codeAddr;
	location->mFileOffset = fileOffset;
	::strcpy(location->mSymbolName, symbolName);
	::strcpy(location->mFileName, fileName);
}

struct CodeFragment {
	CodeFragment* mNext;
	char* mDataStart;
	char* mDataEnd;
	char* mCodeStart;
	char* mCodeEnd;
	FSSpec mFragmentSpec;
	CodeLocationCache mLocations;
	sym_file* mSymbols;
	
	CodeFragment(char* dataStart, char* dataEnd,
                 char* codeStart, char* codeEnd,
                 const FSSpec* fragmentSpec);

	~CodeFragment();
	
	void* operator new(size_t n) { return ::GC_malloc(n); }
	void operator delete(void* ptr) { return ::GC_free(ptr); }     
};

CodeFragment::CodeFragment(char* dataStart, char* dataEnd,
                           char* codeStart, char* codeEnd,
                           const FSSpec* fragmentSpec)
	:	mDataStart(dataStart), mDataEnd(dataEnd),
		mCodeStart(codeStart), mCodeEnd(codeEnd),
		mFragmentSpec(*fragmentSpec), mSymbols(NULL), mNext(NULL)
{
	// need to eagerly open symbols file eagerly, otherwise we're in the middle of a GC!
	FSSpec symSpec = mFragmentSpec;
	UInt8 len = symSpec.name[0];
	symSpec.name[++len] = '.';
	symSpec.name[++len] = 'x';
	symSpec.name[++len] = 'S';
	symSpec.name[++len] = 'Y';
	symSpec.name[++len] = 'M';
	symSpec.name[0] = len;

	mSymbols = open_sym_file(&symSpec);
}

CodeFragment::~CodeFragment()
{
	if (mSymbols) {
		close_sym_file(mSymbols);
		mSymbols = NULL;
	}
}

static CodeFragment* theFragments = NULL;

static CodeFragment** find_fragment(char* codeAddr)
{
	CodeFragment** link = &theFragments;
	CodeFragment* fragment = *link;
	while (fragment != NULL) {
		if (codeAddr >= fragment->mCodeStart && codeAddr < fragment->mCodeEnd)
			return link;
		link = &fragment->mNext;
		fragment = *link;
	}
	return NULL;
}

void GC_register_fragment(char* dataStart, char* dataEnd,
                          char* codeStart, char* codeEnd,
                          const FSSpec* fragmentSpec)
{
	// register the roots.
	GC_add_roots(dataStart, dataEnd);

	// create an entry for this fragment.
	CodeFragment* fragment = new CodeFragment(dataStart, dataEnd,
	                                          codeStart, codeEnd,
	                                          fragmentSpec);
	if (fragment != NULL) {
		fragment->mNext = theFragments;
		theFragments = fragment;
	}
}

void GC_unregister_fragment(char* dataStart, char* dataEnd,
                            char* codeStart, char* codeEnd)
{
	// try not to crash when running under the MW debugger.
	if (!AmIBeingMWDebugged()) {
		CodeFragment** link = find_fragment(codeStart);
		if (link != NULL) {
			CodeFragment* fragment = *link;
			*link = fragment->mNext;
			delete fragment;
		}
	}

	// remove the roots.
	GC_remove_roots(dataStart, dataEnd);
}

int GC_address_to_source(char* codeAddr, char symbolName[256], char fileName[256], UInt32* fileOffset)
{
	CodeFragment** link = find_fragment(codeAddr);
	if (link != NULL) {
		CodeFragment* fragment = *link;
		// always move this fragment to the head of the list, to speed up searches.
		if (theFragments != fragment) {
			*link = fragment->mNext;
			fragment->mNext = theFragments;
			theFragments = fragment;
		}
		// see if this is a cached location.
		CodeLocation* location = fragment->mLocations.findLocation(codeAddr);
		if (location != NULL) {
			::strcpy(symbolName, location->mSymbolName);
			::strcpy(fileName, location->mFileName);
			*fileOffset = location->mFileOffset;
			return 1;
		}
		sym_file* symbols = fragment->mSymbols;
		if (symbols != NULL) {
			if (get_source(symbols, UInt32(codeAddr - fragment->mCodeStart), symbolName, fileName, fileOffset)) {
				// save this location in the per-fragment cache.
				fragment->mLocations.saveLocation(codeAddr, symbolName, fileName, *fileOffset);
				return 1;
			}
		}
	}
	return 0;
}
