// test the nsIScriptableInterfaceInfo stuff...
// We ought to leverage this in SOAP code.

const IInfo = new Components.Constructor("@mozilla.org/scriptableInterfaceInfo;1",
                                         "nsIScriptableInterfaceInfo",
                                         "init");

const nsIDataType = Components.interfaces.nsIDataType;
const nsISupports = Components.interfaces.nsISupports;

const PARAM_NAME_PREFIX = "arg";

/**
* takes:   {xxx}
* returns: uuid(xxx)
*/
function formatID(str) 
{
    return "uuid("+str.substring(1,str.length-1)+")";
}

function formatConstType(type)
{
    switch(type) 
    {
        case nsIDataType.VTYPE_INT16:
            return "PRInt16";
        case nsIDataType.VTYPE_INT32:
            return "PRInt32";
        case nsIDataType.VTYPE_UINT16:
            return "PRUint16";
        case nsIDataType.VTYPE_UINT32:
            return "PRUint32";
        default:
            return "!!!! bad const type !!!";
    }
}

function formatTypeName(info, methodIndex, param, dimension) 
{
    var type = param.type;
     
    switch(type.dataType)
    {
        case nsIDataType.VTYPE_INT8 :
            return "PRInt8";
        case nsIDataType.VTYPE_INT16:
            return "PRInt16";
        case nsIDataType.VTYPE_INT32:
            return "PRInt32";
        case nsIDataType.VTYPE_INT64:
            return "PRInt64";
        case nsIDataType.VTYPE_UINT8:
            return "PRUint8";
        case nsIDataType.VTYPE_UINT16:
            return "PRUint16";
        case nsIDataType.VTYPE_UINT32:
            return "PRUint32";
        case nsIDataType.VTYPE_UINT64:
            return "PRUint64";
        case nsIDataType.VTYPE_FLOAT:
            return "float";
        case nsIDataType.VTYPE_DOUBLE:
            return "double";
        case nsIDataType.VTYPE_BOOL:
            return "PRBool";
        case nsIDataType.VTYPE_CHAR:
            return "char";
        case nsIDataType.VTYPE_WCHAR:
            return "PRUnichar";
        case nsIDataType.VTYPE_VOID:
            if(type.isPointer)
                return "voidPtr";
            return "void";
        case nsIDataType.VTYPE_ID:
            // order matters here...
            if(type.isReference)
                return "nsIDRef";
            if(type.isPointer)
                return "nsIDPtr";
            return "nsID";
        case nsIDataType.VTYPE_ASTRING:
            return "aString";
        case nsIDataType.VTYPE_CHAR_STR:
            return "string";
        case nsIDataType.VTYPE_WCHAR_STR:
            return "wstring";
        case nsIDataType.VTYPE_INTERFACE:
            return info.getInfoForParam(methodIndex, param).name;
        case nsIDataType.VTYPE_INTERFACE_IS:
            return "nsQIResult";
        case nsIDataType.VTYPE_ARRAY:
            return formatTypeName(info, methodIndex, param, dimension+1); 
        case nsIDataType.VTYPE_STRING_SIZE_IS:
            return "string";
        case nsIDataType.VTYPE_WSTRING_SIZE_IS:
            return "wstring";
        default:
            return "!!!! bad data type !!!";
    }
}

function formatBracketForParam(info, methodIndex, param)
{
    var type = param.type;
    var str = "[";
    var found = 0;
    var size_is_ArgNum;
    var length_is_ArgNum;

    if(param.isRetval)
    {
        if(found++)    
            str += ", ";
        str += "retval"
    }

    if(param.isShared)
    {
        if(found++)    
            str += ", ";
        str += "shared"
    }

    if(type.isArray)
    {
        if(found++)    
            str += ", ";
        str += "array"
    }

    if(type.dataType == nsIDataType.VTYPE_INTERFACE_IS)
    {
        if(found++)    
            str += ", ";
        str += "iid_is("+ 
                 PARAM_NAME_PREFIX + 
                 info.getInterfaceIsArgNumberForParam(methodIndex, param) + ")";
    }

    if(type.isArray ||
       type.dataType == nsIDataType.VTYPE_STRING_SIZE_IS ||
       type.dataType == nsIDataType.VTYPE_WSTRING_SIZE_IS)
    {
        if(found++)    
            str += ", ";

        size_is_ArgNum =
            info.getSizeIsArgNumberForParam(methodIndex, param, 0);

        str += "size_is("+ PARAM_NAME_PREFIX + size_is_ArgNum + ")";

        length_is_ArgNum =
            info.getLengthIsArgNumberForParam(methodIndex, param, 0);

        if(length_is_ArgNum != size_is_ArgNum)
        {
            str += ", ";
            str += "length_is("+ PARAM_NAME_PREFIX + length_is_ArgNum + ")";
        }
    }

    if(!found)
        return "";
    str += "] ";
    return str
}

