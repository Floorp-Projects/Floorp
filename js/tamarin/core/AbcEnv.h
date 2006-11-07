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

#ifndef __avmplus_AbcEnv__
#define __avmplus_AbcEnv__


namespace avmplus
{

	// runtime info associated with a pool
	class AbcEnv : public MMgc::GCObject
	{
	public:
		PoolObject* const pool;
		DomainEnv* const domainEnv;
		CodeContext* const codeContext;
		MultinameHashtable privateScriptEnvs;
		MethodEnv* methods[1]; // actual size will hold pool->methodCount methods

		AbcEnv(PoolObject* pool,
			   DomainEnv* domainEnv,
		       CodeContext *codeContext)
			: pool(pool),
			  domainEnv(domainEnv),
			  codeContext(codeContext)
		{
		}

		void setMethod(int i, MethodEnv* env)
		{
			WB(pool->core->GetGC(), this, &methods[i], env);
		}

		static size_t calcExtra(PoolObject* pool)
		{
			if (pool->methodCount <= 1)
				return 0;
			else
				return sizeof(MethodEnv*)*(pool->methodCount-1);
		}

		ScriptEnv* getScriptEnv(Stringp name, Namespacep ns);
		ScriptEnv* getScriptEnv(Multiname *m);
	};

}

#endif // __avmplus_AbcEnv__
