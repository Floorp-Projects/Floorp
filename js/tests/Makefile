# -*- Makefile -*-

TEST_HTTP ?= test.mozilla.com
TEST_JS = $(shell find . -name '*.js' -print)
CURRDIR=$(shell pwd)
JSDIR=$(shell basename $(CURRDIR))

all: menu.html js-list.txt e4x-list.txt lc-list.txt menu-list.txt

menu.html: menuhead.html menufoot.html Makefile spidermonkey-n.tests $(TEST_JS)
	perl mklistpage.pl > menubody.html
	cat menuhead.html menubody.html menufoot.html > menu.html

menu-list.txt:
	echo "http://$(TEST_HTTP)/tests/mozilla.org/$(JSDIR)/menu.html" > menu-list.txt

js-list.txt: $(TEST_JS)
	./list.sh ecma ecma_2 ecma_3 js1_1 js1_2 js1_3 js1_4 js1_5 js1_6 js1_7 | sort > js-list.txt

e4x-list.txt: $(TEST_JS)
	./list.sh e4x | sort > e4x-list.txt

lc-list.txt: $(TEST_JS)
	./list.sh lc2 lc3 | sort > lc-list.txt

clean:
	rm -f menubody.html menu.html js-list.txt e4x-list.txt lc-list.txt menu-list.txt

