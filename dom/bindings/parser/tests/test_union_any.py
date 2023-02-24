def WebIDLTest(parser, harness):
    threw = False
    try:
        parser.parse(
            """
            interface AnyNotInUnion {
              undefined foo((any or DOMString) arg);
            };
        """
        )

        parser.finish()
    except Exception:
        threw = True

    harness.ok(threw, "Should have thrown.")
