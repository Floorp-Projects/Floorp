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

class InterfaceGenerator
{
    static ModuleBuilder module;

    static InterfaceGenerator()
    {
        AssemblyName an = new AssemblyName();
        an.Version = new Version(1, 0, 0, 0);
        an.Name = "Mozilla.XPCOM.Interfaces";

        AppDomain curDom = AppDomain.CurrentDomain;
        AssemblyBuilder ab =
            curDom.DefineDynamicAssembly(an, AssemblyBuilderAccess.Run);

        module = ab.DefineDynamicModule(an.Name);
    }

    String typeName;
    TypeBuilder tb;
    
    internal InterfaceGenerator(String name)
    {
        typeName = name;
    }

    const PropertyAttributes PROPERTY_ATTRS = PropertyAttributes.None;
    const MethodAttributes METHOD_ATTRS = MethodAttributes.Public |
            MethodAttributes.Abstract | MethodAttributes.Virtual;
    PropertyBuilder lastProperty = null;

    Type FixupInterfaceType(TypeInfo.ParamDescriptor desc)
    {
        try {
            return TypeInfo.TypeForIID(desc.GetIID());
        } catch (Exception e) {
            // Console.WriteLine(e);
            return typeof(object);
        }
    }

    void GenerateInterfaceMethod(TypeInfo.MethodDescriptor desc)
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
                                             desc.argTypes);

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

    internal Assembly Generate()
    {
        if (module.GetType(typeName) != null)
            return module.Assembly;

        String ifaceName = typeName.Replace("Mozilla.XPCOM.Interfaces.", "");
        TypeInfo.MethodDescriptor[] descs =
            TypeInfo.GetMethodData(ifaceName);

        ushort inheritedMethodCount;
        String parentName = TypeInfo.GetParentInfo(ifaceName,
                                                   out inheritedMethodCount);
        Type parentType;

        if (parentName != null) {
            parentName = "Mozilla.XPCOM.Interfaces." + parentName;
            parentType = module.Assembly.GetType(parentName);
            if (parentType == null) {
                InterfaceGenerator gen = new InterfaceGenerator(parentName);
                parentType = gen.Generate().GetType(parentName);
            }
        } else {
            parentType = typeof(object);
        }

        tb = module.DefineType(typeName,
                               TypeAttributes.Public | TypeAttributes.Interface,
                               parentType);

        for (int i = inheritedMethodCount; i < descs.Length; i++) {
            if (descs[i] != null)
                GenerateInterfaceMethod(descs[i]);
        }

        tb.CreateType();

        return module.Assembly;
    }

}

}
