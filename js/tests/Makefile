# -*- Makefile -*-

TEST_HTTP ?= test.mozilla.com
TEST_JS = $(shell find . -name '*.js' -print)
CURRDIR=$(shell pwd)
JSDIR=$(shell basename $(CURRDIR))

all: menu.html 1.8.0-list.txt 1.8.1-list.txt menu-list.txt excluded-n.tests spidermonkey-extensions-n.tests

menu.html: menuhead.html menufoot.html Makefile excluded-n.tests $(TEST_JS)
	perl mklistpage.pl > menubody.html
	cat menuhead.html menubody.html menufoot.html > menu.html

excluded-n.tests: spidermonkey-n.tests slow-n.tests
	cat $^ > $@

spidermonkey-extensions-n.tests: $(TEST_JS)
	find . -name '*.js' | grep -v shell.js | grep -v browser.js | grep '/extensions/' | sort > $@

menu-list.txt:
	echo "http://$(TEST_HTTP)/tests/mozilla.org/$(JSDIR)/menu.html" > menu-list.txt

1.8.0-list.txt: $(TEST_JS) excluded-n.tests
	./list.sh e4x ecma ecma_2 ecma_3 js1_1 js1_2 js1_3 js1_4 js1_5 js1_6 | sort > 1.8.0-list.txt

1.8.1-list.txt: $(TEST_JS) 1.8.0-list.txt  excluded-n.tests
	cat 1.8.0-list.txt > 1.8.1-list.txt
	./list.sh js1_7 | sort >> 1.8.1-list.txt

#e4x-list.txt: $(TEST_JS)
#	./list.sh e4x | sort > e4x-list.txt

#lc2-list.txt: $(TEST_JS)
#	./list.sh lc2 | sort > lc2-list.txt

ecma_3/RegExp/re_tests:
	curl -LsS -m 1800 "http://www.cpan.org/src/stable.tar.bz2" -o stable.tar.bz2
	tar -jxvf stable.tar.bz2
	cp `find . -maxdepth 1 -name 'perl-*'`/t/op/re_tests ecma_3/RegExp/re_tests
	rm -fR `find . -maxdepth 1 -name 'perl-*'` stable.tar.bz2

clean:
	rm -f menubody.html menu.html *-list.txt excluded-n.tests

