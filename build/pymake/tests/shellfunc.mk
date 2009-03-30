all: testfile
	test "$(shell cat $<)" = "Hello world"
	test "$(shell printf "\n")" = ""
	@echo TEST-PASS

testfile:
	printf "Hello\nworld\n" > $@
