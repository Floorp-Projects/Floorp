import WebIDL


def WebIDLTest(parser, harness):
    threw = False
    try:
        parser.parse(
            """
            [Global, Exposed=TestConstructorGlobal]
            interface TestConstructorGlobal {
              constructor();
            };
        """
        )

        parser.finish()
    except WebIDL.WebIDLError:
        threw = True

    harness.ok(threw, "Should have thrown.")

    parser = parser.reset()
    threw = False
    try:
        parser.parse(
            """
            [Global, Exposed=TestLegacyFactoryFunctionGlobal,
             LegacyFactoryFunction=FooBar]
            interface TestLegacyFactoryFunctionGlobal {
            };
        """
        )
        parser.finish()
    except WebIDL.WebIDLError:
        threw = True

    harness.ok(threw, "Should have thrown.")

    parser = parser.reset()
    threw = False
    try:
        parser.parse(
            """
            [LegacyFactoryFunction=FooBar, Global,
             Exposed=TestLegacyFactoryFunctionGlobal]
            interface TestLegacyFactoryFunctionGlobal {
            };
        """
        )
        parser.finish()
    except WebIDL.WebIDLError:
        threw = True

    harness.ok(threw, "Should have thrown.")

    parser = parser.reset()
    threw = False
    try:
        parser.parse(
            """
            [Global, Exposed=TestHTMLConstructorGlobal]
            interface TestHTMLConstructorGlobal {
              [HTMLConstructor] constructor();
            };
        """
        )

        parser.finish()
    except WebIDL.WebIDLError:
        threw = True

    harness.ok(threw, "Should have thrown.")
