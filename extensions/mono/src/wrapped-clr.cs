using System;
using System.Reflection;
using System.Runtime.InteropServices;
using Mozilla.XPCOM;

using MethodDescriptor = Mozilla.XPCOM.TypeInfo.MethodDescriptor;
using ParamDescriptor = Mozilla.XPCOM.TypeInfo.ParamDescriptor;
using ParamFlags = Mozilla.XPCOM.TypeInfo.ParamFlags;
using TypeDescriptor = Mozilla.XPCOM.TypeInfo.TypeDescriptor;
using TypeFlags = Mozilla.XPCOM.TypeInfo.TypeFlags;
using TypeTag = Mozilla.XPCOM.TypeInfo.TypeTag;

namespace Mozilla.XPCOM {
internal class CLRWrapper
{
    public static IntPtr Wrap(object o, ref Guid iid)
    {
        CLRWrapper wrapper = new CLRWrapper(o, ref iid);
        return wrapper.MakeXPCOMProxy();
    }
    
    int InvokeMethod(int index, IntPtr args)
    {
        Console.WriteLine("invoking method {0} on {1}", index,
                          wrappedObj);
        MethodDescriptor desc = TypeInfo.GetMethodData(wrappedAsIID, index);
        Type ifaceType = TypeInfo.TypeForIID(wrappedAsIID);
        // Console.WriteLine("ifaceType: {0}", ifaceType);
        MethodInfo meth = ifaceType.GetMethod(desc.name);
        // Console.WriteLine("meth: {0}", meth);
        // This might just throw on you, if it's not a void-taking method
        meth.Invoke(wrappedObj, new object[0]);
        return 0;
    }
    
    CLRWrapper(object o, ref Guid iid)
    {
        wrappedObj = o;
        wrappedAsIID = iid;
    }
    
    object wrappedObj;
    Guid wrappedAsIID;
    
    delegate int MethodInvoker(int index, IntPtr args);
    
    [DllImport("xpcom-dotnet.so")]
        static extern IntPtr WrapCLRObject(MethodInvoker del, ref Guid iid);
    
    IntPtr MakeXPCOMProxy()
    {
        return WrapCLRObject(new MethodInvoker(this.InvokeMethod),
                             ref wrappedAsIID);
    }
}

} // namespace Mozilla.XPCOM
