def WebIDLTest(parser, harness):
    threw = False
    try:
        parser.parse(
            """
            interface VoidArgument1 {
              void foo(void arg2);
            };
        """
        )

        parser.finish()
    except Exception:
        threw = True

    harness.ok(threw, "Should have thrown.")
