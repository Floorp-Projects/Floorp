$(shell \
touch test.file; \
touch .testhidden; \
mkdir foo; \
touch foo/testfile; \
)

all:
	test "$(abspath test.file)" = "$(CURDIR)/test.file"
	test "$(realpath test.file)" = "$(CURDIR)/test.file"
	test "$(sort $(wildcard *))" = "foo test.file"
# commented out because GNU make matches . and .. while python doesn't, and I don't
# care enough
#	test "$(sort $(wildcard .*))" = ". .. .testhidden"
	test "$(sort $(wildcard test*))" = "test.file"
	test "$(sort $(wildcard foo/*))" = "foo/testfile"
	test "$(sort $(wildcard ./*))" = "./foo ./test.file"
	test "$(sort $(wildcard f?o/*))" = "foo/testfile"
	@echo TEST-PASS
