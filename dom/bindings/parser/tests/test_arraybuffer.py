import WebIDL

def WebIDLTest(parser, harness):
    parser.parse("""
        interface TestArrayBuffer {
          attribute ArrayBuffer attr;
          void method(ArrayBuffer arg1, ArrayBuffer? arg2, ArrayBuffer[] arg3, sequence<ArrayBuffer> arg4);
        };
    """)

    results = parser.finish()

    iface = results[0]

    harness.ok(True, "TestArrayBuffer interface parsed without error")
    harness.check(len(iface.members), 2, "Interface should have two members")

    attr = iface.members[0]
    method = iface.members[1]

    harness.ok(isinstance(attr, WebIDL.IDLAttribute), "Expect an IDLAttribute")
    harness.ok(isinstance(method, WebIDL.IDLMethod), "Expect an IDLMethod")

    harness.check(str(attr.type), "ArrayBuffer", "Expect an ArrayBuffer type")
    harness.ok(attr.type.isArrayBuffer(), "Expect an ArrayBuffer type")
