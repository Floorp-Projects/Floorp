def WebIDLTest(parser, harness):
    threw = False
    try:
        parser.parse("""
            interface AttrSquenceType {
              attribute sequence<bool> foo;
            };
        """)

        results = parser.finish()
    except:
        threw = True

    harness.ok(threw, "Should have thrown.")
