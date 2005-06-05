using System;
using System.Runtime.InteropServices;
using Mozilla.XPCOM;

namespace Mozilla.XPCOM {

/* This doesn't need to be public, but making it 'internal' causes stress with
 * TypeDescriptor.op_Implicit(XPTType). */
[StructLayout(LayoutKind.Sequential)]
public struct XPTType
{
    public const byte FlagMask = 0xE0;
    public const byte TagMask = 0x1F;
    public byte   prefix;
    public byte   arg1;
    public byte   arg2;
    public byte   padding;
    public UInt16 arg3;

    static public implicit operator XPTType(TypeInfo.TypeDescriptor td)
    {
        XPTType t = new XPTType();
        t.prefix = (byte) ((byte)td.flags | (byte)td.tag);
        t.arg1 = td.arg1;
        t.arg2 = td.arg2;
        t.arg3 = td.arg3;
        return t;
    }
}

[StructLayout(LayoutKind.Sequential)]
struct XPTParamDescriptor
{
    public byte    param_flags;
    public byte    padding1;
    public XPTType type;
}

[StructLayout(LayoutKind.Sequential,CharSet=CharSet.Ansi)]
struct XPTMethodDescriptor
{
    public String name;
    public IntPtr args; /* XPTParamDescriptor */
    public IntPtr result; /* XPTParamDescriptor */
    public byte   flags;
    public byte   numArgs;
}

public class TypeInfo
{
    [Flags]
    public enum TypeFlags : byte
    {
        Pointer = 0x80, UniquePointer = 0x40, Reference = 0x20
    }
    
    public enum TypeTag : byte
    {
        Int8 = 0, Int16 = 1, Int32 = 2, Int64 = 3,
        UInt8 = 4, UInt16 = 5, UInt32 = 6, UInt64 = 7,
        Float = 8, Double = 9, Bool = 10, Char = 11,
        WChar = 12, Void = 13, NSIdPtr = 14, AString = 15,
        String = 16, WString = 17, Interface = 18, InterfaceIs = 19,
        Array = 20, StringSizeIs = 21, WStringSizeIs = 22, UTF8String = 23,
        CString = 24, AString_dup = 25
    }

    public struct TypeDescriptor
    {
        public TypeFlags flags;
        public TypeTag   tag;
        public byte      arg1;
        public byte      arg2;
        public UInt16    arg3;
        
        public override string ToString()
        {
            string res = String.Empty;
            if ((Int32)flags != 0)
                res = "[" + flags.ToString() + "]";
            res += tag.ToString();
            return res;
        }

        public bool IsScalar()
        {
            if (tag <= TypeTag.WChar ||
                tag == TypeTag.String || tag == TypeTag.WString) {
                return true;
            }
            return false;
        }

        static public implicit operator TypeDescriptor(XPTType t)
        {
            TypeDescriptor td = new TypeInfo.TypeDescriptor();
            td.flags = (TypeFlags)(t.prefix & XPTType.FlagMask);
            td.tag = (TypeTag)(t.prefix & XPTType.TagMask);
            td.arg1 = t.arg1;
            td.arg2 = t.arg2;
            td.arg3 = t.arg3;
            return td;
        }

        public Type AsCLRType()
        {
            switch (tag) {
            case TypeTag.Int8:
                return typeof(sbyte);
            case TypeTag.Int16:
                return typeof(Int16);
            case TypeTag.Int32:
                return typeof(Int32);
            case TypeTag.Int64:
                return typeof(Int64);

            case TypeTag.UInt8:
                return typeof(byte);
            case TypeTag.UInt16:
                return typeof(UInt16);
            case TypeTag.UInt32:
                return typeof(UInt32);
            case TypeTag.UInt64:
                return typeof(UInt64);

            case TypeTag.Float:
                return typeof(Single);
            case TypeTag.Double:
                return typeof(Double);
            
            case TypeTag.Bool:
                return typeof(bool);

            case TypeTag.String:
                return typeof(string);
            case TypeTag.WString:
                return typeof(string);

            case TypeTag.Interface:
                return typeof(object);
            case TypeTag.InterfaceIs:
                return typeof(object);

            case TypeTag.Char:
                return typeof(byte);
            case TypeTag.WChar:
                return typeof(char);

            case TypeTag.NSIdPtr:
                return Type.GetType("System.Guid&");
                // return typeof(Guid).MakeByRefType(); NET_2_0

            default:
                string msg = String.Format("type {0} not yet supported ",
                                           tag.ToString());
                
                throw new Exception(msg);
            }
        }
    }

