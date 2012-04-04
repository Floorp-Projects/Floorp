import WebIDL

def WebIDLTest(parser, harness):
    parser.parse("""
        [Flippety]
        interface TestExtendedAttr {
          [Foopy] attribute byte b;
        };
    """)

    results = parser.finish()