function doInterface(iid)
{
    var i, k, t, m, m2, c, p, readonly, bracketed, paramCount, retvalTypeName;

    var info = new IInfo(iid);
    var parent = info.parent;

    var constBaseIndex = parent ? parent.constantCount : 0;
    var constTotalCount =  info.constantCount;
    var constLocalCount = constTotalCount - constBaseIndex;
    
    var methodBaseIndex = parent ? parent.methodCount : 0;
    var methodTotalCount =  info.methodCount;
    var methodLocalCount = methodTotalCount - methodBaseIndex;

    
    // maybe recurring to emit parent declarations is not a good idea...
//    if(parent)
//        doInterface(parent.interfaceID);

    print();
    
    // comment out nsISupports
//    if(iid.equals(nsISupports))
//        print("/*\n");

    print("[" + (info.isScriptable ? "scriptable, " : "") +
           formatID(info.interfaceID.number) + "]");

    print("interface "+ info.name + (parent ? (" : "+parent.name) : "") + " {");

    if(constLocalCount)
    {
        for(i = constBaseIndex; i < constTotalCount; i++)
        {
            c = info.getConstant(i);
            print("  const " + formatConstType(c.type.dataType) + " " +
                  c.name + " = " + c.value + ";");
        }
    }    

    if(constLocalCount && methodLocalCount)
        print();

    if(methodLocalCount)
    {
        for(i = methodBaseIndex; i < methodTotalCount; i++)
        {
            m = info.getMethodInfo(i);

            if(m.isNotXPCOM)
                bracketed = "[notxpcom] " 
            else if(m.isHidden)
                bracketed = "[noscript] " 
            else
                bracketed = "";

            if(m.isGetter)
            {
                // Is an attribute

                // figure out if this is readonly
                m2 = i+1 < methodTotalCount ? info.getMethodInfo(i+1) : null;
                readonly = !m2 || m2.name != m.name;

                print("  " + bracketed + (readonly ? "readonly " : "") +
                      "attribute " +
                      formatTypeName(info, i, m.getParam(0), 0) +
                      " " + m.name + ";\n");

                if(!readonly)
                    i++; // skip the next one, we have it covered.    
            
                continue;
            }
            // else...

            paramCount = m.paramCount;

            // 'last' param is used to figure out retvalTypeName

            p = paramCount ? m.getParam(paramCount-1) : null;

            if(m.isNotXPCOM)
                retvalTypeName = formatTypeName(info, i, m.result, 0);
            else if(p && "[retval] " == formatBracketForParam(info, i, p))
            {
                // Check for the exact string above because anything else
                // indicates that there either is no expilict retval
                // or there are additional braketed attributes (meaning that
                // the retval must appear in the param list and not
                // preceeding the method name).
                retvalTypeName = formatTypeName(info, i, p, 0);
                // no need to print it in the param list
                paramCount-- ;
            }       
            else    
                retvalTypeName = "void";
            
            // print method name

            print("  " + bracketed + retvalTypeName + " " + m.name + "(" +
                  (paramCount ? "" : ");"));

            // print params

            for(k = 0; k < paramCount; k++)
            {
                p = m.getParam(k);
                print("              "+ 
                      formatBracketForParam(info, i, p) +
                      (p.isOut ? p.isIn ? "inout " : "out " : "in ") +
                      formatTypeName(info, i, p, 0) + " " +
                      PARAM_NAME_PREFIX+k +
                      (k+1 == paramCount ? ");\n" : ", "));
            }
        }
    }

    print("};\n");

    // comment out nsISupports
//    if(iid.equals(nsISupports))
//        print("\n*/\n");
}

function appendForwardDeclarations(list, info)
{
    list.push(info.name);
    if(info.parent)
        appendForwardDeclarations(list, info.parent);

    var i, k, m, p;

    for(i = 0; i < info.methodCount; i++)
    {
        m = info.getMethodInfo(i);
        
        for(k = 0; k < m.paramCount; k++)
        {
            p = m.getParam(k);
            
            if(p.type.dataType == nsIDataType.VTYPE_INTERFACE)
            {
                list.push(info.getInfoForParam(i, p).name);
            }
        }
    }
}    

function doForwardDeclarations(iid)
{
    var i, cur, prev;
    var list = [];
    appendForwardDeclarations(list, new IInfo(iid));
    list.sort();
    
    print("// forward declarations...");

    for(i = 0; i < list.length; i++) 
    {
        cur = list[i];
        if(cur != prev && cur != "nsISupports") 
        {
            print("interface " + cur +";");
            prev = cur;    
        }    
    }
    print("");
}    

function info2IDL(iid)
{
    print();
    print('#include "nsISupports.idl"');
    print();

    doForwardDeclarations(iid)
    doInterface(iid);
}

/***************************************************************************/
/***************************************************************************/
// test...

print();
print();
//info2IDL(Components.interfaces.nsISupports);
print("//------------------------------------------------------------");
info2IDL(Components.interfaces.nsIDataType);
print("//------------------------------------------------------------");
info2IDL(Components.interfaces.nsIScriptableInterfaceInfo);
print("//------------------------------------------------------------");
info2IDL(Components.interfaces.nsIVariant);
print();
print();
