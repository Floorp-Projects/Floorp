using System;
using System.Runtime.InteropServices;
using System.Reflection;
using System.Reflection.Emit;
using Mozilla.XPCOM;
using MethodDescriptor = Mozilla.XPCOM.TypeInfo.MethodDescriptor;
using TypeTag = Mozilla.XPCOM.TypeInfo.TypeTag;
using ParamFlags = Mozilla.XPCOM.TypeInfo.ParamFlags;

public class Test
{
    [DllImport("xpcom-dotnet.so")]
    static extern int StartXPCOM(out IntPtr srvmgr);
    
    static IntPtr srvmgr;

    [DllImport("test.so", EntryPoint="GetImpl")]
    public static extern IntPtr GetTestImpl();

    static void GenerateInterfaceMethod(TypeBuilder tb, MethodDescriptor desc)
    {
        if (!desc.IsVisible()) {
            Console.WriteLine("HIDDEN: {0}", desc);
            return;
        }
        const MethodAttributes attrs = MethodAttributes.Public |
            MethodAttributes.Abstract | MethodAttributes.Virtual;
        tb.DefineMethod(desc.name, attrs, desc.resultType, desc.argTypes);
        Console.WriteLine("\t{0}", desc);
    }

    static void EmitPtrAndFlagsStore(ILGenerator ilg, LocalBuilder bufLocal,
                                     int argnum, IntPtr ptr, sbyte flags)
    {
        //= bufLocal[argnum].ptr = ptr;
        ilg.Emit(OpCodes.Ldloc, bufLocal);
        ilg.Emit(OpCodes.Ldc_I4, argnum * VARIANT_SIZE + 8);
        ilg.Emit(OpCodes.Add);
        ilg.Emit(OpCodes.Ldc_I4, ptr.ToInt32());
        ilg.Emit(OpCodes.Stind_I4);

        //= bufLocal[argnum].flags = flags;
        ilg.Emit(OpCodes.Ldloc, bufLocal);
        ilg.Emit(OpCodes.Ldc_I4, argnum * VARIANT_SIZE + 13);
        ilg.Emit(OpCodes.Add);
        ilg.Emit(OpCodes.Ldc_I4, (Int32)flags);
        ilg.Emit(OpCodes.Stind_I1);
    }

    static void EmitTypeStore(ILGenerator ilg, LocalBuilder bufLocal,
                              TypeInfo.TypeDescriptor t, int argnum)
    {
        ilg.Emit(OpCodes.Ldloc, bufLocal);
        ilg.Emit(OpCodes.Ldc_I4, argnum * VARIANT_SIZE + 12);
        ilg.Emit(OpCodes.Add);
        ilg.Emit(OpCodes.Ldc_I4, (Int32)t.tag);
        ilg.Emit(OpCodes.Stind_I4);
    }

    static void EmitComputeBufferLoc(ILGenerator ilg, LocalBuilder bufLocal,
                                     int argnum)
    {
        ilg.Emit(OpCodes.Ldloc, bufLocal);
        ilg.Emit(OpCodes.Ldc_I4, argnum * VARIANT_SIZE);
        ilg.Emit(OpCodes.Add);
    }

    static void EmitPrepareArgStore(ILGenerator ilg, LocalBuilder bufLocal,
                                    int argnum)
    {
        EmitComputeBufferLoc(ilg, bufLocal, argnum);
        EmitLoadArg(ilg, argnum);
    }

    static void EmitLoadArg(ILGenerator ilg, int argnum)
    {
        switch (argnum) {
        case 0:
            ilg.Emit(OpCodes.Ldarg_1);
            break;
        case 1:
            ilg.Emit(OpCodes.Ldarg_2);
            break;
        case 2:
            ilg.Emit(OpCodes.Ldarg_3);
            break;
        default:
            if (argnum < 254)
                ilg.Emit(OpCodes.Ldarg_S, argnum + 1);
            else
                ilg.Emit(OpCodes.Ldarg, argnum + 1);
            break;
        }
    }

    static void EmitLoadReturnSlot_1(ILGenerator ilg, LocalBuilder bufLocal,
                                     int slotnum)
    {
        ilg.Emit(OpCodes.Ldloc, bufLocal);
        ilg.Emit(OpCodes.Ldc_I4, (slotnum - 1) * VARIANT_SIZE);
        ilg.Emit(OpCodes.Add);
        ilg.Emit(OpCodes.Ldind_I4);
    }

