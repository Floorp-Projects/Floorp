def WebIDLTest(parser, harness):
    threw = False
    try:
        parser.parse("""
            interface Promise {};
            interface A {
              legacycaller Promise foo();
            };
        """)
        results = parser.finish()

    except:
        threw = True
    harness.ok(threw,
               "Should not allow Promise return values for legacycaller.")

    parser = parser.reset()
    threw = False
    try:
        parser.parse("""
            interface Promise {};
            interface A {
              Promise foo();
              long foo(long arg);
            };
        """)
        results = parser.finish();
    except:
        threw = True
    harness.ok(threw,
               "Should not allow overloads which have both Promise and "
               "non-Promise return types.")

    parser = parser.reset()
    threw = False
    try:
        parser.parse("""
            interface Promise {};
            interface A {
              long foo(long arg);
              Promise foo();
            };
        """)
        results = parser.finish();
    except:
        threw = True
    harness.ok(threw,
               "Should not allow overloads which have both Promise and "
               "non-Promise return types.")

    parser = parser.reset()
    parser.parse("""
        interface Promise {};
        interface A {
          Promise foo();
          Promise foo(long arg);
        };
    """)
    results = parser.finish();

    harness.ok(True,
               "Should allow overloads which only have Promise and return "
               "types.")
