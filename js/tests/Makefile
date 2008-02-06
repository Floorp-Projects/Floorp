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

failures.txt: public-failures.txt confidential-failures.txt
	cp public-failures.txt public-failures.txt.save
	cp confidential-failures.txt confidential-failures.txt.save
	sort < public-failures.txt | uniq | ./create-patterns.pl > public-failures.$$
	mv public-failures.$$ public-failures.txt
	sort < confidential-failures.txt | uniq | ./create-patterns.pl > confidential-failures.$$
	mv confidential-failures.$$ confidential-failures.txt
	cat public-failures.txt confidential-failures.txt | sort | uniq > failures.txt

clean:
	rm -f menubody.html menu.html menu-list.txt failures.txt excluded-*.tests included-*.tests urllist*.html urllist*.tests


