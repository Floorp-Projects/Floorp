using System;
using System.Runtime.InteropServices;
using Mozilla.XPCOM;
using MethodDescriptor = Mozilla.XPCOM.TypeInfo.MethodDescriptor;

public class Test
{
    [DllImport("xpcom-dotnet.so")]
    static extern int StartXPCOM(out IntPtr srvmgr);
    
    static IntPtr srvmgr;

    [DllImport("test.so", EntryPoint="GetImpl")]
    public static extern IntPtr GetTestImpl();

    public static int xptinfo_test(string[] args)
    {
        int index = Int32.Parse(args[2]);
        MethodDescriptor meth = TypeInfo.GetMethodData(args[1], index);
        Console.WriteLine("{0}#{1}: {2}", args[1], index, meth.ToString());
        return 0;
    }

    public static int xptinvoke_test_cb()
    {
        object o = new object();

        IntPtr impl = GetTestImpl();
        Invoker.Invoke(impl, "test", "callback", o);
        return 0;
    }

    public static int xptinvoke_test_add(string[] args)
    {
        int a = Int32.Parse(args[1]);
        int b = Int32.Parse(args[2]);

        IntPtr impl = GetTestImpl();
        Invoker.Invoke(impl, "test", "add", a, b);
        return 0;
    }

    public static int Main(string[] args)
    {
        int res = StartXPCOM(out srvmgr);

        if (res != 0) {
            Console.WriteLine("StartXPCOM failed: {0:X2}", res);
            return 1;
        }

        if (args[0] == "add")
            return xptinvoke_test_add(args);
        if (args[0] == "xptinfo")
            return xptinfo_test(args);
        if (args[0] == "cb")
            return xptinvoke_test_cb();
        Console.WriteLine("Unknown test mode: {0}", args[0]);
        return 1;
    }
}
