all: testfile
	test "$(shell cat $<)" = "Hello world"
	@echo TEST-PASS

testfile:
	printf "Hello\nworld\n" > $@
