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

#ifndef __avmplus_NamespaceSet__
#define __avmplus_NamespaceSet__


namespace avmplus
{
	/**
	 * NamespaceSet is a reference to 0 or more namespaces.  It consists
	 * of a list of namespaces.
	 */
	class NamespaceSet : public MMgc::GCObject
	{
		friend class Codegen;
		#ifdef AVMPLUS_MIR
		friend class CodegenMIR;
		#endif 
		#ifdef AVMPLUS_INTERP
		friend class Interpreter;
		#endif

	public:
		int size;
		Namespace* namespaces[1/*namespaceCount*/];

		NamespaceSet(int namespaceCount);

		NamespaceSet(Namespace* ns)
		{
			this->size = 1;
			this->namespaces[0] = ns;
		}

		bool contains(Namespace* ns) const
		{
			for (int i=0,n=size; i < n; i++)
				if (namespaces[i] == ns)
					return true;
			return false;
		}

//#ifdef AVMPLUS_VERBOSE
	public:
		Stringp format(AvmCore* core) const;
//#endif
	};
}

#endif /* __avmplus_NamespaceSet__ */
