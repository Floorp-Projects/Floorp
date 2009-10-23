all: test
	@echo TEST-PASS

test: ; test "Hello \
	  world" = "Hello   world"
