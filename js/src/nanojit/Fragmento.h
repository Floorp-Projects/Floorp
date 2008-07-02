/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: t; tab-width: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is [Open Source Virtual Machine].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2004-2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adobe AS3 Team
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#ifndef __nanojit_Fragmento__
#define __nanojit_Fragmento__

#ifdef AVMPLUS_VERBOSE
extern void drawTraceTrees(Fragmento *frago, FragmentMap * _frags, avmplus::AvmCore *core, avmplus::AvmString fileName);
#endif

namespace nanojit
{
	struct GuardRecord;
	class Assembler;
	
    struct PageHeader
    {
        struct Page *next;
        verbose_only (int seq;) // sequence # of page
    };
    struct Page: public PageHeader
    {
        union {
            LIns lir[(NJ_PAGE_SIZE-sizeof(PageHeader))/sizeof(LIns)];
            NIns code[(NJ_PAGE_SIZE-sizeof(PageHeader))/sizeof(NIns)];
        };
    };
	typedef avmplus::List<Page*,avmplus::LIST_NonGCObjects>	AllocList;

	typedef avmplus::SortedMap<const void*, uint32_t, avmplus::LIST_NonGCObjects> BlockSortedMap;
	class BlockHist: public BlockSortedMap
	{
	public:
		BlockHist(GC*gc): BlockSortedMap(gc)
		{
		}
		uint32_t count(const void *p) {
			uint32_t c = 1+get(p);
			put(p, c);
			return c;
		}
	};

	/*
	 *
	 * This is the main control center for creating and managing fragments.
	 */
	class Fragmento : public GCFinalizedObject
	{
		public:
			Fragmento(AvmCore* core);
			~Fragmento();

			void		addMemory(void* firstPage, uint32_t pageCount);  // gives memory to the Assembler
			Assembler*	assm();
			AvmCore*	core();
			Page*		pageAlloc();
			void		pageFree(Page* page);
			
            Fragment*	getLoop(const avmplus::InterpState &is);
			void		clearFrags();	// clear all fragments from the cache
            Fragment*	getMerge(GuardRecord *lr, const avmplus::InterpState &is);
            Fragment*   createBranch(GuardRecord *lr, const avmplus::InterpState &is);
            Fragment*   newFrag(const avmplus::InterpState &is);
            Fragment*   newBranch(Fragment *from, const avmplus::InterpState &is);

            verbose_only ( uint32_t pageCount(); )
			verbose_only ( void dumpStats(); )
			verbose_only ( void dumpRatio(const char*, BlockHist*);)
			verbose_only ( void dumpFragStats(Fragment*, int level, 
				int& size, uint64_t &dur, uint64_t &interpDur); )
				verbose_only ( void countBlock(BlockHist*, avmplus::FOpcodep pc); )
			verbose_only ( void countIL(uint32_t il, uint32_t abc); )
			verbose_only( void addLabel(Fragment* f, const char *prefix, int id); )
			
			// stats
			struct 
			{
				uint32_t	pages;					// pages consumed
				uint32_t	flushes, ilsize, abcsize, compiles, totalCompiles, freePages;
			}
			_stats;

			verbose_only( DWB(BlockHist*)		enterCounts; )
			verbose_only( DWB(BlockHist*)		mergeCounts; )
			verbose_only( DWB(LabelMap*)        labels; )
			
    		#ifdef AVMPLUS_VERBOSE
    		void	drawTrees(avmplus::AvmString fileName);
            #endif
	

		private:
			void		pagesGrow(int32_t count);

			AvmCore*			_core;
			DWB(Assembler*)		_assm;
			DWB(FragmentMap*)	_frags;		/* map from ip -> Fragment ptr  */
			Page*			_pageList;
			uint32_t        _pageGrowth;

			/* unmanaged mem */
			AllocList	_allocList;
			GCHeap*		_gcHeap;
	};

    struct SideExit
    {
		int32_t f_adj;
        int32_t ip_adj;
		int32_t sp_adj;
		int32_t rp_adj;
        Fragment *target;
		int32_t calldepth;
		void* vmprivate;
		verbose_only( uint32_t sid; )
		verbose_only(Fragment *from;)
    };

	enum TraceKind {
		LoopTrace,
		BranchTrace,
		MergeTrace
	};
	
	/**
	 * Fragments are linear sequences of native code that have a single entry 
	 * point at the start of the fragment and may have one or more exit points 
	 * 
	 * It may turn out that that this arrangement causes too much traffic
	 * between d and i-caches and that we need to carve up the structure differently.
	 */
	class Fragment : public GCFinalizedObject
	{
		public:
			Fragment(FragID);
			~Fragment();

			NIns*			code()							{ return _code; }
			void			setCode(NIns* codee, Page* pages) { _code = codee; _pages = pages; }
			GuardRecord*	links()							{ return _links; }
			int32_t&		hits()							{ return _hits; }
            void            blacklist();
			bool			isBlacklisted()		{ return _hits < 0; }
			void			resetLinks();
			void			addLink(GuardRecord* lnk);
			void			removeLink(GuardRecord* lnk);
			void			link(Assembler* assm);
			void			linkBranches(Assembler* assm);
			void			unlink(Assembler* assm);
			void			unlinkBranches(Assembler* assm);
			debug_only( bool hasOnlyTreeLinks(); )
			void			removeIntraLinks();
            void            removeExit(Fragment *target);
			void			releaseLirBuffer();
			void			releaseCode(Fragmento* frago);
			void			releaseTreeMem(Fragmento* frago);
			bool			isAnchor() { return anchor == this; }
			bool			isRoot() { return root == this; }
			
			verbose_only( uint32_t		_called; )
			verbose_only( uint32_t		_native; )
            verbose_only( uint32_t      _exitNative; )
			verbose_only( uint32_t		_lir; )
			verbose_only( const char*	_token; )
            verbose_only( uint64_t      traceTicks; )
            verbose_only( uint64_t      interpTicks; )
            verbose_only( int32_t line; )
            verbose_only( DRCWB(avmplus::String *)file; )
			verbose_only( DWB(Fragment*) eot_target; )
			verbose_only( uint32_t mergeid;)
			verbose_only( uint32_t		sid;)
			verbose_only( uint32_t		compileNbr;)

            DWB(Fragment*) treeBranches;
            DWB(Fragment*) branches;
            DWB(Fragment*) nextbranch;
            DWB(Fragment*) anchor;
            DWB(Fragment*) root;
			DWB(BlockHist*) mergeCounts;
            DWB(LirBuffer*) lirbuf;
			LIns*			lastIns;
			LIns*		spawnedFrom;
			GuardRecord*	outbound;
			
			TraceKind kind;
			const FragID frid;
			uint32_t guardCount;
            uint32_t xjumpCount;
            int32_t blacklistLevel;
            NIns* fragEntry;
            LInsp param0,param1,sp,rp;
			int32_t calldepth;
			void* vmprivate;
			
		private:
			NIns*			_code;		// ptr to start of code
			GuardRecord*	_links;		// code which is linked (or pending to be) to this fragment
			int32_t			_hits;
			Page*			_pages;		// native code pages 
	};
	
#ifdef NJ_VERBOSE
	inline int nbr(LInsp x) 
	{
        Page *p = x->page();
        return (p->seq * NJ_PAGE_SIZE + (intptr_t(x)-intptr_t(p))) / sizeof(LIns);
	}
#else
    inline int nbr(LInsp x)
    {
        return int(x) & (NJ_PAGE_SIZE-1);
    }
#endif
}
#endif // __nanojit_Fragmento__
