/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/* The nsInterfaceInfoToIDL implementation.*/


/***************************************************************************/
// nsInterfaceInfoToIDL part...

const IInfo = new Components.Constructor("@mozilla.org/scriptableInterfaceInfo;1",
                                         "nsIScriptableInterfaceInfo",
                                         "init");

const nsIDataType = Components.interfaces.nsIDataType;
const nsISupports = Components.interfaces.nsISupports;

const PARAM_NAME_PREFIX = "arg";
const MISSING_INTERFACE = "@";  // flag to indicate missing interfaceinfo

// helpers...

/********************************************************/
// accumulate a string with a stream-ish abstraction

function Buffer() {
    this.buffer = "";
}

Buffer.prototype = {
    write : function(s)  {
        if(s)
            this.buffer += s;
    },

    writeln : function(s)  {
        if(s)
            this.buffer += s + "\n";
        else
            this.buffer += "\n";
    }
}

/********************************************************/

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
    var type = info.getTypeForParam(methodIndex, param, dimension);

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
        case nsIDataType.VTYPE_DOMSTRING:
            return "DOMString";
        case nsIDataType.VTYPE_CHAR_STR:
            return "string";
        case nsIDataType.VTYPE_WCHAR_STR:
            return "wstring";
        case nsIDataType.VTYPE_INTERFACE:
            try  {
                return info.getInfoForParam(methodIndex, param).name;
            } catch(e) {
                return "/*!!! missing interface info, guessing!!!*/ nsISupports";
            }
        case nsIDataType.VTYPE_INTERFACE_IS:
            return "nsQIResult";
        case nsIDataType.VTYPE_ARRAY:
            return formatTypeName(info, methodIndex, param, dimension+1);
        case nsIDataType.VTYPE_STRING_SIZE_IS:
            return "string";
        case nsIDataType.VTYPE_WSTRING_SIZE_IS:
            return "wstring";
        case nsIDataType.VTYPE_UTF8STRING:
            return "AUTF8String";
        case nsIDataType.VTYPE_CSTRING:
            return "ACString";
        case nsIDataType.VTYPE_ASTRING:
            return "AString";
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

function doInterface(out, iid)
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

    out.writeln();

    // comment out nsISupports
//    if(iid.equals(nsISupports))
//        out.writeln("/*\n");

    out.writeln("[" + (info.isScriptable ? "scriptable, " : "") +
                formatID(info.interfaceID.number) + "]");

    out.writeln("interface "+ info.name +
                   (parent ? (" : "+parent.name) : "") + " {");

    if(constLocalCount)
    {
        for(i = constBaseIndex; i < constTotalCount; i++)
        {
            c = info.getConstant(i);
            out.writeln("  const " + formatConstType(c.type.dataType) + " " +
                        c.name + " = " + c.value + ";");
        }
    }

    if(constLocalCount && methodLocalCount)
        out.writeln();

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

                out.writeln("  " + bracketed + (readonly ? "readonly " : "") +
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

            out.writeln("  " + bracketed + retvalTypeName + " " + m.name +
                        "(" + (paramCount ? "" : ");"));

            // print params

            for(k = 0; k < paramCount; k++)
            {
                p = m.getParam(k);
                out.writeln("              "+
                            formatBracketForParam(info, i, p) +
                            (p.isOut ? p.isIn ? "inout " : "out " : "in ") +
                            formatTypeName(info, i, p, 0) + " " +
                            PARAM_NAME_PREFIX+k +
                            (k+1 == paramCount ? ");\n" : ", "));
            }
        }
    }

    out.writeln("};\n");

    // comment out nsISupports
//    if(iid.equals(nsISupports))
//        out.writeln("\n*/\n");
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
                var name;
                try {
                    name = info.getInfoForParam(i, p).name;
                } catch(e)  {
                    name = MISSING_INTERFACE;
                }
                list.push(name);
            }
        }
    }
}

function doForwardDeclarations(out, iid)
{
    var i, cur, prev;
    var list = [];
    appendForwardDeclarations(list, new IInfo(iid));
    list.sort();

    out.writeln("// forward declarations...");

    for(i = 0; i < list.length; i++)
    {
        cur = list[i];
        if(cur != prev && cur != "nsISupports")
        {
            if(cur == MISSING_INTERFACE)
                out.writeln("/*\n * !!! Unable to find details for a declared "+
                            "interface (name unknown)!!!\n */");
            else
                out.writeln("interface " + cur +";");
            prev = cur;
        }
    }
}

