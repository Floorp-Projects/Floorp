#T returncode: 2
all:
	mkdir newdir
	test -d newdir
	touch newdir/newfile
	$(RM) newdir
	@echo TEST-PASS
