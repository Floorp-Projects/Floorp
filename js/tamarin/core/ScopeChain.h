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


namespace avmplus
{

	/**
	 * type descriptor for a captured scope chain
	 */
	class ScopeTypeChain : public MMgc::GCObject
	{
		ScopeTypeChain(ScopeTypeChain* outer, int capture, int extra)
		{
			if (outer)
			{
				size = outer->size + capture;
				for (int i=0, n=outer->size; i < n; i ++)
					scopes[i] = outer->scopes[i];
			}
			else
			{
				size = capture;
			}
			fullsize = size + extra;
		}

	public:
		int size;
		int fullsize;
		struct Entry {
			Traits* traits;
			bool isWith;
		}
		scopes[1]; // actual length = fullsize;
		
		static ScopeTypeChain* create(MMgc::GC* gc, ScopeTypeChain* outer, int capture, int extra=0)
		{
			int pad = capture+extra;
			size_t padSize = sizeof(struct Entry) *
				(((pad > 0) ? (pad - 1) : 0) + (outer ? outer->size : 0));
			return new (gc, padSize) ScopeTypeChain(outer, capture, extra);
		}

    };

	/**
	* a captured scope chain
	*/
	class ScopeChain : public MMgc::GCObject
	{
	public:
		ScopeTypeChain* const scopeTraits;
		DRCWB(Namespace*) const defaultXmlNamespace;
	private:
		friend class CodegenMIR;
		Atom scopes[1]; // actual length == size

	public:

		/*
		dxns is modelled as a variable on an activation object.  The activation
		object will be in several scope chains, so we can't store dxns in the SC.
		When it changes, it's new valuable is visible in all closures in scope.
		*/
		
		ScopeChain(ScopeTypeChain* scopeTraits, ScopeChain* outer, Namespace *dxns)
		  : scopeTraits(scopeTraits), defaultXmlNamespace(dxns)
		{
			if (outer)
			{
				for (int i=0, n=outer->scopeTraits->size;/*outer->getSize();*/ i < n; i ++)
				{
					setScope(i, outer->scopes[i]);
				}
			}
		}
		
		Atom getScope(int i) const
		{
			AvmAssert(i >= 0 && i < scopeTraits->size);
			return scopes[i];
		}

		void setScope(int i, Atom value)
		{
			AvmAssert(i >= 0 && i < scopeTraits->size);
			//scopes[i] = value;
			WBATOM(MMgc::GC::GetGC(this), this, &scopes[i], value);
		}

		int getSize() const {
		  if ( scopeTraits )
			return scopeTraits->size;
		  else
		    return 0;
		}

		Namespace** getDefaultNamespaceAddr() const { 
			return (Namespace**)&defaultXmlNamespace;
		}
		
		static ScopeChain* create(MMgc::GC* gc, ScopeTypeChain *scopeTraits, ScopeChain* outer, Namespace *dxns)
		{
			int depth = scopeTraits->size;
			size_t padSize = depth > 0 ? sizeof(Atom) * (depth-1) : 0;
			return new (gc, padSize) ScopeChain(scopeTraits, outer, dxns);
		}
    };
	
}