    [Flags]
    public enum ParamFlags : byte
    {
        In = 0x80, Out = 0x40, RetVal = 0x20, Shared = 0x10, Dipper = 0x08
    }
    
    public struct ParamDescriptor
    {
        public MethodDescriptor method;
        public int               index;
        public ParamFlags        flags;
        public TypeDescriptor     type;

        public Type GetCLRType()
        {
            Type t = type.AsCLRType();
            if (IsOut() && !IsRetVal()) {
                Type reft = System.Type.GetType(t.FullName + "&");
                Console.WriteLine("{0} -> {1}", t.FullName, reft.FullName);
                t = reft;
            }
            return t;
        }

        public override string ToString()
        {
            return "[" + flags.ToString() + "] " + type.ToString();
        }

        public string Name()
        {
            return method.Name() + ":" + index;
        }

        public bool IsOut()
        {
            return (flags & ParamFlags.Out) != 0;
        }

        public bool IsIn()
        {
            return (flags & ParamFlags.In) != 0;
        }

        public bool IsRetVal()
        {
            return (flags & ParamFlags.RetVal) != 0;
        }

        public Guid GetIID()
        {
            if (type.tag != TypeTag.Interface) {
                throw new Exception(String.Format("{0} not an interface type",
                                                  this));
            }

            Guid iid;
            int res;
            res = typeinfo_GetIIDForParam(method.ifaceName, method.index,
                                          index, out iid);
            if (res != 0) {
                throw new 
                    Exception(String.Format("GetIIDForParam failed: {0:X8}",
                                            res));
            }
            return iid;
        }

        public string GetInterfaceName()
        {
            return TypeInfo.NameForIID(GetIID());
        }
    }
    
    [Flags]
    public enum MethodFlags : byte
    {
        Getter = 0x80, Setter = 0x40, NotXPCOM = 0x20, Constructor = 0x10,
        Hidden = 0x08
    }

    public class MethodDescriptor
    {
        public int               index;
        public String            name;
        public ParamDescriptor[] args;
        public ParamDescriptor   result;
        public Type[]            argTypes;
        public Type              resultType;
        public MethodFlags       flags;
        public String            ifaceName;

        public bool IsVisible()
        {
            return (flags & (MethodFlags.Hidden | MethodFlags.NotXPCOM)) == 0;
        }

        public bool IsSetter()
        {
            return (flags & MethodFlags.Setter) != 0;
        }

        public bool IsGetter()
        {
            return (flags & MethodFlags.Getter) != 0;
        }

        public string Name()
        {
            return ifaceName + ":" + name + "(" + index + ")";
        }

        public Type FindResultType()
        {
            for (int i = 0; i < args.Length; i++) {
                if (args[i].IsRetVal())
                    return args[i].GetCLRType();
            }
            return typeof(void);
        }

        public void BuildArgTypes()
        {
            if (!IsVisible()) {
                argTypes = new Type[] { };
                return;
            }

            if (resultType != typeof(void))
                argTypes = new Type[args.Length - 1];
            else
                argTypes = new Type[args.Length];
            int j = 0;
            for (int i = 0; i < args.Length; i++) {
                if (args[i].IsRetVal())
                    continue;
                argTypes[j++] = args[i].GetCLRType();
            }
        }
        
