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
	$(RM) -r newdir
	test ! -d newdir
	@echo TEST-PASS
