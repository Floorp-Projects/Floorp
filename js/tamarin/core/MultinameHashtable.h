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

#ifndef __avmplus_MultinameHashtable__
#define __avmplus_MultinameHashtable__


namespace avmplus
{
	/**
	 * Hashtable for mapping <name, ns> pairs to a Binding
	 */
	class MultinameHashtable : public MMgc::GCObject
	{
		class Triple 
		{
		public:
			Stringp name;
			Namespace* ns;
			Binding value;
		};

		/**
		 * Finds the hash bucket corresponding to the key <name,ns>
		 * in the hash table starting at t, containing tLen
		 * triples.
		 */
		static int find(Stringp name, Namespace* ns, Triple *t, unsigned tLen);
	    void rehash(Triple *oldAtoms, int oldlen, Triple *newAtoms, int newlen);
		/**
		 * Called to grow the Hashtable, particularly by add.
		 *
		 * - Calculates the needed size for the new Hashtable
		 *   (typically 2X the current size)
		 * - Creates a new array of Atoms
		 * - Rehashes the current table into the new one
		 * - Deletes the old array of Atoms and sets the Atom
		 *   pointer to our new array of Atoms
		 * 
		 */
		void grow();

		/** property hashtable */
		DWB(Triple*) triples;

	public:
		/**
		 * since identifiers are always interned strings, they can't be 0,
		 * so we can use 0 as the empty value.
		 */
		const static Atom EMPTY = 0;

		/** kDefaultCapacity must be a power of 2 */
		const static int kDefaultCapacity = 8;

		/** no. of properties */
		int size;

		/** size of hashtable (number of triples - actual capacity is *3) */
		int numTriplets;

		/**
		 * initialize with a known capacity.  i.e. we can fit minSize
		 * elements in without rehashing.
		 * @param heap
		 * @param capacity  # of logical slots
		 */
		MultinameHashtable(int capacity = kDefaultCapacity);

		~MultinameHashtable();

		bool isFull() const;

		/**
		 * @name operations on name/ns/binding triples
		 */
		/*@{*/
		void    put(Stringp name, Namespace* ns, Binding value);

		Binding get(Stringp name, Namespace* ns) const;
		Binding get(Stringp name, NamespaceSet* nsset) const;
		Binding getMulti(Multiname* name) const;
		Binding getName(Stringp name) const;
		/*@}*/

		/**
		 * Adds a name/value pair to a hash table.  Automatically
		 * grows the hash table if it is full.
		 */
		void add (Stringp name, Namespace* ns, Binding value);

		/**
		 * Allow caller to enumerate all entries in the table.
		 */
		int next(int index);
		Stringp keyAt(int index);
		Namespace* nsAt(int index);
		Binding valueAt(int index);
	protected:
		void Init(int capacity);
	};
}

#endif /* __avmplus_MultinameHashtable__ */
