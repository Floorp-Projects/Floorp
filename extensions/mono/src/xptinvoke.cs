using System;
using System.Runtime.InteropServices;
using Mozilla.XPCOM;
using System.Globalization;

using MethodDescriptor = Mozilla.XPCOM.TypeInfo.MethodDescriptor;
using ParamDescriptor = Mozilla.XPCOM.TypeInfo.ParamDescriptor;
using ParamFlags = Mozilla.XPCOM.TypeInfo.ParamFlags;
using TypeDescriptor = Mozilla.XPCOM.TypeInfo.TypeDescriptor;
using TypeFlags = Mozilla.XPCOM.TypeInfo.TypeFlags;
using TypeTag = Mozilla.XPCOM.TypeInfo.TypeTag;

namespace Mozilla.XPCOM {

[StructLayout(LayoutKind.Explicit)]
struct XPTCMiniVariant
{
    [FieldOffset(0)]
    public sbyte   i8;
    [FieldOffset(0)]
    public Int16  i16;
    [FieldOffset(0)]
    public Int32  i32;
    [FieldOffset(0)]
    public Int64  i64;
    [FieldOffset(0)]
    public byte    u8;
    [FieldOffset(0)]
    public UInt16 u16;
    [FieldOffset(0)]
    public UInt32 u32;
    [FieldOffset(0)]
    public UInt64 u64;
    [FieldOffset(0)]
    public float    f;
    [FieldOffset(0)]
    public double   d;
    [FieldOffset(0)]
    public Int32    b; /* PRBool */
    [FieldOffset(0)]
    public byte     c;
    [FieldOffset(0)]
    public char    wc;
    [FieldOffset(0)]
    public IntPtr   p;
    [FieldOffset(0),MarshalAs(UnmanagedType.LPStr)]
    public string str;
}

[StructLayout(LayoutKind.Explicit)]
struct XPTCVariant
{
    [FieldOffset(0)]
    public XPTCMiniVariant val;
    
    [FieldOffset(8)]
    public IntPtr          ptr;
    
    [FieldOffset(12)] /* XXX assumes 4-byte IntPtr! */
    public XPTType         type;
    
    [FieldOffset(13)]
    public sbyte          flags;
}

public class Invoker
{
    static void MarshalOneArg(ParamDescriptor param, object arg,
                              IntPtr buffer)
    {
        string msg;
        if (param.flags != TypeInfo.ParamFlags.In) {
            msg = String.Format("{0} is {1} (only In " +
                              "supported)", param.Name(),
                              param.flags.ToString());
            throw new Exception(msg);
        }

        TypeInfo.TypeDescriptor type = param.type;
        
        if ((type.flags & TypeFlags.Reference) != 0) {
            if ((type.flags & TypeFlags.Pointer) == 0) {
                throw new Exception("TD is Reference but " +
                                    "not Pointer?! (" +
                                    param.ToString() + ")");
            }
            
            if (arg == null) {
                throw new Exception(param.Name() +
                                    ": null passed as arg for " + 
                                    "Reference param");
            }
        }

        if (type.IsScalar()) {
            
            XPTCVariant variant = new XPTCVariant();
            variant.type = type;
            variant.flags = 0;
            variant.ptr = IntPtr.Zero;
            Marshal.StructureToPtr(variant, buffer, false);

            IntPtr p;
            switch (type.tag) {

            case TypeTag.Int8:
            case TypeTag.Int16:
            case TypeTag.Int32:
            case TypeTag.UInt8:
            case TypeTag.UInt16:
            case TypeTag.UInt32:
            case TypeTag.Char:
            case TypeTag.WChar:
                Marshal.WriteInt32(buffer, (Int32)arg);
                break;

            case TypeTag.UInt64:
            case TypeTag.Int64:
                Marshal.WriteInt64(buffer, (Int64)arg);
                break;

            case TypeTag.Bool:
                bool b = (bool)arg;
                Marshal.WriteInt32(buffer, b ? 1 : 0);
                break;

            case TypeTag.Float:
                float[] f = new float[] { (float)arg };
                Marshal.Copy(f, 0, buffer, 1);
                break;
            case TypeTag.Double:
                double[] d = new double[] { (double)arg };
                Marshal.Copy(d, 0, buffer, 1);
                break;

            case TypeTag.String:
                Marshal.WriteIntPtr(buffer, 
                            Marshal.StringToCoTaskMemAnsi((string)arg));
                break;
            case TypeTag.WString:
                Marshal.WriteIntPtr(buffer, 
                            Marshal.StringToCoTaskMemUni((string)arg));
                break;

            default:
                msg = String.Format("{0}: type {1} not supported",
                                    param.Name(), type.tag.ToString());
                throw new Exception(msg);
            }

            Console.WriteLine("{0} @ {1:X2}", param.Name(),
                              buffer.ToInt32());
            return;
        }

        if (type.tag == TypeTag.Interface) {
            Guid iid = param.GetIID();
            Console.WriteLine("{0} is interface {1}",
                              param.Name(), iid);
            Marshal.WriteIntPtr(buffer, CLRWrapper.Wrap(arg, ref iid));
            Console.WriteLine("{0} @ {1:X2}", param.Name(),
                              buffer.ToInt32());
            return;
        }

        msg = String.Format("{0} type {1} not yet supported ",
                            param.Name(), type.tag.ToString());
        throw new Exception(msg);
    }

