# -*- Makefile -*-
TEST_HTTP ?= test.mozilla.com
TEST_JS = $(shell find . -name '*.js' -print)
CURRDIR=$(shell pwd)
JSDIR=$(shell basename $(CURRDIR))

all: menu.html \
	menu-list.txt \
	spidermonkey-extensions-n.tests \
	failures.txt

menu.html: menuhead.html menufoot.html Makefile  spidermonkey-n.tests $(TEST_JS)
	perl mklistpage.pl > menubody.html
	cat menuhead.html menubody.html menufoot.html > menu.html

spidermonkey-extensions-n.tests: $(TEST_JS)
	find . -name '*.js' | grep -v shell.js | grep -v browser.js | grep '/extensions/' | sed 's|\.\/||' | sort > $@

menu-list.txt:
	echo "http://$(TEST_HTTP)/tests/mozilla.org/$(JSDIR)/menu.html" > menu-list.txt

confidential-failures.txt:
	touch confidential-failures.txt

public-failures.txt.expanded: public-failures.txt universe.data
	pattern-expander.pl public-failures.txt > public-failures.txt.expanded

confidential-failures.txt.expanded: confidential-failures.txt universe.data
	pattern-expander.pl confidential-failures.txt > confidential-failures.txt.expanded

failures.txt: public-failures.txt.expanded confidential-failures.txt.expanded
	sort -u public-failures.txt.expanded confidential-failures.txt.expanded > failures.txt

clean:
	rm -f menubody.html menu.html menu-list.txt failures.txt *failures.txt.expanded excluded-*.tests included-*.tests urllist*.html urllist*.tests
