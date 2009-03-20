$(shell echo "INCLUDED2 = yes" >local-include.inc)

include $(TESTPATH)/include-file.inc local-include.inc

all:
	test "$(INCLUDED)" = "yes"
	test "$(INCLUDED2)" = "yes"
	@echo TEST-PASS
