def WebIDLTest(parser, harness):
    threw = False
    try:
        parser.parse("""
            interface Child : Parent {
            };
            interface Parent {
              [Unforgeable] readonly attribute long foo;
            };
        """)

        results = parser.finish()
    except:
        threw = True

    harness.ok(threw, "Should have thrown.")

    parser = parser.reset();
    threw = False
    try:
        parser.parse("""
            interface Child : Parent {
            };
            interface Parent {};
            interface Consequential {
              [Unforgeable] readonly attribute long foo;
            };
            Parent implements Consequential;
        """)

        results = parser.finish()
    except:
        threw = True

    harness.ok(threw, "Should have thrown.")

    parser = parser.reset();
    threw = False
    try:
        parser.parse("""
            interface iface {
              [Unforgeable] attribute long foo;
            };
        """)

        results = parser.finish()
    except:
        threw = True

    harness.ok(threw, "Should have thrown.")
