using System;
using Mozilla.XPCOM;
using System.Reflection;
using System.Reflection.Emit;
using System.Runtime.InteropServices;

namespace Mozilla.XPCOM {

public class Components
{
    [DllImport("xpcom-dotnet.so")]
    static extern int StartXPCOM(out IntPtr srvmgr);

    static IntPtr serviceManager = IntPtr.Zero;

    public static IntPtr ServiceManager {
        get {
            return serviceManager;
        }
    }

    static void RegisterAppDomainHooks()
    {
        AppDomain.CurrentDomain.TypeResolve += 
            new ResolveEventHandler(TypeResolve);
        AppDomain.CurrentDomain.AssemblyResolve +=
            new ResolveEventHandler(AssemblyResolve);
        Console.WriteLine("XPCOM AppDomain hooks registered.");
    }

    static Assembly AssemblyResolve(object sender, ResolveEventArgs args)
    {
        Console.WriteLine("Resolving: {0}", args.Name);
        return null;
    }

    public static AssemblyBuilder ProxyAssembly {
        get {
            return ProxyGenerator.ProxyAssembly;
        }
    }

    static Assembly TypeResolve(object sender, ResolveEventArgs args)
    {
        if (args.Name.StartsWith("Mozilla.XPCOM.Proxies.")) {
            ProxyGenerator gen = new ProxyGenerator(args.Name);
            return gen.Generate();
        }
        
#if NOTYET
        if (args.Name.StartsWith("Mozilla.XPCOM.Interfaces.")) {
            InterfaceGenerator gen = new InterfaceGenerator(args.Name);
            return gen.Generate();
        }
#endif

        return null;
    }

    public static void Init()
    {
        if (serviceManager != IntPtr.Zero)
            return;

        int res = StartXPCOM(out serviceManager);
        if (res != 0) {
            throw new Exception(String.Format("StartXPCOM failed: {0:X2}",
                                              res));
        }

        RegisterAppDomainHooks();
    }

    [DllImport("test.so", EntryPoint="GetImpl")]
    public static extern IntPtr GetTestImpl();

    public static object CreateInstance(string contractId, Type iface)
    {
        String typeName = iface.FullName;
        String proxyName = typeName.Replace(".Interfaces.", ".Proxies.");
        Console.WriteLine("Need proxy class {0} for {1}", proxyName, typeName);
        Type proxyType = System.Type.GetType(proxyName);
        if (contractId == "@off.net/test-component;1") {
            ConstructorInfo ctor = 
                proxyType.GetConstructor(BindingFlags.NonPublic |
                                         BindingFlags.Instance,
                                         null, new Type[1] { typeof(IntPtr) },
                                         null);
            return ctor.Invoke(new object[] { GetTestImpl() });
        }
        return null;
    }
}

}
