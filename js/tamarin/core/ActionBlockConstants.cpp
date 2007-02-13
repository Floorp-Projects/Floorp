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


#include "avmplus.h"

namespace avmplus
{
	namespace ActionBlockConstants
	{
		const unsigned char kindToPushOp[] = {
			0,
			OP_pushstring, // CONSTANT_Utf8=1
			0, 
			OP_pushint,  // CONSTANT_Int=3
			OP_pushint,  // CONSTANT_UInt=4
			OP_pushnamespace, // CONSTANT_PrivateNs=5
			OP_pushdouble, // CONSTANT_Double=6
			0,
			OP_pushnamespace, // CONSTANT_Namespace=8
			0,
			OP_pushfalse, // CONSTANT_False=10
			OP_pushtrue, // CONSTANT_True=11
			OP_pushnull, // CONSTANT_Null=12
			0,
			0,
			0,
			0,
			0,
			0,
			0,
			0,
			0,
			OP_pushnamespace, //CONSTANT_PackageNamespace=22
			OP_pushnamespace, //CONSTANT_PackageInternalNs=23
			OP_pushnamespace, //CONSTANT_ProtectedNamespace=24
			OP_pushnamespace, //CONSTANT_ExplicitNamespace=25
			OP_pushnamespace, //CONSTANT_StaticProtectedNs=26
		};
			

#ifdef AVMPLUS_VERBOSE
		const char *constantNames[] = {
			"const-0",
			"utf8",//const int CONSTANT_Utf8         = 0x01;
			"const-2",
			"int",//const int CONSTANT_Int = 0x03;
			"uint",//const int CONSTANT_UInt = 0x04;
			"private",//const int CONSTANT_PrivateNS = 0x05;
			"double",//const int CONSTANT_Double       = 0x06;
			"qname",//const int CONSTANT_Qname        = 0x07;
			"namespace",//const int CONSTANT_Namespace    = 0x08;
			"multiname",//const int CONSTANT_Multiname    = 0x09;
			"false",//const inst CONSTANT_False = 0x0A;
			"true",//const int CONSTANT_True         = 0x0B;
			"null",//const int CONSTANT_Null         = 0x0C;
			"@qname",//const int CONSTANT_QnameAttr    = 0x0D;
			"@multiname",//const int CONSTANT_MultinameAttr= 0x0E;
			"rtqname",//const int CONSTANT_RTQname		= 0x0F; // ns::name, var qname, const name
			"@rtqname",//const int CONSTANT_RTQnameA		= 0x10; // @ns::name, var qname, const name
			"rtqnamelate",//const int CONSTANT_RTQnameL		= 0x11; // ns::[], var qname
			"@rtqnamelate",//const int CONSTANT_RTQnameLA	= 0x12; // @ns::[], var qname
			"", //const int CONSTANT_NameL		= 0x13,	// o.[], ns=public implied, rt name
			"", //CONSTANT_NameLA		= 0x14, // o.@[], ns=public implied, rt attr-name
			"namespaceset", //CONSTANT_NamespaceSet = 0x15
            "namespace", //PackageNamespace and Namespace are the same as far as the VM is concerned
			"internal",//const int CONSTANT_PackageInternalNS = 0x17
			"protected",//const int CONSTANT_ProtectedNamespace = 0x18
			"explicit",//const int CONSTANT_ExplicitNamespace = 0x19
			"staticprotected",//const int CONSTANT_StaticProtectedNs = 0x1A,
			"multinamelate", //const int CONSTANT_MultinameL		= 0x1B,	// o.[], ns, rt name
			"@multinamelate" //CONSTANT_MultinameLA		= 0x1C, // o.@[], ns, rt attr-name
		};

		const char *traitNames[] = {
			"slot",//const int TRAIT_Slot			    = 0x00;
			"method",//const int TRAIT_Method			= 0x01;
			"getter",//const int TRAIT_Getter			= 0x02;
			"setter",//const int TRAIT_Setter			= 0x03;
			"class",//const int TRAIT_Class			    = 0x04;
			"function",//const int TRAIT_Function		= 0x05;
			"const",//const int TRAIT_Const             = 0x06;
		};
#endif // AVMPLUS_VERBOSE

		#include "opcodes.cpp"		
	}
}
