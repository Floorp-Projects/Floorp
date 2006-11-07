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


#ifndef __avmplus_TraitsIterator__
#define __avmplus_TraitsIterator__

namespace avmplus
{
	class TraitsIterator
	{
	public:
		TraitsIterator(Traits *traits) :
			traits(traits),
			index(0)
		{
		}

		bool getNext(Stringp& key, Namespace*& ns, Binding& value)
		{
			if (index == -1)
			{
				return false;
			}
			while ((index = traits->next(index)) == 0)
			{
				// ascend to base traits
				traits = traits->base;
				if (!traits)
				{
					// no more traits, bail out.
					index = -1;
					return false;
				}
				// restart iteration on base traits
				index = 0;
			}
			key   = traits->keyAt(index);
			ns    = traits->nsAt(index);
			value = traits->valueAt(index);
			return true;
		}

		Traits* currentTraits()
		{
			// this value changes as we walk up the traits chain
			return traits;
		}

	private:
		Traits *traits;
		int index;
	};
}

#endif /* __avmplus_TraitsIterator__ */
