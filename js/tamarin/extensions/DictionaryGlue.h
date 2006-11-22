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
 * by the Initial Developer are Copyright (C)[ 1993-2006 ] Adobe Systems Incorporated. All Rights 
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


#ifndef DICTIONARYGLUE_INCLUDED
#define DICTIONARYGLUE_INCLUDED

namespace avmplus
{
	class DictionaryObject : public ScriptObject
	{
	public:
		DictionaryObject(VTable *vtable, ScriptObject *delegate);
		~DictionaryObject();
		void constructDictionary(bool weakKeys);
	
		virtual Hashtable* getTable() const { return table; }
	
		// multiname and Stringp forms fall through to ScriptObject
		virtual Atom getAtomProperty(Atom name) const;
		virtual bool hasAtomProperty(Atom name) const;
		virtual bool deleteAtomProperty(Atom name);
		virtual void setAtomProperty(Atom name, Atom value);

		virtual Atom nextName(int index);
		virtual int nextNameIndex(int index);

	private:
		DWB(Hashtable*) table;
		bool weakKeys;

		Atom getKeyFromObject(Atom object) const;
		WeakKeyHashtable *weakTable() const { return (WeakKeyHashtable*)(Hashtable*)table; }
	};

	class DictionaryClass : public ClassClosure
	{
	public:
		DictionaryClass(VTable *vtable);
		ScriptObject *createInstance(VTable *ivtable, ScriptObject *delegate);
		DECLARE_NATIVE_MAP(DictionaryClass)
	};
}

#endif /* DICTIONARYGLUE_INCLUDED */