    static void EmitOutParamPrep(ILGenerator ilg, LocalBuilder bufLocal,
                                 TypeInfo.TypeDescriptor type, int argnum)
    {
        ilg.Emit(OpCodes.Nop);
        ilg.Emit(OpCodes.Ldloc, bufLocal);
        ilg.Emit(OpCodes.Ldc_I4, argnum * VARIANT_SIZE + 13);
        ilg.Emit(OpCodes.Add);
        ilg.Emit(OpCodes.Ldc_I4, 1); // PTR_IS_DATA
        ilg.Emit(OpCodes.Stind_I1);
        
        ilg.Emit(OpCodes.Ldloc, bufLocal);
        ilg.Emit(OpCodes.Ldc_I4, argnum * VARIANT_SIZE + 8); // offsetof(ptr)
        ilg.Emit(OpCodes.Add);
        ilg.Emit(OpCodes.Ldloc, bufLocal);
        ilg.Emit(OpCodes.Ldc_I4, argnum * VARIANT_SIZE + 0); // offsetof(val)
        ilg.Emit(OpCodes.Add);
        ilg.Emit(OpCodes.Stind_I4); /* XXX 64-bitness! */
    }

    static void EmitProxyConstructor(TypeBuilder tb, FieldInfo thisField)
    {
        ConstructorBuilder ctor = 
            tb.DefineConstructor(MethodAttributes.Public,
                                 CallingConventions.Standard,
                                 new Type[1] { typeof(IntPtr) });
        ILGenerator ilg = ctor.GetILGenerator();
        ilg.Emit(OpCodes.Ldarg_0);
        ilg.Emit(OpCodes.Call, typeof(object).GetConstructor(new Type[0]));
        ilg.Emit(OpCodes.Ldarg_0);
        ilg.Emit(OpCodes.Ldarg_1);
        ilg.Emit(OpCodes.Stfld, thisField);
        ilg.Emit(OpCodes.Ret);
    }

    const int VARIANT_SIZE = 16; /* sizeof(XPTCVariant) */
    
    unsafe static void GenerateProxyMethod(TypeBuilder tb,
                                           MethodDescriptor desc,
                                           FieldInfo thisField)
    {
        if (!desc.IsVisible()) {
            Console.WriteLine("HIDDEN: {0}", desc);
            return;
        }
        const MethodAttributes attrs = 
            MethodAttributes.Public | MethodAttributes.Virtual;
        Type ret = desc.resultType;
        MethodBuilder meth =
            tb.DefineMethod(desc.name, attrs, ret, desc.argTypes);
        ILGenerator ilg = meth.GetILGenerator();
        TypeInfo.ParamDescriptor[] args = desc.args;
        
        LocalBuilder bufLocal = 
            ilg.DeclareLocal(System.Type.GetType("System.Int32*"));

        Type marshalType = typeof(System.Runtime.InteropServices.Marshal);

        // Marshal.AllocCoTaskMem(constify(argBufSize))
        int argCount = args.Length;
        int argBufSize = VARIANT_SIZE * args.Length;

        ilg.Emit(OpCodes.Ldc_I4, argBufSize);
        ilg.Emit(OpCodes.Call, marshalType.GetMethod("AllocCoTaskMem"));
        ilg.Emit(OpCodes.Stloc, bufLocal);

        for (int i = 0; i < argCount; i++) {
            TypeInfo.ParamDescriptor param = args[i];
            TypeInfo.TypeDescriptor type = param.type;
            IntPtr ptr = IntPtr.Zero;
            sbyte flags = 0;
            EmitTypeStore(ilg, bufLocal, type, i);

            if ((param.flags & ParamFlags.Out) != 0) {
                EmitOutParamPrep(ilg, bufLocal, type, i);
                continue;
            }
            switch (type.tag) {
            case TypeTag.Int8:
            case TypeTag.Int16:
            case TypeTag.UInt8:
            case TypeTag.UInt16:
            case TypeTag.Char:
            case TypeTag.WChar:
            case TypeTag.UInt32:
                EmitPrepareArgStore(ilg, bufLocal, i);
                // XXX do I need to cast this?
                ilg.Emit(OpCodes.Castclass, typeof(Int32));
                ilg.Emit(OpCodes.Stind_I4);
                break;
            case TypeTag.Int32:
                EmitPrepareArgStore(ilg, bufLocal, i);
                ilg.Emit(OpCodes.Stind_I4);
                break;
            case TypeTag.String:
                EmitPrepareArgStore(ilg, bufLocal, i);
                // the string arg is now on the stack
                ilg.Emit(OpCodes.Call,
                         marshalType.GetMethod("StringToCoTaskMemAnsi"));
                ilg.Emit(OpCodes.Stind_I4);
                break;
            default:
                /*
                String msg = String.Format("{0}: type {1} not supported",
                                    param.Name(), type.tag.ToString());
                throw new Exception(msg);
                */
                break;
            }
            EmitPtrAndFlagsStore(ilg, bufLocal, i, ptr, flags);
        }

        //= (void)XPTC_InvokeByIndex(thisptr, desc.index, length, bufLocal);
        ilg.Emit(OpCodes.Ldarg_0);
        ilg.Emit(OpCodes.Ldfld, thisField);
        ilg.Emit(OpCodes.Ldc_I4, desc.index);
        ilg.Emit(OpCodes.Ldc_I4, args.Length);
        ilg.Emit(OpCodes.Ldloc_0);
        ilg.Emit(OpCodes.Call, typeof(Mozilla.XPCOM.Invoker).
                 GetMethod("XPTC_InvokeByIndex",
                           BindingFlags.Static | BindingFlags.NonPublic));
        ilg.Emit(OpCodes.Pop);

        if (ret == typeof(string)) {
            ilg.Emit(OpCodes.Ldstr, "FAKE RETURN STRING");
        } else if (ret == typeof(object)) {
            ilg.Emit(OpCodes.Newobj,
                     typeof(object).GetConstructor(new Type[0]));
        } else if (ret == typeof(int)) {
            EmitLoadReturnSlot_1(ilg, bufLocal, args.Length);
        } else if (ret == typeof(void)) {
            // Nothing
        } else {
            throw new Exception(String.Format("return type {0} not " +
                                              "supported yet",
                                              desc.result.type.tag));
        }

        //= Marshal.FreeCoTaskMem(bufLocal);
        ilg.Emit(OpCodes.Ldloc, bufLocal);
        ilg.Emit(OpCodes.Call, marshalType.GetMethod("FreeCoTaskMem"));

        ilg.Emit(OpCodes.Ret);
        Console.WriteLine("$\t{0}", desc);
    }

