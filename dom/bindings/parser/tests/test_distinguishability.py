def firstArgType(method):
    return method.signatures()[0][1][0].type

def WebIDLTest(parser, harness):
    parser.parse("""
      dictionary Dict {
      };
      callback interface Foo {
      };
      interface Bar {
        // Bit of a pain to get things that have dictionary types
        void passDict(Dict arg);
        void passFoo(Foo arg);
      };
    """)
    results = parser.finish()

    iface = results[2]
    harness.ok(iface.isInterface(), "Should have interface")
    dictMethod = iface.members[0]
    ifaceMethod = iface.members[1]

    dictType = firstArgType(dictMethod)
    ifaceType = firstArgType(ifaceMethod)

    harness.ok(dictType.isDictionary(), "Should have dictionary type");
    harness.ok(ifaceType.isInterface(), "Should have interface type");
    harness.ok(ifaceType.isCallbackInterface(), "Should have callback interface type");

    harness.ok(not dictType.isDistinguishableFrom(ifaceType),
               "Dictionary not distinguishable from callback interface")
    harness.ok(not ifaceType.isDistinguishableFrom(dictType),
               "Callback interface not distinguishable from dictionary")

    parser = parser.reset()
    parser.parse("""
      interface TestIface {
        void passKid(Kid arg);
        void passParent(Parent arg);
        void passGrandparent(Grandparent arg);
        void passImplemented(Implemented arg);
        void passImplementedParent(ImplementedParent arg);
        void passUnrelated1(Unrelated1 arg);
        void passUnrelated2(Unrelated2 arg);
        void passArrayBuffer(ArrayBuffer arg);
        void passArrayBuffer(ArrayBufferView arg);
      };

      interface Kid : Parent {};
      interface Parent : Grandparent {};
      interface Grandparent {};
      interface Implemented : ImplementedParent {};
      Parent implements Implemented;
      interface ImplementedParent {};
      interface Unrelated1 {};
      interface Unrelated2 {};
    """)
    results = parser.finish()

    iface = results[0]
    harness.ok(iface.isInterface(), "Should have interface")
    argTypes = [firstArgType(method) for method in iface.members]
    unrelatedTypes = [firstArgType(method) for method in iface.members[-3:]]

    for type1 in argTypes:
        for type2 in argTypes:
            distinguishable = (type1 is not type2 and
                               (type1 in unrelatedTypes or
                                type2 in unrelatedTypes))

            harness.check(type1.isDistinguishableFrom(type2),
                          distinguishable,
                          "Type %s should %sbe distinguishable from type %s" %
                          (type1, "" if distinguishable else "not ", type2))
            harness.check(type2.isDistinguishableFrom(type1),
                          distinguishable,
                          "Type %s should %sbe distinguishable from type %s" %
                          (type2, "" if distinguishable else "not ", type1))