function buildForwardDeclarationsList(iid)
{
    var i, cur, prev;
    var list = [];
    var outList = [];
    appendForwardDeclarations(list, new IInfo(iid));
    list.sort();

    for(i = 0; i < list.length; i++)
    {
        cur = list[i];
        if(cur != prev && cur != "nsISupports")
        {
            if(cur != MISSING_INTERFACE)
                outList.push(cur);
            prev = cur;
        }
    }
    return outList;
}

/*********************************************************/

/* Our Componenent ctor */
function nsInterfaceInfoToIDL() {}

/* decorate prototype to provide ``class'' methods and property accessors */
nsInterfaceInfoToIDL.prototype =
{
    // nsIInterfaceInfoToIDL methods...

    // string generateIDL(in nsIIDRef aIID,
    //                    in PRBool withIncludes,
    //                    in PRBool withForwardDeclarations);
    generateIDL : function(aIID, withIncludes, withForwardDeclarations) {
        var out = new Buffer;
        out.writeln();

        if(withIncludes)
        {
            out.writeln('#include "nsISupports.idl"');
            out.writeln();
        }

        if(withForwardDeclarations)
        {
            doForwardDeclarations(out, aIID);
            out.writeln("");
        }

        doInterface(out, aIID);

        return out.buffer;
    },

    // void getReferencedInterfaceNames(in nsIIDRef aIID,
    //                                  out PRUint32 aArrayLength,
    //                                  [retval, array, size_is(aArrayLength)]
    //                                  out string aNames);

    getReferencedInterfaceNames : function(aIID, aArrayLength) {
        var list = buildForwardDeclarationsList(aIID);
        aArrayLength.value = list.length;
        return list;
    },

    // nsISupports methods...
    QueryInterface: function (iid) {
        if (iid.equals(Components.interfaces.nsIInterfaceInfoToIDL) ||
            iid.equals(Components.interfaces.nsISupports))
            return this;

        Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
        return null;
    }
}

/***************************************************************************/
/***************************************************************************/
// parts specific to my use of the the generic module code...

const MODULE_NAME        = "nsInterfaceInfoToIDL";
const MODULE_CONTRACT_ID = "@mozilla.org/interfaceinfotoidl;1";
const MODULE_CID         = "{47d98974-a1b7-46a6-bc99-8abc374bba3f}";
const MODULE_CTOR        = nsInterfaceInfoToIDL;

/***************************************************************************/
/***************************************************************************/
// generic nsIModule part...

function NSGetModule(compMgr, fileSpec) {
    return new GenericModule(MODULE_NAME, MODULE_CONTRACT_ID,
                             MODULE_CID, MODULE_CTOR);
}

function GenericModule (name, contractID, CID, ctor) {
    this.name       = name;
    this.contractID = contractID;
    this.CID        = Components.ID(CID);
    this.ctor       = ctor;
}

GenericModule.prototype = {
    /*
     * RegisterSelf is called at registration time (component installation
     * or the only-until-release startup autoregistration) and is responsible
     * for notifying the component manager of all components implemented in
     * this module.  The fileSpec, location and type parameters are mostly
     * opaque, and should be passed on to the registerComponent call
     * unmolested.
     */
    registerSelf: function (compMgr, fileSpec, location, type) {
        compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);
        compMgr.registerFactoryLocation(this.CID, this.name, this.contractID,
                                        fileSpec, location, type);
    },

    /*
     * The GetClassObject method is responsible for producing Factory and
     * SingletonFactory objects (the latter are specialized for services).
     */
    getClassObject: function (compMgr, cid, iid) {
        if (!cid.equals(this.CID))
            throw Components.results.NS_ERROR_NO_INTERFACE;

        if (!iid.equals(Components.interfaces.nsIFactory))
            throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

        this.myFactory.ctor = this.ctor;
        return this.myFactory;
    },

    /* factory object */
    myFactory: {

        /*
         * Construct an instance of the interface specified by iid, possibly
         * aggregating it with the provided outer.  (If you don't know what
         * aggregation is all about, you don't need to.  It reduces even the
         * mightiest of XPCOM warriors to snivelling cowards.)
         */
        createInstance: function (outer, iid) {
            if (outer != null)
                throw Components.results.NS_ERROR_NO_AGGREGATION;
            return (new this.ctor()).QueryInterface(iid);
        }
    },

    /*
     * The canUnload method signals that the component is about to be unloaded.
     * C++ components can return false to indicate that they don't wish to be
     * unloaded, but the return value from JS components' canUnload is ignored:
     * mark-and-sweep will keep everything around until it's no longer in use,
     * making unconditional ``unload'' safe.
     *
     * You still need to provide a (likely useless) canUnload method, though:
     * it's part of the nsIModule interface contract, and the JS loader _will_
     * call it.
     */
    canUnload: function(compMgr) {
        return true;
    }
}

/***************************************************************************/
