def WebIDLTest(parser, harness):
    threw = False
    try:
        parser.parse(
            """
            interface NullableVoid {
              void? foo();
            };
        """
        )

        parser.finish()
    except Exception:
        threw = True

    harness.ok(threw, "Should have thrown.")
