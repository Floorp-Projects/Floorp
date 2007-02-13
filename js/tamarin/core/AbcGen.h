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
 * The Original Code is [Open Source Virtual Machine.].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2004-2006
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

#ifndef __avmplus_AbcGen__
#define __avmplus_AbcGen__


namespace avmplus
{
	class AbcGen 
	{
	public:
		/**
		 * construct an abc code gen buffer with the given initial capacity
		 */
		AbcGen(MMgc::GC *gc, int initCapacity=128) : bytes(gc, initCapacity) {}

		List<byte,LIST_NonGCObjects>& getBytes() { return bytes; }
		void construct_super() { bytes.add(OP_constructsuper); }
		void pushnan() { bytes.add(OP_pushnan); }
		void pushundefined() { bytes.add(OP_pushundefined); }
		void pushnull() { bytes.add(OP_pushnull); }
		void pushtrue() { bytes.add(OP_pushtrue); }
		void pushconstant(CPoolKind kind, int index) 
		{ 
			// AbcParser should already ensure kind is legal value.
			AvmAssert(kind >=0 && kind <= CONSTANT_StaticProtectedNs && kindToPushOp[kind] != 0);
			int op = kindToPushOp[kind];
			bytes.add((byte)op);
			if(opOperandCount[op] > 0)
				writeInt(index); 
		}
		void getlocalN(int N) { bytes.add((byte)(OP_getlocal0+N)); }
		void setslot(int slot) { bytes.add(OP_setslot); writeInt(slot+1); }
		void abs_jump(const byte *pc, int code_length) 
		{ 
			bytes.add(OP_abs_jump); 
#ifdef AVMPLUS_64BIT
			writeInt((int)(uintptr)pc);
			writeInt((int)(((uintptr)pc) >> 32));
#else			
			writeInt((int)pc); 
#endif			
			writeInt(code_length); 
		}
		void returnvoid() { bytes.add(OP_returnvoid); }
		void writeBytes(List<byte,LIST_NonGCObjects>& b) { bytes.add(b); }
		void writeInt(unsigned int n);
		size_t size() { return bytes.size(); }
	private:
		List<byte, LIST_NonGCObjects> bytes;
	};
}

#endif // __avmplus_MethodEnv__
