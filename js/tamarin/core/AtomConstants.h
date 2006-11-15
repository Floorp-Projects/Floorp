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

#ifndef __avmplus_AtomConstants__
#define __avmplus_AtomConstants__


namespace avmplus
{
	/**
	 * The AtomConstants namespace defines constants for
	 * manipulating atoms.
	 *
	 * The atom is a primitive value in ActionScript.  Since
	 * ActionScript is a dynamically typed language, an atom can
	 * belong to one of several types: null, undefined, number,
	 * integer, string, boolean, object reference.
	 *
	 * Atoms are encoded with care to take up the minimum
	 * possible space.  An atom is represented by a 32-bit
	 * integer, with the bottom 3 bits indicating the type.
	 *
	 *		32 bit atom
	 *
	 *  31             16 15     8 7   3 210
	 *  dddddddd dddddddd dddddddd ddddd TTT
	 *
	 *  TTT
	 *  000  - untagged
	 *  001  object
	 *  010  string
	 *  011  namespace
	 *  100  undefined
	 *  101  boolean
	 *  110  integer
	 *  111  double
	 *
	 *  - using last 3 bits means allocations must be 8-byte aligned.
	 *  - related types are 1 bit apart, e.g. int/double
	 *
	 *  kInteger atoms are used to represent integer values from -2^28..2^28-1,
	 *  regardless of whether the context implies int, uint, or Number.
	 *  If a number doesn't fit into that range it is stored as a kDoubleType
	 *
	 */
	namespace AtomConstants
	{
		/**
		 * @name Atom types
		 * These are the type values that appear in the bottom
		 * 3 bits of an atom.
		 */
		/*@{*/
		// cannot use 0 as tag, breaks atomWriteBarrier
		const Atom kObjectType	  = 1;	// null=1
		const Atom kStringType    = 2;	// null=2
		const Atom kNamespaceType = 3;	// null=3
		const Atom kSpecialType   = 4;	// undefined=4
		const Atom kBooleanType   = 5;	// false=5 true=13
		const Atom kIntegerType   = 6;
		const Atom kDoubleType    = 7;
		/*@}*/

		#define	ISNULL(a) (((uintptr)a) < (uintptr)kSpecialType)

		/* 
		other things you can do with math on atoms
		
		isNull			(unsigned)a < 4
		isUndefined		a == undefinedAtom
		isSpecial		(unsigned)a <= 4
		isNumber		a & 6 == 6

							^8	jlt(a<8)	^2		jle(a<=4)
		true		1110  0110	t			1100	f
		false		0110  1110	f			0100	t
		undefined	0100  1100	f			0110	f
		*/

		/**
		 * @name Special atom values
		 * These are the atoms for undefined, null, true
		 * and false.
		 */
		/*@{*/
		// there is no single null atom.  use isNull() to test, or
		// one of the following typed null atoms.  if you must, use NULL (0).
		const Atom nullObjectAtom = kObjectType|0;
		const Atom nullStringAtom = kStringType|0;
		const Atom nullNsAtom     = kNamespaceType|0;
		const Atom undefinedAtom  = kSpecialType|0; // 0x03
		const Atom trueAtom       = kBooleanType|0x08; // 0x0D
		const Atom falseAtom      = kBooleanType|0x00; // 0x05
		/*@}*/

		/**
		 * @name BIND constants
		 * These BIND constants are used similarly to atom kinds.  Since we
		 * have a conservative collector, the codes aren't important.
		 */
		/*@{*/
		const Binding BIND_NONE      = 0;       // no such binding
		const Binding BIND_AMBIGUOUS = -1;
		const Binding BIND_METHOD    = 1;       // int disp_id local method number
		const Binding BIND_VAR       = 2;       // int local slot number (r/w var)
		const Binding BIND_CONST     = 3;       // int local slot number (r/o const)
		const Binding BIND_ITRAMP    = 4;       // interface trampoline in imt table
		const Binding BIND_GET       = 5;		// get-only property   101
		const Binding BIND_SET		 = 6;       // set-only property   110
		const Binding BIND_GETSET    = 7;       // readwrite property  111
		/*@}*/
	}
}

#endif /* __avmplus_AtomConstants__ */
