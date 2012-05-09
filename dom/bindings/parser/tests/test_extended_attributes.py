import WebIDL

def WebIDLTest(parser, harness):
    parser.parse("""
        [Flippety]
        interface TestExtendedAttr {
          [Foopy] attribute byte b;
        };
    """)

    results = parser.finish()

    parser = parser.reset()
    parser.parse("""
        [Flippety="foo.bar",Floppety=flop]
        interface TestExtendedAttr {
          [Foopy="foo.bar"] attribute byte b;
        };
    """)

    results = parser.finish()
