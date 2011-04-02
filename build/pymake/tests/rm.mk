all:
# $(RM) defaults to -f
	$(RM) nosuchfile
	touch newfile
	test -f newfile
	$(RM) newfile
	test ! -f newfile
	mkdir newdir
	test -d newdir
	touch newdir/newfile
	mkdir newdir/subdir
	$(RM) -r newdir/subdir
	test ! -d newdir/subdir
	test -d newdir
	mkdir newdir/subdir1 newdir/subdir2
	$(RM) -r newdir/subdir1 newdir/subdir2
	test ! -d newdir/subdir1 -a ! -d newdir/subdir2
	test -d newdir
	$(RM) -r newdir
	test ! -d newdir
	@echo TEST-PASS
