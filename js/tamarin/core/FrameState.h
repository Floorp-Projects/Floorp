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

#ifndef __avmplus_FrameState__
#define __avmplus_FrameState__


namespace avmplus
{
	/**
	 * represents a value in the verifier
	 */
	class Value
	{
	public:
		Traits* traits;
		bool notNull;
		bool isWith;
		bool killed;
	#ifdef AVMPLUS_MIR
		bool stored;  // set if codegen has already stored the OP
		CodegenMIR::OP* ins;  // spot for codegen to hang data.
	#endif //AVMPLUS_MIR
	};

	/**
		* this object holds the stack frame state at any given block entry.
		* the frame state consists of the types of each local, each entry on the
		* scope chain, and each operand stack slot.
		*/
	class FrameState: public MMgc::GCObject
	{

	public:
		sintptr pc;
		int scopeDepth;
		int stackDepth;
		Verifier * const verifier;
		int withBase;
		bool initialized;
		bool targetOfBackwardsBranch;
		bool insideTryBlock;
	#ifdef AVMPLUS_MIR
		CodegenMIR::MirLabel label;
	#endif
		
	private:
		Value locals[1]; // actual length is verifier->frameSize.  one entry for each
			       // local, scope-entry, and operand stack entry.

	public:
		FrameState(Verifier *v) : verifier(v)
		{
			this->withBase  = -1;
		}

		bool init(FrameState* other)
		{
			if( verifier != other->verifier ){
				AvmAssertMsg(0, "(verifier == other->verifier)");
				return false;
			}
			scopeDepth = other->scopeDepth;
			stackDepth = other->stackDepth;
			withBase = other->withBase;
			memcpy(locals, other->locals, verifier->frameSize*sizeof(Value));
			return true;
		}

		Value& value(sintptr i)
		{
			return locals[i];
		}

		Value& scopeValue(int i)
		{
			return locals[verifier->scopeBase+i];
		}

		Value& stackValue(int i)
		{
			AvmAssert(i >= 0);
			return locals[verifier->stackBase+i];
		}

		Value& stackTop() {
			return locals[verifier->stackBase+stackDepth-1];
		}

		int sp() const {
			return verifier->stackBase+stackDepth-1;
		}

		void setType(int i, Traits* t, bool notNull=false, bool isWith=false)
		{
			//this->traits = t;
			WB(verifier->core->GetGC(), this, &locals[i].traits, t);
			locals[i].notNull = notNull;
			locals[i].isWith = isWith;
		}

		void pop(int n=1)
		{
			stackDepth -= n;
			AvmAssert(stackDepth >= 0);
		}

		Value& peek(int n=1)
		{
			AvmAssert(stackDepth-n >= 0);
			return locals[verifier->stackBase+stackDepth-n];
		}

		void pop_push(int n, Traits* type, bool notNull=false)
		{
			int _sp = stackDepth - n;
			setType(verifier->stackBase+_sp, type, notNull);
			stackDepth = _sp+1;
		}

		void push(Value& _value)
		{
			AvmAssert(verifier->stackBase+stackDepth+1 <= verifier->frameSize);
			setType(verifier->stackBase+stackDepth++, _value.traits, _value.notNull);
		}

		void push(Traits* traits, bool notNull=false)
		{
			AvmAssert(verifier->stackBase+stackDepth+1 <= verifier->frameSize);
			setType(verifier->stackBase+stackDepth++, traits, notNull);
		}

	};
}

#endif /* __avmplus_FrameState__ */
