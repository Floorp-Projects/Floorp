using System;
using Mozilla.XPCOM;
using System.Reflection;
using System.Reflection.Emit;
using System.Collections;

public class Generate
{
    ModuleBuilder mb;
    Hashtable ifaceTable = new Hashtable();
    bool verbose = false;
    PropertyBuilder lastProperty = null;
    const PropertyAttributes PROPERTY_ATTRS = PropertyAttributes.None;
    const MethodAttributes METHOD_ATTRS = MethodAttributes.Public |
            MethodAttributes.Abstract | MethodAttributes.Virtual |
            MethodAttributes.NewSlot;

    Type FixupInterfaceType(TypeInfo.ParamDescriptor desc)
    {
        try {
            String ifaceName = desc.GetInterfaceName();
            return EmitOneInterface(ifaceName);
        } catch (Exception e) {
            Console.WriteLine(e);
            return typeof(object);
        }
    }

    void GenerateInterfaceMethod(TypeBuilder tb,
                                 TypeInfo.MethodDescriptor desc)
    {
        if (desc == null || !desc.IsVisible())
            return;

        MethodAttributes methodAttrs = METHOD_ATTRS;

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
        MethodBuilder meth = tb.DefineMethod(methodName, methodAttrs, ret,
                                             argTypes);

        if (desc.IsSetter()) {
            if (lastProperty != null && lastProperty.Name == desc.name) {
                lastProperty.SetSetMethod(meth);
            } else {
                tb.DefineProperty(desc.name, PROPERTY_ATTRS, ret, new Type[0])
                    .SetSetMethod(meth);
            }
            lastProperty = null;
        } else if (desc.IsGetter()) {
            lastProperty = tb.DefineProperty(desc.name, PROPERTY_ATTRS, ret,
                                             new Type[0]);
            lastProperty.SetGetMethod(meth);
        } else {
            lastProperty = null;
        }
    }

    Type EmitOneInterface(String name)
    {
        if (ifaceTable.ContainsKey(name))
            return (Type)ifaceTable[name];
        Console.WriteLine("Interface: {0}", name);
        
        ushort inheritedMethodCount;
        String parentName = TypeInfo.GetParentInfo(name,
                                                   out inheritedMethodCount);
        Type parentType;

        if (parentName == null)
            parentType = typeof(object);
        else
            parentType = EmitOneInterface(parentName);

        Console.WriteLine ("Emitting: " + name + " : " + parentType);

        TypeBuilder ifaceTb = mb.DefineType("Mozilla.XPCOM.Interfaces." + name,
                                            TypeAttributes.Public | 
                                            TypeAttributes.Interface |
                                            TypeAttributes.Abstract,
                                            parentType);
        ifaceTable.Add(name, ifaceTb);

        TypeInfo.MethodDescriptor[] descs;
        try {
            descs = TypeInfo.GetMethodData(name);
        } catch (Exception ex) {
            Console.WriteLine("skipping interface {0}: {1}", name,
                              ex.Message);
            return null;
        }

        if (descs == null) {
            Console.WriteLine("No descs!");
            return null;
        }

        if (verbose) {
            Console.WriteLine("{0} has {1} methods ({2} inherited from {3})",
                              name, descs.Length, inheritedMethodCount,
                              parentName);
        }
        for (int i = inheritedMethodCount; i < descs.Length; i++) {
            GenerateInterfaceMethod(ifaceTb, descs[i]);
        }

        ifaceTb.CreateType();
        return ifaceTb;
    }

    void GenerateInterfaceAssembly(string[] args)
    {
        String dllName = args[0];
        String onlyInterface = null;
        if (args.Length > 1)
            onlyInterface = args[1];
        AssemblyName an = new AssemblyName();
        an.Version = new Version(1, 0, 0, 0);
        an.Name = "Mozilla.XPCOM.Interfaces";
        
        AssemblyBuilder ab = AppDomain.CurrentDomain.
            DefineDynamicAssembly(an, AssemblyBuilderAccess.RunAndSave);
        mb = ab.DefineDynamicModule(an.Name, dllName);

        if (onlyInterface != null) {
            verbose = true;
            EmitOneInterface(onlyInterface);
        } else {
            IntPtr e = TypeInfo.typeinfo_EnumerateInterfacesStart();
            if (e == IntPtr.Zero)
                return;
            
            while (true) {
                String name = TypeInfo.typeinfo_EnumerateInterfacesNext(e);
                if (name == null)
                    break;
                EmitOneInterface(name);
            }
            
            TypeInfo.typeinfo_EnumerateInterfacesStop(e);
        }

        ab.Save(dllName);

        Console.WriteLine("Generated metadata for {0} interfaces in {1}",
                          ifaceTable.Count, dllName);
    }
    
    public static void Main(String[] args)
    {
        Components.Init();
        
        Generate gen = new Generate();
        gen.GenerateInterfaceAssembly(args);
    }
}
