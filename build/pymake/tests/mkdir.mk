MKDIR ?= mkdir

all:
	$(MKDIR) newdir
	test -d newdir
	# subdir, parent exists
	$(MKDIR) newdir/subdir
	test -d newdir/subdir
	# -p, existing dir
	$(MKDIR) -p newdir
	# -p, existing subdir
	$(MKDIR) -p newdir/subdir
	# multiple subdirs, existing parent
	$(MKDIR) newdir/subdir1 newdir/subdir2
	test -d newdir/subdir1 -a -d newdir/subdir2
	rm -r newdir
	# -p, subdir, no existing parent
	$(MKDIR) -p newdir/subdir
	test -d newdir/subdir
	rm -r newdir
	# -p, multiple subdirs, no existing parent
	$(MKDIR) -p newdir/subdir1 newdir/subdir2
	test -d newdir/subdir1 -a -d newdir/subdir2
	# -p, multiple existing subdirs
	$(MKDIR) -p newdir/subdir1 newdir/subdir2
	rm -r newdir
	@echo TEST-PASS
