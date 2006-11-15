/* ***** BEGIN LICENSE BLOCK ***** 
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1 
 *
 * The contents of this file are subject to the Mozilla Public License Version 1.1 (the 
 * "License"); you may not use this file except in compliance with the License. You may obtain 
 * a copy of the License at http://www.mozilla.org/MPL/ 
 * 
 * Software distributed under the License is distributed on an "AS IS" basis, WITHOUT 
 * WARRANTY OF ANY KIND, either express or implied. See the License for the specific 
 * language governing rights and limitations under the License. 
 * 
 * The Original Code is [Open Source Virtual Machine.] 
 * 
 * The Initial Developer of the Original Code is Adobe System Incorporated.  Portions created 
 * by the Initial Developer are Copyright (C)[ 2004-2006 ] Adobe Systems Incorporated. All Rights 
 * Reserved. 
 * 
 * Contributor(s): Adobe AS3 Team
 * 
 * Alternatively, the contents of this file may be used under the terms of either the GNU 
 * General Public License Version 2 or later (the "GPL"), or the GNU Lesser General Public 
 * License Version 2.1 or later (the "LGPL"), in which case the provisions of the GPL or the 
 * LGPL are applicable instead of those above. If you wish to allow use of your version of this 
 * file only under the terms of either the GPL or the LGPL, and not to allow others to use your 
 * version of this file under the terms of the MPL, indicate your decision by deleting provisions 
 * above and replace them with the notice and other provisions required by the GPL or the 
 * LGPL. If you do not delete the provisions above, a recipient may use your version of this file 
 * under the terms of any one of the MPL, the GPL or the LGPL. 
 * 
 ***** END LICENSE BLOCK ***** */

#ifndef __avmplus_DynamicProfiler__
#define __avmplus_DynamicProfiler__


#ifdef AVMPLUS_PROFILE
namespace avmplus
{
	class StackMark;
	/**
	 * Provides dynamic profiling support for the AVM+.
	 */
	class DynamicProfiler
	{
		#ifdef WIN32
		inline static int rdtsc() {
			// read the cpu cycle counter.  1 tick = 1 cycle on IA32
			_asm rdtsc;
		}
		#endif

		int counts2[256];		
		int totalCount;
		AbcOpcode lastOp;
		uint64 times[256];
		int lastTime;

		/**
		 * 1. set markOverhead=0
		 * 2. run a test with -Ddprofile
		 * 3. set markOverhead slightly lower than the fastest opcode
		 *    on Pentium-M, 65 is about right.  cpu speed should not
		 *    matter but it will vary between processor families.
		 * 4. rerun tests.  overhead will be subtracted.
		 */
		static const int markOverhead = 61;

	public:
		DynamicProfiler();

		/**
		 * When dprofile is set to true, the VM performs dynamic
		 * profiling.  Every instruction is timed and the VM
		 * prints a report on exit detailing how many CPU cycles
		 * were spent in each instruction type.  This shows
		 * the "instruction mix" of the executing code, and
		 * the performance characteristics of different
		 * bytecode instructions.
		 */
		bool dprofile;

		/**
		 * When sprofile is set to true, the VM performs static
		 * profiling.  The entire program's bytecode is scanned
		 * to determine how many instructions of each type
		 * are used in the program.
		 */
		bool sprofile;

		/**
		 * Dumps the static and dynamic profiling reports to
		 * the specified console output stream.
		 */
		void dump(PrintWriter& console);

		void endmark() { mark((AbcOpcode)0); }

		void mark(AbcOpcode op)
		{
			#ifdef WIN32
				int now = rdtsc();
			#else
			uint64 now = MMgc::GC::GetPerformanceCounter();
			#endif
			counts2[op]++;
			totalCount++;
			times[lastOp] += now-lastTime-markOverhead;
			lastTime = now;
			lastOp = op;
		}

		class StackMark;
		friend class DynamicProfiler::StackMark;

		class StackMark
		{
			AbcOpcode savedOp;
			DynamicProfiler* dprof;

		public:
			StackMark(AbcOpcode op, DynamicProfiler* _dprof) : savedOp(OP_nop)	// init to make GCC happy
			{
				if (_dprof->dprofile)
				{
					savedOp = _dprof->lastOp;
					_dprof->mark(op);
					this->dprof = _dprof;
				}
				else
				{
					this->dprof = NULL;
				}
			}
			~StackMark()
			{
				if(dprof) 
				{
					dprof->mark(savedOp);
					dprof->totalCount--;
					dprof->counts2[savedOp]--;
				}
			}
		};
	};
}
#endif

#endif /* __avmplus_DynamicProfiler__ */
