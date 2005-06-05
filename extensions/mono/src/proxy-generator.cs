using System;
using System.Runtime.InteropServices;
using System.Reflection;
using System.Reflection.Emit;
using Mozilla.XPCOM;
using MethodDescriptor = Mozilla.XPCOM.TypeInfo.MethodDescriptor;
using TypeTag = Mozilla.XPCOM.TypeInfo.TypeTag;
using ParamFlags = Mozilla.XPCOM.TypeInfo.ParamFlags;

namespace Mozilla.XPCOM
{

public class BaseProxy
{
    protected IntPtr thisptr;
    protected BaseProxy(IntPtr ptr) { thisptr = ptr; }
}

class ProxyGenerator
{
    void EmitPtrAndFlagsStore(int argnum, IntPtr ptr, sbyte flags)
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

    void EmitTypeStore(TypeInfo.TypeDescriptor t, int argnum)
    {
        ilg.Emit(OpCodes.Ldloc, bufLocal);
        ilg.Emit(OpCodes.Ldc_I4, argnum * VARIANT_SIZE + 12);
        ilg.Emit(OpCodes.Add);
        ilg.Emit(OpCodes.Ldc_I4, (Int32)t.tag);
        ilg.Emit(OpCodes.Stind_I4);
    }

    void EmitComputeBufferLoc(int argnum)
    {
        ilg.Emit(OpCodes.Ldloc, bufLocal);
        ilg.Emit(OpCodes.Ldc_I4, argnum * VARIANT_SIZE);
        ilg.Emit(OpCodes.Add);
    }

    void EmitPrepareArgStore(int argnum)
    {
        EmitComputeBufferLoc(argnum);
        EmitLoadArg(argnum);
    }

    void EmitLoadArg(int argnum)
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

    void EmitLoadReturnSlot_1(int slotnum)
    {
        ilg.Emit(OpCodes.Ldloc, bufLocal);
        ilg.Emit(OpCodes.Ldc_I4, (slotnum - 1) * VARIANT_SIZE);
        ilg.Emit(OpCodes.Add);
        ilg.Emit(OpCodes.Ldind_I4);
    }

    void EmitOutParamPrep(TypeInfo.TypeDescriptor type, int argnum)
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

    void EmitProxyConstructor()
    {
        ConstructorBuilder ctor = 
            tb.DefineConstructor(MethodAttributes.Family,
                                 CallingConventions.Standard,
                                 new Type[1] { typeof(IntPtr) });
        ILGenerator ilg = ctor.GetILGenerator();
        ilg.Emit(OpCodes.Ldarg_0);
        ilg.Emit(OpCodes.Ldarg_1);
        ilg.Emit(OpCodes.Call, baseProxyCtor);
        ilg.Emit(OpCodes.Ret);
    }

    const int VARIANT_SIZE = 16; // sizeof(XPTCVariant)

    const PropertyAttributes PROPERTY_ATTRS = PropertyAttributes.None;

    PropertyBuilder lastProperty;

    Type FixupInterfaceType(TypeInfo.ParamDescriptor desc)
    {
        try {
            return TypeInfo.TypeForIID(desc.GetIID());
        } catch (Exception e) {
            // Console.WriteLine(e);
            return typeof(object);
        }
    }