    public static void Main(string[] args)
    {
        int res = StartXPCOM(out srvmgr);

        if (res != 0) {
            Console.WriteLine("StartXPCOM failed: {0:X2}", res);
            return;
        }

        string ifaceName = args[0];

        MethodDescriptor[] descs = TypeInfo.GetMethodData(ifaceName);
        Console.WriteLine("Interface {0}:", ifaceName);
        
        AssemblyName an = new AssemblyName();
        an.Version = new Version(1, 0, 0, 0);
        an.Name = "Mozilla.XPCOM.Interfaces." + ifaceName;

        AppDomain currentDomain = AppDomain.CurrentDomain;

        AssemblyBuilderAccess access;
        if (args.Length > 1)
            access = AssemblyBuilderAccess.RunAndSave;
        else
            access = AssemblyBuilderAccess.Run;
        AssemblyBuilder ab = currentDomain.DefineDynamicAssembly(an, access);
        
        ModuleBuilder mb;
        if (args.Length > 1)
            mb = ab.DefineDynamicModule(an.Name, args[1]);
        else
            mb = ab.DefineDynamicModule(an.Name);

        TypeBuilder ifaceTb = mb.DefineType(ifaceName, (TypeAttributes.Public |
                                                   TypeAttributes.Interface));

        for (int i = 3; i < descs.Length; i++) {
            GenerateInterfaceMethod(ifaceTb, descs[i]);
        }

        ifaceTb.CreateType();

        TypeBuilder proxyTb = mb.DefineType(ifaceName + "$Proxy",
                                            (TypeAttributes.Class),
                                            typeof(object),
                                            new Type[1] { ifaceTb } );
        FieldBuilder thisField = proxyTb.DefineField("this", typeof(IntPtr),
                                                     FieldAttributes.Private);
        EmitProxyConstructor(proxyTb, thisField);

        for (int i = 3; i < descs.Length; i++) {
            GenerateProxyMethod(proxyTb, descs[i], thisField);
        }

        if (args.Length > 1)
            ab.Save(args[1]);

        Type proxyType = proxyTb.CreateType();
        Console.WriteLine("proxyType: {0}", proxyType);
        ConstructorInfo proxyCtor = 
            proxyType.GetConstructor(new Type[1] { typeof(IntPtr) });
        Console.WriteLine("proxyCtor: {0}", proxyCtor);

        IntPtr impl = GetTestImpl();
        Console.WriteLine("proxyThis: {0:X2}", impl.ToInt32());
        object proxy = proxyCtor.Invoke(new object[] { impl });

        MethodInfo proxyAdd = proxyType.GetMethod("add");
        Console.WriteLine("proxyAdd: {0}", proxyAdd);
        object proxyRet = proxyAdd.Invoke(proxy, new object[] { 3, 5 });
        Console.WriteLine("proxyRet: {0}", (int)proxyRet);

        MethodInfo proxySay = proxyType.GetMethod("say");
        Console.WriteLine("proxySay: {0}", proxySay);
        proxySay.Invoke(proxy, new object[] { "holy cow!" });

        PropertyInfo proxyIntProp = proxyType.GetProperty("intProp");
        Console.WriteLine("proxyIntProp: {0}", proxyIntProp);
        Console.WriteLine("proxyIntProp(get): {0}",
                          proxyIntProp.GetValue(proxy, null));
        proxyIntProp.SetValue(proxy, 31337, null);
        Console.WriteLine("proxyIntProp(get): {0}",
                          proxyIntProp.GetValue(proxy, null));
    }
}