        public override string ToString()
        {
            string res = String.Empty;
            if (flags != 0)
                res = "[" + flags.ToString() + "] ";
            res += result.ToString() + " " + name + "(";
            for (int i = 0; i < args.Length; i++) {
                if (i != 0)
                    res += ", ";
                res += args[i].ToString();
            }
            res += ")";
            return res;
        }
    }

    [DllImport("xpcom-dotnet.so")]
    static extern int typeinfo_GetAllMethodData(String iface,
                                                out IntPtr infos,
                                                out UInt16 count);

    [DllImport("xpcom-dotnet.so")]
    static extern int typeinfo_GetMethodData(String iface, int method,
                                             out IntPtr info);


    [DllImport("xpcom-dotnet.so")]
    static extern int typeinfo_GetMethodData_byname(String iface,
                                                    String method,
                                                    out UInt16 index,
                                                    out IntPtr info);

    [DllImport("xpcom-dotnet.so")]
    static extern int typeinfo_GetMethodData_iid_index(ref Guid iid, int index,
                                                       out IntPtr xptinfo);

    [DllImport("xpcom-dotnet.so")]
    static extern int typeinfo_GetIIDForParam(String iface, int method,
                                              int param, out Guid iid);

    [DllImport("xpcom-dotnet.so")]
    static extern int typeinfo_GetNameForIID(ref Guid iid, out String name);

    [DllImport("xpcom-dotnet.so")]
    public static extern IntPtr typeinfo_EnumerateInterfacesStart();

    [DllImport("xpcom-dotnet.so")]
    public static extern String typeinfo_EnumerateInterfacesNext(IntPtr e);

    [DllImport("xpcom-dotnet.so")]
    public static extern void typeinfo_EnumerateInterfacesStop(IntPtr e);

    [DllImport("xpcom-dotnet.so")]
    public static extern int typeinfo_GetParentInfo(String name,
                                                    out String parentName,
                                                    out UInt16 methodCount);

    public static String GetParentInfo(String name, out UInt16 methodCount)
    {
        String parentName;
        int res = typeinfo_GetParentInfo(name, out parentName,
                                         out methodCount);
        if (res != 0) {
            throw new Exception(String.Format("GetParentInfo({0}) " +
                                              "failed: {1:X8}", name, res));
        }
        return parentName;
    }
    
    static ParamDescriptor BuildParamDescriptor(IntPtr paramptr, int index,
                                                MethodDescriptor method)
    {
        XPTParamDescriptor xptpd = (XPTParamDescriptor)
            Marshal.PtrToStructure(paramptr, typeof(XPTParamDescriptor));
        ParamDescriptor pd = new ParamDescriptor();
        pd.method = method;
        pd.index = index;
        pd.flags = (ParamFlags) xptpd.param_flags;
        pd.type = xptpd.type;
        return pd;
    }

    static ParamDescriptor BuildRetValDescriptor(IntPtr paramptr,
                                                 MethodDescriptor method)
    {
        return BuildParamDescriptor(paramptr, -1, method);
    }

    static ParamDescriptor[] BuildParamDescriptorArray(IntPtr paramptr,
                                                       int count,
                                                       MethodDescriptor method)
    {
        ParamDescriptor[] parr = new ParamDescriptor[count];
        
        for (int i = 0; i < count; i++) {
            parr[i] = BuildParamDescriptor(paramptr, i, method);
            paramptr = (IntPtr)(paramptr.ToInt32() + 8);
        }
        return parr;
    }
    
