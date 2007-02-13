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

#ifndef __GCStack__
#define __GCStack__

namespace MMgc
{	
	template<typename T, int defSize=512>
	class GCStack
	{
		enum { kDefSize = defSize };
	public:
		GCStack(int defaultSize=kDefSize) : m_iCount(0), m_iAllocSize(defaultSize), m_items(NULL) 
		{
			Alloc();
		}

		~GCStack()
		{
			if ( m_items )
			{
				delete [] m_items;
				m_items = NULL;
			}
			m_iCount = m_iAllocSize = 0;
		}

		void Push(T item)
		{
			if ( ( m_iCount + 1 ) > m_iAllocSize ) 
			{
				// need to allocate a new block first
				m_iAllocSize = m_iAllocSize ? m_iAllocSize*2 : kDefSize;
				Alloc();
			}

			m_items[m_iCount++] = item;
		}

		T Pop()
		{
			T t = m_items[--m_iCount];
#ifdef _DEBUG
			GCAssert(m_iCount>=0);
			memset(&m_items[m_iCount], 0, sizeof(T));
#endif
			return t;
		}

		T Peek()
		{
#ifdef _DEBUG
			GCAssert(m_iCount>=0);
#endif
			T t = m_items[m_iCount-1];
			return t;
		}

		unsigned int Count() { return m_iCount; }

		void Keep(unsigned int num)
		{
			GCAssert(num <= m_iCount);
			m_iCount = num;
		}

		T* GetData() { return m_items; }

	protected:
		// no impl
		GCStack(const GCStack& other);
		GCStack& operator=(const GCStack& other);

	private:
		void Alloc() 
		{
			// need to allocate a new block first
			if(m_iAllocSize) {
				T* items = new T[ m_iAllocSize ];
				if ( items )
				{
					::memcpy(items, m_items, m_iCount * sizeof(T));
				}
				delete [] m_items;
				m_items = items;
			}
		}
		unsigned int m_iCount, m_iAllocSize;
		T *m_items;
	};
}

#endif /* __GCStack__ */
