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

package es4 
{
    use namespace release

    /*
    
    Emit an ABC file
    
    */

    import flash.utils.ByteArray
        
    class ByteCodeFactory
    {
        function ByteCodeFactory() 
        {
        }
        
        function makeByte(bytes,val:int)
        {
            Debug.enter("makeByte",val)
            bytes.writeByte(val)
            Debug.exit("makeByte")
        }
        
        function makeInt16(bytes,val:int)
        {
            Debug.enter("makeShort",val)
            var first = val & 0xFF
            var second = val >> 8
            bytes.writeByte(first)
            bytes.writeByte(second)
            Debug.exit("makeShort")
        }
        
        function makeInt32(bytes,val:int)
        {
            Debug.enter("makeInt32",val)

            if( val < 128 )
            {
                bytes.writeByte(val&0x7F)
            }
            else if ( val < 16384 )
            {
                var first : int = val&0x7F | 0x80
                var second : int = val >> 7
                bytes.writeByte(first)
                bytes.writeByte(second)
            }
            else if ( val < 2097152 )
            {
                var first : int  = val&0x7F | 0x80
                var second : int = val >> 7 | 0x80
                var third : int = val >> 14
                bytes.writeByte(first)
                bytes.writeByte(second)
                bytes.writeByte(third)
            }
            else if ( val < 268435456 )
            {
                var first : int = val&0x7F  | 0x80
                var second : int = val >> 7  | 0x80
                var third : int = val >> 14 | 0x80
                var fourth : int = val >> 21
                bytes.writeByte(first)
                bytes.writeByte(second)
                bytes.writeByte(third)
                bytes.writeByte(fourth)
            }
            else
            {
                var first : int = val&0x7F  | 0x80
                var second : int = val >> 7  | 0x80
                var third : int = val >> 14 | 0x80
                var fourth : int = val >> 21 | 0x80
                var fifth : int = val >> 28
                bytes.writeByte(first)
                bytes.writeByte(second)
                bytes.writeByte(third)
                bytes.writeByte(fourth)
                bytes.writeByte(fifth)
            }
            Debug.exit("makeInt32")
        }
        
        function makeDouble(bytes,val)
        {
            bytes.writeDouble(val)
        }

        function makeBytes(bytes,from)
        {
            Debug.enter("makeBytes length",from.length)
            makeInt32(bytes,from.length)
            bytes.writeBytes(from)
            Debug.exit("makeBytes")
        }
    }
    
    public function testByteCodeFactory()
    {    
        var bytes = new ByteArray
        var bcf = new ByteCodeFactory
        bcf.makeByte(bytes,10)
        bcf.makeInt32(bytes,10)
        bcf.makeInt32(bytes,0x0fffabcd)
        bytes.position = 0
        while ( bytes.bytesAvailable > 0 )
        {
            print(bytes.position+": "+bytes.readByte())
        }
    }

    class ABCEmitter extends ByteCodeFactory
    {
        private const CONSTANT_Utf8               = 0x01
        private const CONSTANT_Integer            = 0x03
        private const CONSTANT_UInt               = 0x04
        private const CONSTANT_PrivateNamespace   = 0x05
        private const CONSTANT_Double             = 0x06
        private const CONSTANT_Qname              = 0x07 // ns::name, const ns, const name
        private const CONSTANT_Namespace          = 0x08
        private const CONSTANT_Multiname          = 0x09 // [ns...]::name, const [ns...], const name
        private const CONSTANT_False              = 0x0A
        private const CONSTANT_True               = 0x0B
        private const CONSTANT_Null               = 0x0C
        private const CONSTANT_QnameA             = 0x0D // @ns::name, const ns, const name
        private const CONSTANT_MultinameA         = 0x0E // @[ns...]::name, const [ns...], const name
        private const CONSTANT_RTQname            = 0x0F // ns::name, var ns, const name
        private const CONSTANT_RTQnameA           = 0x10 // @ns::name, var ns, const name
        private const CONSTANT_RTQnameL           = 0x11 // ns::[name], var ns, var name
        private const CONSTANT_RTQnameLA          = 0x12 // @ns::[name], var ns, var name
        private const CONSTANT_NameL              = 0x13 // o[name], var name
        private const CONSTANT_NameLA             = 0x14 // @[name], var name
        private const CONSTANT_NamespaceSet       = 0x15
        private const CONSTANT_PackageNamespace   = 0x16 // namespace for a package
        private const CONSTANT_PackageInternalNS  = 0x17 
        private const CONSTANT_ProtectedNamespace = 0x18 
        private const CONSTANT_ExplicitNamespace  = 0x19
        private const CONSTANT_StaticProtectedNS  = 0x1A
        private const CONSTANT_MultinameL         = 0x1B
        private const CONSTANT_MultinameLA        = 0x1C

		private const SLOT_var = 0
		private const SLOT_method = 1
		private const SLOT_getter = 2
		private const SLOT_setter = 3
		private const SLOT_class = 4
		private const SLOT_function = 6

		// Method flags

	    private const METHOD_Arguments = 0x1;
    	private const METHOD_Activation = 0x2;
    	private const METHOD_Needrest = 0x4;
    	private const METHOD_HasOptional = 0x8;
    	private const METHOD_IgnoreRest = 0x10;
    	private const METHOD_Native = 0x20;
    	private const METHOD_Setsdxns = 0x40;
    	private const METHOD_HasParamNames = 0x80;

       	private const OP_bkpt = 0x01
        private const OP_nop = 0x02
        private const OP_throw = 0x03
        private const OP_getsuper = 0x04
        private const OP_setsuper = 0x05
        private const OP_dxns = 0x06
        private const OP_dxnslate = 0x07
        private const OP_kill = 0x08
        private const OP_label = 0x09
        private const OP_ifnlt = 0x0C
        private const OP_ifnle = 0x0D
        private const OP_ifngt = 0x0E
        private const OP_ifnge = 0x0F
        private const OP_jump = 0x10
        private const OP_iftrue = 0x11
        private const OP_iffalse = 0x12
        private const OP_ifeq = 0x13
        private const OP_ifne = 0x14
        private const OP_iflt = 0x15
        private const OP_ifle = 0x16
        private const OP_ifgt = 0x17
        private const OP_ifge = 0x18
        private const OP_ifstricteq = 0x19
        private const OP_ifstrictne = 0x1A
        private const OP_lookupswitch = 0x1B
        private const OP_pushwith = 0x1C
        private const OP_popscope = 0x1D
        private const OP_nextname = 0x1E
        private const OP_hasnext = 0x1F
        private const OP_pushnull = 0x20
        private const OP_pushundefined = 0x21
        private const OP_nextvalue = 0x23
        private const OP_pushbyte = 0x24
        private const OP_pushshort = 0x25
        private const OP_pushtrue = 0x26
        private const OP_pushfalse = 0x27
        private const OP_pushnan = 0x28
        private const OP_pop = 0x29
        private const OP_dup = 0x2A
        private const OP_swap = 0x2B
        private const OP_pushstring = 0x2C
        private const OP_pushint = 0x2D
        private const OP_pushuint = 0x2E
        private const OP_pushdouble = 0x2F
        private const OP_pushscope = 0x30
        private const OP_pushnamespace = 0x31
        private const OP_newfunction = 0x40
        private const OP_call = 0x41
        private const OP_construct = 0x42
        private const OP_callmethod = 0x43
        private const OP_callstatic = 0x44
        private const OP_callsuper = 0x45
        private const OP_callproperty = 0x46
        private const OP_returnvoid = 0x47
        private const OP_returnvalue = 0x48
        private const OP_constructsuper = 0x49
        private const OP_constructproperty = 0x4A
        private const OP_callsuperid = 0x4B
        private const OP_callproplex = 0x4C
        private const OP_callinterface = 0x4D
        private const OP_callsupervoid = 0x4E
        private const OP_callpropvoid = 0x4F
        private const OP_newobject = 0x55
        private const OP_newarray = 0x56
        private const OP_newactivation = 0x57
        private const OP_newclass = 0x58
        private const OP_getdescendants = 0x59
        private const OP_findpropstrict = 0x5D
        private const OP_findproperty = 0x5E
        private const OP_finddef = 0x5F
        private const OP_getlex = 0x60
        private const OP_setproperty = 0x61
        private const OP_getlocal = 0x62
        private const OP_setlocal = 0x63
        private const OP_getglobalscope = 0x64
        private const OP_getscopeobject = 0x65
        private const OP_getproperty = 0x66
        private const OP_getpropertylate = 0x67
        private const OP_initproperty = 0x68
        private const OP_deleteproperty = 0x6A
        private const OP_deletepropertylate = 0x6B
        private const OP_getslot = 0x6C
        private const OP_setslot = 0x6D
        private const OP_getglobalslot = 0x6E
        private const OP_setglobalslot = 0x6F
        private const OP_convert_s = 0x70
        private const OP_esc_xelem = 0x71
        private const OP_esc_xattr = 0x72
        private const OP_convert_i = 0x73
        private const OP_convert_u = 0x74
        private const OP_convert_d = 0x75
        private const OP_convert_b = 0x76
        private const OP_convert_o = 0x77
        private const OP_coerce = 0x80
        private const OP_coerce_b = 0x81
        private const OP_coerce_a = 0x82
        private const OP_coerce_i = 0x83
        private const OP_coerce_d = 0x84
        private const OP_coerce_s = 0x85
        private const OP_astype = 0x86
        private const OP_astypelate = 0x87
        private const OP_coerce_u = 0x88
        private const OP_coerce_o = 0x89
        private const OP_negate = 0x90
        private const OP_increment = 0x91
        private const OP_inclocal = 0x92
        private const OP_decrement = 0x93
        private const OP_declocal = 0x94
        private const OP_typeof = 0x95
        private const OP_not = 0x96
        private const OP_bitnot = 0x97
        private const OP_concat = 0x9A
        private const OP_add_d = 0x9B
        private const OP_add = 0xA0
        private const OP_subtract = 0xA1
        private const OP_multiply = 0xA2
        private const OP_divide = 0xA3
        private const OP_modulo = 0xA4
        private const OP_lshift = 0xA5
        private const OP_rshift = 0xA6
        private const OP_urshift = 0xA7
        private const OP_bitand = 0xA8
        private const OP_bitor = 0xA9
        private const OP_bitxor = 0xAA
        private const OP_equals = 0xAB
        private const OP_strictequals = 0xAC
        private const OP_lessthan = 0xAD
        private const OP_lessequals = 0xAE
        private const OP_greaterthan = 0xAF
        private const OP_greaterequals = 0xB0
        private const OP_instanceof = 0xB1
        private const OP_istype = 0xB2
        private const OP_istypelate = 0xB3
        private const OP_in = 0xB4
        private const OP_increment_i = 0xC0
        private const OP_decrement_i = 0xC1
        private const OP_inclocal_i = 0xC2
        private const OP_declocal_i = 0xC3
        private const OP_negate_i = 0xC4
        private const OP_add_i = 0xC5
        private const OP_subtract_i = 0xC6
        private const OP_multiply_i = 0xC7
        private const OP_getlocal0 = 0xD0
        private const OP_getlocal1 = 0xD1
        private const OP_getlocal2 = 0xD2
        private const OP_getlocal3 = 0xD3
        private const OP_setlocal0 = 0xD4
        private const OP_setlocal1 = 0xD5
        private const OP_setlocal2 = 0xD6
        private const OP_setlocal3 = 0xD7
        private const OP_abs_jump = 0xEE
        private const OP_debug = 0xEF
        private const OP_debugline = 0xF0
        private const OP_debugfile = 0xF1
        private const OP_bkptline = 0xF2
        private const OP_timestamp = 0xF3
        private const OP_verifypass = 0xF5
        private const OP_alloc = 0xF6
        private const OP_mark = 0xF7
        private const OP_wb = 0xF8
        private const OP_prologue = 0xF9
        private const OP_sendenter = 0xFA
        private const OP_doubletoatom = 0xFB
        private const OP_sweep = 0xFC
        private const OP_codegenop = 0xFD
        private const OP_verifyop = 0xFE
        private const OP_decode = 0xFF

        // ABC parts
        
        private var minor_version = 16
        private var major_version = 46
        private var int_pool = new Array
        private var uint_pool = new Array
        private var double_pool = new Array
        private var utf8_pool = new Array
        private var namespace_pool = new Array
        private var namespaceset_pool = new Array
        private var multiname_pool = new Array
        private var method_infos = new Array
        private var metadata_infos = new Array
        private var instance_infos = new Array
        private var class_infos = new Array
        private var script_infos = new Array
        private var method_bodys = new Array

		function methodCount() 
		{
			return method_infos.length
		}

        function ABCEmitter(minor=16,major=46)
        {
            if( major != 46 )
            {
                throw "major version " + major + " not supported!"
            }
            this.minor_version = minor
            this.major_version = major
        }
        
		function compareByteArrays(ba1:ByteArray,ba2:ByteArray)
		{
			if( ba1.length != ba2.length ) return false

			for( var i = ba1.length; i>=0; i-- )
			{
				if( ba1[i] != ba2[i] ) return false
			}
			return true
		}

        function addBytesToPool(bytes,pool)
        {
            var count = pool.length
            for( var i = 0; i<count; ++i )
            {
                if( compareByteArrays(bytes,pool[i]) )
                {
                    break
                }
            }
            pool[i] = bytes
            return i+1
        }
        
        // factory methods
		
		var int_pool_out = ["-------","I N T S","-------"]
		var uint_pool_out = ["---------","U I N T S","---------"]
		var double_pool_out = ["-------------","D O U B L E S","-------------"]
		var utf8_pool_out = ["-------------","S T R I N G S","-------------"]
		var namespace_pool_out = ["-------------------","N A M E S P A C E S","-------------------"]
		var namespaceset_pool_out = ["-------------------------","N A M E S P A C E S E T S","-------------------------"]
		var multiname_pool_out = ["-------------------","M U L T I N A M E S","-------------------"]
		var info_out = ["---------","I N F O S","---------"]
		var body_out = ["-----------","B O D I E S","-----------"]
		var code_out = []
        
        function ConstantUtf8(str:String) // **design: anno needed to force conversion to string
        {
            Debug.enter("ConstantUtf8",str.length,str)
            
            var bytes = new ByteArray
            makeInt32(bytes,str.length)
            bytes.writeUTFBytes(str)
            var index = addBytesToPool(bytes,utf8_pool)
            
            Debug.exit("ConstantUtf8",index)
            Debug.log_mode::log("ConstantUtf8 "+str+" -> "+index,utf8_pool_out)
            return index
        }
        
        function ConstantDouble(num:Number) // **design: anno needed to force conversion to string
        {
            Debug.enter("ConstantDouble",num)
            
            var bytes = new ByteArray
			makeDouble(bytes,num)
            var index = addBytesToPool(bytes,double_pool)
            
            Debug.exit("ConstantDouble",index)
            Debug.log_mode::log("ConstantDouble "+num+" -> "+index,double_pool_out)
            return index
        }
        
        function ConstantInt(i:int) // **design: anno needed to force conversion to string
        {
            Debug.enter("ConstantInt",i)
            
            var bytes = new ByteArray
			makeInt32(bytes,i)
            var index = addBytesToPool(bytes,int_pool)
            
            Debug.exit("ConstantInt",index)
            Debug.log_mode::log("ConstantInt "+i+" -> "+index,int_pool_out)
            return index
        }
        
        function ConstantNamespace(uri_index:uint,kind)
        { 
			var kind_str = kind==CONSTANT_PackageNamespace ? "public" :			// package public
						kind==CONSTANT_PackageInternalNS ? "internal" :				// package internal
						kind==CONSTANT_ProtectedNamespace ? "protected" :
						kind==CONSTANT_StaticProtectedNS ? "static protected" :
						kind==CONSTANT_Namespace ? "user" :
						kind==CONSTANT_PrivateNamespace ? "private" : "**error**"


            Debug.enter("ConstantNamespace",uri_index,kind_str)  
            var bytes = new ByteArray         
            makeByte(bytes,kind)
            makeInt32(bytes,uri_index)
            var index = addBytesToPool(bytes,namespace_pool)
            Debug.exit("ConstantNamespace",index)  
            Debug.log_mode::log("ConstantNamespace "+uri_index+" "+kind_str+" -> "+index,namespace_pool_out)
            return index
        }

        function ConstantNamespaceSet(namespaces)
        {            
            var bytes = new ByteArray         
            var count = namespaces.length

            makeInt32(bytes,count)
			var nsset_out = " "
            for( var i = 0; i < count; i++ )
            {
				var name = namespaces[i].@name
				var kind = namespaces[i].@kind=="internal"?CONSTANT_PackageInternalNS:
												"public"?CONSTANT_PackageNamespace:
													CONSTANT_Namespace
                var utf8_index = ConstantUtf8(name)
                var ns_index = ConstantNamespace(utf8_index,kind)
				nsset_out += ns_index + " "
                makeInt32(bytes,ns_index)
            }
            var index = addBytesToPool(bytes,namespaceset_pool)
            Debug.log_mode::log("ConstantNamespaceSet ["+nsset_out+"] -> "+index,namespaceset_pool_out)
            return index
        }

        function ConstantMultiname(name_index,nsset_index,is_attr)
        {            
            var bytes = new ByteArray
            makeByte(bytes,is_attr?CONSTANT_MultinameA:CONSTANT_Multiname)
            makeInt32(bytes,name_index)
            makeInt32(bytes,nsset_index)
            var index = addBytesToPool(bytes,multiname_pool)
            Debug.log_mode::log("ConstantMultiname "+name_index+" "+nsset_index+" -> "+index,multiname_pool_out)
            return index
        }

        function ConstantQualifiedName(name_index,ns_index,is_attr)
        {            
            var bytes = new ByteArray
            makeByte(bytes,is_attr?CONSTANT_QnameA:CONSTANT_Qname)
            makeInt32(bytes,ns_index)
            makeInt32(bytes,name_index)
            var index = addBytesToPool(bytes,multiname_pool)
			Debug.log_mode::log("ConstantQualifiedName "+name_index+" "+ns_index+" "+is_attr+" -> "+index,multiname_pool_out)
            return index
        }

		/*

		MethodInfo {
		    U30 param_count
		    U30 ret_type					  // CONSTANT_Multiname, 0=Object
		    U30 param_types[param_count]	  // CONSTANT_Multiname, 0=Object
		    U30 name_index                    // 0=no name.
		    // 1=need_arguments, 2=need_activation, 4=need_rest 8=has_optional 16=ignore_rest, 32=explicit, 64=setsdxns, 128=has_paramnames
		    U8 flags                          
		    U30 optional_count                // if has_optional
		    ValueKind[optional_count]         // if has_optional
		    U30 param_names[param_count]      // if has_paramnames
		}

		*/

        function MethodInfo(param_count,type,types,name_index,flags,optional_count,optional_kinds)
        {
            Debug.enter("MethodInfo",param_count,type,name_index,flags,optional_count,optional_kinds)

            var method_info = method_infos.length //getMethodInfo(name)
            var bytes = new ByteArray

            makeInt32(bytes,param_count);
            makeInt32(bytes,type);
            for (var i=0; i < param_count; i++)
            {
                makeInt32(bytes,0/*types[i]*/);
            }
            makeInt32(bytes,name_index);
            makeByte(bytes,flags);
            if( false /*flags & HAS_OPTIONAL*/ )
            {
                makeInt32(bytes,optional_count);
                for (var i=0; i < optional_count; i++)
                {
                    makeInt32(bytes,optional_kinds[i]);
                }
            }
//      MethodInfo  param_count=1 return_type=0 param_types={ 0 } debug_name_index=2 needs_arguments=false need_rest=false needs_activation=true has_optional=false ignore_rest=false native=false has_param_names =false -> 0            
            Debug.log_mode::log("MethodInfo param_count="+param_count+" return_type="+type+" param_types={"+types+"} debug_name="+name_index+" flags="+flags+" "+optional_count+" "+optional_kinds+" -> "+method_info,info_out)

            method_infos.push(bytes)
            Debug.exit("MethodInfo")
            return method_info
        }

		function dumpBytes(bytes)
		{
			bytes.position = 0
		    var str = ""
			while( bytes.bytesAvailable ) { str += " "+bytes.readByte() }
		    print(str)
		}
        
        function MethodBody(info_index,max_stack,max_locals,scope_depth,max_scope,code,exceptions,slot_infos)
        {
            Debug.enter("MethodBody",info_index,max_stack,max_locals,scope_depth,max_scope)
            var bytes = new ByteArray

            makeInt32(bytes,info_index)
            makeInt32(bytes,max_stack)
            makeInt32(bytes,max_locals)
            makeInt32(bytes,scope_depth)
            makeInt32(bytes,max_scope)
            makeBytes(bytes,code)
            makeBytes(bytes,exceptions)
			emitInfos(bytes,slot_infos)

            method_bodys.push(bytes)
 
            Debug.log_mode::log("MethodBody "+info_index+" max_stack="+max_stack+" max_locals="+max_locals+" scope_depth="+scope_depth+" max_scope="+max_scope+" code_length="+code.length+" slot_count="+slot_infos.length+" code_size="+bytes.length,body_out)
            Debug.exit("MethodBody")
            return method_bodys.length
        }

        function ScriptInfo(init_index,slot_infos)
        {
            Debug.log_mode::log("ScriptInfo init_index="+init_index+" slots="+slot_infos.length,info_out)
            var bytes = new ByteArray

            makeInt32(bytes,init_index)
			emitInfos(bytes,slot_infos)

            script_infos.push(bytes)
            return script_infos.length
        }

        // Emitter methods

        function emitVersion(bytes,minor,major)
        {
            Debug.enter("emitVersion",minor,major)
            makeInt16(bytes,minor)
            makeInt16(bytes,major)
            Debug.exit("emitVersion",bytes.length)
        }
        
        function emitConstantPool(bytes,pool)
        {
            Debug.enter("emitConstantPool",bytes.length,pool.length)
            var count = pool.length
            makeInt32(bytes,count==0?0:count+1)
            for( var i = 0; i < count; ++i )
            {
                bytes.writeBytes(pool[i])
            }
            Debug.exit("emitConstantPool",bytes.length)
        }
        
        function emitInfos(bytes,infos)
        {
            Debug.enter("emitInfos",infos.length)
            var count = infos.length
            makeInt32(bytes,count)
            for( var i = 0; i < count; ++i )
            {
                bytes.writeBytes(infos[i])
            }
            Debug.exit("emitInfos",bytes.length)
        }
        
        function emitClassInfos(bytes,instance_infos,class_infos)
        {
            Debug.enter("emitClassInfos")
            var count = instance_infos.length
            makeInt32(bytes,count)
            for( var i = 0; i < count; ++i )
            {
                bytes.writeBytes(instance_infos[i])
            }
            for( var i = 0; i < count; ++i )
            {
                bytes.writeBytes(class_infos[i])
            }
            Debug.exit("emitClassInfos",bytes.length)
        }

        public function emit()
        {
            Debug.enter("emit")
            
            var bytes = new ByteArray
            
            emitVersion(bytes,minor_version,major_version)
            emitConstantPool(bytes,int_pool)
            emitConstantPool(bytes,uint_pool)
            emitConstantPool(bytes,double_pool)
            emitConstantPool(bytes,utf8_pool)
            emitConstantPool(bytes,namespace_pool)
            emitConstantPool(bytes,namespaceset_pool)
            emitConstantPool(bytes,multiname_pool)
            emitInfos(bytes,method_infos)
            emitInfos(bytes,metadata_infos)
            emitClassInfos(bytes,instance_infos,class_infos)
            emitInfos(bytes,script_infos)
            emitInfos(bytes,method_bodys)

			Debug.log_mode::dump(int_pool_out)
			Debug.log_mode::dump(uint_pool_out)
			Debug.log_mode::dump(double_pool_out)
			Debug.log_mode::dump(utf8_pool_out)
			Debug.log_mode::dump(namespace_pool_out)
			Debug.log_mode::dump(namespaceset_pool_out)
			Debug.log_mode::dump(multiname_pool_out)
			Debug.log_mode::dump(info_logs)
			Debug.log_mode::dump(body_out)
			Debug.log_mode::dump(code_logs)

            Debug.exit("emit",bytes.length)
            
            return bytes
        }
        
        var code
		var code_blocks = []
		var code_logs = ["-------","C O D E","-------"]
		var info_logs = ["---------","I N F O S","---------"]
		var pending_code_logs = []
		var pending_info_logs = []
 		var initial_scope_depth_stack = []
        
        function StartMethod(name) 
        {
            Debug.enter("StartMethod")

			this.pending_code_logs.push(code_out)  	// save current code log
			this.code_out = []						// and start a new one
			this.pending_info_logs.push(info_out)  	// save current info log
			this.info_out = []						// and start a new one

			Debug.log_mode::log("StartMethod "+name,code_out)
			
			this.code_blocks.push(code)   	// save the current code block
            this.code = new ByteArray     	// create a new one
			initial_scope_depth_stack.push(scope_depth)

            Debug.exit("StartMethod")
        }
        
        function FinishMethod(name,slot_infos,has_arguments,param_count,max_locals) 
        {        
            Debug.enter("FinishMethod",name)
			Debug.log_mode::log("FinishMethod "+name,code_out)

            var type_index  = 0
            var types       = new Array
            var name_index  = ConstantUtf8(name)
            var flags       = METHOD_Activation
			if( has_arguments )
			{
				flags |= METHOD_Arguments
				param_count--
			} 
            var optional_count = 0
            var optional_kinds = new Array
            
            var info_index = MethodInfo(param_count,type_index,types,name_index,flags,optional_count,optional_kinds)
            
            var max_stack = 10   // todo: compute these
            var exceptions = new ByteArray
			var initial_scope_depth = initial_scope_depth_stack.pop()

            MethodBody(info_index,max_stack,max_locals,initial_scope_depth,max_scope_depth,this.code,exceptions,slot_infos)

			this.code = this.code_blocks.pop() // restore the previously active code block

			this.code_logs.push(this.code_out)  			// save the finish code log
			this.code_out = this.pending_code_logs.pop()  	// resume the outer code log, pushing inner one deeper
			this.info_logs.push(this.info_out)
			this.info_out = this.pending_info_logs.pop()
            
			scope_depth = initial_scope_depth

            Debug.exit("FinishMethod",info_index)
            return info_index
        }

		/*

		Common interning patterns

		*/

		function internTypeName(name,namespace)
		{
			if( name == null || name == "*" )
			{
				return 0
			}
			else
			{
				return internQualifiedName(name,namespace)
			}
		}

		function internQualifiedName(name,namespace)
		{
            var identifier_index = ConstantUtf8(name)
			var namespace_index = ConstantNamespace(ConstantUtf8(namespace.@name),getNamespaceKind(namespace.@kind))
			var name_index = ConstantQualifiedName(identifier_index,namespace_index,false)	// is_attr = false
			return name_index
		}

		function getNamespaceKind(kind_str : String)
		{
			var result = 
					kind_str == "internal" ? CONSTANT_PackageInternalNS :
					kind_str == "public" ? CONSTANT_PackageNamespace :
					kind_str == "user" ? CONSTANT_Namespace :
					kind_str == "private" ? CONSTANT_PrivateNamespace : CONSTANT_Namespace
			return result
		}

        function SlotInfo(slot_infos,kind,slot_name,type_name,slot_id)
        {
            Debug.enter("SlotInfo",slot_name.name,kind,slot_name.namespace)

            var info_index = slot_infos.length //getMethodInfo(name)
            var bytes = new ByteArray

			var name_index = internQualifiedName(slot_name.name,slot_name.namespace)
			//var type_index = internTypeName(

			var kind_str = kind==SLOT_var?"var":
							kind==SLOT_function?"function":
							"unimplemented slot kind"
			Debug.log_mode::log("SlotInfo "+kind_str+" "+slot_name.name+" {"+dumpNamespaces([slot_name.namespace])+"} "+slot_id,info_out)


			makeInt32(bytes,name_index)
			makeByte(bytes,kind)

			switch( kind )
			{
				case SLOT_var:
					makeInt32(bytes,slot_id)  // 0 = autoassign
					makeInt32(bytes,0)  // type *
					makeInt32(bytes,0)  // no default value
					break
				case SLOT_method:
				case SLOT_getter:
				case SLOT_setter:
				case SLOT_class:
				case SLOT_function:
					throw "slot kind not implemented"
					break
			}

            slot_infos.push(bytes)

            Debug.exit("SlotInfo")
            return info_index
        }
                
        function StartClass() 
        {
        }
        
        function FinishClass() 
        {
        }
        
        function StartProgram() 
        {
			this.pending_info_logs.push(info_out)  	// save current code log
			this.info_out = []						// and start a new one

			Debug.log_mode::log("StartProgram",code_out)
        }
        
        function FinishProgram(init_index,slot_infos) 
        {
            Debug.enter("FinishProgram",init_index)
			Debug.log_mode::log("FinishProgram",code_out)

            ScriptInfo(init_index,slot_infos)

			this.info_logs.push(this.info_out)
			this.info_out = this.pending_info_logs.pop()

            Debug.exit("FinishProgram")
        }
        
        // Abstract machine methods

        public function LoadThis()
        {
			Debug.log_mode::log("  "+code.length+":LoadThis",code_out)
            makeByte(code,OP_getlocal0)
        }
        
        public function Pop()
        {
			Debug.log_mode::log("  "+code.length+":Pop",code_out)
            makeByte(code,OP_pop)
        }
        
        public function PushNull()
        {
			Debug.log_mode::log("  "+code.length+":PushNull",code_out)
            makeByte(code,OP_pushnull)
        }
        
        public function PushUndefined()
        {
			Debug.log_mode::log("  "+code.length+":PushUndefined",code_out)
            makeByte(code,OP_pushundefined)
        }
        
        public function PushString(str)
        {
			Debug.log_mode::log("  "+code.length+":PushString "+str,code_out)
            var index = ConstantUtf8(str)
            makeByte(code,OP_pushstring)
            makeInt32(code,index)
        }
        
        public function PushNumber(num:Number)
        {
			Debug.log_mode::log("  "+code.length+":PushNumber "+num,code_out)
			var index = ConstantDouble(num)
            makeByte(code,OP_pushdouble)
            makeInt32(code,index)
        }
        
        public function PushInt(num:int)
        {
			Debug.log_mode::log("  "+code.length+":PushInt "+num,code_out)
			var index = ConstantInt(num)
            makeByte(code,OP_pushint)
            makeInt32(code,index)
        }

		var scope_depth = 1
		var max_scope_depth = 1
        
        public function PushScope()
        {
			Debug.log_mode::log("  "+code.length+":PushScope",code_out)
            makeByte(code,OP_pushscope)
			scope_depth++
			if( scope_depth > max_scope_depth )
			{
				max_scope_depth = scope_depth
			}
        }
        
        public function PopScope()
        {
			Debug.log_mode::log("  "+code.length+":PopScope",code_out)
            makeByte(code,OP_popscope)
			scope_depth--
        }
        
        public function Dup()
        {
			Debug.log_mode::log("  "+code.length+":Dup",code_out)
            makeByte(code,OP_dup)
        }
        
		public function dumpNamespaces(nsset)
		{
            Debug.enter("dumpNamespaces",nsset.length)

			var result = ""
			if( nsset != void )
			for each( var ns in nsset )
			{
				if( result != "" )
				{
					result += ","
				}
				result += ns.@kind +':"'+ns.@name+'"'
			}

            Debug.exit("dumpNamespaces",result)

			return result
		}

        public function FindProperty(name,namespaces,is_qualified,is_attr,is_strict)
        {
            Debug.enter("FindProperty",name,namespaces,is_strict,is_qualified,is_attr)
			Debug.log_mode::log("  "+code.length+":FindProperty "+name+" {"+dumpNamespaces(namespaces)+"} is_qualified="+is_qualified+" is_strict="+is_strict,code_out)

            var index,name_index,ns_index
            
            if( name == "*" )
            {
                name_index = 0
            }
            else
            {
                name_index = ConstantUtf8(name)
            }

            if( is_qualified && namespaces.length == 1 )
            {
				var ns_name = namespaces[0].@name
				var ns_kind = namespaces[0].@kind=="internal"?CONSTANT_PackageInternalNS:CONSTANT_Namespace
                var ns_utf8_index = ConstantUtf8(ns_name)
                var ns_index = ConstantNamespace(ns_utf8_index,ns_kind)
                index = ConstantQualifiedName(name_index,ns_index,is_attr)
            }
            else
            {
                ns_index = ConstantNamespaceSet(namespaces)
                index = ConstantMultiname(name_index,ns_index,is_attr)
            }
            
            if( is_strict )
            {
                makeByte(code,OP_findpropstrict);
                makeInt32(code,index);
            }
            else
            {
                makeByte(code,OP_findproperty);
                makeInt32(code,index);
            }
            Debug.exit("FindProperty")
        }
        
        public function CallProperty(name,namespaces,size,is_qualified,is_super,is_attr,is_lex,is_new)
        {
            Debug.enter("CallProperty",name,dumpNamespaces(namespaces),size,is_qualified,is_super,is_attr,is_lex,is_new)
			Debug.log_mode::log("  "+code.length+":CallProperty "+name+" {"+dumpNamespaces(namespaces)+"} arg_count="+size+" is_new="+is_new,code_out)

            var index,name_index,ns_index
            
            if( name == "*" )
            {
                name_index = null
            }
            else
            {
                name_index = ConstantUtf8(name)
            }

            if( is_qualified && namespaces.length == 1 )
            {
				var ns_name = namespaces[0].@name
				var ns_kind = namespaces[0].@kind=="public"?CONSTANT_PackageNamespace:CONSTANT_PackageInternalNS
                var ns_utf8_index = ConstantUtf8(ns_name)
                var ns_index = ConstantNamespace(ns_utf8_index,ns_kind)
                index = ConstantQualifiedName(name_index,ns_index,is_attr)
            }
            else
            {
                ns_index = ConstantNamespaceSet(namespaces)
                index = ConstantMultiname(name_index,ns_index,is_attr)
            }
            
            if( is_super )
            {
                makeByte(code,OP_callsuper);
                makeInt32(code,index);
                makeInt32(code,size);
            }
            else
            if( is_lex )
            {
                makeByte(code,OP_callproplex);
                makeInt32(code,index);
                makeInt32(code,size);
            }
            else
            if( is_new )
            {
                makeByte(code,OP_constructproperty);
                makeInt32(code,index);
                makeInt32(code,size);
            }
            else
            {
                makeByte(code,OP_callproperty);
                makeInt32(code,index);
                makeInt32(code,size);
            }
        }
        
        public function SetProperty(name,namespaces,is_qualified,is_super,is_attr,is_constinit)
        {
            Debug.enter("SetProperty",name,namespaces,is_qualified,is_super,is_attr,is_constinit)
			Debug.log_mode::log("  "+code.length+":SetProperty "+name+" {"+dumpNamespaces(namespaces)+"} is_qualified="+is_qualified,code_out)

            var index,name_index,ns_index
            
            if( name == "*" )
            {
                name_index = 0
            }
            else
            {
                name_index = ConstantUtf8(name)
            }

            if( is_qualified && namespaces.length == 1 )
            {
				var ns_name = namespaces[0].@name
				var ns_kind = namespaces[0].@kind=="public"?CONSTANT_PackageNamespace:CONSTANT_PackageInternalNS
                var ns_utf8_index = ConstantUtf8(ns_name)
                var ns_index = ConstantNamespace(ns_utf8_index,ns_kind)
                index = ConstantQualifiedName(name_index,ns_index,is_attr)
            }
            else
            {
                ns_index = ConstantNamespaceSet(namespaces)
                index = ConstantMultiname(name_index,ns_index,is_attr)
            }
            
            if( is_super )
            {
                makeByte(code,OP_setsuper);
                makeInt32(code,index);
            }
            if( is_constinit )
            {
                makeByte(code,OP_initproperty);
                makeInt32(code,index);
            }
            else
            {
                makeByte(code,OP_setproperty);
                makeInt32(code,index);
            }
        }
        
        public function GetProperty(name,namespaces,is_qualified,is_super,is_attr)
        {
            Debug.enter("GetProperty",name,namespaces,is_qualified,is_super,is_attr)
			Debug.log_mode::log("  "+code.length+":GetProperty "+name+" {"+dumpNamespaces(namespaces)+"}",code_out)

            var index,name_index,ns_index
            
            if( name == "*" )
            {
                name_index = 0
            }
            else
            {
                name_index = ConstantUtf8(name)
            }

            if( is_qualified && namespaces.length == 1 )
            {
				var ns_name = namespaces[0].@name
				var ns_kind = namespaces[0].@kind=="public"?CONSTANT_PackageNamespace:CONSTANT_PackageInternalNS
                var ns_utf8_index = ConstantUtf8(ns_name)
                var ns_index = ConstantNamespace(ns_utf8_index,ns_kind)
                index = ConstantQualifiedName(name_index,ns_index,is_attr)
            }
            else
            {
                ns_index = ConstantNamespaceSet(namespaces)
                index = ConstantMultiname(name_index,ns_index,is_attr)
            }
            
            if( is_super )
            {
                makeByte(code,OP_getsuper)
                makeInt32(code,index)
            }
			else
			{
	            makeByte(code,OP_getproperty)
    	        makeInt32(code,index)
			}
        }
        
        public function DeleteProperty(name,namespaces,is_qualified,is_super,is_attr)
        {
            Debug.enter("DeleteProperty",name,namespaces,is_qualified,is_super,is_attr)
			Debug.log_mode::log("  "+code.length+":DeleteProperty "+name+" {"+dumpNamespaces(namespaces)+"}",code_out)

            var index,name_index,ns_index
            
            if( name == "*" )
            {
                name_index = 0
            }
            else
            {
                name_index = ConstantUtf8(name)
            }

            if( is_qualified && namespaces.length == 1 )
            {
				var ns_name = namespaces[0].@name
				var ns_kind = namespaces[0].@kind=="public"?CONSTANT_PackageNamespace:CONSTANT_PackageInternalNS
                var ns_utf8_index = ConstantUtf8(ns_name)
                var ns_index = ConstantNamespace(ns_utf8_index,ns_kind)
                index = ConstantQualifiedName(name_index,ns_index,is_attr)
            }
            else
            {
                ns_index = ConstantNamespaceSet(namespaces)
                index = ConstantMultiname(name_index,ns_index,is_attr)
            }
            
            if( is_super )
            {
            }
			else
			{
	            makeByte(code,OP_deleteproperty)
    	        makeInt32(code,index)
			}
        }
        
        public function CheckType(name)
        {
			Debug.log_mode::log("  "+code.length+":CheckType "+name,code_out)

            var fullname = name.toString();
            if ("*" == fullname)
            {
                makeByte(code,OP_coerce_a);
            }
            else 
            if ("Object" == fullname)
            {
                makeByte(code,OP_coerce_o);
            }
            else 
            if ("String" == fullname)
            {
                makeByte(code,OP_coerce_s);
            }
            else 
            if ("Boolean" == fullname)
            {
                makeByte(code,OP_coerce_b);
            }
            else 
            if ("Number" == fullname)
            {
                makeByte(code,OP_coerce_d);
            }
            else 
            if ("int" == fullname)
            {
                makeByte(code,OP_coerce_i)
            }
            else if ("uint" == fullname)
            {
                makeByte(code,OP_coerce_u)
            }
            else
            {
/*
                int type_index = AddClassName(name);
                Coerce(*ab->code,class_index);
                makeByte(code,OP_coerce)
                makeInt32(code,type_index)
*/
            }

        }
        
        public function GetSlot(n)
        {
			Debug.log_mode::log("  "+code.length+":GetSlot "+n,code_out)
            makeByte(code,OP_getslot)
            makeInt32(code,n)
        }
        
        public function SetSlot(n)
        {
			Debug.log_mode::log("  "+code.length+":SetSlot "+n,code_out)
            makeByte(code,OP_setslot)
            makeInt32(code,n)
         }
        
        public function GetLocal(n)
        {
			Debug.log_mode::log("  "+code.length+":GetLocal "+n,code_out)
            if( n <= 3 )
            {
                makeByte(code,OP_getlocal0+n)
            }
            else
            {
                makeByte(code,OP_getlocal)
                makeInt32(code,n)
            }
        }
        
        public function SetLocal(n)
        {
			Debug.log_mode::log("  "+code.length+":SetLocal "+n,code_out)
            if( n <= 3 )
            {
                makeByte(code,OP_setlocal0+n)
            }
            else
            {
                makeByte(code,OP_setlocal)
                makeInt32(code,n)
            }
        }
        
        public function FreeTemp(n)
        {
			Debug.log_mode::log("  "+code.length+":FreeTemp",code_out)
            makeByte(code,OP_kill)
            makeInt32(code,n)
        }
        
        public function Return()
        {
			Debug.log_mode::log("  "+code.length+":Return",code_out)
            makeByte(code,OP_returnvalue)
        }
        
	    public function NewFunction(method_index)
    	{
			Debug.log_mode::log("  "+code.length+":NewFunction "+method_index,code_out)
        	makeByte(code,OP_newfunction)
	        makeInt32(code,method_index)
    	}

	    public function NewActivation()
    	{
			Debug.log_mode::log("  "+code.length+":NewActivation",code_out)
        	makeByte(code,OP_newactivation)
    	}

		public function GetGlobalScope()
		{
			Debug.log_mode::log("  "+code.length+":GetGlobalScope",code_out)
        	makeByte(code,OP_getglobalscope)
		}
    
		public function GetScopeObject(n)
		{
			Debug.log_mode::log("  "+code.length+":GetScopeObject "+n,code_out)
        	makeByte(code,OP_getscopeobject)
	        makeInt32(code,n)
		}
    
		public function Call(size:uint)
		{
			Debug.log_mode::log("  "+code.length+":Call",code_out)
	        makeByte(code,OP_call)
			makeInt32(code,size)
		}
    
		function checkBytes(bytes)
		{
			var bi = 0
			var ci = code.length-bytes.length
			for( ; bi < bytes.length; bi++,ci++ )
			{
				if( bytes[bi] != 0xff && 
                    bytes[bi] != code[ci] )
				{
					return "fail: expecting "+bytes[bi]+" found "+code[ci]
				}
				else
				{
				}
			}
			return "pass"
		}

    }

	/*

	Test case for the emitter interface

	*/

	var name = "foo"
	var ns_set = [<Namespace kind="user" name="foo"/>]

	var test_cases = [
		{
			name:"GetProperty",
			args:[
				[name,ns_set,false,false,false],
				[name,ns_set,true,false,false],
				[name,ns_set,false,true,false],
				[name,ns_set,false,false,true],
			],
			bytes:[
				[0x66,0xff],
				[0x66,0xff],
				[0x04,0xff],
				[0x66,0xff],
			]
		},
		{
			name:"SetProperty",
			args:[
				[name,ns_set,false,false,false,false],
				[name,ns_set,true,false,false,false],
				[name,ns_set,false,true,false,false],
				[name,ns_set,false,false,true,false],
				[name,ns_set,false,false,false,true],
			],
			bytes:[
				[0x61,0xff],
				[0x61,0xff],
				[0x61,0xff],
				[0x61,0xff],
				[0x68,0xff],
			]
		},
		{
			name:"CallProperty",
			args:[
				[name,ns_set,0,false,false,false,false,false],
				[name,ns_set,0,true,false,false,false,false],
				[name,ns_set,0,false,true,false,false,false],
				[name,ns_set,0,false,false,true,false,false],
				[name,ns_set,0,false,false,false,true,false],
			],
			bytes:[
				[0x46,0xff,0x00],
				[0x46,0xff,0x00],
				[0x45,0xff,0x00],
				[0x46,0xff,0x00],
				[0x4c,0xff,0x00],
			]
		},
		{
			name:"Dup",
			args:[
				[],
			],
			bytes:[
				[0x2a],
			]
		},
		{
			name:"NewActivation",
			args:[
				[],
			],
			bytes:[
				[0x57],
			]
		},
	]

    public function testABCEmitter() 
    {
        var emitter = new ABCEmitter()
        emitter.StartProgram()
        emitter.StartMethod("test")

		for( var i = 0; i < test_cases.length; i++ )
		{
			testInstruction(emitter,test_cases[i].name,test_cases[i].args,test_cases[i].bytes)
		}
    }
    
		function testInstruction(emitter,name,args_list,bytes_list)
		{
			Debug.debug::log(name)
			for( var i = 0; i < args_list.length; i++ )
			{
				var args = args_list[i]
				var bytes = bytes_list[i]
				switch( args.length )
				{
					case 0:
					emitter[name].apply(emitter,args)
					break
					case 5:
					emitter[name](args[0],args[1],args[2],args[3],args[4])
					break
					case 6:
					emitter[name](args[0],args[1],args[2],args[3],args[4],args[5])
					break
					case 7:
					emitter[name](args[0],args[1],args[2],args[3],args[4],args[5],args[6])
					break
					case 8:
					emitter[name](args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7])
					break
					default:
					print("arg number not implemented " + args.length)
				}
				print("  "+i+":"+emitter.checkBytes(bytes))
			}
		}

}
