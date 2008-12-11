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


#ifndef __nanojit_RegAlloc__
#define __nanojit_RegAlloc__


namespace nanojit
{
	inline RegisterMask rmask(Register r)
	{
		return 1 << r;
	}

	class RegAlloc MMGC_SUBCLASS_DECL
	{
		public:
            RegAlloc() : free(0), used(0), priority(0) {}
			void	clear();
			bool	isFree(Register r); 
			void	addFree(Register r);
			void	removeFree(Register r);
			void	addActive(Register r, LIns* ins);
            void    useActive(Register r);
			void	removeActive(Register r);
			void	retire(Register r);
            bool    isValid() {
                return (free|used) != 0;
            }

            int32_t getPriority(Register r) {
                NanoAssert(r != UnknownReg && active[r]);
                return usepri[r];
            }

	        LIns* getActive(Register r) {
		        NanoAssert(r != UnknownReg);
		        return active[r];
	        }

			debug_only( uint32_t	countFree(); )
			debug_only( uint32_t	countActive(); )
			debug_only( void		checkCount(); )
			debug_only( bool		isConsistent(Register r, LIns* v); )
			debug_only( uint32_t	count; )
			debug_only( RegisterMask managed; )    // bitfield denoting which are under our management                     

			LIns*	active[LastReg + 1];  // active[r] = OP that defines r
			int32_t usepri[LastReg + 1]; // used priority. lower = more likely to spill.
			RegisterMask	free;
			RegisterMask	used;
            int32_t         priority;

			verbose_only( static void formatRegisters(RegAlloc& regs, char* s, Fragment*); )

			DECLARE_PLATFORM_REGALLOC()
	};
}
#endif // __nanojit_RegAlloc__
