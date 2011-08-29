#T gmake skip
all:
	mkdir shell-glob-test
	touch shell-glob-test/foo.txt
	touch shell-glob-test/bar.txt
	touch shell-glob-test/a.foo
	touch shell-glob-test/b.foo
	$(RM) shell-glob-test/*.txt
	$(RM) shell-glob-test/?.foo
	rmdir shell-glob-test
	@echo TEST-PASS