    [DllImport("xpcom-dotnet.so")]
    public static extern IntPtr WrapCLRObject(IntPtr obj, Guid id);

    [DllImport("xpcom-dotnet.so")]
    static extern IntPtr typeinfo_WrapUnicode(IntPtr str,
                                              UInt32 length);

    [DllImport("xpcom-dotnet.so")]
    static extern void typeinfo_FreeWrappedUnicode(IntPtr str);

    static IntPtr MarshalArgs(MethodDescriptor desc, object[] args)
    {
        if (args.Length != desc.args.Length) {
            string msg = String.Format("{0} needs {1} args, {2} passed",
                                       desc.name, desc.args.Length, 
                                       args.Length);
            throw new Exception(msg);
        }

        int variantsz = 16; /* sizeof(nsXPTCVariant) */
        int size = variantsz * args.Length;
        IntPtr argbuf = Marshal.AllocCoTaskMem(size);

        for (int i = 0; i < args.Length; i++) {
            ParamDescriptor param = desc.args[i];
            IntPtr current = (IntPtr)(argbuf.ToInt32() + variantsz * i);
            object arg = args[i];

            MarshalOneArg(param, arg, current);
        }

        return argbuf;
    }

    static void FreeOneMarshalledArg(ParamDescriptor param, IntPtr buffer)
    {
    }

    static void DemarshalArgs(MethodDescriptor desc,
                              IntPtr argbuf)
    {
        int variantsz = Marshal.SizeOf(typeof(XPTCVariant));
        for (int i = 0; i < desc.args.Length; i++) {
            ParamDescriptor param = desc.args[i];
            IntPtr current = (IntPtr)(argbuf.ToInt32() + variantsz * i);
            FreeOneMarshalledArg(param, current);
        }
        Marshal.FreeCoTaskMem(argbuf);
    }

    public static int XPTC_InvokeByIndexSafe(IntPtr that, Int32 index,
                                             UInt32 argCount, IntPtr args)
    {
        Console.WriteLine("XPTC_IBI: {0:X2}:{1}({2}:{3:X2})",
                          that.ToInt32(), index, argCount, args.ToInt32());
        return XPTC_InvokeByIndex(that, index, argCount, args);
    }

    [DllImport("libxpcom.so")]
    static extern int XPTC_InvokeByIndex(IntPtr that, Int32 methodIndex,
                                         UInt32 argCount,
                                         IntPtr args);

    public static object Invoke(IntPtr that, string iface,
                                string method, params object[] args)
    {
        return Invoke(that, TypeInfo.GetMethodData(iface, method), args);
    }

    public static object Invoke(IntPtr that, string iface,
                                int method, params object[] args)
    {
        return Invoke(that, TypeInfo.GetMethodData(iface, method), args);
    }

    public static object Invoke(IntPtr that, MethodDescriptor desc,
                                params object[] args)
    {
        IntPtr argbuf = MarshalArgs(desc, args);
        int res = XPTC_InvokeByIndex(that, desc.index,
                                     (UInt32)args.Length, argbuf);
        DemarshalArgs(desc, argbuf);
        if (res != 0) {
            throw new Exception(String.Format("XPCOM Error: {0:X2}",
                                              res));
        }
        return null;
    }
}

} // namespace Mozilla.XPCOM
