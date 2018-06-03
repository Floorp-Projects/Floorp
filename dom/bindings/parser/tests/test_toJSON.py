def WebIDLTest(parser, harness):
    threw = False
    try:
        parser.parse(
            """
            interface Test {
              object toJSON();
            };
            """)
        results = parser.finish()
    except:
        threw = True
    harness.ok(not threw, "Should allow a toJSON method.")

    parser = parser.reset()
    threw = False
    try:
        parser.parse(
            """
            interface Test {
              object toJSON(object arg);
              object toJSON(long arg);
            };
            """)
        results = parser.finish()
    except:
        threw = True
    harness.ok(threw, "Should not allow overloads of a toJSON method.")

    parser = parser.reset()
    threw = False
    try:
        parser.parse(
            """
            interface Test {
              object toJSON(object arg);
            };
            """)
        results = parser.finish()
    except:
        threw = True
    harness.ok(threw, "Should not allow a toJSON method with arguments.")

    parser = parser.reset()
    threw = False
    try:
        parser.parse(
            """
            interface Test {
              long toJSON();
            };
            """)
        results = parser.finish()
    except:
        threw = True
    harness.ok(not threw, "Should allow a toJSON method with 'long' as return type.")

    parser = parser.reset()
    threw = False
    try:
        parser.parse(
            """
            interface Test {
              [Default] object toJSON();
            };
            """)
        results = parser.finish()
    except:
        threw = True
    harness.ok(not threw, "Should allow a default toJSON method with 'object' as return type.")

    parser = parser.reset()
    threw = False
    try:
        parser.parse(
            """
            interface Test {
              [Default] long toJSON();
            };
            """)
        results = parser.finish()
    except:
        threw = True
    harness.ok(threw, "Should not allow a default toJSON method with non-'object' as return type.")

    parser = parser.reset()
    threw = False
    try:
        parser.parse(
            """
            interface Test {
              any toJSON();
            };
            """)
        results = parser.finish()
    except:
        threw = True
    harness.ok(threw, "Should not allow a toJSON method with a non-JSON return type.")

    # We should probably write some tests here about what types are
    # considered JSON types.  Bug 1462537.
