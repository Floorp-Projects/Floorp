#T returncode: 1
all:
	mkdir newdir/subdir
	test ! -d newdir/subdir
	test ! -d newdir
	rm -r newdir
	@echo TEST-PASS
