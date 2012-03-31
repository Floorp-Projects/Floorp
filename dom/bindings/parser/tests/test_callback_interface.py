import WebIDL

def WebIDLTest(parser, harness):
    parser.parse("""
        callback interface TestCallbackInterface {
          attribute boolean bool;
        };
    """)

    results = parser.finish()

    iface = results[0]

    harness.ok(iface.isCallback(), "Interface should be a callback")
