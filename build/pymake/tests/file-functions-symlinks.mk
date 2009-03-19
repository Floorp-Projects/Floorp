#T returncode-on: {'win32': 2}
$(shell \
touch test.file; \
ln -s test.file test.symlink; \
ln -s test.missing missing.symlink; \
touch .testhidden; \
mkdir foo; \
touch foo/testfile; \
ln -s foo symdir; \
)

all:
	test "$(abspath test.file test.symlink)" = "$(CURDIR)/test.file $(CURDIR)/test.symlink"
	test "$(realpath test.file test.symlink)" = "$(CURDIR)/test.file $(CURDIR)/test.file"
	test "$(sort $(wildcard *))" = "foo symdir test.file test.symlink"
	test "$(sort $(wildcard .*))" = ". .. .testhidden"
	test "$(sort $(wildcard test*))" = "test.file test.symlink"
	test "$(sort $(wildcard foo/*))" = "foo/testfile"
	test "$(sort $(wildcard ./*))" = "./foo ./symdir ./test.file ./test.symlink"
	test "$(sort $(wildcard f?o/*))" = "foo/testfile"
	test "$(sort $(wildcard */*))" = "foo/testfile symdir/testfile"
	@echo TEST-PASS