    unsafe void GenerateProxyMethod(MethodDescriptor desc)
    {
        if (!desc.IsVisible()) {
            Console.WriteLine("HIDDEN: {0}", desc);
            return;
        }
        MethodAttributes methodAttrs = 
            MethodAttributes.Public | MethodAttributes.Virtual;

        String methodName = desc.name;
        if (desc.IsGetter()) {
            methodName = "get_" + desc.name;
            methodAttrs |= MethodAttributes.SpecialName;
        } else if (desc.IsSetter()) {
            methodName = "set_" + desc.name;
            methodAttrs |= MethodAttributes.SpecialName;
        }
            
        // Fix up interface types in parameters
        Type ret = desc.resultType;
        if (ret == typeof(object))
            ret = FixupInterfaceType(desc.args[desc.args.Length - 1]);
        Type[] argTypes = (Type[])desc.argTypes.Clone();
        for (int i = 0; i < argTypes.Length; i++) {
            if (argTypes[i] == typeof(object))
                argTypes[i] = FixupInterfaceType(desc.args[i]);
        }
        MethodBuilder meth = tb.DefineMethod(methodName, methodAttrs, ret, argTypes);

        ilg = meth.GetILGenerator();
        bufLocal = ilg.DeclareLocal(System.Type.GetType("System.Int32*"));
        LocalBuilder guidLocal = ilg.DeclareLocal(typeof(System.Guid));
        TypeInfo.ParamDescriptor[] args = desc.args;

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
            EmitTypeStore(type, i);

            if (param.IsOut()) {
                EmitOutParamPrep(type, i);
                if (!param.IsIn())
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
                EmitPrepareArgStore(i);
                // XXX do I need to cast this?
                ilg.Emit(OpCodes.Castclass, typeof(Int32));
                ilg.Emit(OpCodes.Stind_I4);
                break;
            case TypeTag.Int32:
                EmitPrepareArgStore(i);
                ilg.Emit(OpCodes.Stind_I4);
                break;
            case TypeTag.NSIdPtr:
                EmitPrepareArgStore(i);
                ilg.Emit(OpCodes.Stind_I4); // XXX 64-bitness
                break;
            case TypeTag.String:
                EmitPrepareArgStore(i);
                // the string arg is now on the stack
                ilg.Emit(OpCodes.Call,
                         marshalType.GetMethod("StringToCoTaskMemAnsi"));
                ilg.Emit(OpCodes.Stind_I4);
                break;
            case TypeTag.Interface:
                EmitPrepareArgStore(i);
                // MRP is the object passed as this arg
                ilg.Emit(OpCodes.Ldloca_S, guidLocal);
                ilg.Emit(OpCodes.Ldstr, param.GetIID().ToString());
                ilg.Emit(OpCodes.Call, guidCtor);
                ilg.Emit(OpCodes.Ldloca_S, guidLocal);
                // stack is now objarg, ref guid
                ilg.Emit(OpCodes.Call, typeof(CLRWrapper).GetMethod("Wrap"));
                // now stack has the IntPtr in position to be stored.
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
            EmitPtrAndFlagsStore(i, ptr, flags);
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
            EmitLoadReturnSlot_1(args.Length);
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

        ilg = null;
        bufLocal = null;

        if (desc.IsSetter()) {
            if (lastProperty != null && lastProperty.Name == desc.name) {
                lastProperty.SetSetMethod(meth);
            } else {
                tb.DefineProperty(desc.name, PROPERTY_ATTRS, desc.resultType,
                                  new Type[0]).SetSetMethod(meth);
            }
            lastProperty = null;
        } else if (desc.IsGetter()) {
            lastProperty = tb.DefineProperty(desc.name, PROPERTY_ATTRS,
                                             desc.resultType, new Type[0]);
            lastProperty.SetGetMethod(meth);
        } else {
            lastProperty = null;
        }

    }

    static ModuleBuilder module;
    static AssemblyBuilder builder;
    static ConstructorInfo baseProxyCtor;
    static ConstructorInfo guidCtor;

    internal static AssemblyBuilder ProxyAssembly {
        get { return builder; }
    }

    static ProxyGenerator()
    {
        AssemblyName an = new AssemblyName();
        an.Version = new Version(1, 0, 0, 0);
        an.Name = "Mozilla.XPCOM.Proxies";
        
        AppDomain curDom = AppDomain.CurrentDomain;
        builder = curDom.DefineDynamicAssembly(an, AssemblyBuilderAccess.RunAndSave);
        
        String proxyDll =
            System.Environment.GetEnvironmentVariable("XPCOM_DOTNET_SAVE_PROXIES");
        if (proxyDll != null) {
            module = builder.DefineDynamicModule(an.Name, proxyDll);
        } else {
            module = builder.DefineDynamicModule(an.Name);
        }

        baseProxyCtor = typeof(BaseProxy).
            GetConstructor(BindingFlags.NonPublic | BindingFlags.Instance,
                           null, new Type[1] { typeof(IntPtr) }, null);
        guidCtor = typeof(System.Guid).
            GetConstructor(new Type[1] {typeof(string)});
    }

    TypeBuilder tb;
    FieldInfo thisField;
    LocalBuilder bufLocal;
    ILGenerator ilg;

    String proxyName;

    internal ProxyGenerator(String name)
    {
        proxyName = name;
    }

    internal Assembly Generate()
    {
        if (module.GetType(proxyName) != null)
            return module.Assembly;

        String baseIfaceName = proxyName.Replace("Mozilla.XPCOM.Proxies.", "");
        String ifaceName = "Mozilla.XPCOM.Interfaces." + baseIfaceName;

        Type ifaceType = System.Type.GetType(ifaceName + 
                                             ",Mozilla.XPCOM.Interfaces", true);

        ushort inheritedMethodCount;
        String parentName = TypeInfo.GetParentInfo(baseIfaceName,
                                                   out inheritedMethodCount);

        Type parentType;
        if (parentName == null) {
            parentType = typeof(BaseProxy);
        } else {
            parentType = System.Type.GetType("Mozilla.XPCOM.Proxies." +
                                             parentName +
                                             ",Mozilla.XPCOM.Proxies");
        }

        Console.WriteLine("Defining {0} (inherits {1}, impls {2})",
                          proxyName, parentType, ifaceName);
        tb = module.DefineType(proxyName, TypeAttributes.Class, parentType,
                               new Type[1] { ifaceType });
        
        thisField = typeof(BaseProxy).GetField("thisptr",
                                               BindingFlags.NonPublic |
                                               BindingFlags.Instance);

        EmitProxyConstructor();
        
        MethodDescriptor[] descs = TypeInfo.GetMethodData(baseIfaceName);

        for (int i = inheritedMethodCount; i < descs.Length; i++) {
            if (descs[i] != null) 
                GenerateProxyMethod(descs[i]);
        }

        tb.CreateType();

        thisField = null;
        tb = null;

        return module.Assembly;
    }
}

}
