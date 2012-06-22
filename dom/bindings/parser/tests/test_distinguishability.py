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

    def firstArgType(method):
        return method.signatures()[0][1][0].type

    dictType = firstArgType(dictMethod)
    ifaceType = firstArgType(ifaceMethod)

    harness.ok(dictType.isDictionary(), "Should have dictionary type");
    harness.ok(ifaceType.isInterface(), "Should have interface type");
    harness.ok(ifaceType.isCallbackInterface(), "Should have callback interface type");

    harness.ok(not dictType.isDistinguishableFrom(ifaceType),
               "Dictionary not distinguishable from callback interface")
    harness.ok(not ifaceType.isDistinguishableFrom(dictType),
               "Callback interface not distinguishable from dictionary")
