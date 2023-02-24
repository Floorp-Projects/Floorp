def WebIDLTest(parser, harness):
    try:
        parser.parse(
            """
            enum TestEnumDuplicateValue {
              "",
              ""
            };
        """
        )
        harness.ok(False, "Should have thrown!")
    except Exception:
        harness.ok(True, "Enum TestEnumDuplicateValue should throw")