    static MethodDescriptor ConvertMethodDescriptor(IntPtr xptinfo,
                                                    string ifaceName,
                                                    int index)
    {
        XPTMethodDescriptor info;
        info = (XPTMethodDescriptor)
            Marshal.PtrToStructure(xptinfo, typeof(XPTMethodDescriptor));
        MethodDescriptor meth = new MethodDescriptor();
        meth.ifaceName = ifaceName;
        meth.index = index;
        meth.name = System.Char.ToUpper(info.name[0]) + info.name.Substring(1);
        meth.flags = (MethodFlags)info.flags;
        meth.args = BuildParamDescriptorArray(info.args, info.numArgs, meth);
        meth.result = BuildRetValDescriptor(info.result, meth);
        if (meth.IsVisible()) {
            meth.resultType = meth.FindResultType();
            meth.BuildArgTypes();
        }
        return meth;
    }

    static public MethodDescriptor[] GetMethodData(string ifaceName)
    {
        IntPtr buffer;
        UInt16 count;

        int res = typeinfo_GetAllMethodData(ifaceName, out buffer, out count);
        if (res != 0) {
            throw new Exception(String.Format("GetAllMethodData({0}) " +
                                              "failed: {1:X8}",
                                              ifaceName, res));
        }            

        MethodDescriptor[] methods = new MethodDescriptor[count];

        for (int i = 0; i < (UInt16)count; i++) {
            IntPtr infoptr = Marshal.ReadIntPtr(buffer, i * 
                                                Marshal.SizeOf(typeof(IntPtr)));
            try {
                methods[i] = ConvertMethodDescriptor(infoptr, ifaceName, i);
            } catch (Exception e) {
                Console.WriteLine("skipping {0}[{1}]: {2}",
                                  ifaceName, i, e.Message);
            }
        }

        if (count > 0)
            Marshal.FreeCoTaskMem(buffer);

        return methods;
    }

    static public MethodDescriptor GetMethodData(Guid iid, int index)
    {
        IntPtr xptinfo;
        int res = typeinfo_GetMethodData_iid_index(ref iid, index,
                                                   out xptinfo);
        if (res != 0) {
            throw new Exception(String.Format("GetMethodData({0}.{1}) " +
                                              "failed: {2:X8}",
                                              iid, index, res));
        }

        String name;
        res = typeinfo_GetNameForIID(ref iid, out name);
        if (res != 0) {
            throw new Exception(String.Format("GetNameForIID failed: {0:X8}",
                                              res));
        }
        return ConvertMethodDescriptor(xptinfo, name, index);
    }

    static public String NameForIID(Guid iid)
    {
        String name;
        int res = typeinfo_GetNameForIID(ref iid, out name);
        if (res != 0) {
            throw new 
                Exception(String.Format("GetNameForIID failed: {0:X8}",
                                        res));
        }
        return name;
    }

    static public Type TypeForIID(Guid iid)
    {
        String ifaceName = "Mozilla.XPCOM.Interfaces." + NameForIID(iid) +
            ",Mozilla.XPCOM.Interfaces";
        return System.Type.GetType(ifaceName);
    }

    static public MethodDescriptor GetMethodData(string ifaceName,
                                                 string methodName)
    {
        IntPtr xptinfo;
        UInt16 index;
        int res = typeinfo_GetMethodData_byname(ifaceName, methodName,
                                                out index, out xptinfo);

        if (res != 0) {
            throw new Exception(String.Format("GetMethodData({0}.{1}) " +
                                              "failed: {2:X8}",
                                              ifaceName, methodName, res));
        }

        // Console.WriteLine("{0} is index {1}", methodName, index);

        return ConvertMethodDescriptor(xptinfo, ifaceName, (int)index);
    }

    static public MethodDescriptor GetMethodData(String ifaceName,
                                                 int methodIndex)
    {
        IntPtr xptinfo;
        
        int res = typeinfo_GetMethodData(ifaceName, methodIndex, out xptinfo);

        if (xptinfo == IntPtr.Zero) {
            throw new Exception(String.Format("GetMethodData({0}:{1}) failed:" +
                                              " {2:X8}",
                                              ifaceName, methodIndex, res));
        }

        return ConvertMethodDescriptor(xptinfo, ifaceName, methodIndex);
    }
}

} // namespace Mozilla.XPCOM
