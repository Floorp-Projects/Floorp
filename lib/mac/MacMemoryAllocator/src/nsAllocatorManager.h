/* -*- Mode: C++;    tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- 
 * 
 * The contents of this file are subject to the Netscape Public License 
 * Version 1.0 (the "NPL"); you may not use this file except in 
 * compliance with the NPL. You may obtain a copy of the NPL at  
 * http://www.mozilla.org/NPL/ 
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis, 
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL 
 * for the specific language governing rights and limitations under the 
 * NPL. 
 * 
 * The Initial Developer of this code under the NPL is Netscape 
 * Communications Corporation. Portions created by Netscape are 
 * Copyright (C) 1998 Netscape Communications Corporation. All Rights    
 * Reserved. */


#include <MacTypes.h>

#if STATS_MAC_MEMORY
#include "prio.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

void *malloc(size_t blockSize);
void free(void *deadBlock);
void* realloc(void* block, size_t newSize);
void *calloc(size_t nele, size_t elesize);


#if STATS_MAC_MEMORY
void WriteString(PRFileDesc *file, const char * string);
#endif


#ifdef __cplusplus
}
#endif


//--------------------------------------------------------------------
class nsHeapZoneHeader
{
	public:
	
								nsHeapZoneHeader(Ptr zonePtr, Size ptrSize);
								nsHeapZoneHeader(Handle zoneHandle, Size handleSize);
								~nsHeapZoneHeader();
								
		nsHeapZoneHeader *		GetNextZone()							{ return mNextHeapZone;	}
		void					SetNextZone(nsHeapZoneHeader *nextZone)	{ mNextHeapZone = nextZone; }

		Ptr						AllocateZonePtr(Size ptrSize);
		void					DisposeZonePtr(Ptr thePtr, Boolean &outWasLastChunk);
		
#if DEBUG_HEAP_INTEGRITY
		Boolean					IsGoodZone()	{ return (mSignature == kHeapZoneSignature); }
#endif

		static nsHeapZoneHeader*	GetZoneFromPtr(Ptr subheapPtr);
		
	protected:

		void					SetupHeapZone(Ptr zonePtr, Size zoneSize);		
		
		enum
		{
			kHeapZoneMasterPointers = 24			// this number doesn't really matter, because we never
													// allocate handles in our heap zones
		};

		
#if DEBUG_HEAP_INTEGRITY
		enum {
			kHeapZoneSignature = 'HZne'
		};

		OSType					mSignature;
#endif
		
		nsHeapZoneHeader		*mNextHeapZone;
		Handle					mZoneHandle;		// the handle containing the zone. Nil if Ptr in app heap
		UInt32					mChunkCount;		// how many chunks are allocated in this zone
		THz						mHeapZone;
};




class nsAllocatorManager
{
	public:

		static const SInt32			kNumMasterPointerBlocks;
		static const SInt32			kApplicationStackSizeIncrease;
		
		static const SInt32			kHeapZoneHeapPercentage;
		static const SInt32			kTempMemHeapZoneSize;
		static const SInt32			kTempMemHeapMinZoneSize;
		
		static const Size			kChunkSizeMultiple;

		static nsAllocatorManager*	GetAllocatorManager()	{ return sAllocatorManager ? sAllocatorManager : CreateAllocatorManager(); }

									nsAllocatorManager();
									~nsAllocatorManager();
									
		static OSErr				InitializeMacMemory(SInt32 inNumMasterPointerBlocks,
															SInt32 inAppStackSizeInc);
		
		nsMemAllocator*				GetAllocatorForBlockSize(size_t blockSize);

		
		Ptr							AllocateSubheap(Size preferredSize, Size &outActualSize);
		void						FreeSubheap(Ptr subheapPtr);		

#if STATS_MAC_MEMORY
		void						DumpMemoryStats();
#endif

	protected:
	
		static nsAllocatorManager *	CreateAllocatorManager();

		static const Size			kMacMemoryPtrOvehead;

		nsHeapZoneHeader *			MakeNewHeapZone(Size zoneSize, Size minZoneSize);
		
	private:
	
		SInt32						mNumFixedSizeAllocators;
		SInt32						mNumSmallBlockAllocators;
		
		UInt32						mMinSmallBlockSize;			// blocks >= this size come out of the small block allocator
		UInt32						mMinLargeBlockSize;			// blocks >= this size come out of the large allocator
		
		nsMemAllocator**			mFixedSizeAllocators;		// array of pointers to allocator objects
		nsMemAllocator**			mSmallBlockAllocators;		// array of pointers to allocator objects
	
		nsMemAllocator*				mLargeAllocator;

		nsHeapZoneHeader*			mFirstHeapZone;				// first of a linked list of heap zones
		nsHeapZoneHeader*			mLastHeapZone;				// last of a linked list of heap zones
		
		THz							mHeapZone;					// the heap zone for our memory heaps

		static nsAllocatorManager	*sAllocatorManager;		

};

